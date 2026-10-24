// SPDX-License-Identifier: GPL-3.0-or-later
#include "object_data.h"
#include "byte_reader.h"
bool decodeObjects(const uint8_t *data, size_t size, unsigned count, size_t tableOffset,
                   bool bigEndian, DecodedObjects &output) {
	if (!data || !count || count > 255 || size > 16 * 1024 * 1024 || tableOffset > size ||
	    count * 4 > size - tableOffset)
		return false;
	ByteReader reader(data, size);
	auto u16 = [&] { return bigEndian ? reader.be16() : reader.le16(); };
	reader.seek(tableOffset);
	std::vector<uint32_t> offsets(count + 1);
	for (unsigned i = 0; i < count; ++i)
		offsets[i] = bigEndian ? reader.be32() : reader.le32();
	offsets[count] = size;
	DecodedObjects parsed;
	for (unsigned i = 0; i < count; ++i) {
		if (offsets[i] < tableOffset + count * 4 || offsets[i] > offsets[i + 1] ||
		    offsets[i + 1] > size)
			return false;
		if (i && offsets[i] == offsets[i - 1]) {
			parsed.map.push_back(parsed.map.back());
			continue;
		}
		unsigned end = i + 1;
		while (end < count && offsets[end] == offsets[i])
			++end;
		const size_t length = offsets[end] - offsets[i];
		if (length < 2 || (length - 2) % 18)
			return false;
		reader.seek(offsets[i]);
		const unsigned objects = u16();
		if (objects != (length - 2) / 18)
			return false;
		parsed.map.push_back(parsed.nodes.size());
		parsed.nodes.emplace_back(objects);
		for (auto &o : parsed.nodes.back()) {
			o.type = u16();
			o.dx = reader.u8();
			o.dy = reader.u8();
			o.init_obj_type = u16();
			o.opcode2 = reader.u8();
			o.opcode1 = reader.u8();
			o.flags = reader.u8();
			o.opcode3 = reader.u8();
			o.init_obj_number = u16();
			o.opcode_arg1 = u16();
			o.opcode_arg2 = u16();
			o.opcode_arg3 = u16();
		}
	}
	if (!reader.good())
		return false;
	output = std::move(parsed);
	return true;
}
