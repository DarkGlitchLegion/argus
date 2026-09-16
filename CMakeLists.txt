cmake_minimum_required(VERSION 4.3)
project(argus)

set(CMAKE_CXX_STANDARD 20)

add_executable(argus
    main.cpp
        controller/controller.cpp
        signal/signal_client.cpp
        handler/remote_command_handler.cpp
)
