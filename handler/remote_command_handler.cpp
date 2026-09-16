#include "../remote_command_handler.hpp"
#include "signal_client.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

using namespace std;

//Constructor
RemoteCommandHandler::RemoteCommandHandler(SignalClient* signal): signal(signal) {
  signal->addMessageHandler(
      [this](const json& message){
        dispatchMessage(message);
      });
}

//UUID Generator
string RemoteCommandHandler::generateUUID() {
  static random_device rd;
  static mt19937 gen(rd());

  uniform_int_distribution<> dist(0, 15);
  uniform_int_distribution<> dist2(8, 11);

  stringstream ss;
  ss << hex;

  for(int i = 0; i < 8; i++) ss << dist(gen);
  ss << "-";
  for(int i = 0; i < 4; i++) ss << dist(gen);
  ss << "-4";
  for(int i = 0; i < 3; i++) ss << dist(gen);
  ss << "-";
  ss << dist2(gen);
  for(int i = 0; i < 3; i++) ss dist(gen);
  ss << "-";
  for(int i = 0; i < 12; i++) ss << dist(gen);

  return ss.str();
}

// SEND COMMAND
json RemoteCommandHandler::sendCommand(const string& target, const string& command, const string& requestIdInput, bool waitForResult, int timeout) {
    if(command.empty()){
        throw runtime_error("COMMAND MUST BE A NON-EMPTY STRING");
    }

    string requestId = requestIdInput.empty() ? generateUUID(): requestIdInput;

    json packet = {
        {"type","offer"},
        {"target",target},
        {"sdp",
            json({
                {"type","remote-command"},
                {"command",command},
                {"request_id",requestId}
            }).dump()
        }
    };

    cout << "SENDING COMMAND REQUEST_ID: "
         << requestId
         << " TARGET: "
         << target
         << " COMMAND: "
         << command
         << endl;

    shared_ptr<PendingResult> pending = nullptr;

    if(waitForResult){
        pending = make_shared<PendingResult>();

        lock_guard<mutex> lock(pendingMutex);
        pendingResults[requestId] = pending;
    }

    signal->send(packet);

    if(!waitForResult){
        return json{{"request_id",requestId}};
    }

    unique_lock<mutex> lock(pending->mutex);

    bool success = pending->cv.wait_for(lock, chrono::seconds(timeout), [&]{ return pending->done; });
    {
        lock_guard<mutex> cleanup(pendingMutex);
        pendingResults.erase(requestId);
    }
    if(!success){
        throw runtime_error("COMMAND TIMEOUT");
    }

    return pending->response;
}


// MESSAGE ROUTER
void RemoteCommandHandler::dispatchMessage(const json& message)
{
    if(!message.is_object()){
        cout << "INVALID MESSAGE\n";
        return;
    }

    string type = message.value("type","");

    if(type == "offer"){
        handleOffer(message);
    }
    else if(type == "answer"){
        handleAnswer(message);
    }
    else if(type == "remote-command"){
        processCommandRequest(message);
    }
    else if(type == "remote-command-result"){
        handleCommandResult(message);
    }
}


// OFFER HANDLER
void RemoteCommandHandler::handleOffer(const json& message) {
    if(!message.contains("sdp")) return;

    string sdp = message["sdp"];

    json payload;

    try{
        payload = json::parse(sdp);
    }
    catch(...){
        cout << "INVALID JSON SDP\n";
        return;
    }

    if(payload.value("type","") != "remote-command"){
        return;
    }

    json request = {
        {"sender", message.value("sender","")},
        {"command", payload.value("command","")},
        {"request_id", payload.value("request_id","")}
    };

    processCommandRequest(request);
}


// ANSWER HANDLER
void RemoteCommandHandler::handleAnswer(const json& message) {
    if(!message.contains("sdp")) return;

    string sdp = message["sdp"];

    json payload;

    try{
        payload = json::parse(sdp);
    }
    catch(...){
        cout << "INVALID JSON ANSWER\n";
        return;
    }

    if(payload.value("type","") == "remote-command-result"){
        handleCommandResult(payload);
    }
}

// PROCESS COMMAND REQUEST
void RemoteCommandHandler::processCommandRequest(const json& message) {
    string sender = message.value("sender","");
    string command = message.value("command","");
    string requestId = message.value("request_id","");

    if(command.empty()){
        sendResult(sender,requestId,"error","",
                   "INVALID COMMAND");
        return;
    }

    cout << "EXECUTING COMMAND FROM "
         << sender
         << " : "
         << command
         << endl;

    auto [status,output,error] = executeCommand(command);

    sendResult(sender,requestId,status,output,error);
}

// EXECUTE COMMAND
tuple<string,string,string> RemoteCommandHandler::executeCommand(const string& command) {
    string tempFile = "/tmp/argus_output.txt";

    string shellCommand =
        command + " > " + tempFile + " 2>&1";

    int result = system(shellCommand.c_str());

    ifstream file(tempFile);

    stringstream buffer;
    buffer << file.rdbuf();

    string output = buffer.str();

    remove(tempFile.c_str());

    if(result == 0){
        if(output.empty()){
            output = "COMMAND EXECUTED SUCCESSFULLY";
        }

        return {"success",output,""};
    }

    return {"error","","COMMAND FAILED\n" + output};
}

// HANDLE RESULT
void RemoteCommandHandler::handleCommandResult(const json& message) {
    string requestId =
        message.value("request_id","");

    if(requestId.empty()) return;

    shared_ptr<PendingResult> pending = nullptr;

    {
        lock_guard<mutex> lock(pendingMutex);

        auto it = pendingResults.find(requestId);

        if(it == pendingResults.end()){
            return;
        }

        pending = it->second;
    }

    {
        lock_guard<mutex> lock(pending->mutex);

        pending->response = message;
        pending->done = true;
    }

    pending->cv.notify_one();
}


// SEND RESULT
void RemoteCommandHandler::sendResult(const string& target, const string& requestId, const string& status, const string& output, const string& error) {
    json payload = {
        {"type","remote-command-result"},
        {"request_id",requestId},
        {"status",status}
    };

    if(!output.empty()){
        payload["output"] = output;
    }

    if(!error.empty()){
        payload["error"] = error;
    }

    json packet = {
        {"type","answer"},
        {"target",target},
        {"sdp",payload.dump()}
    };

    cout << "SENDING RESULT REQUEST_ID: "
         << requestId
         << " STATUS: "
         << status
         << endl;

    signal->send(packet);
}
