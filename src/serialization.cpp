#include "sap_core/serialization.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace sap {

    // Network byte order helpers for u64 (htonl/ntohl only handle u32)
    inline u64 htonll(u64 value) {
        const u32 high = htonl(static_cast<u32>(value >> 32));
        const u32 low = htonl(static_cast<u32>(value & 0xFFFFFFFF));
        return (static_cast<u64>(low) << 32) | high;
    }

    inline u64 ntohll(u64 value) {
        const u32 high = ntohl(static_cast<u32>(value >> 32));
        const u32 low = ntohl(static_cast<u32>(value & 0xFFFFFFFF));
        return (static_cast<u64>(low) << 32) | high;
    }

    stl::result<> ByteWriter::write_u8(u8 val) {
        m_buffer.push_back(static_cast<stl::byte>(val));
        return stl::success;
    }

    stl::result<> ByteWriter::write_u16(u16 val) {
        u16 net = htons(val);
        append_bytes(&net, sizeof(u16));
        return stl::success;
    }

    stl::result<> ByteWriter::write_u32(u32 val) {
        u32 net = htonl(val);
        append_bytes(&net, sizeof(u32));
        return stl::success;
    }

    stl::result<> ByteWriter::write_u64(u64 val) {
        u64 net = htonll(val);
        append_bytes(&net, sizeof(u64));
        return stl::success;
    }

    stl::result<> ByteWriter::write_f32(f32 val) {
        u32 bits;
        std::memcpy(&bits, &val, sizeof(u32));
        u32 net = htonl(bits);
        append_bytes(&net, sizeof(u32));
        return stl::success;
    }

    stl::result<> ByteWriter::write_f64(f64 val) {
        u64 bits;
        std::memcpy(&bits, &val, sizeof(u64));
        u64 net = htonll(bits);
        append_bytes(&net, sizeof(u64));
        return stl::success;
    }

    stl::result<> ByteWriter::write_string(stl::string_view str) {
        u64 size = str.size();
        write_u64(size);
        append_bytes(str.data(), size);
        return stl::success;
    }

    stl::result<> ByteWriter::write_bytes(stl::span<const stl::byte> data) {
        m_buffer.insert(m_buffer.end(), data.begin(), data.end());
        return stl::success;
    }

    void ByteWriter::append_bytes(const void* data, size_t len) {
        const auto* bytes = static_cast<const stl::byte*>(data);
        m_buffer.insert(m_buffer.end(), bytes, bytes + len);
    }

    ByteReader::ByteReader(stl::span<const stl::byte> buffer) : m_buffer(buffer), m_offset(0) {}

    stl::result<u8> ByteReader::read_u8() {
        if (m_offset + sizeof(u8) > m_buffer.size()) {
            return stl::make_error<u8>("buffer underflow: need 1 byte, have {}", remaining());
        }
        u8 value = static_cast<u8>(m_buffer[m_offset]);
        m_offset += sizeof(u8);
        return value;
    }

    stl::result<u16> ByteReader::read_u16() {
        if (m_offset + sizeof(u16) > m_buffer.size()) {
            return stl::make_error<u16>("buffer underflow: need 2 bytes, have {}", remaining());
        }
        u16 net;
        std::memcpy(&net, m_buffer.data() + m_offset, sizeof(u16));
        m_offset += sizeof(u16);
        return ntohs(net);
    }

    stl::result<u32> ByteReader::read_u32() {
        if (m_offset + sizeof(u32) > m_buffer.size()) {
            return stl::make_error<u32>("buffer underflow: need 4 bytes, have {}", remaining());
        }
        u32 net;
        std::memcpy(&net, m_buffer.data() + m_offset, sizeof(u32));
        m_offset += sizeof(u32);
        return ntohl(net);
    }

    stl::result<u64> ByteReader::read_u64() {
        if (m_offset + sizeof(u64) > m_buffer.size()) {
            return stl::make_error<u64>("buffer underflow: need 8 bytes, have {}", remaining());
        }
        u64 net;
        std::memcpy(&net, m_buffer.data() + m_offset, sizeof(u64));
        m_offset += sizeof(u64);
        return ntohll(net);
    }

    stl::result<f32> ByteReader::read_f32() {
        auto u32_res = read_u32();
        if (!u32_res)
            return stl::make_error<f32>("{}", u32_res.error());
        f32 val;
        u32 bits = *u32_res;
        std::memcpy(&val, &bits, sizeof(f32));
        return val;
    }

    stl::result<f64> ByteReader::read_f64() {
        auto u64_res = read_u64();
        if (!u64_res)
            return stl::make_error<f64>("{}", u64_res.error());
        f64 val;
        u64 bits = *u64_res;
        std::memcpy(&val, &bits, sizeof(f64));
        return val;
    }

    stl::result<stl::string_view> ByteReader::read_string() {
        auto len_result = read_u64();
        if (!len_result) {
            return stl::make_error<stl::string_view>("failed to read string length: {}", len_result.error());
        }
        u64 len = *len_result;
        if (m_offset + len > m_buffer.size()) {
            return stl::make_error<stl::string_view>("buffer underflow: need {} bytes for string, have {}", len, remaining());
        }
        const auto* start = reinterpret_cast<const char*>(m_buffer.data() + m_offset);
        m_offset += len;
        return stl::string_view{start, len};
    }

    stl::result<stl::span<const stl::byte>> ByteReader::read_bytes(size_t len) {
        if (m_offset + len > m_buffer.size()) {
            return stl::make_error<stl::span<const stl::byte>>("buffer underflow: need {} bytes, have {}", len, remaining());
        }
        auto result = m_buffer.subspan(m_offset, len);
        m_offset += len;
        return result;
    }

} // namespace sap