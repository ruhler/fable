
#include "memory.h"

#include <sys/time.h>       // for getrusage
#include <sys/resource.h>   // for getrusage

size_t FbleGetMaxMemoryUsageKB()
{
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss;
}
