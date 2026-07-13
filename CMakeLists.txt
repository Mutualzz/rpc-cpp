cmake_minimum_required(VERSION 3.16)
project(mutualzz_rpc VERSION 1.0.0 LANGUAGES CXX)

add_library(mutualzz_rpc src/rpc.cpp)
add_library(mutualzz::rpc ALIAS mutualzz_rpc)

target_include_directories(mutualzz_rpc
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(mutualzz_rpc PUBLIC cxx_std_17)

if(WIN32)
  target_link_libraries(mutualzz_rpc PUBLIC)
else()
  target_compile_definitions(mutualzz_rpc PRIVATE _DEFAULT_SOURCE)
endif()

option(MUTUALZZ_RPC_BUILD_EXAMPLES "Build Mutualzz RPC examples" OFF)
if(MUTUALZZ_RPC_BUILD_EXAMPLES)
  add_executable(mutualzz_rpc_example examples/set_activity.cpp)
  target_link_libraries(mutualzz_rpc_example PRIVATE mutualzz::rpc)
endif()

install(TARGETS mutualzz_rpc
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)
