cmake_minimum_required(VERSION 3.10.0)
project(moe-graphics VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# whether to enable mimalloc
# enable this in release build
option(USE_MIMALLOC "Use mimalloc as the memory allocator" OFF)

add_subdirectory(vendors/glfw)
add_subdirectory(vendors/glm)
add_subdirectory(vendors/Vulkan-Headers)
add_subdirectory(vendors/vk-bootstrap)

set(VULKAN_HEADERS_INSTALL_DIR ${PROJECT_SOURCE_DIR}/vendors/Vulkan-Headers)
add_subdirectory(vendors/VulkanMemoryAllocator)

add_subdirectory(vendors/volk)

add_subdirectory(vendors/fmt)
set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(vendors/spdlog)

add_subdirectory(vendors/tinygltf)

add_subdirectory(vendors/JoltPhysics/Build)

add_subdirectory(vendors/harfbuzz)
add_subdirectory(vendors/freetype)
add_subdirectory(vendors/utfcpp)

add_subdirectory(vendors/openal-soft)
add_subdirectory(vendors/ogg)
add_subdirectory(vendors/vorbis)

add_subdirectory(vendors/concurrentqueue)

set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FLATBUFFERS_BUILD_FLATC OFF CACHE BOOL "" FORCE)
add_subdirectory(vendors/flatbuffers)

add_subdirectory(vendors/enet)
set(ENET_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/vendors/enet/include)

if(USE_MIMALLOC)
    message(STATUS "moe-graphics: Using mimalloc as the memory allocator")
    add_subdirectory(vendors/mimalloc)

    # use static mimalloc
    set(MIMALLOC_LIB mimalloc-static)
    set(MIMALLOC_CPP_IMPL "src/MiMallocImpl.cpp")
else()
    message(STATUS "moe-graphics: Not using mimalloc")
    set(MIMALLOC_LIB "")
    set(MIMALLOC_CPP_IMPL "")
endif()

include_directories(include)

file(GLOB_RECURSE RENDER_SOURCES src/Render/*.cpp)
file(GLOB_RECURSE CORE_SOURCES src/Core/*.cpp)
file(GLOB_RECURSE MATH_SOURCES src/Math/*.cpp)
file(GLOB_RECURSE AUDIO_SOURCES src/Audio/*.cpp)
file(GLOB_RECURSE PHYSICS_SOURCES src/Physics/*.cpp)
file(GLOB_RECURSE UI_SOURCES src/UI/*.cpp)

file(GLOB IMGUI_SOURCES 
  vendors/imgui/imgui*.h 
  vendors/imgui/imgui*.cpp 
  vendors/imgui/backends/imgui_impl_glfw.*
  vendors/imgui/backends/imgui_impl_vulkan.*)

file(GLOB IM3D_SOURCES vendors/im3d/im3d.cpp)

file(GLOB STB_IMAGES_SOURCES 
  vendors/stb/stb_image.h
)

file(GLOB_RECURSE GAME_SOURCES game/*.cpp)

add_executable(moe-graphics 
  main.cpp
  ${RENDER_SOURCES}
  ${CORE_SOURCES}
  ${MATH_SOURCES}
  ${AUDIO_SOURCES}
  ${PHYSICS_SOURCES}
  ${UI_SOURCES}

  ${IMGUI_SOURCES}
  ${IM3D_SOURCES}
  ${STB_IMAGES_SOURCES}

  ${GAME_SOURCES}

  ${MIMALLOC_CPP_IMPL}
)

target_link_libraries(moe-graphics PRIVATE
  glfw glm::glm
  vk-bootstrap::vk-bootstrap 
  VulkanMemoryAllocator volk_headers
  spdlog::spdlog fmt::fmt
  tinygltf
  Jolt
  utf8cpp freetype harfbuzz
  OpenAL::OpenAL ogg vorbis vorbisfile
  concurrentqueue flatbuffers
  enet
  ${MIMALLOC_LIB}
)

target_include_directories(moe-graphics PRIVATE
  vendors/span/include
  vendors/imgui
  vendors/imgui/backends
  vendors/im3d
  vendors/stb

  ${ENET_INCLUDE_DIR}
)

target_include_directories(moe-graphics PRIVATE
  game
)

#find_package(Vulkan REQUIRED)
#target_link_libraries(moe-graphics PRIVATE ${Vulkan_LIBRARIES})

target_compile_definitions(moe-graphics PRIVATE 
  VK_NO_PROTOTYPES
  IMGUI_IMPL_VULKAN_USE_VOLK # for imgui volk integration
)

include(CTest)
enable_testing()

# tools
message(STATUS "Configuring moe-graphics utilities...")
add_subdirectory(tools/hako-ify)

