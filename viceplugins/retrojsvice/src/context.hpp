#pragma once

#include "common.hpp"

namespace retrojsvice {

class TaskQueue;

// The implementation of the vice plugin context, exposed through the C API in
// vice_plugin_api.cpp.
class Context : public enable_shared_from_this<Context> {
SHARED_ONLY_CLASS(Context);
public:
    // Returns either a successfully constructed context or an error message.
    static variant<shared_ptr<Context>, string> init(
        vector<pair<string, string>> options
    );

    // Private constructor.
    Context(CKey, CKey);
    ~Context();

    // Public API functions:
    void start(
        function<void()> eventNotifyCallback,
        function<void()> shutdownCompleteCallback
    );
    void shutdown();

    void pumpEvents();

    // Returns (name, valSpec, desc, defaultValStr)-tuples.
    static vector<tuple<string, string, string, string>> getOptionDocs();

private:
    void shutdownComplete_();

    enum {Pending, Running, ShutdownComplete} state_;
    bool shutdownPending_;
    atomic<bool> inAPICall_;

    shared_ptr<TaskQueue> taskQueue_;

    function<void()> shutdownCompleteCallback_;

    class APILock;
    class RunningAPILock;
};

}
