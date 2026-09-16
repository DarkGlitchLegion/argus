#include "controller/controller.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <room> <client-id> <host> <username>\n";
        return 1;
    }

    listenBashMode(argv[1], argv[2], argv[3], argv[4]);
    return 0;
}
