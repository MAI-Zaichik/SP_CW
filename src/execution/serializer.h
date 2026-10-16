#pragma once
#include <vector>
#include <cstdint>
#include "core/types.h"

namespace Serializer {

void encodeValue(const Value& v, std::vector<uint8_t>& buf);

std::vector<uint8_t> encodeRow(const std::vector<Value>& row);

Value decodeValue(const std::vector<uint8_t>& buf, size_t& pos);

std::vector<Value> decodeRow(const std::vector<uint8_t>& buf);

}