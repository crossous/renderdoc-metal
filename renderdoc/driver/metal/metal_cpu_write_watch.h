// SPDX-License-Identifier: MIT
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <initializer_list>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

// Experimental, opt-in source observation only. This never changes captured bytes,
// replay eligibility or upload decisions. A fault marks a whole VM page, not the
// bytes written by the CPU; a dirty page may also contain GPU-written data.
class MetalDiagnosticCPUWriteWatch
{
public:
  static bool Enabled()
  {
    static const bool enabled = []() {
      const char *value = getenv("RENDERDOC_METAL_DIAG_CPU_WRITE_WATCH");
      return value && value[0] == '1' && !value[1];
    }();
    return enabled;
  }
  static MetalDiagnosticCPUWriteWatch &Get()
  {
    static MetalDiagnosticCPUWriteWatch instance;
    return instance;
  }

  bool Begin(void *owner)
  {
    if(!Enabled())
      return false;
    std::lock_guard<std::mutex> lock(m_Lock);
    if(m_Owner)
      return false;
    m_PageBytes = (uintptr_t)sysconf(_SC_PAGESIZE);
    if(!m_PageBytes || (m_PageBytes & (m_PageBytes - 1)))
      return false;
    struct sigaction action = {};
    action.sa_sigaction = Fault;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    if(sigaction(SIGSEGV, &action, &m_PreviousSegv))
      return false;
    if(sigaction(SIGBUS, &action, &m_PreviousBus))
    {
      sigaction(SIGSEGV, &m_PreviousSegv, NULL);
      return false;
    }
    m_Owner = owner;
    fprintf(stderr, "Metal CPU write watch begin pageBytes=%lu capacity=%u diagnosticOnly=1\n",
            (unsigned long)m_PageBytes, MaxPages);
    return true;
  }

  // Caller retains the Native storage only when this returns true. Retention is
  // released after all protections are restored. Register aliases by actual CPU
  // VM page; logical ResourceIds are labels, never the identity of a watch.
  bool Track(void *owner, const char *id, void *native, void (*release)(void *),
             const void *data, size_t length)
  {
    std::lock_guard<std::mutex> lock(m_Lock);
    if(m_Owner != owner || !data || !length)
      return false;
    for(unsigned i = 0; i < m_RetentionCount; ++i)
      if(m_Retentions[i].native == native)
        return false;
    uintptr_t begin = (uintptr_t)data;
    if(length > UINTPTR_MAX - begin)
      return false;
    // Protect only whole pages inside the allocation, never neighboring storage
    // or a partial page belonging to a Native allocator's own bookkeeping.
    uintptr_t end = (begin + length) & ~(m_PageBytes - 1);
    uintptr_t first = (begin + m_PageBytes - 1) & ~(m_PageBytes - 1);
    if(first < begin || end <= first)
      return false;
    unsigned count = m_Count.load();
    if((end - first) / m_PageBytes > MaxPages - count || m_RetentionCount == MaxRetentions)
    {
      fprintf(stderr, "Metal CPU write watch resource=%s limited=1 bytes=%lu\n", id,
              (unsigned long)length);
      return false;
    }
    unsigned added = 0;
    for(uintptr_t address = first; address < end; address += m_PageBytes)
    {
      bool known = false;
      for(unsigned i = 0; i < count; ++i)
        if(m_Pages[i].address == address)
        {
          known = true;
          break;
        }
      if(known)
        continue;
      Page &page = m_Pages[count];
      page.address = address;
      page.dirty.store(0);
      m_Count.store(++count);
      if(mprotect((void *)address, m_PageBytes, PROT_READ))
      {
        m_Count.store(--count);
        fprintf(stderr, "Metal CPU write watch resource=%s protectionFailed=1\n", id);
        break;
      }
      ++added;
    }
    if(!added)
      return false;
    m_Retentions[m_RetentionCount++] = {native, release};
    fprintf(stderr, "Metal CPU write watch resource=%s registeredPages=%u bytes=%lu partial=%d\n",
            id, added, (unsigned long)length, first != begin || end != begin + length);
    return true;
  }

  void Report(void *owner, const char *id, const void *data, size_t begin, size_t end,
              bool baseline)
  {
    std::lock_guard<std::mutex> lock(m_Lock);
    if(m_Owner != owner || !data || end <= begin)
      return;
    uintptr_t base = (uintptr_t)data;
    if(end > UINTPTR_MAX - base)
      return;
    size_t clean = 0, dirty = 0;
    const unsigned count = m_Count.load();
    for(unsigned i = 0; i < count; ++i)
    {
      const Page &page = m_Pages[i];
      uintptr_t first = page.address > base + begin ? page.address : base + begin;
      uintptr_t last = page.address + m_PageBytes < base + end ?
                           page.address + m_PageBytes : base + end;
      if(last > first)
      {
        if(page.dirty.load()) dirty += last - first;
        else clean += last - first;
      }
    }
    fprintf(stderr, "Metal CPU write watch diff resource=%s offset=%lu bytes=%lu baseline=%d "
                    "cleanBytes=%lu dirtyPageBytes=%lu unobservedBytes=%lu\n", id,
            (unsigned long)begin, (unsigned long)(end - begin), baseline,
            (unsigned long)clean, (unsigned long)dirty,
            (unsigned long)(end - begin - clean - dirty));
  }

  void End(void *owner)
  {
    std::lock_guard<std::mutex> lock(m_Lock);
    if(m_Owner != owner)
      return;
    unsigned count = m_Count.load(), dirty = 0, failed = 0;
    for(unsigned i = 0; i < count; ++i)
    {
      dirty += m_Pages[i].dirty.load() != 0;
      failed += mprotect((void *)m_Pages[i].address, m_PageBytes, PROT_READ | PROT_WRITE) != 0;
    }
    // Do not replace a signal handler installed by the application during the
    // observation. Restoration failures are actual diagnostic failures.
    struct sigaction current = {};
    unsigned changed = 0;
    for(int sig : {SIGSEGV, SIGBUS})
    {
      sigaction(sig, NULL, &current);
      if((current.sa_flags & SA_SIGINFO) && current.sa_sigaction == Fault)
        sigaction(sig, sig == SIGSEGV ? &m_PreviousSegv : &m_PreviousBus, NULL);
      else
        ++changed;
    }
    m_Count.store(0);
    fprintf(stderr, "Metal CPU write watch end pages=%u dirtyPages=%u restorationFailed=%u "
                    "handlerChanged=%u diagnosticOnly=1\n", count, dirty, failed, changed);
    if(failed)
      _exit(93);
    for(unsigned i = 0; i < m_RetentionCount; ++i)
      m_Retentions[i].release(m_Retentions[i].native);
    m_RetentionCount = 0;
    m_Owner = NULL;
  }

private:
  static constexpr unsigned MaxPages = 8192, MaxRetentions = 512;
  struct Page { uintptr_t address = 0; std::atomic<unsigned> dirty{0}; };
  struct Retention { void *native; void (*release)(void *); };
  std::mutex m_Lock;
  void *m_Owner = NULL;
  uintptr_t m_PageBytes = 0;
  std::atomic<unsigned> m_Count{0};
  Page m_Pages[MaxPages];
  Retention m_Retentions[MaxRetentions];
  unsigned m_RetentionCount = 0;
  struct sigaction m_PreviousSegv = {}, m_PreviousBus = {};
  static_assert(ATOMIC_INT_LOCK_FREE == 2, "Signal observation requires lock-free page flags");

  static void Fault(int sig, siginfo_t *info, void *context)
  {
    MetalDiagnosticCPUWriteWatch &watch = Get();
    uintptr_t address = (uintptr_t)info->si_addr;
    unsigned count = watch.m_Count.load();
    for(unsigned i = 0; i < count; ++i)
      if(address >= watch.m_Pages[i].address &&
         address - watch.m_Pages[i].address < watch.m_PageBytes)
      {
        watch.m_Pages[i].dirty.store(1);
        if(mprotect((void *)watch.m_Pages[i].address, watch.m_PageBytes,
                    PROT_READ | PROT_WRITE)) _exit(94);
        return;
      }
    const struct sigaction &previous = sig == SIGSEGV ? watch.m_PreviousSegv : watch.m_PreviousBus;
    if(previous.sa_handler == SIG_DFL || previous.sa_handler == SIG_IGN)
    {
      sigaction(sig, &previous, NULL);
      raise(sig);
      return;
    }
    if(previous.sa_flags & SA_SIGINFO) previous.sa_sigaction(sig, info, context);
    else previous.sa_handler(sig);
  }
};
