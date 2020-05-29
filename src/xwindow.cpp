#include "xwindow.hpp"

#include <X11/Xlib.h>

class XWindow::Impl {
public:
    Impl() {
        display_ = XOpenDisplay(nullptr);
        if(display_ == nullptr) {
            LOG(ERROR) << "Opening X display failed";
            CHECK(false);
        }

        unsigned long color = BlackPixel(display_, DefaultScreen(display_));
        window_ = XCreateSimpleWindow(
            display_,
            DefaultRootWindow(display_),
            0, 0,
            1, 1,
            0,
            color, color
        );
    }

    ~Impl() {
        XDestroyWindow(display_, window_);
        XCloseDisplay(display_);
    }

    DISABLE_COPY_MOVE(Impl);

    uint64_t handle() {
        uint64_t ret = window_;
        CHECK((Window)ret == window_);
        return ret;
    }

private:
    Display* display_;
    Window window_;
};

XWindow::XWindow(CKey) {
    impl_ = make_unique<Impl>();
}

XWindow::~XWindow() {}

uint64_t XWindow::handle() {
    CEF_REQUIRE_UI_THREAD();
    return impl_->handle();
}
