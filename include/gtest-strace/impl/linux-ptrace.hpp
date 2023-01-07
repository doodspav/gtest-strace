#ifndef GTEST_STRACE_IMPL_LINUX_PTRACE_HPP
#define GTEST_STRACE_IMPL_LINUX_PTRACE_HPP


#ifndef __linux__
#error Header file linux-ptrace.hpp included without __linux__ defined
#endif


#include <gtest/gtest.h>

#include <sched.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <exception>
#include <csignal>


namespace testing
{
namespace internal
{
namespace strace
{
namespace linux_ptrace
{

    /// @brief Struct used as deleter for memory allocated with mmap.
    struct MUnmapDeleter
    {
        std::size_t size;

        void operator()(void *ptr) const noexcept
        {
            (void) munmap(ptr, size);
        }
    };


    /// @brief Struct containing args to be passed to cloned function.
    struct WrappedArgs
    {
        void (*fn) (void *);
        void *args;
        std::exception_ptr *excp;
    };


    /// @brief Function passed to clone.
    [[nodiscard]] inline int
    WrapperFunctionForClone(void *args) noexcept
    {
        // convert to real args
        auto *cwa = reinterpret_cast<const WrappedArgs *>(args);

        // call SIGSTOP so that parent process can wait on us
        (void) std::raise(SIGSTOP);

        // catch exception so that we can rethrow from parent
        try {
            cwa->fn(cwa->args);
            return 0;
        }
        catch (...) {
            *(cwa->excp) = std::current_exception();
            return 1;
        }
    }


    /// @brief Get page size for current platform, or implementation defined value.
    [[nodiscard, gnu::const]] inline unsigned long
    GetPageSize() noexcept
    {
        constexpr long default_size = 4 * 1024;  // 4KB

        const long res = sysconf(_SC_PAGESIZE);
        return (res == -1) ? default_size : res;
    }


    /// @brief Get stack size from RLIMIT, or implementation defined value.
    /// @note  RLIMIT is assumed to not change during process lifetime.
    [[nodiscard, gnu::const]] inline unsigned long
    GetInitialStackSize() noexcept
    {
        const unsigned long guard_pages = 2 * GetPageSize();
        constexpr unsigned long default_size = 2 * 1024 * 1024;  // 2MB
        constexpr unsigned long min_size = 16 * 1024;  // 16KB

        rlimit limit {};
        if ( (getrlimit(RLIMIT_STACK, &limit) != 0) ||
             (limit.rlim_cur == RLIM_INFINITY) )
        {
            return default_size + guard_pages;
        }
        else
        {
            return std::max(limit.rlim_cur, min_size) + guard_pages;
        }
    }


}  // namespace linux_ptrace
}  // namespace strace
}  // namespace internal
}  // namespace testing


#endif  // !GTEST_STRACE_IMPL_LINUX_PTRACE_HPP
