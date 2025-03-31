#include <sys/mman.h>
#include <cerrno>
#include <unistd.h>
#include "utils.h"
#include <syscall.h>
#include <unistd.h>
#include "logging.h"

// https://stackoverflow.com/a/68051325
bool is_pointer_valid(void *p) {
    /* get the page size */
    size_t page_size = sysconf(_SC_PAGESIZE);
    /* find the address of the page that contains p */
    void *base = (void *)((((size_t)p) / page_size) * page_size);
    /* call msync, if it returns non-zero, return false */
    int ret = msync(base, page_size, MS_ASYNC) != -1;
    return ret ? ret : errno != ENOMEM;
}

// https://maskray.me/blog/2020-11-08-stack-unwinding#%E4%B8%AD%E6%96%87%E7%89%88
bool is_pointer_valid(void *p, size_t len) {
    auto pgsz = getpagesize();
    auto mask = ~(pgsz - 1);
    auto addr = (uintptr_t) p;
#if defined(__aarch64__)
    addr &= (1ull << 56) - 1;
#endif
    auto start = reinterpret_cast<uintptr_t>(addr) & mask;
    auto end = (reinterpret_cast<uintptr_t>(addr) + len) & mask;
    if (end < start) return false;
    for (auto d = start; d <= end; d += pgsz) {
        if (d == 0) {
            // assume 0 address is invalid
            return false;
        }
        // LOGD("trying %p", d);
        errno = 0;
        syscall(SYS_rt_sigprocmask, ~0, d, nullptr, /*sizeof(kernel_sigset_t)=*/8);
        // PLOGE("sigprocmask");
        if (errno == EFAULT) {
            return false;
        }
    }
    return true;
}
