#ifndef GTEST_STRACE_IMPL_LINUX_PTRACE_HPP
#define GTEST_STRACE_IMPL_LINUX_PTRACE_HPP


#ifndef __linux__
#error Header file linux-ptrace.hpp included without __linux__ defined
#endif


#include "util/expected-assertion.hpp"

#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <exception>
#include <csignal>
#include <memory>


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
        /// @brief Size of allocation passed to mmap.
        std::size_t size;

        /// @brief Deallocate ptr obtained from mmap call using size member.
        void operator()(void *ptr) const noexcept
        {
            (void) munmap(ptr, size);
        }
    };


    /// @brief Struct representing a thread's stack.
    struct ThreadStack
    {
        /// @brief Whole allocation including guard pages.
        std::unique_ptr<char, MUnmapDeleter> memory;

        /// @brief Pointer to start of stack, not including guard pages.
        /// @note  May be top or bottom of stack, depending on which direction stack grows.
        char *start;

        /// @brief Size of stack in bytes, not including guard pages.
        std::size_t size;
    };


    /// @brief Struct containing args to be passed to cloned function.
    struct WrappedArgs
    {
        /// @brief Function to be called by clone(...).
        void (*fn) (void *);

        /// @brief Args to be passed to function.
        void *args;

        /// @brief Holds any exceptions thrown by function.
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
#if GTEST_HAS_EXCEPTIONS
        try {
            cwa->fn(cwa->args);
            return 0;
        }
        catch (...) {
            *(cwa->excp) = std::current_exception();
            return 1;
        }
#else
        cwa->fn(cwa->args);
        return 0;
#endif
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


    /// @brief   Checks that stack grows from high address to low address.
    /// @returns True if stack grows down, false if it doesn't.
    [[nodiscard, gnu::const]] inline constexpr bool
    CheckStackGrowsDownwards() noexcept
    {
        // on linux only HP PA platforms have a stack that doesn't grow downwards
#if defined(__hppa__) || defined(__HPPA__) || defined(__hppa)
        return false;
#else
        return true;
#endif

    }


    /// @brief   Create stack to be used by new thread.
    /// @returns Newly allocated memory region suitable for stack of new thread.
    [[nodiscard]] Expected<ThreadStack>
    CreateStack() noexcept
    {
        // allocate memory for stack
        std::size_t stack_size = GetInitialStackSize();
        char *stack_ptr = reinterpret_cast<char *>(mmap(
            nullptr, stack_size, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK,
            -1, 0
        ));

        // check for failure
        if (!stack_ptr)
        {
            return Unexpected{ AssertionFailure()
                << "Failed to mmap " << stack_size << " bytes for new stack"
                << " with errno: " << errno };
        }

        // store in unique_ptr now, so that on failure we can return without
        // needing to manually free the memory
        MUnmapDeleter del {stack_size};
        std::unique_ptr<char, MUnmapDeleter> uptr {stack_ptr, del};

        // make non-guard page portions readable and writable
        // add guard page at start of stack
        stack_ptr  += GetPageSize();
        stack_size -= GetPageSize();
        // add guard page at end of stack
        stack_size -= GetPageSize();
        // change protections
        int res = mprotect(
            stack_ptr, stack_size,
            PROT_READ | PROT_WRITE
        );

        // check for failure
        if (res == -1)
        {
            return Unexpected{ AssertionFailure()
                << "Failed to change memory protections to READ | WRITE on"
                << " memory region " << stack_ptr << " with size " << stack_size
                << " with errno: " << errno };
        }

        // get start of stack (so high address if stack grows downwards)
        constexpr bool flip = CheckStackGrowsDownwards();
        stack_ptr += (flip * stack_size);

        // success
        return ThreadStack{std::move(uptr), stack_ptr, stack_size};
    }


    /// @brief   Run wrapped function in child process.
    /// @details Child process shares all system resources with parent process.
    /// @returns Child process's pid_t.
    /// @warning Parameters passed to function must remain valid until child process terminates.
    [[nodiscard]] Expected<pid_t>
    RunWithClone(WrappedArgs *wargs, ThreadStack *ts) noexcept
    {
        // setup flags for clone
        int flags =
            // share virtual memory (to be able to pass args and save exception)
            CLONE_VM      |
            // share sysv semaphores
            CLONE_SYSVSEM |
            // share files and filesystem
            CLONE_FILES   |
            CLONE_FS      |
            // share io
            CLONE_IO      |
            // so that we can use waitpid
            SIGCHLD       |
            // to obtain child pid
            CLONE_PARENT_SETTID;

        // clone
        pid_t pid;
        void *args = static_cast<void *>(wargs);
        int res    = clone(WrapperFunctionForClone, ts->start, flags, args, &pid);

        // check for failure
        if (res == -1)
        {
            return Unexpected{ AssertionFailure()
                << "Failed to run function with clone and flags " << flags
                << " with errno: " << errno };
        }

        // success
        return pid;
    }


    /// @brief  Setup child process to be traced, and continue its execution,
    ///         stopping at the next syscall.
    [[nodiscard]] Expected<void>
    SetupPtraceSyscall(pid_t pid) noexcept
    {
        // wait for stopped child process (was not stopped via ptrace)
        int status;
        auto wres = waitpid(pid, &status, WSTOPPED | WUNTRACED);

        // check result
        if (wres == -1)
        {
            return Unexpected{ AssertionFailure()
                << "Failed to waitpid(pid=" << pid << ", status, options="
                << "WSTOPPED | WUNTRACED) with errno: " << errno };
        }

        // check that we stopped because of a signal
        if (!WIFSTOPPED(status))
        {
            return Unexpected{ AssertionFailure()
                << "Process " << pid << " was stopped for reasons "
                << "other than the delivery of a signal" };
        }

        // check that the signal was SIGSTOP
        if (WSTOPSIG(status) != SIGSTOP)
        {
            return Unexpected{ AssertionFailure()
                << "Process " << pid << " was not stopped by SIGSTOP "
                << "but by signal: " << WSTOPSIG(status) };
        }

        // seize process
        auto pres = ptrace(PTRACE_SEIZE, pid, nullptr, nullptr);
        if (pres == -1)
        {
            return Unexpected{ AssertionFailure()
                << "Could not PTRACE_SEIZE process " << pid << " with errno: "
                << errno };
        }

        // set ptrace options
        pres = ptrace(PTRACE_SETOPTIONS, pid, nullptr,
            PTRACE_O_TRACEEXIT | PTRACE_O_TRACESYSGOOD);
        if (pres == -1)
        {
            return Unexpected{ AssertionFailure()
                << "Failed to set ptrace options PTRACE_O_TRACEEXIT | "
                << "PTRACE_O_TRACESYSGOOD on tracee pid " << pid << " with "
                << "errno: " << errno };
        }

        // continue execution, stopping at next syscall
        pres = ptrace(PTRACE_SYSCALL, pid, nullptr, 0);
        if (pres == -1)
        {
            return Unexpected{ AssertionFailure()
                << "Failed to resume ptrace tracee process " << pid << " using "
                << "request PTRACE_SYSCALL with errno: " << errno };
        }

        // success
        return Expected<void> {};
    }


    /// @brief   Process a single ptrace interruption, expecting syscall or exit.
    /// @returns The syscall value if stopped for a syscall, or -1 if the process terminated.
    [[nodiscard]] Expected<int>
    SinglePtrace(pid_t pid) noexcept
    {
        (void) ptrace(PTRACE_CONT, pid, nullptr, nullptr);
        int status;
        (void) waitpid(pid, &status, 0);
        (void) kill(pid, SIGCONT);
        return -1;
    }


    [[nodiscard]] Expected<void>
    Strace(void (fn)(void *), void *args)
#if !GTEST_HAS_EXCEPTIONS
        noexcept
#endif
    {
        std::exception_ptr excp;
        WrappedArgs wargs {fn, args, &excp};

        // setup stack
        auto ets = CreateStack();
        if (!ets) { return Unexpected{ ets.error() }; }
        auto ts = std::move(ets.value());

        // start function execution
        auto epid = RunWithClone(&wargs, &ts);
        if (!epid) { return Unexpected{ epid.error() }; }
        auto pid = epid.value();

        // setup ptrace
        auto evoid = SetupPtraceSyscall(pid);
        if (!evoid) { return Unexpected{ evoid.error() }; }

        // go through all syscalls until error or termination
        Expected<int> esys;
        while ((esys = SinglePtrace(pid)) && esys.value() != -1)
            ;

        // check for error
        if (!esys) { return Unexpected{ esys.error() }; }

        // re-throw exception if occurred
#if GTEST_HAS_EXCEPTIONS
        if (excp) { std::rethrow_exception(excp); }
#endif

        // success
        return Expected<void>{};
    }

}  // namespace linux_ptrace
}  // namespace strace
}  // namespace internal
}  // namespace testing


#endif  // !GTEST_STRACE_IMPL_LINUX_PTRACE_HPP
