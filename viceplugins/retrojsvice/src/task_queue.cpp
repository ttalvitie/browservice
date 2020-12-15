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
    steady_clock::time_point now = steady_clock::now();
    bool runDelayedTasks;
    bool shutdownPending;
    {
        lock_guard lock(mutex_);
        REQUIRE(state_ != ShutdownComplete);

        runTasksPending_ = false;

        swap(tasks_, tasksToRun);

        runDelayedTasks =
            !delayedTasks_.empty() &&
            delayedTasks_.begin()->first <= now;

        shutdownPending = state_ == ShutdownPending;
    }

    for(function<void()>& task : tasksToRun) {
        task();
        task = []() {};
    }

    if(runDelayedTasks) {
        while(true) {
            function<void()> task;
            {
                lock_guard lock(mutex_);
                if(delayedTasks_.empty() || delayedTasks_.begin()->first > now) {
                    break;
                }

                weak_ptr<DelayedTaskTag> tagWeak;
                tie(tagWeak, task) = delayedTasks_.begin()->second;
                delayedTasks_.erase(delayedTasks_.begin());

                shared_ptr<DelayedTaskTag> tag = tagWeak.lock();
                REQUIRE(tag);
                REQUIRE(tag->inQueue_);
                tag->inQueue_ = false;
            }
            task();
        }
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

DelayedTaskTag::DelayedTaskTag(CKey, CKey) {}

DelayedTaskTag::~DelayedTaskTag() {
    {
        lock_guard lock(taskQueue_->mutex_);
        if(inQueue_) {
            REQUIRE(taskQueue_->state_ != TaskQueue::ShutdownComplete);
            taskQueue_->delayedTasks_.erase(iter_);
        }
    }

    taskQueue_->delayThreadCv_.notify_one();
}

shared_ptr<DelayedTaskTag> postDelayedTask(
    steady_clock::duration delay,
    function<void()> func
) {
    REQUIRE(activeTaskQueue);

    steady_clock::time_point time = steady_clock::now() + delay;

    shared_ptr<DelayedTaskTag> tag;
    {
        lock_guard lock(activeTaskQueue->mutex_);
        REQUIRE(activeTaskQueue->state_ != TaskQueue::ShutdownComplete);

        tag = DelayedTaskTag::create(DelayedTaskTag::CKey());

        auto iter = activeTaskQueue->delayedTasks_.emplace(
            time,
            pair<weak_ptr<DelayedTaskTag>, function<void()>>(tag, func)
        );

        tag->taskQueue_ = activeTaskQueue;
        tag->inQueue_ = true;
        tag->iter_ = iter;
    }

    activeTaskQueue->delayThreadCv_.notify_one();

    return tag;
}

}
