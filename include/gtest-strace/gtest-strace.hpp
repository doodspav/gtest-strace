#ifndef GTEST_STRACE_HPP
#define GTEST_STRACE_HPP

#include <gtest/gtest.h>


#define GTEST_STRACE_TEST(test_suite_name, test_name) GTEST_TEST(test_suite_name, test_name)
#define GTEST_STRACE_TEST_F(test_fixture, test_name) GTEST_TEST_F(test_fixture, test_name)


#if !GTEST_DONT_DEFINE_STRACE_TEST
#define STRACE_TEST(test_suite_name, test_name) GTEST_STRACE_TEST(test_suite_name, test_name)
#endif

#if !GTEST_DONT_DEFINE_STRACE_TEST_F
#define STRACE_TEST_F(test_fixture, test_name) GTEST_STRACE_TEST_F(test_fixture, test_name)
#endif


#endif  // !GTEST_STRACE_HPP
