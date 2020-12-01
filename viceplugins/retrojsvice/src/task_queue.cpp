#include "task_queue.hpp"

namespace retrojsvice {

namespace {

thread_local shared_ptr<TaskQueue> activeTaskQueue;

}

TaskQueue::TaskQueue(CKey, function<void()> newTasksCallback) {
    REQUIRE_API_THREAD();

    running_ = true;
    newTasksCallback_ = newTasksCallback;

    runningTasks_ = false;
    runningLastTask_ = false;
}

TaskQueue::~TaskQueue() {
    REQUIRE(tasks_.empty());
    REQUIRE(!runningTasks_);
}

void TaskQueue::runTasks() {
    REQUIRE_API_THREAD();

    REQUIRE(!runningTasks_);
    runningTasks_ = true;

    vector<function<void()>> tasksToRun;
    {
        lock_guard lock(mutex_);
        REQUIRE(running_);

        swap(tasks_, tasksToRun);
    }

    for(size_t i = 0; i < tasksToRun.size(); ++i) {
        if(i + 1 == tasksToRun.size()) {
            runningLastTask_ = true;
        }
        tasksToRun[i]();
        runningLastTask_ = false;
    }

    runningTasks_ = false;
}

void TaskQueue::shutdown() {
    REQUIRE_API_THREAD();
    lock_guard lock(mutex_);

    REQUIRE(tasks_.empty());
    REQUIRE(!runningTasks_ || runningLastTask_);

    running_ = false;
    newTasksCallback_ = []() {};
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

    function<void()> runAfterPost = []() {};

    {
        lock_guard lock(activeTaskQueue->mutex_);
        REQUIRE(activeTaskQueue->running_);

        if(activeTaskQueue->tasks_.empty()) {
            runAfterPost = activeTaskQueue->newTasksCallback_;
        }
        activeTaskQueue->tasks_.push_back(func);
    }

    runAfterPost();
}

}
