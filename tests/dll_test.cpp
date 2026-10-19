#include <cstdio>
#include <cstdlib>
#include <windows.h>

int main() {
  // Try loading libskia.dll directly
  HMODULE h = LoadLibraryA("libskia.dll");
  if (!h) {
    printf("LoadLibrary failed: %lu\n", GetLastError());
    return 1;
  }
  printf("libskia.dll loaded OK at %p\n", h);
  FreeLibrary(h);
  return 0;
}