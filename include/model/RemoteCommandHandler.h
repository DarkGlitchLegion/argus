//
// Created by darkglitch on 9/17/26.
//

#ifndef ARGUS_REMOTECOMMANDHANDLER_H
#define ARGUS_REMOTECOMMANDHANDLER_H
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>


class MalwareSignal;

struct CommandResult {
    std::string requestId;
    std::string status;
    std::string output;
    std::string error;
};

class RemoteCommandHandler {
public:
    explicit RemoteCommandHandler(std::shared_ptr<MalwareSignal> signal);

    // Sends a request to another client.
    // If waitForResult is true, the returned future contains the result.
    std::future<CommandResult> sendCommand(
        const std::string& target,
        const std::string& command,
        bool waitForResult = false,
        int timeoutSeconds = 120
    );

    // Called by SignalClient when a JSON message arrives.
    void dispatchMessage(const std::string& message);

private:
    void handleOffer(const std::string& message);
    void handleAnswer(const std::string& message);

    void processCommandRequest(
        const std::string& sender,
        const std::string& command,
        const std::string& requestId
    );

    CommandResult executeAllowedCommand(
        const std::string& command,
        const std::string& requestId
    );

    void sendResult(
        const std::string& target,
        const CommandResult& result
    );

    void handleCommandResult(const CommandResult& result);

    std::shared_ptr<MalwareSignal> signal_;

    std::mutex pendingMutex_;

    std::unordered_map<
        std::string,
        std::shared_ptr<std::promise<CommandResult>>
    > pendingResults_;
};


#endif //ARGUS_REMOTECOMMANDHANDLER_H
