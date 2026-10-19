# FindV8.cmake - MSYS2/MinGW64 V8 탐색 (선택적)
include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_V8 QUIET v8)
endif()

find_path(V8_INCLUDE_DIRS
  NAMES v8.h v8/v8.h
  HINTS
    ${PC_V8_INCLUDE_DIRS}
    ${PC_V8_INCLUDEDIR}
    "C:/msys64/mingw64/include/v8"
    "/mingw64/include/v8"
    "/usr/include/v8"
    "/usr/local/include/v8"
  PATH_SUFFIXES v8 include/v8
)

find_library(V8_LIBRARIES
  NAMES v8 v8_monolith v8.dll.a libv8.a libv8_monolith.a
  HINTS
    ${PC_V8_LIBRARY_DIRS}
    ${PC_V8_LIBDIR}
    "C:/msys64/mingw64/lib"
    "/mingw64/lib"
    "/usr/lib"
    "/usr/local/lib"
)

find_package_handle_standard_args(V8
  REQUIRED_VARS V8_INCLUDE_DIRS V8_LIBRARIES
)

mark_as_advanced(V8_INCLUDE_DIRS V8_LIBRARIES)