cmake_minimum_required(VERSION 3.20)
project(TinyBrowser VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Network)
if(NOT Qt6_FOUND)
  find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets Network)
  set(QT_MAJOR_VERSION 5)
else()
  set(QT_MAJOR_VERSION 6)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Skia QUIET)
find_package(V8 QUIET)
find_package(Gumbo REQUIRED)

add_subdirectory(cssparser)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(Skia_FOUND)
  include_directories(${Skia_INCLUDE_DIRS})
endif()

if(V8_FOUND)
  include_directories(${V8_INCLUDE_DIRS})
endif()

include_directories(${Gumbo_INCLUDE_DIRS})

set(SOURCES
  src/main.cpp
  src/ui/MainWindow.cpp
  src/ui/WebCanvas.cpp
  src/rendering/RenderEngine.cpp
  src/rendering/LayoutEngine.cpp
  src/scripting/ScriptEngine.cpp
  src/scripting/V8DOMBridge.cpp   # <-- 이 줄을 추가해주세요
  src/parser/HtmlParser.cpp
  src/parser/StyleSheet.cpp
  src/network/NetworkManager.cpp
)

set(HEADERS
  include/TinyBrowser/MainWindow.h
  include/TinyBrowser/WebCanvas.h
  include/TinyBrowser/RenderEngine.h
  include/TinyBrowser/LayoutEngine.h
  include/TinyBrowser/ComputedStyle.h
  include/TinyBrowser/LayoutNode.h
  include/TinyBrowser/StyleSheet.h
  include/TinyBrowser/ScriptEngine.h
  include/TinyBrowser/HtmlParser.h
  include/TinyBrowser/NetworkManager.h
)

set(RESOURCES
  resources/resources.qrc
)

add_executable(TinyBrowser WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

if(QT_MAJOR_VERSION EQUAL 6)
  target_link_libraries(TinyBrowser PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
  )
else()
  target_link_libraries(TinyBrowser PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Network
  )
endif()

if(Skia_FOUND)
  target_link_libraries(TinyBrowser PRIVATE ${Skia_LIBRARIES})
  target_compile_definitions(TinyBrowser PRIVATE TINYBROWSER_HAS_SKIA=1)
else()
  message(WARNING "Skia not found - using QPainter fallback rendering.")
  target_compile_definitions(TinyBrowser PRIVATE TINYBROWSER_HAS_SKIA=0)
endif()

if(V8_FOUND)
  target_link_libraries(TinyBrowser PRIVATE ${V8_LIBRARIES})
  target_compile_definitions(TinyBrowser PRIVATE TINYBROWSER_HAS_V8=1)
else()
  target_compile_definitions(TinyBrowser PRIVATE TINYBROWSER_HAS_V8=0)
endif()

target_link_libraries(TinyBrowser PRIVATE ${Gumbo_LIBRARIES} cssparser)
target_include_directories(TinyBrowser PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

message(STATUS "Skia: ${Skia_FOUND}  V8: ${V8_FOUND}  Gumbo: ${Gumbo_FOUND}")