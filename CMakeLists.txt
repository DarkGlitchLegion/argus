cmake_minimum_required(VERSION 4.3)
project(argus)

set(CMAKE_CXX_STANDARD 20)

add_executable(argus
    main.cpp
        src/model/MalwareSignal.cpp
        include/model/MalwareSignal.h
        src/model/RemoteCommandHandler.cpp
        include/model/RemoteCommandHandler.h
        src/controller/Listener.cpp
        include/controller/Listener.h
)
