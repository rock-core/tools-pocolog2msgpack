cmake_minimum_required(VERSION 2.4)
project(pocolog2msgpack)

set(CMAKE_CXX_STANDARD 11)

include(FindPkgConfig)
find_package(Boost COMPONENTS program_options REQUIRED)
pkg_check_modules(POCOLOG_CPP "pocolog_cpp" REQUIRED)
pkg_check_modules(TYPELIB "typelib" REQUIRED)
pkg_check_modules(MSGPACK "msgpack" REQUIRED)
include_directories(
    src ${POCOLOG_CPP_INCLUDE_DIRS} ${TYPELIB_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${MSGPACK_INCLUDE_DIRS}
)
link_directories(
    ${POCOLOG_CPP_LIBRARY_DIRS} ${TYPELIB_LIBRARY_DIRS} ${Boost_LIBRARY_DIRS} ${MSGPACK_LIBRARY_DIRS}
)

add_executable(pocolog2msgpack src/pocolog2msgpack.cpp src/Converter.cpp)
target_link_libraries(
    pocolog2msgpack
    ${POCOLOG_CPP_LIBRARIES} ${TYPELIB_LIBRARIES} ${Boost_LIBRARIES} ${MSGPACK_LIBRARIES}
)
install(
    TARGETS pocolog2msgpack
    DESTINATION bin
)

find_package(PythonInterp)
if(PYTHONINTERP_FOUND)
    install(
        CODE "execute_process(
            COMMAND ${PYTHON_EXECUTABLE} setup.py install --prefix=${CMAKE_INSTALL_PREFIX}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})"
    )
endif(PYTHONINTERP_FOUND)

find_package(Doxygen)
if (DOXYGEN_FOUND)
    if(DOXYGEN_DOT_EXECUTABLE)
        SET(DOXYGEN_DOT_FOUND YES)
    else(DOXYGEN_DOT_EXECUTABLE)
        SET(DOXYGEN_DOT_FOUND NO)
        SET(DOXYGEN_DOT_EXECUTABLE "")
    endif(DOXYGEN_DOT_EXECUTABLE)
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(doc doxygen Doxyfile)
endif(DOXYGEN_FOUND)
