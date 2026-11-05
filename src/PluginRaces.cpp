#include "PluginRaces.h"

#include <cstring>
#include <fstream>

#include <zlib.h>

namespace RMA::PluginRaces
{
	namespace
	{
		constexpr std::size_t   kHeaderSize = 24;  // a record's or a group's header
		constexpr std::uint32_t kFlagLocalized = 0x80;
		constexpr std::uint32_t kFlagCompressed = 0x40000;
		constexpr std::uint32_t kMaxGroup = 256u * 1024u * 1024u;  // a damaged size must not ask for gigabytes

		std::uint32_t U32(const char* a_at)
		{
			std::uint32_t value = 0;
			std::memcpy(&value, a_at, 4);
			return value;
		}

		std::uint16_t U16(const char* a_at)
		{
			std::uint16_t value = 0;
			std::memcpy(&value, a_at, 2);
			return value;
		}

		bool Is(const char* a_at, const char* a_type)
		{
			return std::memcmp(a_at, a_type, 4) == 0;
		}

		std::string ZString(std::string_view a_field)
		{
			return std::string(a_field.substr(0, a_field.find('\0')));
		}

		// a_fn(type, data) for each field of a record; XXXX carries the size of a field too long for its own 16 bits
		template <class Fn>
		void ForEachField(std::string_view a_record, Fn&& a_fn)
		{
			std::size_t   at = 0;
			std::uint32_t longSize = 0;
			while (at + 6 <= a_record.size()) {
				const char*       head = a_record.data() + at;
				const std::size_t size = longSize ? longSize : U16(head + 4);
				longSize = 0;
				at += 6;
				if (size > a_record.size() - at) {
					return;
				}
				if (Is(head, "XXXX") && size == 4) {
					longSize = U32(a_record.data() + at);
				} else {
					a_fn(head, a_record.substr(at, size));
				}
				at += size;
			}
		}

		bool Inflate(std::string_view a_packed, std::string& a_out)
		{
			if (a_packed.size() < 4) {
				return false;
			}
			const std::uint32_t size = U32(a_packed.data());
			if (size > kMaxGroup) {
				return false;
			}
			a_out.resize(size);
			uLongf written = size;
			if (uncompress(reinterpret_cast<Bytef*>(a_out.data()), &written, reinterpret_cast<const Bytef*>(a_packed.data() + 4), static_cast<uLong>(a_packed.size() - 4)) != Z_OK) {
				return false;
			}
			a_out.resize(written);
			return true;
		}

		void ReadRecord(std::string_view a_fields, std::uint32_t a_formID, bool a_localized, std::vector<Race>& a_races)
		{
			Race race;
			race.formID = a_formID;
			ForEachField(a_fields, [&](const char* a_type, std::string_view a_data) {
				if (Is(a_type, "EDID")) {
					race.editorID = ZString(a_data);
				} else if (Is(a_type, "SPLO") && a_data.size() >= 4) {
					race.spellList.push_back(U32(a_data.data()));
				} else if (Is(a_type, "DESC")) {
					if (!a_localized) {
						race.description = ZString(a_data);
					} else if (a_data.size() >= 4) {
						race.stringID = U32(a_data.data());
					}
				}
			});
			a_races.push_back(std::move(race));
		}

		void ReadGroup(std::string_view a_group, bool a_localized, std::vector<Race>& a_races)
		{
			std::size_t at = 0;
			std::string unpacked;
			while (at + kHeaderSize <= a_group.size()) {
				const char* head = a_group.data() + at;
				if (Is(head, "GRUP")) {
					const std::uint32_t size = U32(head + 4);  // the RACE group holds none, but a size is a size
					if (size < kHeaderSize) {
						return;
					}
					at += size;
					continue;
				}
				const std::uint32_t size = U32(head + 4);
				const std::uint32_t flags = U32(head + 8);
				const std::uint32_t formID = U32(head + 12);
				at += kHeaderSize;
				if (size > a_group.size() - at) {
					return;
				}
				const auto fields = a_group.substr(at, size);
				at += size;
				if (flags & kFlagCompressed) {
					if (Inflate(fields, unpacked)) {
						ReadRecord(unpacked, formID, a_localized, a_races);
					}
				} else {
					ReadRecord(fields, formID, a_localized, a_races);
				}
			}
		}
	}

	File Read(const std::string& a_path)
	{
		File          out;
		std::ifstream file(a_path, std::ios::binary);
		char          head[kHeaderSize];
		if (!file || !file.read(head, kHeaderSize) || !Is(head, "TES4")) {
			return out;
		}
		const std::uint32_t headerSize = U32(head + 4);
		if (headerSize > kMaxGroup) {
			return out;
		}
		out.localized = (U32(head + 8) & kFlagLocalized) != 0;
		std::string fields(headerSize, '\0');
		if (!file.read(fields.data(), headerSize)) {
			return out;
		}
		ForEachField(fields, [&](const char* a_type, std::string_view a_data) {
			if (Is(a_type, "MAST")) {
				out.masters.push_back(ZString(a_data));
			}
		});
		out.readable = true;

		// the top-level groups, one after another; only RACE is read, the rest are stepped over
		while (file.read(head, kHeaderSize)) {
			if (!Is(head, "GRUP")) {
				break;
			}
			const std::uint32_t size = U32(head + 4);
			if (size < kHeaderSize) {
				break;
			}
			if (!Is(head + 8, "RACE")) {
				file.seekg(static_cast<std::streamoff>(size) - static_cast<std::streamoff>(kHeaderSize), std::ios::cur);
				continue;
			}
			if (size > kMaxGroup) {
				break;
			}
			std::string group(size - kHeaderSize, '\0');
			if (file.read(group.data(), static_cast<std::streamsize>(group.size()))) {
				ReadGroup(group, out.localized, out.races);
			}
			break;
		}
		return out;
	}

	std::string LookupDLString(std::string_view a_data, std::uint32_t a_id)
	{
		if (a_data.size() < 8 || a_id == 0) {
			return {};
		}
		const std::uint32_t count = U32(a_data.data());
		if (count > (a_data.size() - 8) / 8) {
			return {};
		}
		const std::size_t base = 8 + static_cast<std::size_t>(count) * 8;
		for (std::uint32_t i = 0; i < count; ++i) {
			const char* row = a_data.data() + 8 + static_cast<std::size_t>(i) * 8;
			if (U32(row) != a_id) {
				continue;
			}
			const std::size_t at = base + U32(row + 4);
			if (at + 4 > a_data.size()) {
				return {};
			}
			const std::uint32_t length = U32(a_data.data() + at);  // DLSTRINGS: a length, then the text with its terminator
			if (length > a_data.size() - at - 4) {
				return {};
			}
			return ZString(a_data.substr(at + 4, length));
		}
		return {};
	}
}
