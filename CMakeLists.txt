cmake_minimum_required(VERSION 2.4)
project(pocolog2msgpack)

set(CMAKE_CXX_STANDARD 11)

include(FindPkgConfig)
find_package(Boost COMPONENTS system filesystem REQUIRED)
pkg_check_modules(POCOLOG_CPP "pocolog_cpp" REQUIRED)
pkg_check_modules(TYPELIB "typelib" REQUIRED)
include_directories(
    ${POCOLOG_CPP_INCLUDE_DIRS} ${TYPELIB_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS}
)
link_directories(${POCOLOG_CPP_LIBRARY_DIRS} ${TYPELIB_LIBRARY_DIRS})

add_executable(pocolog2msgpack src/pocolog2msgpack.cpp)
target_link_libraries(
    pocolog2msgpack
    ${POCOLOG_CPP_LIBRARIES} ${TYPELIB_LIBRARIES}
)
install(
    TARGETS pocolog2msgpack
    DESTINATION bin
)
