#include <iostream>
#include <nlohmann/json.hpp>
#include <functional>

using namespace std;

class SignalClient {
  public:
    void connect();
    void listen();
    void send(const nlohmann::json& packet);
    void addMessageHandler(function<void(const nlohmann::json)> handler);
    void handleMessage(const nlohmann::json& message);
    void sendRegistration();
    void close();
  private:
    std::vector<std::function<void(const json&)>> handlers;
};
