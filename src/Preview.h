#pragma once

// Small previews of paint / tint textures. SKSE Menu Framework's LoadTexture
// only reads loose files, while most tint masks live in BSAs, so the DDS is
// read through the game's resource system and one small mip is uploaded.

namespace RMA::Preview
{
	struct Image
	{
		void* texture{ nullptr };  // ID3D11ShaderResourceView*, usable as ImTextureID
		float width{ 0.0f };
		float height{ 0.0f };
	};

	// nullptr when the file is missing or in a format without a preview
	const Image* Get(const std::string& a_texturePath);

	// drops every cached preview; the views are released by the next Collect()
	void Clear();

	// call once per frame before anything is drawn
	void Collect();
}
