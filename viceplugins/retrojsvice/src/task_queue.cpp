#include "task_queue.hpp"

namespace retrojsvice {

namespace {

thread_local shared_ptr<TaskQueue> activeTaskQueue;

}

TaskQueue::TaskQueue(CKey, weak_ptr<TaskQueueEventHandler> eventHandler) {
    REQUIRE_API_THREAD();

    eventHandler_ = eventHandler;
    state_ = Running;
    runningTasks_ = false;
}

TaskQueue::~TaskQueue() {
    REQUIRE(state_ == ShutdownComplete);
}

void TaskQueue::runTasks() {
    REQUIRE_API_THREAD();

    REQUIRE(!runningTasks_);
    runningTasks_ = true;

    vector<function<void()>> tasksToRun;
    bool shutdownPending;
    {
        lock_guard lock(mutex_);
        REQUIRE(state_ != ShutdownComplete);

        swap(tasks_, tasksToRun);
        shutdownPending = state_ == ShutdownPending;
    }

    for(function<void()> task : tasksToRun) {
        task();
    }

    bool shutdownComplete = false;
    if(shutdownPending) {
        lock_guard lock(mutex_);
        REQUIRE(state_ == ShutdownPending);

        if(tasks_.empty()) {
            state_ = ShutdownComplete;
            shutdownComplete = true;
        }
    }

    if(shutdownComplete) {
        INFO_LOG("Task queue shutdown complete");

        if(shared_ptr<TaskQueueEventHandler> eventHandler = eventHandler_.lock()) {
            eventHandler->onTaskQueueShutdownComplete();
        }
    }

    runningTasks_ = false;
}

void TaskQueue::shutdown() {
    REQUIRE_API_THREAD();

    {
        lock_guard lock(mutex_);
        REQUIRE(state_ == Running);

        state_ = ShutdownPending;
    }

    INFO_LOG("Shutting down task queue");

    // Make sure runTasks will be called
    postTask([]() {});
}

shared_ptr<TaskQueue> TaskQueue::getActiveQueue() {
    REQUIRE(activeTaskQueue);
    return activeTaskQueue;
}

ActiveTaskQueueLock::ActiveTaskQueueLock(shared_ptr<TaskQueue> taskQueue) {
    REQUIRE(!activeTaskQueue);
    REQUIRE(taskQueue);
    activeTaskQueue = taskQueue;
}

ActiveTaskQueueLock::~ActiveTaskQueueLock() {
    REQUIRE(activeTaskQueue);
    activeTaskQueue.reset();
}

void postTask(function<void()> func) {
    REQUIRE(activeTaskQueue);

    bool needsRunTasks = false;

    {
        lock_guard lock(activeTaskQueue->mutex_);
        REQUIRE(activeTaskQueue->state_ != TaskQueue::ShutdownComplete);

        if(activeTaskQueue->tasks_.empty()) {
            needsRunTasks = true;
        }
        activeTaskQueue->tasks_.push_back(func);
    }

    if(needsRunTasks) {
        if(
            shared_ptr<TaskQueueEventHandler> eventHandler =
                activeTaskQueue->eventHandler_.lock()
        ) {
            eventHandler->onTaskQueueNeedsRunTasks();
        }
    }
}

}
