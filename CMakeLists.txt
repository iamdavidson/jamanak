cmake_minimum_required(VERSION 3.16)

project(jamanak VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_EXAMPLES "Build example executable" ON)

# ---- Library ----
add_library(jamanak SHARED src/jamanak.cpp)
add_library(jamanak::jamanak ALIAS jamanak)

target_compile_features(jamanak PUBLIC cxx_std_17)

target_include_directories(jamanak
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# ---- Lib version ----
set_target_properties(jamanak PROPERTIES
  VERSION   ${PROJECT_VERSION}
  SOVERSION 0
)

# ---- Example ----
if(BUILD_EXAMPLES)
  add_executable(jamanak_example src/example.cpp)
  target_link_libraries(jamanak_example PRIVATE jamanak::jamanak)
endif()

# ---- Install ----
include(GNUInstallDirs)

install(TARGETS jamanak
  EXPORT jamanakTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)


# ----  Package export ----
install(EXPORT jamanakTargets
  FILE jamanakTargets.cmake
  NAMESPACE jamanak::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jamanak
)
