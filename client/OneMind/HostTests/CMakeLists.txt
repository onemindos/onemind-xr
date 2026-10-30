# Host build of the engine-independent OneMind core (Plugins/OneMindCore) plus unit tests,
# integration tests against a NATS server's WebSocket listener, and the headless client.
# The flags mirror Unreal's defaults for module code: no exceptions, no RTTI, warnings
# as errors, shadowing reported. That catches most engine-build problems early; it does not
# replace the Unreal build on the Mac (OWNER-STEPS.md), which has not been run.
cmake_minimum_required(VERSION 3.20)
project(OneMindHostTests CXX)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)   # 20 is also tested (Unreal 5.8 builds modules as C++20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(OM_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Plugins/OneMindCore/Source/OneMindCore)
file(GLOB OM_CORE_SRC CONFIGURE_DEPENDS ${OM_CORE_DIR}/Private/*.cpp)
# The Unreal module glue is the only engine-dependent file in OneMindCore.
list(FILTER OM_CORE_SRC EXCLUDE REGEX "OneMindCoreModule\\.cpp$")

set(OM_WARN -Wall -Wextra -Wshadow -Wno-unused-parameter -Werror -fno-exceptions -fno-rtti)
option(OM_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
if(OM_SANITIZE)
  list(APPEND OM_WARN -fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(onemind_core STATIC ${OM_CORE_SRC})
target_include_directories(onemind_core PUBLIC ${OM_CORE_DIR}/Public)
target_compile_options(onemind_core PRIVATE ${OM_WARN})

add_library(onemind_host STATIC host/PosixWsTransport.cpp)
target_include_directories(onemind_host PUBLIC host)
target_link_libraries(onemind_host PUBLIC onemind_core)
target_compile_options(onemind_host PRIVATE ${OM_WARN})
find_package(Threads REQUIRED)
target_link_libraries(onemind_host PUBLIC Threads::Threads)

file(GLOB OM_UNIT_SRC CONFIGURE_DEPENDS tests/unit_*.cpp)
add_executable(om_unit_tests tests/test_main.cpp ${OM_UNIT_SRC})
target_include_directories(om_unit_tests PRIVATE tests)
target_link_libraries(om_unit_tests PRIVATE onemind_core)
target_compile_options(om_unit_tests PRIVATE ${OM_WARN})
target_compile_definitions(om_unit_tests PRIVATE OM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

file(GLOB OM_IT_SRC CONFIGURE_DEPENDS tests/it_*.cpp)
add_executable(om_integration_tests tests/test_main.cpp ${OM_IT_SRC})
target_include_directories(om_integration_tests PRIVATE tests)
target_link_libraries(om_integration_tests PRIVATE onemind_host)
target_compile_options(om_integration_tests PRIVATE ${OM_WARN})
target_compile_definitions(om_integration_tests PRIVATE OM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_executable(onemind_headless tools/onemind_headless.cpp)
target_link_libraries(onemind_headless PRIVATE onemind_host)
target_compile_options(onemind_headless PRIVATE ${OM_WARN})
