#include "window.hpp"

namespace retrojsvice {

Window::Window(CKey, uint64_t handle) {
    REQUIRE(handle);
    handle_ = handle;
}

}
