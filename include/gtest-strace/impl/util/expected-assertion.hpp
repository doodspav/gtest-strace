#ifndef GTEST_STRACE_IMPL_UTIL_EXPECTED_ASSERTION_HPP
#define GTEST_STRACE_IMPL_UTIL_EXPECTED_ASSERTION_HPP


#include "../std/expected.hpp"

#include <gtest/gtest-assertion-result.h>


namespace testing
{
namespace internal
{

    /// @brief Expected result type with AssertionResult as error type.
    template <class T>
    using Expected = tl::expected<T, AssertionResult>;


    /// @brief Helper Unexpected type to aid in constructing Expected from AssertionResult.
    using Unexpected = tl::unexpected<AssertionResult>;


}  // namespace internal
}  // namespace testing


#endif  // !GTEST_STRACE_IMPL_UTIL_EXPECTED_ASSERTION_HPP
