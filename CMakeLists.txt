cmake_minimum_required(VERSION 3.20)
project(argus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.74 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(argus
    main.cpp
    src/model/MalwareSignal.cpp
    include/model/MalwareSignal.h
    src/model/RemoteCommandHandler.cpp
    include/model/RemoteCommandHandler.h
    src/controller/Listener.cpp
    include/controller/Listener.h
)

target_link_libraries(argus PRIVATE
    Boost::headers
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)
