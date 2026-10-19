# FindGumbo.cmake - Locate gumbo-parser (MSYS2/MinGW64)
include(FindPackageHandleStandardArgs)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PC_Gumbo QUIET gumbo)
endif()

find_path(Gumbo_INCLUDE_DIRS
  NAMES gumbo.h
  HINTS
    ${PC_Gumbo_INCLUDE_DIRS}
    ${PC_Gumbo_INCLUDEDIR}
    "C:/msys64_20251213/mingw64/include"
    "C:/msys64/mingw64/include"
    "/mingw64/include"
    "/usr/include"
    "/usr/local/include"
)

find_library(Gumbo_LIBRARIES
  NAMES gumbo gumbo.dll.a libgumbo.a libgumbo
  HINTS
    ${PC_Gumbo_LIBRARY_DIRS}
    ${PC_Gumbo_LIBDIR}
    "C:/msys64_20251213/mingw64/lib"
    "C:/msys64/mingw64/lib"
    "/mingw64/lib"
    "/usr/lib"
    "/usr/local/lib"
  PATH_SUFFIXES lib
)

find_package_handle_standard_args(Gumbo
  REQUIRED_VARS Gumbo_INCLUDE_DIRS Gumbo_LIBRARIES
)

mark_as_advanced(Gumbo_INCLUDE_DIRS Gumbo_LIBRARIES)