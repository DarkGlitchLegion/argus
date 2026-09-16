cmake_minimum_required(VERSION 4.3)
project(argus)

set(CMAKE_CXX_STANDARD 20)

add_executable(argus
    main.cpp
    controller.cpp
    signal_client.cpp
    remote_command_handler.cpp
)
