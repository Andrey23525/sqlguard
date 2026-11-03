cmake_minimum_required(VERSION 3.20)

project(sqlguard_package_consumer LANGUAGES CXX)

find_package(sqlguard 1.0 CONFIG REQUIRED)

add_executable(consumer_shared main.cpp)
target_link_libraries(consumer_shared PRIVATE sqlguard::shared)

add_executable(consumer_static main.cpp)
target_link_libraries(consumer_static PRIVATE sqlguard::static)
