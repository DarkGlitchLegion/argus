//
// Created by darkglitch on 9/17/26.
//

#include "../../include/model/RemoteCommandHandler.h"
#include "../../include/model/MalwareSignal.h"
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <random>
#include <sstream>

using json = nlohmann::json;

namespace {

std::string generateRequestId()
{
    static std::mt19937_64 rng{
        std::random_device{}()
    };

    std::stringstream ss;
    ss << std::hex << rng();
    return ss.str();
}

} // namespace


RemoteCommandHandler::RemoteCommandHandler(
    std::shared_ptr<MalwareSignal> signal
)
    : signal_(std::move(signal))
{
    if (signal_) {
        signal_->addHandler(
            [this](const std::string& message) {
                dispatchMessage(message);
            }
        );
    }
}


std::future<CommandResult>
RemoteCommandHandler::sendCommand(
    const std::string& target,
    const std::string& command,
    bool waitForResult,
    int timeoutSeconds
)
{
    if (command.empty()) {
        throw std::invalid_argument(
            "command must not be empty"
        );
    }

    const std::string requestId = generateRequestId();

    json payload = {
        {"type", "remote-command"},
        {"command", command},
        {"request_id", requestId}
    };

    json packet = {
        {"type", "offer"},
        {"target", target},
        {"sdp", payload.dump()}
    };

    std::future<CommandResult> future;

    if (waitForResult) {
        auto promise =
            std::make_shared<std::promise<CommandResult>>();

        future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(pendingMutex_);

            pendingResults_[requestId] = promise;
        }
    }

    if (!signal_->send(packet.dump())) {
        std::lock_guard<std::mutex> lock(pendingMutex_);

        pendingResults_.erase(requestId);

        throw std::runtime_error(
            "failed to send command request"
        );
    }

    if (!waitForResult) {
        std::promise<CommandResult> completed;
        auto result = completed.get_future();

        CommandResult r;
        r.requestId = requestId;
        r.status = "sent";

        completed.set_value(r);

        return result;
    }

    return future;
}


void RemoteCommandHandler::dispatchMessage(
    const std::string& message
)
{
    try {
        const json packet = json::parse(message);

        const std::string type =
            packet.value("type", "");

        if (type == "offer") {
            handleOffer(message);
        }
        else if (type == "answer") {
            handleAnswer(message);
        }
        else if (type == "remote-command") {
            processCommandRequest(
                packet.value("sender", ""),
                packet.value("command", ""),
                packet.value("request_id", "")
            );
        }
        else if (type == "remote-command-result") {
            CommandResult result;

            result.requestId =
                packet.value("request_id", "");

            result.status =
                packet.value("status", "");

            result.output =
                packet.value("output", "");

            result.error =
                packet.value("error", "");

            handleCommandResult(result);
        }
    }
    catch (const json::exception& e) {
        std::cerr
            << "Invalid JSON message: "
            << e.what()
            << '\n';
    }
}

void RemoteCommandHandler::handleOffer(
    const std::string& message
)
{
    try {
        const json packet = json::parse(message);

        if (!packet.contains("sdp") ||
            !packet["sdp"].is_string()) {
            return;
            }

        const json payload =
            json::parse(packet["sdp"].get<std::string>());

        if (payload.value("type", "") !=
            "remote-command") {
            return;
            }

        processCommandRequest(
            packet.value("sender", ""),
            payload.value("command", ""),
            payload.value("request_id", "")
        );
    }
    catch (const json::exception& e) {
        std::cerr
            << "Invalid offer: "
            << e.what()
            << '\n';
    }
}

void RemoteCommandHandler::handleAnswer(
    const std::string& message
)
{
    try {
        const json packet = json::parse(message);

        if (!packet.contains("sdp") ||
            !packet["sdp"].is_string()) {
            return;
            }

        const json payload =
            json::parse(packet["sdp"].get<std::string>());

        if (payload.value("type", "") !=
            "remote-command-result") {
            return;
            }

        CommandResult result;

        result.requestId =
            payload.value("request_id", "");

        result.status =
            payload.value("status", "");

        result.output =
            payload.value("output", "");

        result.error =
            payload.value("error", "");

        handleCommandResult(result);
    }
    catch (const json::exception& e) {
        std::cerr
            << "Invalid answer: "
            << e.what()
            << '\n';
    }
}

CommandResult RemoteCommandHandler::executeAllowedCommand(
    const std::string& command,
    const std::string& requestId
)
{
    CommandResult result;
    result.requestId = requestId;

    if (command == "ping") {
        result.status = "success";
        result.output = "pong";
        return result;
    }

    if (command == "status") {
        result.status = "success";
        result.output = "online";
        return result;
    }

    if (command == "version") {
        result.status = "success";
        result.output = "1.0.0";
        return result;
    }

    result.status = "error";
    result.error = "COMMAND_NOT_ALLOWED";

    return result;
}

void RemoteCommandHandler::processCommandRequest(
    const std::string& sender,
    const std::string& command,
    const std::string& requestId
)
{
    if (sender.empty() ||
        command.empty() ||
        requestId.empty()) {
        return;
        }

    std::cout
        << "Processing command request "
        << requestId
        << '\n';

    CommandResult result =
        executeAllowedCommand(command, requestId);

    sendResult(sender, result);
}

void RemoteCommandHandler::handleCommandResult(
    const CommandResult& result
)
{
    std::shared_ptr<std::promise<CommandResult>> promise;

    {
        std::lock_guard<std::mutex> lock(pendingMutex_);

        auto it = pendingResults_.find(result.requestId);

        if (it == pendingResults_.end()) {
            return;
        }

        promise = it->second;
        pendingResults_.erase(it);
    }

    promise->set_value(result);
}

void RemoteCommandHandler::sendResult(
    const std::string& target,
    const CommandResult& result
)
{
    json payload = {
        {"type", "remote-command-result"},
        {"request_id", result.requestId},
        {"status", result.status}
    };

    if (!result.output.empty()) {
        payload["output"] = result.output;
    }

    if (!result.error.empty()) {
        payload["error"] = result.error;
    }

    json packet = {
        {"type", "answer"},
        {"target", target},
        {"sdp", payload.dump()}
    };

    signal_->send(packet.dump());
}
