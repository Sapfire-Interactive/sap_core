#pragma once

#include "sap_core/stl/result.h"
#include "sap_core/stl/vector.h"
#include "sap_core/stl/string.h"
#include "types.h"

namespace sap {
    class ByteWriter {
    public:
        stl::result<> write_u8(u8 val);
        stl::result<> write_u16(u16 val);
        stl::result<> write_u32(u32 val);
        stl::result<> write_u64(u64 val);
        stl::result<> write_f32(f32 val);
        stl::result<> write_f64(f64 val);
        stl::result<> write_string(stl::string_view str);
        stl::result<> write_bytes(stl::span<const stl::byte> data);
        inline const stl::vector<stl::byte>& buffer() const { return m_buffer; }

    private:
        void append_bytes(const void* data, size_t len);

    private:
        stl::vector<stl::byte> m_buffer;
    };

    class ByteReader {
    public:
        explicit ByteReader(stl::span<const stl::byte> buffer);
        [[nodiscard]] stl::result<u8> read_u8();
        [[nodiscard]] stl::result<u16> read_u16();
        [[nodiscard]] stl::result<u32> read_u32();
        [[nodiscard]] stl::result<u64> read_u64();
        [[nodiscard]] stl::result<f32> read_f32();
        [[nodiscard]] stl::result<f64> read_f64();
        [[nodiscard]] stl::result<stl::string_view> read_string();
        [[nodiscard]] stl::result<stl::span<const stl::byte>> read_bytes(size_t len);

        [[nodiscard]] size_t bytes_read() const { return m_offset; }
        [[nodiscard]] size_t remaining() const { return m_buffer.size() - m_offset; }

    private:
        stl::span<const stl::byte> m_buffer;
        size_t m_offset;
    };
} // namespace sap