cmake_minimum_required(VERSION 3.10)
project(distributed-messaging-service C)

set(CMAKE_C_STANDARD 11)

# Create libraries
add_library(users_llist SHARED include/users_llist.c include/users_llist.h)
add_library(lines SHARED include/lines.c include/lines.h)

# Create executables
add_executable(server server.c)

# Link pthread library
find_package(Threads REQUIRED)
target_link_libraries(server ${CMAKE_THREAD_LIBS_INIT})

# Link libraries to targets
target_link_libraries(server lines)
target_link_libraries(server users_llist)
