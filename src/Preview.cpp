#include "Preview.h"

#include "REX/W32/D3D11.h"

namespace RMA::Preview
{
	namespace
	{
		namespace W = REX::W32;

		constexpr std::uint32_t kMaxPreviewSize = 256;
		constexpr std::uint32_t kMaxFileSize = 64u * 1024u * 1024u;
		constexpr std::size_t   kMaxCached = 256;

		constexpr std::uint32_t FourCC(const char (&a_c)[5])
		{
			return static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[0])) |
			       (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[1])) << 8) |
			       (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[2])) << 16) |
			       (static_cast<std::uint32_t>(static_cast<std::uint8_t>(a_c[3])) << 24);
		}

#pragma pack(push, 1)
		struct PixelFormat
		{
			std::uint32_t size, flags, fourCC, rgbBitCount;
			std::uint32_t rMask, gMask, bMask, aMask;
		};
		struct Header
		{
			std::uint32_t size, flags, height, width, pitchOrLinearSize, depth, mipMapCount;
			std::uint32_t reserved1[11];
			PixelFormat   pf;
			std::uint32_t caps, caps2, caps3, caps4, reserved2;
		};
		struct HeaderDX10
		{
			std::uint32_t dxgiFormat, resourceDimension, miscFlag, arraySize, miscFlags2;
		};
#pragma pack(pop)

		enum class Convert
		{
			None,      // upload as stored
			Rgb24,     // B,G,R -> RGBA8
			Lum8,      // L -> RGBA8 grey
			LumAlpha,  // L,A -> RGBA8
			Alpha8     // A -> white RGBA8
		};

		struct Layout
		{
			W::DXGI_FORMAT   format{ W::DXGI_FORMAT_UNKNOWN };
			std::uint32_t blockBytes{ 0 };  // 0 when not block compressed
			std::uint32_t pixelBytes{ 0 };  // source bytes per pixel when uncompressed
			Convert       convert{ Convert::None };
		};

		std::optional<Layout> Describe(const Header& a_header, const HeaderDX10* a_dx10)
		{
			const auto& pf = a_header.pf;
			if (a_dx10) {
				switch (a_dx10->dxgiFormat) {
				case 71:
				case 72:
				case 80:
				case 81:
					return Layout{ static_cast<W::DXGI_FORMAT>(a_dx10->dxgiFormat), 8 };
				case 74:
				case 75:
				case 77:
				case 78:
				case 83:
				case 84:
				case 98:
				case 99:
					return Layout{ static_cast<W::DXGI_FORMAT>(a_dx10->dxgiFormat), 16 };
				case 28:
				case 29:
				case 87:
				case 88:
				case 91:
					return Layout{ static_cast<W::DXGI_FORMAT>(a_dx10->dxgiFormat), 0, 4 };
				default:
					return std::nullopt;
				}
			}
			if (pf.flags & 0x4) {  // DDPF_FOURCC
				switch (pf.fourCC) {
				case FourCC("DXT1"):
					return Layout{ W::DXGI_FORMAT_BC1_UNORM, 8 };
				case FourCC("DXT2"):
				case FourCC("DXT3"):
					return Layout{ W::DXGI_FORMAT_BC2_UNORM, 16 };
				case FourCC("DXT4"):
				case FourCC("DXT5"):
					return Layout{ W::DXGI_FORMAT_BC3_UNORM, 16 };
				case FourCC("ATI1"):
				case FourCC("BC4U"):
					return Layout{ W::DXGI_FORMAT_BC4_UNORM, 8 };
				case FourCC("ATI2"):
				case FourCC("BC5U"):
					return Layout{ W::DXGI_FORMAT_BC5_UNORM, 16 };
				default:
					return std::nullopt;
				}
			}
			if ((pf.flags & 0x40) && pf.rgbBitCount == 32) {  // DDPF_RGB
				return Layout{ pf.rMask == 0x000000ff ? W::DXGI_FORMAT_R8G8B8A8_UNORM : W::DXGI_FORMAT_B8G8R8A8_UNORM, 0, 4 };
			}
			if ((pf.flags & 0x40) && pf.rgbBitCount == 24) {
				return Layout{ W::DXGI_FORMAT_R8G8B8A8_UNORM, 0, 3, Convert::Rgb24 };
			}
			if ((pf.flags & 0x20000) && pf.rgbBitCount == 8) {  // DDPF_LUMINANCE
				return Layout{ W::DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1, Convert::Lum8 };
			}
			if ((pf.flags & 0x20000) && pf.rgbBitCount == 16) {
				return Layout{ W::DXGI_FORMAT_R8G8B8A8_UNORM, 0, 2, Convert::LumAlpha };
			}
			if ((pf.flags & 0x2) && pf.rgbBitCount == 8) {  // DDPF_ALPHA
				return Layout{ W::DXGI_FORMAT_R8G8B8A8_UNORM, 0, 1, Convert::Alpha8 };
			}
			return std::nullopt;
		}

		std::size_t LevelBytes(const Layout& a_layout, std::uint32_t a_w, std::uint32_t a_h)
		{
			if (a_layout.blockBytes) {
				return static_cast<std::size_t>(std::max(1u, (a_w + 3) / 4)) * std::max(1u, (a_h + 3) / 4) * a_layout.blockBytes;
			}
			return static_cast<std::size_t>(a_w) * a_h * a_layout.pixelBytes;
		}

		bool ReadFile(const std::string& a_path, std::vector<std::uint8_t>& a_out)
		{
			std::string normalized = a_path;
			std::ranges::replace(normalized, '/', '\\');
			const auto lower = [](std::string s) {
				for (auto& c : s) {
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				}
				return s;
			}(normalized);

			std::vector<std::string> candidates;
			if (lower.starts_with("data\\")) {
				candidates.push_back(normalized.substr(5));
			} else if (lower.starts_with("textures\\")) {
				candidates.push_back(normalized);
			} else {
				candidates.push_back("textures\\" + normalized);
				candidates.push_back(normalized);
			}

			for (const auto& candidate : candidates) {
				RE::BSResourceNiBinaryStream stream(candidate.c_str());
				if (!stream.good()) {
					continue;
				}
				// read in blocks: the size reported by get_info is not reliable
				// for files that come out of a BSA
				a_out.clear();
				std::uint8_t buffer[64 * 1024];
				for (;;) {
					std::uint64_t got = 0;
					stream.stream->DoRead(buffer, sizeof(buffer), got);
					if (got == 0) {
						break;
					}
					a_out.insert(a_out.end(), buffer, buffer + got);
					if (a_out.size() > kMaxFileSize) {
						break;
					}
				}
				if (a_out.size() >= 128 && a_out.size() <= kMaxFileSize) {
					return true;
				}
				logger::info("preview: {} opened as {} but read {} bytes", a_path, candidate, a_out.size());
			}
			if (a_out.empty()) {
				logger::info("preview: {} not found (tried {})", a_path, candidates.front());
			}
			return false;
		}

		std::optional<Image> Load(const std::string& a_path)
		{
			std::vector<std::uint8_t> file;
			if (!ReadFile(a_path, file)) {
				return std::nullopt;
			}
			if (std::memcmp(file.data(), "DDS ", 4) != 0) {
				logger::info("preview: {} is not a DDS file", a_path);
				return std::nullopt;
			}
			Header header;
			std::memcpy(&header, file.data() + 4, sizeof(Header));
			if (header.size != 124 || header.width == 0 || header.height == 0) {
				return std::nullopt;
			}
			std::size_t offset = 4 + sizeof(Header);
			HeaderDX10  dx10{};
			const bool  hasDx10 = (header.pf.flags & 0x4) && header.pf.fourCC == FourCC("DX10");
			if (hasDx10) {
				if (file.size() < offset + sizeof(HeaderDX10)) {
					return std::nullopt;
				}
				std::memcpy(&dx10, file.data() + offset, sizeof(HeaderDX10));
				offset += sizeof(HeaderDX10);
			}
			const auto layout = Describe(header, hasDx10 ? &dx10 : nullptr);
			if (!layout) {
				logger::info("preview: {} has an unsupported format (fourCC {:08X}, {} bpp, flags {:X})", a_path, header.pf.fourCC, header.pf.rgbBitCount, header.pf.flags);
				return std::nullopt;
			}

			// the first mip small enough for a preview; block formats need
			// dimensions in whole blocks
			const std::uint32_t mips = std::max(1u, header.mipMapCount);
			std::uint32_t       w = header.width, h = header.height;
			std::size_t         at = offset;
			std::size_t         chosenAt = offset;
			std::uint32_t       chosenW = w, chosenH = h;
			for (std::uint32_t mip = 0; mip < mips; ++mip) {
				const auto bytes = LevelBytes(*layout, w, h);
				if (at + bytes > file.size()) {
					break;
				}
				const bool blockable = !layout->blockBytes || (w % 4 == 0 && h % 4 == 0);
				if (blockable) {
					chosenAt = at;
					chosenW = w;
					chosenH = h;
				}
				if (blockable && w <= kMaxPreviewSize && h <= kMaxPreviewSize) {
					break;
				}
				at += bytes;
				w = std::max(1u, w / 2);
				h = std::max(1u, h / 2);
			}
			if (chosenAt + LevelBytes(*layout, chosenW, chosenH) > file.size()) {
				return std::nullopt;
			}

			std::vector<std::uint8_t> converted;
			const std::uint8_t*       pixels = file.data() + chosenAt;
			std::uint32_t             pitch = layout->blockBytes ? std::max(1u, (chosenW + 3) / 4) * layout->blockBytes : chosenW * layout->pixelBytes;
			if (layout->convert != Convert::None) {
				converted.resize(static_cast<std::size_t>(chosenW) * chosenH * 4);
				for (std::size_t i = 0; i < static_cast<std::size_t>(chosenW) * chosenH; ++i) {
					const std::uint8_t* src = pixels + i * layout->pixelBytes;
					std::uint8_t*       dst = converted.data() + i * 4;
					switch (layout->convert) {
					case Convert::Rgb24:
						dst[0] = src[2], dst[1] = src[1], dst[2] = src[0], dst[3] = 255;
						break;
					case Convert::Lum8:
						dst[0] = dst[1] = dst[2] = src[0], dst[3] = 255;
						break;
					case Convert::LumAlpha:
						dst[0] = dst[1] = dst[2] = src[0], dst[3] = src[1];
						break;
					case Convert::Alpha8:
						dst[0] = dst[1] = dst[2] = 255, dst[3] = src[0];
						break;
					default:
						break;
					}
				}
				pixels = converted.data();
				pitch = chosenW * 4;
			}

			const auto renderer = RE::BSGraphics::Renderer::GetSingleton();
			const auto device = renderer ? renderer->GetRuntimeData().forwarder : nullptr;
			if (!device) {
				return std::nullopt;
			}

			W::D3D11_TEXTURE2D_DESC desc{};
			desc.width = chosenW;
			desc.height = chosenH;
			desc.mipLevels = 1;
			desc.arraySize = 1;
			desc.format = layout->format;
			desc.sampleDesc.count = 1;
			desc.usage = W::D3D11_USAGE_IMMUTABLE;
			desc.bindFlags = W::D3D11_BIND_SHADER_RESOURCE;

			W::D3D11_SUBRESOURCE_DATA data{};
			data.sysMem = pixels;
			data.sysMemPitch = pitch;

			W::ID3D11Texture2D* texture = nullptr;
			if (const auto hr = device->CreateTexture2D(&desc, &data, &texture); hr < 0 || !texture) {
				logger::info("preview: {} CreateTexture2D failed ({}x{}, format {}, hr {:08X})", a_path, chosenW, chosenH, static_cast<int>(layout->format), static_cast<std::uint32_t>(hr));
				return std::nullopt;
			}
			W::ID3D11ShaderResourceView* view = nullptr;
			const auto                hr = device->CreateShaderResourceView(texture, nullptr, &view);
			texture->Release();
			if (hr < 0 || !view) {
				return std::nullopt;
			}
			return Image{ view, static_cast<float>(chosenW), static_cast<float>(chosenH) };
		}

		std::mutex                                            g_lock;
		std::unordered_map<std::string, std::optional<Image>> g_cache;
		// ImGui keeps raw texture pointers in the draw list until the frame is
		// presented, so views are released one frame after they were dropped
		std::vector<void*> g_trash;

		void DropAll()
		{
			for (auto& [path, image] : g_cache) {
				if (image && image->texture) {
					g_trash.push_back(image->texture);
				}
			}
			g_cache.clear();
		}
	}

	const Image* Get(const std::string& a_texturePath)
	{
		if (a_texturePath.empty() || a_texturePath.find("..") != std::string::npos) {
			return nullptr;
		}
		std::scoped_lock lock(g_lock);
		auto             it = g_cache.find(a_texturePath);
		if (it == g_cache.end()) {
			if (g_cache.size() >= kMaxCached) {
				DropAll();
			}
			it = g_cache.emplace(a_texturePath, Load(a_texturePath)).first;
		}
		return it->second ? &*it->second : nullptr;
	}

	void Clear()
	{
		std::scoped_lock lock(g_lock);
		DropAll();
	}

	void Collect()
	{
		std::scoped_lock lock(g_lock);
		for (auto* view : g_trash) {
			static_cast<W::ID3D11ShaderResourceView*>(view)->Release();
		}
		g_trash.clear();
	}
}
