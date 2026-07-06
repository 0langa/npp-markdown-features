cmake_minimum_required(VERSION 3.25)

project(NppMarkdownFeatures VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
  message(FATAL_ERROR "NppMarkdownFeatures only builds on Windows.")
endif()

include(FetchContent)
include(CTest)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(CMARK_TESTS OFF CACHE BOOL "" FORCE)
set(CMARK_SHARED OFF CACHE BOOL "" FORCE)
set(CMARK_STATIC ON CACHE BOOL "" FORCE)
FetchContent_Declare(
  cmark-gfm
  GIT_REPOSITORY https://github.com/github/cmark-gfm.git
  GIT_TAG 0.29.0.gfm.13
)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(cmark-gfm nlohmann_json)

set(WEBVIEW2_VERSION "1.0.4022.49" CACHE STRING "Microsoft.Web.WebView2 NuGet version")
set(WEBVIEW2_NUGET_DIR "${CMAKE_BINARY_DIR}/_deps/nuget")
set(WEBVIEW2_PACKAGE_DIR "${WEBVIEW2_NUGET_DIR}/Microsoft.Web.WebView2")
set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_PACKAGE_DIR}/build/native/include")
set(WEBVIEW2_LOADER_LIB "${WEBVIEW2_PACKAGE_DIR}/build/native/x64/WebView2LoaderStatic.lib")

if(NOT EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h" OR NOT EXISTS "${WEBVIEW2_LOADER_LIB}")
  find_program(NUGET_EXE nuget REQUIRED)
  execute_process(
    COMMAND "${NUGET_EXE}" install Microsoft.Web.WebView2 -Version ${WEBVIEW2_VERSION} -OutputDirectory "${WEBVIEW2_NUGET_DIR}" -ExcludeVersion -NonInteractive
    RESULT_VARIABLE WEBVIEW2_NUGET_RESULT
  )
  if(NOT WEBVIEW2_NUGET_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to install Microsoft.Web.WebView2 ${WEBVIEW2_VERSION} with NuGet.")
  endif()
endif()

if(NOT EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h")
  message(FATAL_ERROR "WebView2.h not found at ${WEBVIEW2_INCLUDE_DIR}")
endif()

if(NOT EXISTS "${WEBVIEW2_LOADER_LIB}")
  message(FATAL_ERROR "WebView2LoaderStatic.lib not found at ${WEBVIEW2_LOADER_LIB}")
endif()

add_library(WebView2LoaderStatic STATIC IMPORTED GLOBAL)
set_target_properties(WebView2LoaderStatic PROPERTIES IMPORTED_LOCATION "${WEBVIEW2_LOADER_LIB}")

add_library(nmf_core
  src/core/FeatureRegistry.cpp
  src/core/ListEditing.cpp
  src/core/MarkdownOutline.cpp
  src/core/MarkdownViewFeature.cpp
  src/core/ScrollSync.cpp
  src/core/Settings.cpp
  src/core/SettingsStore.cpp
  src/core/Strings.cpp
  src/rendering/MarkdownRenderer.cpp
  src/rendering/WebViewUserDataFolder.cpp
)
target_include_directories(nmf_core PUBLIC
  src
  "${cmark-gfm_SOURCE_DIR}/src"
  "${cmark-gfm_BINARY_DIR}/src"
  "${cmark-gfm_SOURCE_DIR}/extensions"
  "${cmark-gfm_BINARY_DIR}/extensions"
)
target_link_libraries(nmf_core PUBLIC libcmark-gfm_static libcmark-gfm-extensions_static nlohmann_json::nlohmann_json)
target_compile_definitions(nmf_core PUBLIC
  UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  CMARK_GFM_STATIC_DEFINE CMARK_GFM_EXTENSIONS_STATIC_DEFINE
)

add_library(NppMarkdownFeatures SHARED
  src/plugin/Plugin.cpp
  src/plugin/ListEditingController.cpp
  src/plugin/NppApi.cpp
  src/rendering/WebViewHost.cpp
  src/ui/AboutDialog.cpp
  src/ui/OutlinePanel.cpp
  src/ui/SettingsDialog.cpp
  src/ui/ToolbarIcon.cpp
)
set_target_properties(NppMarkdownFeatures PROPERTIES
  OUTPUT_NAME "NppMarkdownFeatures"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
target_include_directories(NppMarkdownFeatures PRIVATE src "${WEBVIEW2_INCLUDE_DIR}")
target_link_libraries(NppMarkdownFeatures PRIVATE
  nmf_core
  WebView2LoaderStatic
  ole32
  shell32
  shlwapi
  user32
  gdi32
  comctl32
  advapi32
)
target_compile_definitions(NppMarkdownFeatures PRIVATE NMF_PLUGIN_BUILD)

if(MSVC)
  target_compile_options(nmf_core PRIVATE /W4 /permissive-)
  target_compile_options(NppMarkdownFeatures PRIVATE /W4 /permissive-)
endif()

if(BUILD_TESTING)
  add_executable(nmf_tests
    tests/main.cpp
  )
  target_link_libraries(nmf_tests PRIVATE nmf_core)
  target_compile_definitions(nmf_tests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  add_test(NAME nmf_tests COMMAND nmf_tests)
endif()
