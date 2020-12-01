#pragma once

#include "common.hpp"

namespace retrojsvice {

// A queue used to defer tasks to be run later in the API thread. Normally, the
// queue is used as follows:
// A Context sets its TaskQueue as the active task queue for the current thread
// for the duration of an API function call using ActiveTaskQueueLock. This
// means that all the tasks posted using postTask in that thread will be posted
// to that queue. The posted tasks will be run in the API thread when
// Context::pumpEvents invokes the runTasks member function.
//
// When starting a background thread that needs to call postTask, one should
// call getActiveTaskQueue in the API thread, copy the returned shared pointer
// to the started thread and set it as active there using ActiveTaskQueueLock.
class TaskQueue {
SHARED_ONLY_CLASS(TaskQueue);
public:
    // Creates a new TaskQueue that will call newTasksCallback (possibly in a
    // background thread) if runTasks needs to be called.
    TaskQueue(CKey, function<void()> newTasksCallback);

    ~TaskQueue();

    void runTasks();

    // Shut down the queue. Panics if there are tasks in the queue or being run
    // by runTasks (apart from possibly the task calling this function). All
    // attempts to post new tasks to the queue or call runTasks will panic.
    void shutdown();

    // Returns the active task queue for the current thread; panics if there is
    // none.
    static shared_ptr<TaskQueue> getActiveQueue();

private:
    mutex mutex_;
    bool running_;
    function<void()> newTasksCallback_;
    vector<function<void()>> tasks_;

    bool runningTasks_;
    bool runningLastTask_;

    friend void postTask(function<void()> func);
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

}
