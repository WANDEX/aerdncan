cmake_minimum_required(VERSION 3.29..4.1)

## Disable modules support, this is broken with clang-tidy at the moment
## ref: github.com/cpp-best-practices/cmake_template/blob/main/CMakeLists.txt
cmake_policy(SET CMP0155 OLD)

## ^ POLICIES
project(
  aerdncan
  VERSION       0.0.1
  LANGUAGES     C ASM
  DESCRIPTION   "aerdncan - stm32 project with droneCAN & ina169."
  HOMEPAGE_URL  "https://github.com/WANDEX/aerdncan"
)

if(NOT DEFINED CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)
endif()
set(CMAKE_C_EXTENSIONS OFF)

# if(NOT DEFINED CMAKE_CXX_STANDARD)
#   set(CMAKE_CXX_STANDARD 20)
#   set(CMAKE_CXX_STANDARD_REQUIRED ON)
# endif()
# set(CMAKE_CXX_EXTENSIONS OFF)

option(AERDNCAN_DISABLE_TOP_PROJECT OFF)
if(PROJECT_IS_TOP_LEVEL AND NOT AERDNCAN_DISABLE_TOP_PROJECT)
  set(AERDNCAN_IS_TOP_PROJECT ON)
else()
  set(AERDNCAN_IS_TOP_PROJECT OFF)
endif()

## search path for cmake modules to be loaded by: include() find_package().
## NOTE: works properly only if defined at the top-level CMakeLists.txt.
list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ## project modules
)

macro(targets_and_linking)
  # add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/...")

  # include(wndx_sane_create_targets)
  # ## library targets & compilation options/flags
  # wndx_sane_create_targets(PFX wndx LIB aerdncan
  #   # CXX_STD cxx_std_${CMAKE_CXX_STANDARD}
  #   PHD "stm/Inc"
  # )
  #
  # ## link sources with project dependencies from the core library target
  # target_link_libraries(aerdncan_src INTERFACE wndx::aerdncan::core)
  #
  # ## link sources with project dev interface (for the compilation flags/options)
  # target_link_libraries(aerdncan_src PRIVATE wndx::aerdncan::dev)
  #
  # ## link sources with project dependencies
  # target_link_libraries(aerdncan_src PUBLIC wndx::aerdncan::deps)

  if(AERDNCAN_BUILD_SRC OR NOT AERDNCAN_IS_TOP_PROJECT)
    ## find/fetch dependencies:
    # find_package(fmt REQUIRED)
    add_subdirectory(stm)
  endif()
endmacro(targets_and_linking)

if(NOT AERDNCAN_IS_TOP_PROJECT)
  targets_and_linking()
  return() ## finish processing of the file
endif()

## MAIN
option(AERDNCAN_BUILD_SRC "whether or not src should be built" ON)

## for the list of supported compilers visit:
## https://cmake.org/cmake/help/latest/prop_tgt/COMPILE_WARNING_AS_ERROR.html
option(CMAKE_COMPILE_WARNING_AS_ERROR "treat compilation warnings as errors" OFF)

## output dir is correct by default for most build setups.
## however, when building lib as a DLL, it is important to have the DLL in the same dir
## as the executable using it. Thus, we put all executables in a single /bin dir.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
## on macOS, property ignored for the linker import files
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

targets_and_linking()
