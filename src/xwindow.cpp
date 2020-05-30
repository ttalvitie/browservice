#include "xwindow.hpp"

#include "timeout.hpp"

#include <xcb/xcb.h>

class XWindow::Impl : public enable_shared_from_this<XWindow::Impl> {
SHARED_ONLY_CLASS(Impl);
public:
    Impl(CKey) {
        mode_ = Idle;
        pasteTimeout_ = Timeout::create(300);
        pasteCallback_ = [](string) {};

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

        clipboardAtom_ = getAtom_("CLIPBOARD");
        utf8StringAtom_ = getAtom_("UTF8_STRING");
        incrAtom_ = getAtom_("INCR");
    }

    ~Impl() {
        CHECK(mode_ == Closed);
    }

    void close() {
        CHECK(mode_ != Closed);

        mode_ = Closed;
        pasteTimeout_->clear(false);
        pasteCallback_ = [](string) {};
        copyText_.clear();

        CHECK(xcb_request_check(connection_,
            xcb_destroy_window_checked(connection_, window_)
        ) == nullptr);

        eventHandlerThread_.join();

        xcb_disconnect(connection_);
    }

    void pasteFromClipboard(function<void(string)> callback) {
        requireUIThread();
        CHECK(mode_ != Closed);

        if(mode_ == Pasting) {
            pasteCallback_ = callback;
        } else if(mode_ == Copying) {
            string text = copyText_;
            postTask([callback, text{move(text)}]() { callback(text); });
        } else {
            CHECK(mode_ == Idle);

            xcb_get_selection_owner_reply_t* reply = xcb_get_selection_owner_reply(
                connection_,
                xcb_get_selection_owner(connection_, clipboardAtom_),
                nullptr
            );
            xcb_window_t owner = 0;
            if(reply != nullptr) {
                owner = reply->owner;
                free(reply);
            }

            if(owner != 0) {
                mode_ = Pasting;
                pasteCallback_ = callback;

                shared_ptr<Impl> self = shared_from_this();
                pasteTimeout_->set([self]() {
                    self->pasteTimedOut_();
                });

                CHECK(xcb_request_check(connection_,
                    xcb_convert_selection_checked(
                        connection_,
                        window_,
                        clipboardAtom_,
                        utf8StringAtom_,
                        clipboardAtom_,
                        XCB_CURRENT_TIME
                    )
                ) == nullptr);
            }
        }
    }

    void copyToClipboard(string text) {
        requireUIThread();
        CHECK(mode_ != Closed);

        copyText_ = move(text);
        if(mode_ != Copying) {
            CHECK(mode_ == Pasting || mode_ == Idle);

            if(mode_ == Pasting) {
                pasteTimeout_->clear(false);
                pasteCallback_ = [](string) {};
            }

            mode_ = Copying;
            // TODO: start copying
        }
    }

private:
    void afterConstruct_(shared_ptr<Impl> self) {
        eventHandlerThread_ = thread([self]() {
            self->runEventHandlerThread_();
        });
    }

    xcb_atom_t getAtom_(string name) {
        xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(
            connection_,
            xcb_intern_atom(connection_, 0, name.size(), name.data()),
            nullptr
        );
        CHECK(reply != nullptr);
        xcb_atom_t result = reply->atom;
        free(reply);
        return result;
    }

    void pasteTimedOut_() {
        requireUIThread();
        CHECK(mode_ == Pasting);

        mode_ = Idle;
        pasteCallback_ = [](string) {};
    }

    void pasteResponseReceived_(string text) {
        requireUIThread();

        if(mode_ == Pasting) {
            mode_ = Idle;
            pasteTimeout_->clear(false);

            function<void(string)> callback = pasteCallback_;
            pasteCallback_ = [](string) {};
            postTask([callback, text{move(text)}]() { callback(text); });
        }
    }

    void handleSelectionNotifyEvent_(xcb_selection_notify_event_t* event) {
        if(event->property == 0 || event->target == 0) {
            return;
        }

        xcb_get_property_reply_t* reply = xcb_get_property_reply(
            connection_,
            xcb_get_property(
                connection_,
                1,
                window_,
                event->property,
                event->target,
                0,
                INT_MAX / 32
            ),
            nullptr
        );
        if(reply != nullptr) {
            if(reply->type != incrAtom_) {
                const char* data = (const char*)xcb_get_property_value(reply);
                int length = xcb_get_property_value_length(reply);
                string text(data, length);
                postTask(
                    shared_from_this(),
                    &Impl::pasteResponseReceived_,
                    move(text)
                );
            }
            free(reply);
        }
    }

    void runEventHandlerThread_() {
        while(true) {
            xcb_generic_event_t* event = xcb_wait_for_event(connection_);
            CHECK(event != nullptr);

            int type = event->response_type & ~0x80;

            if(type == XCB_DESTROY_NOTIFY) {
                break;
            }

            if(type == XCB_SELECTION_NOTIFY) {
                handleSelectionNotifyEvent_(
                    (xcb_selection_notify_event_t*)event
                );
            }
        }
    }

    bool closed_;
    thread eventHandlerThread_;

    xcb_connection_t* connection_;
    xcb_screen_t* screen_;
    xcb_window_t window_;

    xcb_atom_t clipboardAtom_;
    xcb_atom_t utf8StringAtom_;
    xcb_atom_t incrAtom_;

    enum {Pasting, Copying, Idle, Closed} mode_;
    shared_ptr<Timeout> pasteTimeout_;
    function<void(string)> pasteCallback_;
    string copyText_;
};

XWindow::XWindow(CKey) {
    impl_ = Impl::create();
}

XWindow::~XWindow() {
    impl_->close();
}

void XWindow::pasteFromClipboard(function<void(string)> callback) {
    requireUIThread();
    impl_->pasteFromClipboard(callback);
}

void XWindow::copyToClipboard(string text) {
    requireUIThread();
    impl_->copyToClipboard(text);
}
