#include "execution/serializer.h"
#include "core/string_pool.h"
#include <stdexcept>
#include <cstring>
#include <climits>

namespace Serializer {

static constexpr size_t BYTE_BITS = CHAR_BIT; //standart c++ (8bit)
static constexpr size_t U32_BYTES = sizeof(uint32_t);

static constexpr uint32_t SHIFT_B0 = 0;
static constexpr uint32_t SHIFT_B1 = BYTE_BITS;
static constexpr uint32_t SHIFT_B2 = BYTE_BITS * 2;
static constexpr uint32_t SHIFT_B3 = BYTE_BITS * 3;
static constexpr uint32_t BYTE_MASK = (1u << BYTE_BITS) - 1;

enum Tag : uint8_t {
    NUL = 0x00,
    INT = 0x01,
    STRING = 0x02,
};

static void pushU32(uint32_t v, std::vector<uint8_t>& buf) {
    buf.push_back((v >> SHIFT_B0) & BYTE_MASK);
    buf.push_back((v >> SHIFT_B1) & BYTE_MASK);
    buf.push_back((v >> SHIFT_B2) & BYTE_MASK);
    buf.push_back((v >> SHIFT_B3) & BYTE_MASK);
}

static uint32_t readU32(const std::vector<uint8_t>& buf, size_t pos) {
    if (pos + U32_BYTES > buf.size())
        throw std::runtime_error("Serializer: buffer too short for u32");

    return (static_cast<uint32_t>(buf[pos]) << SHIFT_B0) |
            (static_cast<uint32_t>(buf[pos + 1]) << SHIFT_B1) |
            (static_cast<uint32_t>(buf[pos + 2]) << SHIFT_B2) |
            (static_cast<uint32_t>(buf[pos + 3]) << SHIFT_B3);
}

void encodeValue(const Value& v, std::vector<uint8_t>& buf) {
    if (std::holds_alternative<std::monostate>(v)) {
        buf.push_back(Tag::NUL);
    } else if (std::holds_alternative<int>(v)) {
        buf.push_back(Tag::INT);
        int32_t n = static_cast<int32_t>(std::get<int>(v));
        uint32_t raw;
        std::memcpy(&raw, &n, U32_BYTES);
        pushU32(raw, buf);
    } else {
        // Value хранит string_view
        const auto s = std::get<std::string_view>(v);
        buf.push_back(Tag::STRING);
        pushU32(static_cast<uint32_t>(s.size()), buf);
        buf.insert(buf.end(), s.begin(), s.end());
    }
}

std::vector<uint8_t> encodeRow(const std::vector<Value>& row) {
    std::vector<uint8_t> buf;
    for (const auto& v : row) {
        encodeValue(v, buf);
    }
    return buf;
}

Value decodeValue(const std::vector<uint8_t>& buf, size_t& pos) {
    if (pos >= buf.size()) throw std::runtime_error("Serializer: unexpected end of buffer");
    auto tag = static_cast<Tag>(buf[pos++]);

    if (tag == Tag::NUL) {
        return std::monostate{};
    }
    if (tag == Tag::INT) {
        uint32_t raw = readU32(buf, pos); pos += U32_BYTES;
        int32_t n;
        std::memcpy(&n, &raw, U32_BYTES);
        return static_cast<int>(n);
    }
    if (tag == Tag::STRING) {
        uint32_t len = readU32(buf, pos); pos += U32_BYTES;
        if (pos + len > buf.size()) throw std::runtime_error("Serializer: string out of bounds");
        std::string s(buf.begin() + pos, buf.begin() + pos + len);
        pos += len;
        return StringPool::instance().intern(std::move(s));
    }
    throw std::runtime_error("Serializer: unknown tag " + std::to_string(static_cast<uint8_t>(tag)));
}

std::vector<Value> decodeRow(const std::vector<uint8_t>& buf) {
    std::vector<Value> row;
    size_t pos = 0;
    while (pos < buf.size()) row.push_back(decodeValue(buf, pos));
    return row;
}

}