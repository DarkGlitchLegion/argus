//
// Created by darkglitch on 9/17/26.
//

#ifndef ARGUS_LISTENER_H
#define ARGUS_LISTENER_H
#include <memory>
#include "../model/MalwareSignal.h"
#include "../model/RemoteCommandHandler.h"


class RemoteCommandHandler;

class Listener {
public:
    Listener(
        std::shared_ptr<MalwareSignal> signal,
        std::shared_ptr<RemoteCommandHandler> handler
    );

    void run();
    void stop();

private:
    std::shared_ptr<MalwareSignal> signal_;
    std::shared_ptr<RemoteCommandHandler> handler_;

    bool running_ = true;
};


#endif //ARGUS_LISTENER_H
