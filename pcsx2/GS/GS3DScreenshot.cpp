// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GS/GS3DScreenshot.h"

#include "common/FileSystem.h"
#include "common/Path.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "fmt/format.h"

namespace
{
	// PCSX2 disables exceptions (including in fmt). fmt::print would abort on
	// an I/O error; use stdio so the caller can check ferror/fclose instead.
	template <typename... Args>
	void WriteText(std::FILE* file, fmt::format_string<Args...> format, Args&&... args)
	{
		const std::string text = fmt::format(format, std::forward<Args>(args)...);
		std::fwrite(text.data(), 1, text.size(), file);
	}
} // namespace

bool GS3DScreenshot::IsEmpty() const
{
	return m_tris.empty();
}

void GS3DScreenshot::SetTextureName(std::string new_name)
{
	const u32 next_texture_index = m_textures.size();
	const auto [it, inserted] = m_texture_map.insert({new_name, next_texture_index});
	if (inserted)
		m_textures.push_back(new_name);

	m_cur_texture_index = it->second;
}

void GS3DScreenshot::SetTextureRegion(const TextureRegion& region, u32 twidth, u32 theight)
{
	// Translate so the subreigon is at the origin, then scale so
	// it fits the unit square.
	m_u_offset = -float(region.u_min) / twidth;
	m_v_offset = -float(region.v_min) / theight;
	m_u_scale = float(twidth) / (region.u_max - region.u_min + 1);
	m_v_scale = float(theight) / (region.v_max - region.v_min + 1);
}

GS3DScreenshot::TextureRegion GS3DScreenshot::GetTextureRegionForCLAMP(
	GIFRegCLAMP CLAMP, u32 twidth, u32 theight)
{
	// See section 3.4.5 "Texture Wrap Modes" in the GS Manual

	GS3DScreenshot::TextureRegion region;

	for (int i = 0; i < 2; i++)
	{
		// 0 = u direction, 1 = v direction
		const auto WM = i == 0 ? CLAMP.WMS : CLAMP.WMT;
		const u32 MIN = i == 0 ? CLAMP.MINU : CLAMP.MINV;
		const u32 MAX = i == 0 ? CLAMP.MAXU : CLAMP.MAXV;
		const u32 dim = i == 0 ? twidth : theight;

		u32 min, max;

		if (WM == CLAMP_REGION_CLAMP)
		{
			min = MIN;
			max = MAX;
		}
		else if (WM == CLAMP_REGION_REPEAT)
		{
			// Quoting the manual:
			//
			// """
			// The following operations are applied to the
			// integer parts (u_int, v_int) of the texel
			// coordinates, and the texel coordinate values are
			// calculated.
			//
			//   u' = (u_int & UMSK) | UFIX
			//   v' = (v_int & VMSK) | VFIX
			//
			// UMSK, VMSK, UFIX, and VFIX are specified in the
			// CLAMP_1 or CLAMP_2 register. They are the same
			// bits as the MINU, MINV, MAXU, and MAXV fields
			// respectively, but are processed differently,
			// according to the wrap mode.
			// """
			//
			// Following the example pictured on the same page,
			// if we assume MSK masks off the low n bits and FIX
			// has the low n bits clear, then this mode repeats a
			// rectangle with FIX giving the offset and MSK the
			// width.

			const auto MSK = MIN;
			const auto FIX = MAX;

			if (
				((MSK + 1) & MSK) == 0 && // MSK = 0b00011111
				(FIX & MSK) == 0 // FIX = 0bXXX00000
			)
			{
				min = FIX;
				max = FIX + MSK;
			}
			else
			{
				// Too weird. Fall back to exporting the whole
				// texture. User will have to figure it out.
				min = 0;
				max = dim - 1;
			}
		}
		else // CLAMP_CLAMP or CLAMP_REPEAT
		{
			// Full texture
			min = 0;
			max = dim - 1;
		}

		max = std::min(max, dim - 1);
		min = std::min(min, max);

		(i == 0 ? region.u_min : region.v_min) = min;
		(i == 0 ? region.u_max : region.v_max) = max;
	}

	return region;
}

void GS3DScreenshot::AddTri(Tri tri)
{
	// Invalid Q can produce infinities/NaNs during perspective recovery. Do not
	// write them into OBJ files (or silently clamp them into giant garbage faces).
	for (const auto& v : tri.verts)
	{
		if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z) ||
			(tri.texture_enabled && (!std::isfinite(v.u) || !std::isfinite(v.v))))
			return;
	}

	// Reject only exactly degenerate geometry, not small or offscreen triangles.
	const auto& a = tri.verts[0];
	const auto& b = tri.verts[1];
	const auto& c = tri.verts[2];
	const double abx = double(b.x) - a.x, aby = double(b.y) - a.y, abz = double(b.z) - a.z;
	const double acx = double(c.x) - a.x, acy = double(c.y) - a.y, acz = double(c.z) - a.z;
	if (aby * acz == abz * acy && abz * acx == abx * acz && abx * acy == aby * acx)
		return;

	if (tri.texture_enabled)
	{
		tri.texture_index = m_cur_texture_index;
		for (int i = 0; i < 3; i++)
		{
			tri.verts[i].u += m_u_offset;
			tri.verts[i].v += m_v_offset;
			tri.verts[i].u *= m_u_scale;
			tri.verts[i].v *= m_v_scale;
			if (!std::isfinite(tri.verts[i].u) || !std::isfinite(tri.verts[i].v))
				return;
		}
	}
	else
	{
		// OBJ still writes vt records, even though this face has no texture.
		for (auto& v : tri.verts)
			v.u = v.v = 0.0f;
	}
	m_tris.push_back(tri);
}

bool GS3DScreenshot::DumpToFile(const std::string& filename) const
{
	return !m_texture_dump_failed && DumpMTL(filename) && DumpOBJ(filename);
}

bool GS3DScreenshot::DumpOBJ(const std::string& filename) const
{
	const std::string path = Path::Combine(m_dump_dir, filename + ".obj");
	auto file = FileSystem::OpenManagedCFile(path.c_str(), "wb");
	if (!file)
		return false;
	std::FILE* fp = file.get();

	WriteText(fp, "# PCSX2 3D Screenshot\n");
	WriteText(fp, "mtllib {}.mtl\n\n", filename);

	for (size_t i = 0; i != m_tris.size(); i++)
	{
		const auto tri = m_tris[i];

		// v - Vertex position and color
		for (u32 i = 0; i < 3; i++)
			WriteText(fp,
				"v {} {} {} {:.3f} {:.3f} {:.3f}\n",
				tri.verts[i].x,
				tri.verts[i].y,
				tri.verts[i].z,
				tri.verts[i].r / 255.0f,
				tri.verts[i].g / 255.0f,
				tri.verts[i].b / 255.0f);

		// vt - Texture coordinates
		for (u32 i = 0; i < 3; i++)
			WriteText(fp,
				"vt {} {}\n",
				tri.verts[i].u,
				1.0f - tri.verts[i].v // UV up conversion
			);

		// g - Group
		// Only if different from last tri
		if (i == 0 || m_tris[i - 1].culled != tri.culled)
			WriteText(fp, "g {}\n", tri.culled ? "Culled" : "Normal");

		// usemtl - Material
		// Only if different from last tri
		if (
			i == 0 ||
			m_tris[i - 1].texture_enabled != tri.texture_enabled ||
			m_tris[i - 1].texture_index != tri.texture_index)
		{
			if (!tri.texture_enabled)
				WriteText(fp, "usemtl NoTexture\n");
			else
				WriteText(fp, "usemtl {}\n", m_textures[tri.texture_index]);
		}

		// f - Face
		WriteText(fp, "f -3/-3 -1/-1 -2/-2\n\n");
	}

	const bool write_ok = std::ferror(fp) == 0;
	return std::fclose(file.release()) == 0 && write_ok;
}

bool GS3DScreenshot::DumpMTL(const std::string& filename) const
{
	const std::string path = Path::Combine(m_dump_dir, filename + ".mtl");
	auto file = FileSystem::OpenManagedCFile(path.c_str(), "wb");
	if (!file)
		return false;
	std::FILE* fp = file.get();

	WriteText(fp, "newmtl NoTexture\n");
	WriteText(fp, "Kd 1 1 1\n\n");

	for (const auto& texture : m_textures)
	{
		WriteText(fp, "newmtl {}\n", texture);
		WriteText(fp, "map_Kd {}\n\n", texture);
	}

	const bool write_ok = std::ferror(fp) == 0;
	return std::fclose(file.release()) == 0 && write_ok;
}

std::vector<u32> GS3DScreenshot::CopyTextureRegion(const u8* source, u32 pitch,
	const u32* palette, const TextureRegion& region)
{
	std::vector<u32> pixels(region.Width() * region.Height());
	u32* dest = pixels.data();
	for (u32 y = region.v_min; y <= region.v_max; y++)
	{
		const u8* row = source + y * pitch;
		if (palette)
		{
			for (u32 x = region.u_min; x <= region.u_max; x++)
				*dest++ = palette[row[x]];
		}
		else
		{
			std::memcpy(dest, row + region.u_min * sizeof(u32), region.Width() * sizeof(u32));
			dest += region.Width();
		}
	}
	return pixels;
}
