#include "sap_core/serialization.h"
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

// =============================================================================
// ByteWriter / ByteReader round-trip
// =============================================================================

TEST(Serialization, RoundTripU8) {
    sap::ByteWriter w;
    EXPECT_TRUE(w.write_u8(0).has_value());
    EXPECT_TRUE(w.write_u8(255).has_value());

    sap::ByteReader r(w.buffer());
    auto v1 = r.read_u8();
    auto v2 = r.read_u8();
    ASSERT_TRUE(v1.has_value());
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(*v1, 0u);
    EXPECT_EQ(*v2, 255u);
}

TEST(Serialization, RoundTripU16) {
    sap::ByteWriter w;
    w.write_u16(0);
    w.write_u16(1234);
    w.write_u16(65535);

    sap::ByteReader r(w.buffer());
    EXPECT_EQ(*r.read_u16(), 0u);
    EXPECT_EQ(*r.read_u16(), 1234u);
    EXPECT_EQ(*r.read_u16(), 65535u);
}

TEST(Serialization, RoundTripU32) {
    sap::ByteWriter w;
    w.write_u32(0);
    w.write_u32(123456789);
    w.write_u32(std::numeric_limits<u32>::max());

    sap::ByteReader r(w.buffer());
    EXPECT_EQ(*r.read_u32(), 0u);
    EXPECT_EQ(*r.read_u32(), 123456789u);
    EXPECT_EQ(*r.read_u32(), std::numeric_limits<u32>::max());
}

TEST(Serialization, RoundTripU64) {
    sap::ByteWriter w;
    w.write_u64(0);
    w.write_u64(0xDEADBEEFCAFEBABE);
    w.write_u64(std::numeric_limits<u64>::max());

    sap::ByteReader r(w.buffer());
    EXPECT_EQ(*r.read_u64(), 0u);
    EXPECT_EQ(*r.read_u64(), 0xDEADBEEFCAFEBABEu);
    EXPECT_EQ(*r.read_u64(), std::numeric_limits<u64>::max());
}

TEST(Serialization, RoundTripF32) {
    sap::ByteWriter w;
    w.write_f32(0.0f);
    w.write_f32(3.14f);
    w.write_f32(-1.5f);
    w.write_f32(std::numeric_limits<f32>::infinity());
    w.write_f32(std::numeric_limits<f32>::max());

    sap::ByteReader r(w.buffer());
    EXPECT_FLOAT_EQ(*r.read_f32(), 0.0f);
    EXPECT_FLOAT_EQ(*r.read_f32(), 3.14f);
    EXPECT_FLOAT_EQ(*r.read_f32(), -1.5f);
    EXPECT_TRUE(std::isinf(*r.read_f32()));
    EXPECT_FLOAT_EQ(*r.read_f32(), std::numeric_limits<f32>::max());
}

TEST(Serialization, RoundTripF64) {
    sap::ByteWriter w;
    w.write_f64(0.0);
    w.write_f64(3.141592653589793);
    w.write_f64(-1e300);
    w.write_f64(std::numeric_limits<f64>::infinity());

    sap::ByteReader r(w.buffer());
    EXPECT_DOUBLE_EQ(*r.read_f64(), 0.0);
    EXPECT_DOUBLE_EQ(*r.read_f64(), 3.141592653589793);
    EXPECT_DOUBLE_EQ(*r.read_f64(), -1e300);
    EXPECT_TRUE(std::isinf(*r.read_f64()));
}

TEST(Serialization, RoundTripString) {
    sap::ByteWriter w;
    w.write_string("hello");
    w.write_string("");
    w.write_string("world with spaces and special chars: !@#$%");

    sap::ByteReader r(w.buffer());
    EXPECT_EQ(*r.read_string(), "hello");
    EXPECT_EQ(*r.read_string(), "");
    EXPECT_EQ(*r.read_string(), "world with spaces and special chars: !@#$%");
}

TEST(Serialization, RoundTripBytes) {
    std::vector<stl::byte> data = {stl::byte{0x01}, stl::byte{0x02}, stl::byte{0xFF}};
    sap::ByteWriter w;
    w.write_bytes(data);

    sap::ByteReader r(w.buffer());
    auto result = r.read_bytes(3);
    ASSERT_TRUE(result.has_value());
    auto span = *result;
    EXPECT_EQ(span.size(), 3u);
    EXPECT_EQ(span[0], stl::byte{0x01});
    EXPECT_EQ(span[1], stl::byte{0x02});
    EXPECT_EQ(span[2], stl::byte{0xFF});
}

// =============================================================================
// Mixed types in sequence
// =============================================================================

TEST(Serialization, MixedTypes) {
    sap::ByteWriter w;
    w.write_u8(1);
    w.write_u16(2);
    w.write_u32(3);
    w.write_u64(4);
    w.write_f32(5.0f);
    w.write_f64(6.0);
    w.write_string("seven");

    sap::ByteReader r(w.buffer());
    EXPECT_EQ(*r.read_u8(), 1u);
    EXPECT_EQ(*r.read_u16(), 2u);
    EXPECT_EQ(*r.read_u32(), 3u);
    EXPECT_EQ(*r.read_u64(), 4u);
    EXPECT_FLOAT_EQ(*r.read_f32(), 5.0f);
    EXPECT_DOUBLE_EQ(*r.read_f64(), 6.0);
    EXPECT_EQ(*r.read_string(), "seven");
}

// =============================================================================
// ByteReader tracking
// =============================================================================

TEST(Serialization, BytesReadAndRemaining) {
    sap::ByteWriter w;
    w.write_u32(42);
    w.write_u32(99);

    sap::ByteReader r(w.buffer());
    EXPECT_EQ(r.bytes_read(), 0u);
    EXPECT_EQ(r.remaining(), 8u);

    (void)r.read_u32();
    EXPECT_EQ(r.bytes_read(), 4u);
    EXPECT_EQ(r.remaining(), 4u);

    (void)r.read_u32();
    EXPECT_EQ(r.bytes_read(), 8u);
    EXPECT_EQ(r.remaining(), 0u);
}

// =============================================================================
// Buffer underflow errors
// =============================================================================

TEST(Serialization, UnderflowU8) {
    sap::ByteReader r({});
    auto result = r.read_u8();
    EXPECT_TRUE(result.has_error());
}

TEST(Serialization, UnderflowU16) {
    sap::ByteWriter w;
    w.write_u8(0); // only 1 byte
    sap::ByteReader r(w.buffer());
    (void)r.read_u8();
    auto result = r.read_u16(); // need 2, have 0
    EXPECT_TRUE(result.has_error());
}

TEST(Serialization, UnderflowU32) {
    sap::ByteReader r({});
    EXPECT_TRUE(r.read_u32().has_error());
}

TEST(Serialization, UnderflowU64) {
    sap::ByteReader r({});
    EXPECT_TRUE(r.read_u64().has_error());
}

TEST(Serialization, UnderflowF32) {
    sap::ByteReader r({});
    EXPECT_TRUE(r.read_f32().has_error());
}

TEST(Serialization, UnderflowF64) {
    sap::ByteReader r({});
    EXPECT_TRUE(r.read_f64().has_error());
}

TEST(Serialization, UnderflowString) {
    sap::ByteReader r({});
    EXPECT_TRUE(r.read_string().has_error());
}

TEST(Serialization, UnderflowStringBody) {
    // Write string length but truncate the body
    sap::ByteWriter w;
    w.write_u64(100); // says 100 bytes but we won't write them
    sap::ByteReader r(w.buffer());
    auto result = r.read_string();
    EXPECT_TRUE(result.has_error());
}

TEST(Serialization, UnderflowBytes) {
    sap::ByteReader r({});
    EXPECT_TRUE(r.read_bytes(1).has_error());
}

// =============================================================================
// Empty buffer
// =============================================================================

TEST(Serialization, EmptyBuffer) {
    sap::ByteWriter w;
    EXPECT_TRUE(w.buffer().empty());
    sap::ByteReader r(w.buffer());
    EXPECT_EQ(r.bytes_read(), 0u);
    EXPECT_EQ(r.remaining(), 0u);
}

// =============================================================================
// Network byte order
// =============================================================================

TEST(Serialization, ByteOrderU16) {
    sap::ByteWriter w;
    w.write_u16(0x0102);
    auto& buf = w.buffer();
    // Big-endian: 0x01, 0x02
    EXPECT_EQ(static_cast<u8>(buf[0]), 0x01);
    EXPECT_EQ(static_cast<u8>(buf[1]), 0x02);
}

TEST(Serialization, ByteOrderU32) {
    sap::ByteWriter w;
    w.write_u32(0x01020304);
    auto& buf = w.buffer();
    EXPECT_EQ(static_cast<u8>(buf[0]), 0x01);
    EXPECT_EQ(static_cast<u8>(buf[1]), 0x02);
    EXPECT_EQ(static_cast<u8>(buf[2]), 0x03);
    EXPECT_EQ(static_cast<u8>(buf[3]), 0x04);
}

// =============================================================================
// Large data
// =============================================================================

TEST(Serialization, LargeString) {
    std::string large(10000, 'x');
    sap::ByteWriter w;
    w.write_string(large);

    sap::ByteReader r(w.buffer());
    auto result = r.read_string();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 10000u);
    EXPECT_EQ(*result, large);
}

TEST(Serialization, ManyValues) {
    sap::ByteWriter w;
    for (u32 i = 0; i < 1000; ++i)
        w.write_u32(i);

    sap::ByteReader r(w.buffer());
    for (u32 i = 0; i < 1000; ++i) {
        auto val = r.read_u32();
        ASSERT_TRUE(val.has_value());
        EXPECT_EQ(*val, i);
    }
    EXPECT_EQ(r.remaining(), 0u);
}

// =============================================================================
// Special float values
// =============================================================================

TEST(Serialization, NaN) {
    sap::ByteWriter w;
    w.write_f32(std::numeric_limits<f32>::quiet_NaN());
    w.write_f64(std::numeric_limits<f64>::quiet_NaN());

    sap::ByteReader r(w.buffer());
    EXPECT_TRUE(std::isnan(*r.read_f32()));
    EXPECT_TRUE(std::isnan(*r.read_f64()));
}

TEST(Serialization, NegativeZero) {
    sap::ByteWriter w;
    w.write_f32(-0.0f);
    w.write_f64(-0.0);

    sap::ByteReader r(w.buffer());
    f32 f = *r.read_f32();
    f64 d = *r.read_f64();
    EXPECT_FLOAT_EQ(f, 0.0f);
    EXPECT_DOUBLE_EQ(d, 0.0);
    EXPECT_TRUE(std::signbit(f));
    EXPECT_TRUE(std::signbit(d));
}
