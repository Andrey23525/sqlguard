set(sqlguardTestSources
    test_main.cpp
    test_detector.cpp
    test_identifier.cpp
    test_result.cpp
    test_statement.cpp
    test_value.cpp
)

function(sqlguard_add_test_suite suffix library)
    set(targetName sqlguard_tests_${suffix})
    add_executable(${targetName} ${sqlguardTestSources})
    target_link_libraries(${targetName} PRIVATE ${library})
    target_compile_features(${targetName} PRIVATE cxx_std_17)
    set_target_properties(${targetName} PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_STANDARD_REQUIRED ON
    )
    if(MSVC)
        target_compile_options(${targetName} PRIVATE
            /W4 /permissive- /utf-8 /wd4251)
    else()
        target_compile_options(${targetName} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow)
    endif()
    add_test(NAME ${targetName} COMMAND ${targetName})
endfunction()

if(TARGET sqlguard)
    sqlguard_add_test_suite(shared sqlguard)
endif()

if(TARGET sqlguard_static)
    sqlguard_add_test_suite(static sqlguard_static)
endif()
