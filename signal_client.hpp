#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;

class SignalClient {
  void send(const nlohmann::json& packet);
  void addMessageHandler(function<void(const nlohmann::json)> handler);
};
