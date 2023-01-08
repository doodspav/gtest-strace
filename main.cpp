#include <gtest-strace/gtest-strace.hpp>
#include <gtest-strace/impl/linux-ptrace.hpp>

#include <iostream>

void todo(void *args) noexcept
{
    (void) args;
    std::cout << "uhhhhhhh";
}

TEST(Suite, Name)
{
    using namespace testing::internal::strace::linux_ptrace;

    std::exception_ptr excp;
    WrappedArgs wargs {todo, nullptr, &excp};

    auto ets = CreateStack();
    if (!ets) { ASSERT_TRUE(ets.error()); }
    auto ts = std::move(ets.value());

    auto epid = RunWithClone(&wargs, &ts);
    if (!epid) { ASSERT_TRUE(epid.error()); }
    auto pid = epid.value();

    int status;
    (void) waitpid(pid, &status, WSTOPPED);
    (void) kill(pid, SIGCONT);
    (void) waitpid(pid, &status, 0);
}
