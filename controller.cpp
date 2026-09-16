#include "signal.hpp"
#include "remote_command_handler.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std;

void listenBashMode(const string& room, const string& clientId, const string& host, const string& username) {
  const int retryDelay = 10;

  Signal signal(room, clientId, host, username);

  //Register dispatchMessage() with Signal.
  RemoteCommandHandler receiver(&signal);

  while (true) {
    try {
      signal.connect();
      cout << "[+] LISTENING AS "
        << username
        << " : "
        << clientId
        << endl;
      //Blocks while the WebSocket is connected
      signal.listen();
    } catch(const exception& e) {
      cout << "[!] CONNECTION LOST: "
        << e.what()
        << endl;
    }

    signal.close();
    cout << "[!] RECONNECTING IN"
      << retryDelay
      << " seconds"
      << endl;

    this_thread::sleep_for(chrono::seconds(retryDelay));

  }

}
