#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <windows.h>

int main() {
  // Read libskia.dll from disk
  const char* path = "D:\\msys64_20251213\\mingw64\\bin\\libskia.dll";
  HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    printf("Cannot open file\n");
    return 1;
  }
  DWORD fileSize = GetFileSize(hFile, NULL);
  printf("File size: %lu bytes\n", fileSize);

  BYTE* data = (BYTE*)malloc(fileSize);
  DWORD read;
  ReadFile(hFile, data, fileSize, &read, NULL);
  CloseHandle(hFile);

  // Scan for AVX (VEX-encoded) instructions: C5 F8-FB or C4 80-BF
  int vex_c5 = 0, vex_c4 = 0, evex_62 = 0;
  for (DWORD i = 0; i < fileSize - 3; i++) {
    // VEX2: C5 followed by byte where bits 7:3 are 1111x (i.e. 0xF8-0xFF)
    if (data[i] == 0xC5 && (data[i+1] & 0xF8) == 0xF8) vex_c5++;
    // VEX3: C4 followed by byte where bit 7 is set and bits 6:3 have high bits
    if (data[i] == 0xC4 && (data[i+1] & 0xC0) == 0xC0) vex_c4++;
    // EVEX: 0x62
    if (data[i] == 0x62 && (data[i+1] & 0xC0) == 0x00) evex_62++;
  }
  printf("VEX2 (C5 xx): %d\n", vex_c5);
  printf("VEX3 (C4 xx): %d\n", vex_c4);
  printf("EVEX (62 xx): %d\n", evex_62);

  // Specifically look for VZEROUPPER: C5 F8 77
  int vzeroupper = 0;
  for (DWORD i = 0; i < fileSize - 3; i++) {
    if (data[i] == 0xC5 && data[i+1] == 0xF8 && data[i+2] == 0x77) vzeroupper++;
  }
  printf("VZEROUPPER: %d\n", vzeroupper);

  free(data);
  return 0;
}