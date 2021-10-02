/*
#include "xwindow.hpp"

#include "timeout.hpp"

#include <xcb/xcb.h>

namespace browservice {

class XWindow::Impl : public enable_shared_from_this<XWindow::Impl> {
SHARED_ONLY_CLASS(Impl);
public:
    Impl(CKey) {
        mode_ = Idle;
        pasteTimeout_ = Timeout::create(300);
        pasteCallback_ = [](string) {};

        connection_ = xcb_connect(nullptr, nullptr);
        if(connection_ == nullptr || xcb_connection_has_error(connection_)) {
            PANIC("Opening X display failed");
        }

        const xcb_setup_t* setup = xcb_get_setup(connection_);
        REQUIRE(setup != nullptr);
        screen_ = xcb_setup_roots_iterator(setup).data;
        REQUIRE(screen_ != nullptr);

        uint32_t eventMask =
            XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;

        window_ = xcb_generate_id(connection_);
        REQUIRE(xcb_request_check(connection_,
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
        targetsAtom_ = getAtom_("TARGETS");
        atomAtom_ = getAtom_("ATOM");
    }

    ~Impl() {
        REQUIRE(mode_ == Closed);
    }

    void close() {
        REQUIRE(mode_ != Closed);

        mode_ = Closed;
        pasteTimeout_->clear(false);
        pasteCallback_ = [](string) {};
        {
            lock_guard<mutex> lock(copyTextMutex_);
            copyText_.clear();
        }

        REQUIRE(xcb_request_check(connection_,
            xcb_destroy_window_checked(connection_, window_)
        ) == nullptr);

        eventHandlerThread_.join();

        xcb_disconnect(connection_);
    }

    void pasteFromClipboard(function<void(string)> callback) {
        REQUIRE_UI_THREAD();
        REQUIRE(mode_ != Closed);

        if(mode_ == Pasting) {
            pasteCallback_ = callback;
        } else if(mode_ == Copying) {
            lock_guard<mutex> lock(copyTextMutex_);
            string text = copyText_;
            postTask([callback, text{move(text)}]() { callback(text); });
        } else {
            REQUIRE(mode_ == Idle);

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

                REQUIRE(xcb_request_check(connection_,
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
        REQUIRE_UI_THREAD();
        REQUIRE(mode_ != Closed);

        if(mode_ == Copying) {
            lock_guard<mutex> lock(copyTextMutex_);
            copyText_ = move(text);
        } else {
            if(mode_ == Pasting) {
                pasteTimeout_->clear(false);
                pasteCallback_ = [](string) {};
                mode_ = Idle;
            }
            REQUIRE(mode_ == Idle);

            xcb_generic_error_t* err = xcb_request_check(connection_,
                xcb_set_selection_owner_checked(
                    connection_,
                    window_,
                    clipboardAtom_,
                    XCB_CURRENT_TIME
                )
            );
            if(err == nullptr) {
                mode_ = Copying;
                lock_guard<mutex> lock(copyTextMutex_);
                copyText_ = move(text);
            } else {
                free(err);
            }
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
        REQUIRE(reply != nullptr);
        xcb_atom_t result = reply->atom;
        free(reply);
        return result;
    }

    void pasteTimedOut_() {
        REQUIRE_UI_THREAD();
        REQUIRE(mode_ == Pasting);

        mode_ = Idle;
        pasteCallback_ = [](string) {};
    }

    void pasteResponseReceived_(string text) {
        REQUIRE_UI_THREAD();

        if(mode_ == Pasting) {
            mode_ = Idle;
            pasteTimeout_->clear(false);

            function<void(string)> callback = pasteCallback_;
            pasteCallback_ = [](string) {};
            postTask([callback, text{move(text)}]() { callback(text); });
        }
    }

    void copyCleared_() {
        REQUIRE_UI_THREAD();

        if(mode_ == Copying) {
            mode_ = Idle;
            lock_guard<mutex> lock(copyTextMutex_);
            copyText_.clear();
        }
    }

    void handleSelectionNotifyEvent_(xcb_selection_notify_event_t* event) {
        if(event->property == 0 || event->target == 0) {
            return;
        }

        // Our request for clipboard content has been responded by setting a
        // property in our window; fetch and delete it
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

    void handleSelectionRequestEvent_(xcb_selection_request_event_t* event) {
        bool needsNotify = false;
        if(event->selection == clipboardAtom_) {
            if(event->target == targetsAtom_) {
                // We are asked to serve the list of formats we can serve to a
                // property of the requesting window; we respond that we only serve
                // UTF8 strings
                xcb_atom_t targets[2] = {targetsAtom_, utf8StringAtom_};
                xcb_change_property(
                    connection_,
                    XCB_PROP_MODE_REPLACE,
                    event->requestor,
                    event->property,
                    atomAtom_,
                    8 * sizeof(xcb_atom_t),
                    2,
                    targets
                );
                needsNotify = true;
            } else if(event->target == utf8StringAtom_) {
                // We are asked to serve the clipboard content to a property of the
                // requesting window
                lock_guard<mutex> lock(copyTextMutex_);
                xcb_change_property(
                    connection_,
                    XCB_PROP_MODE_REPLACE,
                    event->requestor,
                    event->property,
                    event->target,
                    8,
                    copyText_.size(),
                    copyText_.data()
                );
                needsNotify = true;
            }
        }

        if(needsNotify) {
            // Notify the requesting window of the property changes
            xcb_selection_notify_event_t notify;
            notify.response_type = XCB_SELECTION_NOTIFY;
            notify.pad0 = 0;
            notify.sequence = 0;
            notify.time = event->time;
            notify.requestor = event->requestor;
            notify.selection = clipboardAtom_;
            notify.target = event->target;
            notify.property = event->property;

            xcb_send_event(
                connection_,
                false,
                event->requestor,
                XCB_EVENT_MASK_NO_EVENT,
                (const char*)&notify
            );
            xcb_flush(connection_);
        }
    }

    void handleSelectionClearEvent_(xcb_selection_clear_event_t* event) {
        // We do not own the clipboard anymore; change mode accordingly
        if(event->selection == clipboardAtom_) {
            postTask(shared_from_this(), &Impl::copyCleared_);
        }
    }

    void runEventHandlerThread_() {
        while(true) {
            xcb_generic_event_t* event = xcb_wait_for_event(connection_);
            REQUIRE(event != nullptr);

            int type = event->response_type & ~0x80;

            if(type == XCB_DESTROY_NOTIFY) {
                break;
            }

            if(type == XCB_SELECTION_NOTIFY) {
                handleSelectionNotifyEvent_(
                    (xcb_selection_notify_event_t*)event
                );
            }

            if(type == XCB_SELECTION_REQUEST) {
                handleSelectionRequestEvent_(
                    (xcb_selection_request_event_t*)event
                );
            }

            if(type == XCB_SELECTION_CLEAR) {
                handleSelectionClearEvent_(
                    (xcb_selection_clear_event_t*)event
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
    xcb_atom_t targetsAtom_;
    xcb_atom_t atomAtom_;

    enum {Pasting, Copying, Idle, Closed} mode_;
    shared_ptr<Timeout> pasteTimeout_;
    function<void(string)> pasteCallback_;
    string copyText_;
    mutex copyTextMutex_;
};

XWindow::XWindow(CKey) {
    REQUIRE_UI_THREAD();
    impl_ = Impl::create();
}

XWindow::~XWindow() {
    impl_->close();
}

void XWindow::pasteFromClipboard(function<void(string)> callback) {
    REQUIRE_UI_THREAD();
    impl_->pasteFromClipboard(callback);
}

void XWindow::copyToClipboard(string text) {
    REQUIRE_UI_THREAD();
    impl_->copyToClipboard(text);
}

}
*/
