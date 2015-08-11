cmake_minimum_required(VERSION 2.8.4)
project(iris)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS_INIT} -std=c++11 -Wall") ## Optimize
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wunreachable-code")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -ggdb3")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O2")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

########################################
# Boost

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.49.0 REQUIRED program_options)

include_directories(${Boost_INCLUDE_DIR})
set(LINK_LIBS ${LINK_LIBS} ${Boost_LIBRARIES})

########################################
# OpenGL + Co
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
find_package(GLM REQUIRED)
pkg_search_module(FREETYPE REQUIRED freetype2)

include_directories(${GLM_INCLUDE_DIRS}
                    ${GLFW_INCLUDE_DIRS}
                    ${OpenGL_INCLUDE_DIRS}
                    ${FREETYPE_INCLUDE_DIRS})

link_directories(${GLFW_LIBRARY_DIRS}
                 ${OpenGL_LIBRARY_DIRS}
                 ${OPENCL_LIBRARY_DIRS})

set(LINK_LIBS ${LINK_LIBS} ${OPENGL_LIBRARIES} ${GLFW_LIBRARIES} ${FREETYPE_LIBRARIES})

if(NOT APPLE)
  pkg_search_module(GLEW REQUIRED glew)
  include_directories(${GLEW_INCLUDE_DIRS})
  link_directories(${GLEW_LIBRARY_DIRS})
  set(LINK_LIBS ${LINK_LIBS} ${GLEW_LIBRARIES})
endif()

########################################
# HDF5

set(HDF5_USE_STATIC_LIBS OFF)
find_package (HDF5 REQUIRED COMPONENTS C)
include_directories (${HDF5_INCLUDE_DIR})
set (LINK_LIBS ${LINK_LIBS} ${HDF5_LIBRARIES})

#######################################
# cminpack

find_package(CMinpack REQUIRED)
set (LINK_LIBS ${LINK_LIBS} ${CMINPACK_LIBRARIES})

#######################################
# BLAS & LAPACK

find_package(LAPACK REQUIRED)
set(LINK_LIBS ${LINK_LIBS} ${LAPACK_LIBRARIES})

if(NOT APPLE)
  set(LINK_LIBS ${LINK_LIBS} -llapacke)
endif()

find_package(BLAS REQUIRED)
set(LINK_LIBS ${LINK_LIBS} ${BLAS_LIBRARIES})

#######################################
# YAML CPP

pkg_check_modules(YAMLCPP REQUIRED yaml-cpp)

include_directories(${YAMLCPP_INCLUDE_DIRS})
link_directories(${YAMLCPP_LIBRARY_DIRS})
set(LINK_LIBS ${LINK_LIBS} ${YAMLCPP_LIBRARIES})

#######################################
# lib

if(NOT APPLE)
   set(LINK_LIBS ${LINK_LIBS} -lstdc++)
endif()

include_directories(lib)
file(GLOB_RECURSE lib_SOURCES lib/*.cc lib/*.cpp)
file(GLOB_RECURSE lib_INCLUDES lib/*.h lib/*.hpp)

add_definitions(-DHAVE_IRIS)
add_library(iris SHARED ${lib_INCLUDES} ${lib_SOURCES})
target_link_libraries(iris PUBLIC ${LINK_LIBS})

#######################################
# tools

set(lpm_SOURCES tools/lpm.cc)
add_executable(lpm ${lpm_SOURCES})
target_link_libraries(lpm iris)

set(LEDPhotoSpectrum_SOURCES tools/LEDPhotoSpectrum.cc)
add_executable(iris-LEDPhotoSpectrum ${LEDPhotoSpectrum_SOURCES})
target_link_libraries(iris-LEDPhotoSpectrum iris)

set(lpmThresholder_SOURCES tools/thresholder.cc)
add_executable(iris-lpmThresholder ${lpmThresholder_SOURCES})
target_link_libraries(iris-lpmThresholder iris)

set(pr655_SOURCES tools/pr655.cc)
add_executable(pr655 ${pr655_SOURCES})
target_link_libraries(pr655 iris)

set(info_SOURCES tools/info.cc)
add_executable(iris-info ${info_SOURCES})
target_link_libraries(iris-info iris)

set(measure_SOURCES tools/measure.cc)
add_executable(iris-measure ${measure_SOURCES})
target_link_libraries(iris-measure iris)

add_executable(iris-cgen tools/cgen.cc)
target_link_libraries(iris-cgen iris)

add_executable(iris-calibrate tools/calibrate.cc)
target_link_libraries(iris-calibrate iris)

add_executable(iris-colorcircle tools/colorcircle.cc)
target_link_libraries(iris-colorcircle iris)

add_executable(iris-fitiso tools/fitiso.cc)
target_link_libraries(iris-fitiso iris)

add_executable(iris-board tools/board.cc)
target_link_libraries(iris-board iris)

add_executable(iris-convert tools/conv.cc)
target_link_libraries(iris-convert iris)

add_executable(iris-store tools/store.cc)
target_link_libraries(iris-store iris)

add_executable(iris-isoslant tools/isoslant.cc)
target_link_libraries(iris-isoslant iris)

add_executable(iris-rndgen tools/rndgen.cc)
target_link_libraries(iris-rndgen iris)

########################################
# UI tools (depend on Qt5)

find_package(Qt5Widgets QUIET)
find_package(Qt5PrintSupport QUIET)

message("Have Qt5 Widgets: ${Qt5Widgets_FOUND}, PrintSupport: ${Qt5PrintSupport_FOUND}")

set(HAVE_UI NO)
if (${Qt5Widgets_FOUND} AND ${Qt5PrintSupport_FOUND})

  set(CMAKE_INCLUDE_CURRENT_DIR ON)
  set(CMAKE_AUTOMOC ON)

  include_directories(ui ${CMAKE_BINARY_DIR})

  QT5_WRAP_UI(ISOSLANT_UI_HDRS
              ui/isoslantwnd.ui)

  add_executable(iris-ui-isoslant ui/qcustomplot.cpp ui/isoslant.cc ui/isoslantwnd.cc ${ISOSLANT_UI_HDRS})
  target_link_libraries(iris-ui-isoslant Qt5::Widgets Qt5::PrintSupport iris)
  set(HAVE_UI YES)
endif()

########################################
# playground

if(EXISTS "${CMAKE_SOURCE_DIR}/play.cc")
  add_executable(iris-play EXCLUDE_FROM_ALL play.cc)
  target_link_libraries(iris-play iris)
endif()

########################################
# install

install(TARGETS iris
        FRAMEWORK DESTINATION "/Library/Frameworks"
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(TARGETS pr655 iris-info iris-measure iris-cgen iris-calibrate iris-colorcircle iris-fitiso iris-board iris-convert iris-isoslant iris-rndgen
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
        COMPONENT applications)

install(DIRECTORY lib/ DESTINATION include/iris
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp")


########################################
# Status
MESSAGE("===============================")
MESSAGE("Binary dir: ${CMAKE_BINARY_DIR}")
MESSAGE("Configuration:")
MESSAGE("Freetype:")
MESSAGE("\tinc: ${FREETYPE_INCLUDE_DIRS}")
MESSAGE("\tlib: ${FREETYPE_LIBRARIES}")