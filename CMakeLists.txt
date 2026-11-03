cmake_minimum_required(VERSION 3.20)

project(
    sqlguard
    VERSION 1.0.0
    DESCRIPTION "SQL injection defence primitives for C++17"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(sqlguardTopLevel ON)
else()
    set(sqlguardTopLevel OFF)
endif()

option(SQLGUARD_BUILD_SHARED "Build the shared sqlguard library" ON)
option(SQLGUARD_BUILD_STATIC "Build the static sqlguard library" ON)
if(sqlguardTopLevel)
    option(SQLGUARD_BUILD_TESTS "Build SQLGuard tests" ON)
    option(SQLGUARD_BUILD_EXAMPLES "Build SQLGuard examples" ON)
else()
    option(SQLGUARD_BUILD_TESTS "Build SQLGuard tests" OFF)
    option(SQLGUARD_BUILD_EXAMPLES "Build SQLGuard examples" OFF)
endif()

if(NOT SQLGUARD_BUILD_SHARED AND NOT SQLGUARD_BUILD_STATIC)
    message(FATAL_ERROR
        "At least one of SQLGUARD_BUILD_SHARED or SQLGUARD_BUILD_STATIC must be ON")
endif()

if(sqlguardTopLevel)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()

set(sqlguardSources
    src/detector.cpp
    src/dialect.cpp
    src/error.cpp
    src/identifier.cpp
    src/sql_scanner.cpp
    src/statement.cpp
    src/value.cpp
)

set(sqlguardPublicHeaders
    include/sqlguard/detector.h
    include/sqlguard/dialect.h
    include/sqlguard/error.h
    include/sqlguard/export.h
    include/sqlguard/identifier.h
    include/sqlguard/result.h
    include/sqlguard/sqlguard.h
    include/sqlguard/statement.h
    include/sqlguard/value.h
)

function(sqlguard_configure_target targetName)
    target_sources(${targetName} PRIVATE
        ${sqlguardSources}
        ${sqlguardPublicHeaders}
    )
    target_include_directories(${targetName}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_features(${targetName} PUBLIC cxx_std_17)
    set_target_properties(${targetName} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
        POSITION_INDEPENDENT_CODE ON
    )
    if(MSVC)
        target_compile_options(${targetName} PRIVATE
            /W4
            /permissive-
            /utf-8
            /Zc:__cplusplus
            /wd4251
        )
    else()
        target_compile_options(${targetName} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
        )
    endif()
endfunction()

if(SQLGUARD_BUILD_SHARED)
    add_library(sqlguard SHARED)
    add_library(sqlguard::shared ALIAS sqlguard)
    sqlguard_configure_target(sqlguard)
    target_compile_definitions(sqlguard PRIVATE SQLGUARD_BUILDING_LIBRARY)
    set_target_properties(sqlguard PROPERTIES
        OUTPUT_NAME sqlguard
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        EXPORT_NAME shared
    )
endif()

if(SQLGUARD_BUILD_STATIC)
    add_library(sqlguard_static STATIC)
    add_library(sqlguard::static ALIAS sqlguard_static)
    sqlguard_configure_target(sqlguard_static)
    target_compile_definitions(sqlguard_static PUBLIC SQLGUARD_STATIC)
    set_target_properties(sqlguard_static PROPERTIES
        OUTPUT_NAME sqlguard_static
        EXPORT_NAME static
    )
endif()

set(sqlguardInstallTargets)
if(TARGET sqlguard)
    list(APPEND sqlguardInstallTargets sqlguard)
endif()
if(TARGET sqlguard_static)
    list(APPEND sqlguardInstallTargets sqlguard_static)
endif()

if(SQLGUARD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SQLGUARD_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

install(TARGETS ${sqlguardInstallTargets}
    EXPORT sqlguardTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/sqlguard
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT sqlguardTargets
    FILE sqlguardTargets.cmake
    NAMESPACE sqlguard::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlguard
)

configure_package_config_file(
    cmake/sqlguardConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/sqlguardConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlguard
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/sqlguardConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/sqlguardConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/sqlguardConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sqlguard
)
