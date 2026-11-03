add_executable(sqlguard_demo demo.cpp)

if(TARGET sqlguard)
    target_link_libraries(sqlguard_demo PRIVATE sqlguard)
else()
    target_link_libraries(sqlguard_demo PRIVATE sqlguard_static)
endif()
