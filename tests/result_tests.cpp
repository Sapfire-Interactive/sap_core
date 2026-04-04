#include "sap_core/stl/result.h"
#include <gtest/gtest.h>

#include <memory>
#include <string>

// =============================================================================
// Construction
// =============================================================================

TEST(Result, DefaultConstructValue) {
    stl::result<int> r;
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 0);
}

TEST(Result, ConstructWithSuccessTag) {
    stl::result<int> r(stl::success, 42);
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 42);
}

TEST(Result, ConstructWithErrorTag) {
    stl::result<int> r(stl::error, "something failed");
    EXPECT_TRUE(r.has_error());
    EXPECT_EQ(r.error(), "something failed");
}

TEST(Result, ImplicitFromValue) {
    stl::result<int> r = 42;
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 42);
}

TEST(Result, ImplicitFromMoveValue) {
    auto str = std::string("hello");
    stl::result<std::string> r = std::move(str);
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), "hello");
}

// =============================================================================
// State checking
// =============================================================================

TEST(Result, HasValueOnSuccess) {
    stl::result<int> r(stl::success, 1);
    EXPECT_TRUE(r.has_value());
    EXPECT_FALSE(r.has_error());
    EXPECT_TRUE(static_cast<bool>(r));
}

TEST(Result, HasErrorOnFailure) {
    stl::result<int> r(stl::error, "err");
    EXPECT_FALSE(r.has_value());
    EXPECT_TRUE(r.has_error());
    EXPECT_FALSE(static_cast<bool>(r));
}

// =============================================================================
// Value / Error access
// =============================================================================

TEST(Result, ValueAccess) {
    stl::result<int> r(stl::success, 99);
    EXPECT_EQ(r.value(), 99);
    EXPECT_EQ(*r, 99);
}

TEST(Result, ValueConstAccess) {
    const stl::result<int> r(stl::success, 99);
    EXPECT_EQ(r.value(), 99);
    EXPECT_EQ(*r, 99);
}

TEST(Result, ValueMoveAccess) {
    stl::result<std::string> r(stl::success, "hello");
    std::string moved = std::move(r).value();
    EXPECT_EQ(moved, "hello");
}

TEST(Result, ErrorAccess) {
    stl::result<int> r(stl::error, "bad input");
    EXPECT_EQ(r.error(), "bad input");
}

TEST(Result, ErrorConstAccess) {
    const stl::result<int> r(stl::error, "bad");
    EXPECT_EQ(r.error(), "bad");
}

TEST(Result, ErrorMoveAccess) {
    stl::result<int> r(stl::error, "bad");
    std::string moved = std::move(r).error();
    EXPECT_EQ(moved, "bad");
}

TEST(Result, ArrowOperator) {
    stl::result<std::string> r(stl::success, "hello");
    EXPECT_EQ(r->size(), 5u);
}

TEST(Result, ArrowOperatorConst) {
    const stl::result<std::string> r(stl::success, "hello");
    EXPECT_EQ(r->size(), 5u);
}

// =============================================================================
// Copy / Move
// =============================================================================

TEST(Result, CopyValue) {
    stl::result<int> r1(stl::success, 42);
    stl::result<int> r2(r1);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), 42);
}

TEST(Result, CopyError) {
    stl::result<int> r1(stl::error, "err");
    stl::result<int> r2(r1);
    EXPECT_TRUE(r2.has_error());
    EXPECT_EQ(r2.error(), "err");
}

TEST(Result, MoveValue) {
    stl::result<std::string> r1(stl::success, "hello");
    stl::result<std::string> r2(std::move(r1));
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), "hello");
}

TEST(Result, MoveError) {
    stl::result<int> r1(stl::error, "err");
    stl::result<int> r2(std::move(r1));
    EXPECT_TRUE(r2.has_error());
    EXPECT_EQ(r2.error(), "err");
}

// =============================================================================
// Assignment
// =============================================================================

TEST(Result, CopyAssignValueToValue) {
    stl::result<int> r1(stl::success, 1);
    stl::result<int> r2(stl::success, 2);
    r2 = r1;
    EXPECT_EQ(r2.value(), 1);
}

TEST(Result, CopyAssignErrorToError) {
    stl::result<int> r1(stl::error, "a");
    stl::result<int> r2(stl::error, "b");
    r2 = r1;
    EXPECT_EQ(r2.error(), "a");
}

TEST(Result, CopyAssignValueToError) {
    stl::result<int> r1(stl::success, 42);
    stl::result<int> r2(stl::error, "err");
    r2 = r1;
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value(), 42);
}

TEST(Result, CopyAssignErrorToValue) {
    stl::result<int> r1(stl::error, "err");
    stl::result<int> r2(stl::success, 42);
    r2 = r1;
    EXPECT_TRUE(r2.has_error());
    EXPECT_EQ(r2.error(), "err");
}

TEST(Result, MoveAssignValueToValue) {
    stl::result<std::string> r1(stl::success, "hello");
    stl::result<std::string> r2(stl::success, "world");
    r2 = std::move(r1);
    EXPECT_EQ(r2.value(), "hello");
}

TEST(Result, MoveAssignErrorToValue) {
    stl::result<std::string> r1(stl::error, "err");
    stl::result<std::string> r2(stl::success, "val");
    r2 = std::move(r1);
    EXPECT_TRUE(r2.has_error());
    EXPECT_EQ(r2.error(), "err");
}

TEST(Result, SelfAssignment) {
    stl::result<int> r(stl::success, 42);
    r = r;
    EXPECT_EQ(r.value(), 42);
}

TEST(Result, SelfMoveAssignment) {
    stl::result<int> r(stl::success, 42);
    r = std::move(r);
    EXPECT_EQ(r.value(), 42);
}

// =============================================================================
// Emplace
// =============================================================================

TEST(Result, EmplaceValue) {
    stl::result<std::string> r(stl::error, "err");
    r.emplace_value("new_value");
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), "new_value");
}

TEST(Result, EmplaceError) {
    stl::result<int> r(stl::success, 42);
    r.emplace_error("new_error");
    EXPECT_TRUE(r.has_error());
    EXPECT_EQ(r.error(), "new_error");
}

TEST(Result, EmplaceValueOverValue) {
    stl::result<int> r(stl::success, 1);
    r.emplace_value(2);
    EXPECT_EQ(r.value(), 2);
}

TEST(Result, EmplaceErrorOverError) {
    stl::result<int> r(stl::error, "a");
    r.emplace_error("b");
    EXPECT_EQ(r.error(), "b");
}

// =============================================================================
// Helper functions
// =============================================================================

TEST(Result, MakeErrorFromString) {
    auto r = stl::make_error<int>("something broke");
    EXPECT_TRUE(r.has_error());
    EXPECT_EQ(r.error(), "something broke");
}

TEST(Result, MakeErrorFromFormat) {
    auto r = stl::make_error<int>("code: {}", 404);
    EXPECT_TRUE(r.has_error());
    EXPECT_EQ(r.error(), "code: 404");
}

TEST(Result, ResultSuccess) {
    auto r = stl::result_success();
    EXPECT_TRUE(r.has_value());
}

// =============================================================================
// Default result<> (unsigned, string)
// =============================================================================

TEST(Result, DefaultTemplateArgs) {
    stl::result<> r;
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), 0u);
}

TEST(Result, DefaultTemplateArgsError) {
    stl::result<> r(stl::error, "err");
    EXPECT_TRUE(r.has_error());
}

// =============================================================================
// Non-trivial types
// =============================================================================

TEST(Result, WithUniquePtr) {
    stl::result<std::unique_ptr<int>, std::string> r(stl::success, std::make_unique<int>(42));
    EXPECT_TRUE(r.has_value());
    EXPECT_EQ(**r, 42);
}

TEST(Result, MoveOnlyValue) {
    stl::result<std::unique_ptr<int>, std::string> r1(stl::success, std::make_unique<int>(99));
    auto r2 = std::move(r1);
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(**r2, 99);
}

// =============================================================================
// Custom error type
// =============================================================================

TEST(Result, CustomErrorType) {
    stl::result<int, int> r(stl::error, 404);
    EXPECT_TRUE(r.has_error());
    EXPECT_EQ(r.error(), 404);
}
