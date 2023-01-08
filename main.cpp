#include <gtest-strace/gtest-strace.hpp>
#include <gtest-strace/impl/linux-ptrace.hpp>

#include <iostream>

TEST(Suite, Name)
{
    using namespace testing::internal::strace::linux_ptrace;

    auto ear = CreateStack();
    if (!ear) { ASSERT_TRUE(ear.error()); }
    auto ts = std::move(ear.value());

    std::cout << "ThreadStack{.memory=" << (void*) ear.value().memory.get()
              << ", .start=" << (void*) ear.value().start
              << ", .size=" << ear.value().size
              << '}' << std::endl;
}
