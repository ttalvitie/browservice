#pragma once

#include "common.hpp"

namespace retrojsvice {

class TaskQueueEventHandler {
public:
    // May be called from any thread at any time to signal that
    // TaskQueue::runTasks needs to be called.
    virtual void onTaskQueueNeedsRunTasks() = 0;

    // Called to signal that shutdown has completed, which means that no more
    // tasks may be posted and the task queue may be destructed.
    virtual void onTaskQueueShutdownComplete() = 0;
};

class DelayedTaskTag;

// A queue used to defer tasks to be run later in the API thread. Normally, the
// queue is used as follows:
// A Context sets its TaskQueue as the active task queue for the current thread
// for the duration of an API function call using ActiveTaskQueueLock. This
// means that all the tasks posted using postTask and postDelayedTask in that
// thread will be posted to that queue. The posted tasks will be run in the API
// thread when Context::pumpEvents invokes the runTasks member function.
//
// When starting a background thread that needs to call postTask or
// postDelayedTask, one should call getActiveTaskQueue in the API thread, copy
// the returned shared pointer to the started thread and set it as active there
// using ActiveTaskQueueLock.
//
// Before destruction, the task queue must be shut down by calling shutdown and
// waiting for the onTaskQueueShutdownComplete event. 
class TaskQueue {
SHARED_ONLY_CLASS(TaskQueue);
public:
    // Creates a new TaskQueue that will call newTasksCallback (possibly in a
    // background thread) if runTasks needs to be called.
    TaskQueue(CKey, weak_ptr<TaskQueueEventHandler> eventHandler);

    ~TaskQueue();

    void runTasks();

    // Start shutting down the queue. The shutdown will complete the next time
    // the queue is completely empty. Upon completion, the
    // onTaskQueueShutdownComplete event handler will be called. All
    // attempts to post new tasks to the queue or call runTasks after this will
    // panic.
    void shutdown();

    // Returns the active task queue for the current thread; panics if there is
    // none.
    static shared_ptr<TaskQueue> getActiveQueue();

private:
    void afterConstruct_(shared_ptr<TaskQueue> self);

    void needsRunTasks_();

    weak_ptr<TaskQueueEventHandler> eventHandler_;

    mutex mutex_;
    enum {Running, ShutdownPending, ShutdownComplete} state_;
    bool runTasksPending_;
    vector<function<void()>> tasks_;
    multimap<
        steady_clock::time_point,
        pair<weak_ptr<DelayedTaskTag>, function<void()>>
    > delayedTasks_;

    thread delayThread_;
    condition_variable delayThreadCv_;

    bool runningTasks_;

    friend void postTask(function<void()> func);
    friend class DelayedTaskTag;
    friend shared_ptr<DelayedTaskTag> postDelayedTask(
        steady_clock::duration delay,
        function<void()> func
    );
};

// RAII object that sets given task queue as active for the current thread;
// panics if this thread already has an active task queue. The task queue stays
// active until the returned object is destructed.
class ActiveTaskQueueLock {
public:
    ActiveTaskQueueLock(shared_ptr<TaskQueue> taskQueue);
    ~ActiveTaskQueueLock();

    DISABLE_COPY_MOVE(ActiveTaskQueueLock);
};

void postTask(function<void()> func);

template <typename T, typename... Args>
void postTask(shared_ptr<T> ptr, void (T::*func)(Args...), Args... args) {
    postTask([ptr, func, args...]() {
        (ptr.get()->*func)(args...);
    });
}

template <typename T, typename... Args>
void postTask(weak_ptr<T> weakPtr, void (T::*func)(Args...), Args... args) {
    postTask([weakPtr, func, args...]() {
        if(shared_ptr<T> ptr = weakPtr.lock()) {
            (ptr.get()->*func)(args...);
        }
    });
}

// Object returned by postDelayedTask. If the object is destructed and the delay
// for the task has not yet been reached, the task will be cancelled.
class DelayedTaskTag {
SHARED_ONLY_CLASS(DelayedTaskTag);
public:
    // Private constructor
    DelayedTaskTag(CKey, CKey);

    ~DelayedTaskTag();

private:
    shared_ptr<TaskQueue> taskQueue_;
    bool inQueue_;
    multimap<
        steady_clock::time_point,
        pair<weak_ptr<DelayedTaskTag>, function<void()>>
    >::iterator iter_;

    friend class TaskQueue;
    friend shared_ptr<DelayedTaskTag> postDelayedTask(
        steady_clock::duration delay,
        function<void()> func
    );
};

shared_ptr<DelayedTaskTag> postDelayedTask(
    steady_clock::duration delay,
    function<void()> func
);

template <typename T, typename... Args>
shared_ptr<DelayedTaskTag> postDelayedTask(
    steady_clock::duration delay,
    shared_ptr<T> ptr,
    void (T::*func)(Args...),
    Args... args
) {
    return postDelayedTask(delay, [ptr, func, args...]() {
        (ptr.get()->*func)(args...);
    });
}

template <typename T, typename... Args>
shared_ptr<DelayedTaskTag> postDelayedTask(
    steady_clock::duration delay,
    weak_ptr<T> weakPtr,
    void (T::*func)(Args...),
    Args... args
) {
    return postDelayedTask(delay, [weakPtr, func, args...]() {
        if(shared_ptr<T> ptr = weakPtr.lock()) {
            (ptr.get()->*func)(args...);
        }
    });
}

}
