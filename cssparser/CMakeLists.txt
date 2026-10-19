# cssparser - standalone pure C++ CSS parser library (no Qt/Gumbo dependency)
add_library(cssparser STATIC
  CSSParser.cpp
  CSSUtils.cpp
  CSSProperties.cpp
  CSSDeNest.cpp
)

target_include_directories(cssparser PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(cssparser PUBLIC cxx_std_17)

# Optional command line test tool (parses a CSS file and dumps tokens)
option(BUILD_CSSPARSER_CLI "Build the cssparser command line test tool" OFF)
if(BUILD_CSSPARSER_CLI)
  add_executable(cssparser_cli main.cpp)
  target_link_libraries(cssparser_cli PRIVATE cssparser)
endif()
