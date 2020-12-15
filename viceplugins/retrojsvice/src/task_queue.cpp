#include "task_queue.hpp"

namespace retrojsvice {

namespace {

thread_local shared_ptr<TaskQueue> activeTaskQueue;

}

TaskQueue::TaskQueue(CKey, weak_ptr<TaskQueueEventHandler> eventHandler) {
    REQUIRE_API_THREAD();

    eventHandler_ = eventHandler;
    state_ = Running;
    runTasksPending_ = false;
    runningTasks_ = false;

    // Initialization is completed in afterConstruct_
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

        runTasksPending_ = false;

        swap(tasks_, tasksToRun);

        steady_clock::time_point now = steady_clock::now();
        while(!delayedTasks_.empty() && delayedTasks_.begin()->first <= now) {
            tasksToRun.push_back(delayedTasks_.begin()->second);
            delayedTasks_.erase(delayedTasks_.begin());
        }

        shutdownPending = state_ == ShutdownPending;
    }

    for(function<void()>& task : tasksToRun) {
        task();
        task = []() {};
    }

    bool shutdownComplete = false;
    if(shutdownPending) {
        lock_guard lock(mutex_);
        REQUIRE(state_ == ShutdownPending);

        if(tasks_.empty() && delayedTasks_.empty()) {
            state_ = ShutdownComplete;
            shutdownComplete = true;
        }
    }

    delayThreadCv_.notify_one();

    if(shutdownComplete) {
        delayThread_.join();

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

void TaskQueue::afterConstruct_(shared_ptr<TaskQueue> self) {
    delayThread_ = thread([self]() {
        unique_lock<mutex> lock(self->mutex_);
        while(true) {
            if(self->state_ == ShutdownComplete) {
                break;
            } else if(self->delayedTasks_.empty() || self->runTasksPending_) {
                self->delayThreadCv_.wait(lock);
            } else {
                steady_clock::time_point now = steady_clock::now();
                steady_clock::time_point wakeup = self->delayedTasks_.begin()->first;
                if(wakeup <= now) {
                    self->runTasksPending_ = true;
                    self->needsRunTasks_();
                    self->delayThreadCv_.wait(lock);
                } else {
                    self->delayThreadCv_.wait_for(lock, wakeup - now);
                }
            }
        }
    });
}

void TaskQueue::needsRunTasks_() {
    if(shared_ptr<TaskQueueEventHandler> eventHandler = eventHandler_.lock()) {
        eventHandler->onTaskQueueNeedsRunTasks();
    }
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

    lock_guard lock(activeTaskQueue->mutex_);
    REQUIRE(activeTaskQueue->state_ != TaskQueue::ShutdownComplete);

    activeTaskQueue->tasks_.push_back(func);

    if(!activeTaskQueue->runTasksPending_) {
        activeTaskQueue->runTasksPending_ = true;
        activeTaskQueue->needsRunTasks_();
    }
}

void postDelayedTask(steady_clock::duration delay, function<void()> func) {
    REQUIRE(activeTaskQueue);

    steady_clock::time_point time = steady_clock::now() + delay;

    lock_guard lock(activeTaskQueue->mutex_);
    REQUIRE(activeTaskQueue->state_ != TaskQueue::ShutdownComplete);

    activeTaskQueue->delayedTasks_.emplace(time, func);
    activeTaskQueue->delayThreadCv_.notify_one();
}

}
