#include "task_queue.hpp"

namespace retrojsvice {

class Window {
SHARED_ONLY_CLASS(Window);
public:
    Window(CKey, uint64_t handle);

private:
    uint64_t handle_;
};

}
