// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "GS/GSRegs.h"

#include <string>
#include <unordered_map>
#include <vector>

class GS3DScreenshot
{
public:
	struct Tri
	{
		// Vertex data
		struct Vert
		{
			float x, y, z, q;
			float u, v;
			u8 r, g, b, a;
		} verts[3];

		// Poly data
		bool culled = false;
		bool texture_enabled = false;
		u32 texture_index = 0;
	};

	// Defines the bounds of a sub-region of a texture for the
	// CLAMP_REGION or REPEAT_REGION wrap mode.
	struct TextureRegion
	{
		u16 u_min, u_max, v_min, v_max;

		u32 Width() const { return u_max - u_min + 1; }
		u32 Height() const { return v_max - v_min + 1; }

		bool operator==(const TextureRegion& other) const
		{
			return (
				u_min == other.u_min && u_max == other.u_max &&
				v_min == other.v_min && v_max == other.v_max);
		}
	};

	bool IsEmpty() const;

	void AddTri(Tri tri);
	void SetTextureName(std::string new_name);
	void SetTextureRegion(const TextureRegion& region, u32 twidth, u32 theight);

	// Decode a cached texture region; pitch is in bytes, palette is null for RGBA.
	static std::vector<u32> CopyTextureRegion(const u8* source, u32 pitch,
		const u32* palette, const TextureRegion& region);

	static GS3DScreenshot::TextureRegion GetTextureRegionForCLAMP(
		GIFRegCLAMP CLAMP, u32 twidth, u32 theight);

	// Where textures/model files do.
	std::string m_dump_dir;

	// Dumps to .obj/.mtl files.
	// filename is without the file extension.
	bool DumpToFile(const std::string& filename) const;

	bool m_texture_dump_failed = false;

private:
	std::vector<Tri> m_tris;

	// List of all texture filenames used.
	std::vector<std::string> m_textures;
	// Index of currently active texture in m_textures.
	u32 m_cur_texture_index = 0;
	// Lookup the index of a string in m_textures by value.
	std::unordered_map<std::string, u32> m_texture_map;

	// When exporting just a subregion of a texture, we need to
	// transform the UVs so [0,1]x[0,1] covers just that region
	// instead of the whole texture.
	//
	//    +------------+  <- original texture
	//    |            |
	//    |    +-----+ |  <- subregion used for wrapping
	//    |    |     | |
	//    |    +-----+ |
	//    +------------+
	float m_u_offset = 0.0f;
	float m_v_offset = 0.0f;
	float m_u_scale = 1.0f;
	float m_v_scale = 1.0f;

	bool DumpOBJ(const std::string& filename) const;
	bool DumpMTL(const std::string& filename) const;
};
