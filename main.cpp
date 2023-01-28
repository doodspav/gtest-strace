#include <gtest-strace/gtest-strace.hpp>
#include <gtest-strace/impl/linux-ptrace.hpp>

#include <iostream>

void todo(void *args)
{
    (void) args;
    std::cout << "uhhhhhhh\n";
    throw std::bad_alloc{};
}

TEST(Suite, Name)
{
    using namespace testing::internal::strace::linux_ptrace;

    (void) Strace(todo, nullptr);
}
