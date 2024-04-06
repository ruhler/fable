
#include "memory.h"

#ifdef __WIN32
#include <windows.h>
#include <psapi.h>

size_t FbleGetMaxMemoryUsageKB()
{
  PROCESS_MEMORY_COUNTERS pmc;
  HANDLE process = GetCurrentProcess();
  GetProcessMemoryInfo(process, &pmc, sizeof(pmc));
  return pmc.PeakWorkingSetSize / 1024;
}

#else // __WIN32

#include <sys/resource.h>   // for getrusage

size_t FbleGetMaxMemoryUsageKB()
{
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss;
}

#endif // __WIN32
