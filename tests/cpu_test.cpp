#include <cstdio>
#include <intrin.h>

int main() {
  int cpuInfo[4];
  __cpuid(cpuInfo, 0);
  int maxLeaf = cpuInfo[0];
  printf("Max CPUID leaf: %d\n", maxLeaf);

  __cpuid(cpuInfo, 1);
  bool sse41 = (cpuInfo[2] >> 19) & 1;
  bool sse42 = (cpuInfo[2] >> 20) & 1;
  bool avx   = (cpuInfo[2] >> 28) & 1;
  bool fma   = (cpuInfo[2] >> 12) & 1;
  printf("SSE4.1: %d, SSE4.2: %d, AVX: %d, FMA: %d\n", sse41, sse42, avx, fma);

  if (maxLeaf >= 7) {
    __cpuidex(cpuInfo, 7, 0);
    bool avx2    = (cpuInfo[1] >> 5) & 1;
    bool bmi2    = (cpuInfo[1] >> 8) & 1;
    bool avx512f = (cpuInfo[1] >> 16) & 1;
    printf("AVX2: %d, BMI2: %d, AVX512F: %d\n", avx2, bmi2, avx512f);
  }
  return 0;
}