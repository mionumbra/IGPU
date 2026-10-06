set(SRC_DIR ${CMAKE_CURRENT_LIST_DIR})

# =============================================================================
# GameMaker iOS integration (optional)
# =============================================================================
set(EXT_GM_XCODEPROJ_OVERRIDE "/ABS/PATH/TO/GAME.xcodeproj" CACHE STRING "" FORCE)
set(EXT_GM_APP_TARGET_OVERRIDE "" CACHE STRING "" FORCE)

# =============================================================================
# Public headers for Apple XCFramework packaging
# Users should edit these to match their project structure.
# The packager will copy these into a headers folder passed to xcodebuild.
# =============================================================================
set(EXT_PUBLIC_HEADERS_IOS
    "${CMAKE_SOURCE_DIR}/src/ios/${PROJECT_NAME}_ios.h;${CMAKE_SOURCE_DIR}/code_gen/ios/${PROJECT_NAME}Internal_ios.h"
    CACHE STRING "Semicolon-separated list of public headers to embed in the iOS xcframework")

set(EXT_PUBLIC_HEADERS_TVOS
    "${CMAKE_SOURCE_DIR}/src/tvos/${PROJECT_NAME}_tvos.h;${CMAKE_SOURCE_DIR}/code_gen/tvos/${PROJECT_NAME}Internal_tvos.h"
    CACHE STRING "Semicolon-separated list of public headers to embed in the tvOS xcframework")

# =============================================================================
# User implementation sources per platform (editable template)
# =============================================================================
if(WIN32)
  file(GLOB TARGET_SRCS CONFIGURE_DEPENDS
      "${SRC_DIR}/native/*.cpp"
      "${SRC_DIR}/native/d3d11/*.cpp")

elseif(APPLE)
  if(CMAKE_OSX_SYSROOT MATCHES "iphone")
    file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/ios/*.mm" "${SRC_DIR}/ios/*.cpp" "${SRC_DIR}/ios/*.swift")
  elseif(CMAKE_OSX_SYSROOT MATCHES "appletv")
    file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/tvos/*.mm" "${SRC_DIR}/tvos/*.cpp" "${SRC_DIR}/tvos/*.swift")
  else()
    file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/native/*.mm" "${SRC_DIR}/native/*.cpp" "${SRC_DIR}/native/*.swift")
  endif()

elseif(ANDROID)
  file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/android/*.cpp")

elseif(UNIX)
  file(GLOB TARGET_SRCS CONFIGURE_DEPENDS "${SRC_DIR}/native/*.cpp")
endif()

target_sources(${PROJECT_NAME} PRIVATE ${TARGET_SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE "${SRC_DIR}")

# =============================================================================
# IGPU backend dependencies
# =============================================================================
# GameMaker owns the D3D11 device; we borrow it and use the runtime HLSL
# compiler (d3dcompiler_47) to compile shaders on the fly.
if(WIN32)
  include(FetchContent)

  set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
  set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
  set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
  set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
  set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
  set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)

  # These libraries are linked into IGPU.dll. The project builds shared
  # libraries, so force the dependencies to stay static.
  set(_igpu_build_shared ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    glslang
    URL https://github.com/KhronosGroup/glslang/archive/refs/tags/16.6.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_Declare(
    spirv_cross
    GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
    GIT_TAG aa217aeb6c9f0ace7a0ab233b28807edf45eb165)
  FetchContent_MakeAvailable(glslang spirv_cross)

  set(BUILD_SHARED_LIBS ${_igpu_build_shared} CACHE BOOL "" FORCE)

  target_include_directories(${PROJECT_NAME} PRIVATE ${glslang_SOURCE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11 dxgi d3dcompiler dxguid
    glslang
    SPIRV
    glslang-default-resource-limits
    spirv-cross-core
    spirv-cross-glsl
    spirv-cross-hlsl)

  # Test-only executable. It is not the GameMaker DLL and must not be copied
  # into project/extensions/IGPU.
  file(GLOB IGPU_PROBE_SRCS CONFIGURE_DEPENDS
      "${SRC_DIR}/native/*.cpp"
      "${SRC_DIR}/native/d3d11/*.cpp"
      "${SRC_DIR}/native/gl/*.cpp"
      "${CMAKE_SOURCE_DIR}/code_gen/core/*.cpp"
      "${CMAKE_SOURCE_DIR}/code_gen/native/*.cpp")
  add_executable(igpu_gl_probe
      "${CMAKE_SOURCE_DIR}/tests/gl_probe/main.cpp"
      "${CMAKE_SOURCE_DIR}/tests/gl_probe/wgl_host.cpp"
      ${IGPU_PROBE_SRCS})
  target_include_directories(igpu_gl_probe PRIVATE
      "${CMAKE_SOURCE_DIR}/tests/gl_probe"
      "${SRC_DIR}"
      "${SRC_DIR}/native"
      "${CMAKE_SOURCE_DIR}/code_gen"
      "${CMAKE_SOURCE_DIR}/code_gen/core"
      ${glslang_SOURCE_DIR})
  target_compile_definitions(igpu_gl_probe PRIVATE
      IGPU_HAS_OPENGL=1 OS_WINDOWS EXTGEN_HAS_JNI=0
      _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(igpu_gl_probe PRIVATE /W4 /permissive-)
  target_link_libraries(igpu_gl_probe PRIVATE
      opengl32
      d3d11 dxgi d3dcompiler dxguid
      glslang
      SPIRV
      glslang-default-resource-limits
      spirv-cross-core
      spirv-cross-glsl
      spirv-cross-hlsl)
endif()

