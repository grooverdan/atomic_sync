#include "atomic_sux_lock.h"
#include <thread>

#ifdef _WIN32
#elif __cplusplus >= 202002L
#else /* Emulate the C++20 primitives */
# if defined __linux__
#  include <linux/futex.h>
#  include <unistd.h>
#  include <sys/syscall.h>
#  define FUTEX(op,n) \
   syscall(SYS_futex, this, FUTEX_ ## op ## _PRIVATE, n, nullptr, nullptr, 0)
# elif defined __OpenBSD__
#  include <sys/time.h>
#  include <sys/futex.h>
#  define FUTEX(op,n) \
   futex((volatile uint32_t*) this, FUTEX_ ## op, n, nullptr, nullptr)
# else
#  error "no C++20 nor futex support"
# endif
void atomic_sux_lock::notify_one() noexcept { FUTEX(WAKE, 1); }
inline void atomic_sux_lock::wait(uint32_t old) const noexcept
{ FUTEX(WAIT, old); }
#endif

void atomic_sux_lock::x_wait(uint32_t lk) noexcept
{
  assert(ex.is_locked());
  assert(lk);
  assert(lk < X);
  lk |= X;
  do
  {
    assert(lk > X);
    wait(lk);
    lk = load(std::memory_order_acquire);
  }
  while (lk != X);
}

void atomic_sux_lock::s_wait() noexcept
{
  for (;;)
  {
    ex.lock();
    uint32_t lk = fetch_add(1, std::memory_order_acquire);
    if (lk == X)
    {
      fetch_sub(1, std::memory_order_relaxed);
      notify_one();
      ex.unlock();
      std::this_thread::yield();
      continue;
    }
    assert(!(lk & X));
    break;
  }
  ex.unlock();
}