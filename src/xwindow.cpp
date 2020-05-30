#include "xwindow.hpp"

#include "timeout.hpp"

#include <xcb/xcb.h>

class XWindow::Impl {
SHARED_ONLY_CLASS(Impl);
public:
    Impl(CKey) {
        mode_ = Idle;

        connection_ = xcb_connect(nullptr, nullptr);
        if(connection_ == nullptr || xcb_connection_has_error(connection_)) {
            LOG(ERROR) << "Opening X display failed";
            CHECK(false);
        }

        const xcb_setup_t* setup = xcb_get_setup(connection_);
        CHECK(setup != nullptr);
        screen_ = xcb_setup_roots_iterator(setup).data;
        CHECK(screen_ != nullptr);

        uint32_t eventMask =
            XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

        window_ = xcb_generate_id(connection_);
        CHECK(xcb_request_check(connection_,
            xcb_create_window_checked(
                connection_,
                screen_->root_depth,
                window_,
                screen_->root,
                0, 0,
                1, 1,
                0,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                screen_->root_visual,
                XCB_CW_EVENT_MASK,
                &eventMask
            )
        ) == nullptr);
    }

    ~Impl() {
        CHECK(mode_ == Closed);
    }

    void close() {
        {
            lock_guard<mutex> lock(mutex_);
            CHECK(mode_ != Closed);

            mode_ = Closed;
            pasteCallback_ = [](string) {};
            copyText_.clear();
        }

        CHECK(xcb_request_check(connection_,
            xcb_destroy_window_checked(connection_, window_)
        ) == nullptr);

        eventHandlerThread_.join();

        xcb_disconnect(connection_);
    }

    void pasteFromClipboard(function<void(string)> callback) {
        lock_guard<mutex> lock(mutex_);
        CHECK(mode_ != Closed);

        if(mode_ == Pasting) {
            pasteCallback_ = callback;
        } else if(mode_ == Copying) {
            string text = copyText_;
            postTask([callback, text{move(text)}]() { callback(text); });
        } else {
            CHECK(mode_ == Idle);
            mode_ = Pasting;
            pasteCallback_ = callback;
            // TODO: start pasting
        }
    }

    void copyToClipboard(string text) {
        lock_guard<mutex> lock(mutex_);
        CHECK(mode_ != Closed);

        copyText_ = move(text);
        if(mode_ != Copying) {
            CHECK(mode_ == Pasting || mode_ == Idle);
            mode_ = Copying;
            pasteCallback_ = [](string) {};
            // TODO: start copying
        }
    }

private:
    void afterConstruct_(shared_ptr<Impl> self) {
        eventHandlerThread_ = thread([self]() {
            self->runEventHandlerThread_();
        });
    }

    void runEventHandlerThread_() {
        while(true) {
            xcb_generic_event_t* event = xcb_wait_for_event(connection_);
            CHECK(event != nullptr);

            int type = event->response_type & ~0x80;

            if(type == XCB_DESTROY_NOTIFY) {
                break;
            }
        }
    }

    bool closed_;
    thread eventHandlerThread_;

    xcb_connection_t* connection_;
    xcb_screen_t* screen_;
    xcb_window_t window_;

    mutex mutex_;
    enum {Pasting, Copying, Idle, Closed} mode_;
    function<void(string)> pasteCallback_;
    string copyText_;
};

XWindow::XWindow(CKey) {
    impl_ = Impl::create();
}

XWindow::~XWindow() {
    impl_->close();
}

namespace {

// Avoid callbacks dangling for a long time
class PasteCallbackWrapper {
public:
    PasteCallbackWrapper(function<void(string)> callback, int64_t timeoutMs) {
        callback_ = make_shared<function<void(string)>>(callback);
        timeout_ = Timeout::create(timeoutMs);

        shared_ptr<function<void(string)>> sharedCallback = callback_;
        timeout_->set([sharedCallback]() {
            *sharedCallback = [](string) {};
        });
    }
    ~PasteCallbackWrapper() {
        timeout_->clear(false);
    }

    void operator()(string text) {
        CEF_REQUIRE_UI_THREAD();
        (*callback_)(text);
    }

private:
    shared_ptr<function<void(string)>> callback_;
    shared_ptr<Timeout> timeout_;
};

}

void XWindow::pasteFromClipboard(
    function<void(string)> callback,
    int64_t timeoutMs
) {
    CEF_REQUIRE_UI_THREAD();
    impl_->pasteFromClipboard(PasteCallbackWrapper(callback, timeoutMs));
}

void XWindow::copyToClipboard(string text) {
    CEF_REQUIRE_UI_THREAD();
    impl_->copyToClipboard(text);
}
