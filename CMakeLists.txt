cmake_minimum_required(VERSION 3.25)
project(sireflect LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
option(BUILD_SHARED_LIBS "Build shared libraries" OFF)

find_package(sicore CONFIG REQUIRED)

add_library(sireflect
    src/sireflect_assert.c
    src/sireflect_enum.c
    src/sireflect_error.c
    src/sireflect_extra.c
    src/sireflect_field.c
    src/sireflect_parser.c
    src/sireflect_registry.c
    src/sireflect_struct.c
    src/sireflect_type.c
    src/sireflect_walk.c)
add_library(sireflect::sireflect ALIAS sireflect)
target_compile_features(sireflect PUBLIC c_std_17)
set_target_properties(sireflect PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES)
target_include_directories(sireflect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(sireflect PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(sireflect PRIVATE sireflect_EXPORTS)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(sireflect PUBLIC sireflect_STATIC)
endif()
target_link_libraries(sireflect PUBLIC sicore::sicore)

install(TARGETS sireflect EXPORT sireflectTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/sireflect.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES include/sireflect/export.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/sireflect)
install(EXPORT sireflectTargets NAMESPACE sireflect:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sireflect)
configure_package_config_file(cmake/sireflectConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/sireflectConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sireflect)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sireflectConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sireflect)

