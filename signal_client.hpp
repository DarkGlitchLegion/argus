#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;

class SignalClient {
  void connect();
  void listen();
  void send(const nlohmann::json& packet);
  void addMessageHandler(function<void(const nlohmann::json)> handler);
  void handleMessage(const nlohmann::json& message);
  void sendRegistration();
  void close();
};
