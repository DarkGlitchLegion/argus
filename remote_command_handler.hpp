#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <condition_varaible>
#include <functional>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace std;

class SignalClient;

using json = nlohmann:json;

class RemoteCommandHandler {
  public:
    explicit RemoteCommandHandler(SignalClient* signal);

    //Send a remote command.
    json sendCommand(const string& target, const string& command, bool waitForResult = true, int timeoutSeconds = 120);
ent* signal_;

    struct PendingRequest {
      mutex mutex;
      condition_varaible cv;
      bool completed = false;
      json result;
    };

    unordered_map<string, shared_ptr<PendingRequest>> pendingResults_;
    mutex pendingMutex;

    //Internal handlers
    void handleOffer(const json& message);
    void handleAnswer(const json& message);
    void processCommandRequest(const json& message);
    void handleCommandResult(const json& message);

    //Command execution
    tuple<string, string, string>executeCommand(const string& command);

    //Send response
    void sendResult(
      const string& target,
      const string& requestId,
      const string& status,
      const string& output = "",
      const string& error = ""
    );

    string generateUUID();
};
