#include <gtest-strace/gtest-strace.hpp>
#include <gtest-strace/impl/linux-ptrace.hpp>

#include <iostream>

void todo(void *args)
{
    (void) args;
    std::cout << "uhhhhhhh\n";
}

TEST(Suite, Name)
{
    using namespace testing::internal::strace::linux_ptrace;

    auto res = Strace(todo, nullptr);
    if (!res) { ASSERT_TRUE(res.error()); }
}
