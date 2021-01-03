#include "server.hpp"

#include "globals.hpp"
#include "quality.hpp"
#include "xwindow.hpp"

namespace {

regex sessionPathRegex("/([0-9]+)/.*");
/*
string htmlEscapeString(string src) {
    string ret;
    for(char c : src) {
        if(c == '&') {
            ret += "&amp;";
        } else if(c == '<') {
            ret += "&lt;";
        } else if(c == '>') {
            ret += "&gt;";
        } else if(c == '"') {
            ret += "&quot;";
        } else if(c == '\'') {
            ret += "&apos;";
        } else {
            ret.push_back(c);
        }
    }
    return ret;
}
*/
}

Server::Server(CKey,
    weak_ptr<ServerEventHandler> eventHandler,
    shared_ptr<ViceContext> viceCtx
) {
    REQUIRE_UI_THREAD();
    eventHandler_ = eventHandler;
    state_ = Running;
    nextSessionID_ = 1;
    viceCtx_ = viceCtx;
    // Setup is finished in afterConstruct_
}

void Server::shutdown() {
    REQUIRE_UI_THREAD();

    if(state_ == Running) {
        state_ = WaitSessions;
        INFO_LOG("Shutting down server");

        map<uint64_t, shared_ptr<Session>> sessions = sessions_;
        for(pair<uint64_t, shared_ptr<Session>> p : sessions) {
            p.second->close();
            viceCtx_->closeWindow(p.first);
        }

        checkSessionsEmpty_();
    }
}

uint64_t Server::onViceContextCreateWindowRequest(string& reason) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    if(state_ != Running) {
        reason = "Server is shutting down";
        return 0;
    }

    INFO_LOG("Got request for new session from vice plugin");

    if((int)sessions_.size() >= globals->config->sessionLimit) {
        INFO_LOG("Denying session creation due to session limit");
        reason = "Maximum number of concurrent sessions exceeded";
        return 0;
    }

    uint64_t id = nextSessionID_++;
    REQUIRE(id);

    shared_ptr<Session> session = Session::tryCreate(shared_from_this(), id);
    if(session) {
        sessions_[id] = session;
        return id;
    } else {
        reason = "Creating browser for session failed";
        return 0;
    }
}

void Server::onViceContextCloseWindow(uint64_t window) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    auto it = sessions_.find(window);
    REQUIRE(it != sessions_.end());

    it->second->close();
}

void Server::onViceContextFetchWindowImage(
    uint64_t window,
    function<void(const uint8_t*, size_t, size_t, size_t)> putImage
) {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ != ShutdownComplete);

    auto it = sessions_.find(window);
    REQUIRE(it != sessions_.end());

    ImageSlice image = it->second->getViewImage();
    if(image.width() < 1 || image.height() < 1) {
        image = ImageSlice::createImage(1, 1);
    }
    putImage(image.buf(), image.width(), image.height(), image.pitch());
}

void Server::onViceContextShutdownComplete() {
    REQUIRE_UI_THREAD();
    REQUIRE(state_ == WaitViceContext);

    state_ = ShutdownComplete;
    INFO_LOG("Server shutdown complete");
    postTask(eventHandler_, &ServerEventHandler::onServerShutdownComplete);
}
/*
void Server::onHTTPServerRequest(shared_ptr<HTTPRequest> request) {
    REQUIRE_UI_THREAD();

    if(!globals->config->httpAuth.empty()) {
        optional<string> credentials = request->getBasicAuthCredentials();
        if(!credentials || *credentials != globals->config->httpAuth) {
            request->sendTextResponse(
                401,
                "Unauthorized",
                true,
                {{
                    "WWW-Authenticate",
                    "Basic realm=\"Browservice\", charset=\"UTF-8\""
                }}
            );
            return;
        }
    }

    string method = request->method();
    string path = request->path();

    if(method == "GET" && path == "/") {
        if((int)sessions_.size() >= globals->config->sessionLimit) {
            request->sendTextResponse(
                503, "ERROR: Maximum number of concurrent sessions exceeded"
            );
        } else {
            shared_ptr<Session> session = Session::tryCreate(
                shared_from_this(), hasPNGSupport(request->userAgent())
            );
            if(session) {
                sessions_[session->id()] = session;
                request->sendHTMLResponse(200, writeNewSessionHTML, {session->id()});
            } else {
                request->sendTextResponse(
                    500, "ERROR: Creating session failed"
                );
            }
        }
        return;
    }

    if(path == "/clipboard/") {
        handleClipboardRequest_(request);
        return;
    }

    smatch match;
    if(regex_match(path, match, sessionPathRegex)) {
        REQUIRE(match.size() == 2);
        optional<uint64_t> sessionID = parseString<uint64_t>(match[1]);
        if(sessionID) {
            auto it = sessions_.find(*sessionID);
            if(it != sessions_.end()) {
                shared_ptr<Session> session = it->second;
                session->handleHTTPRequest(request);
            } else {
                request->sendTextResponse(400, "ERROR: Invalid session ID");
            }
            return;
        }
    }

    request->sendTextResponse(400, "ERROR: Invalid request URI or method");
}

void Server::onHTTPServerShutdownComplete() {
    REQUIRE_UI_THREAD();
    checkShutdownStatus_();
}
*/
void Server::onSessionClosing(uint64_t id) {
    REQUIRE_UI_THREAD();
    PANIC("Session closing on its own; not implemented");
}

void Server::onSessionClosed(uint64_t id) {
    REQUIRE_UI_THREAD();

    auto it = sessions_.find(id);
    REQUIRE(it != sessions_.end());
    sessions_.erase(it);

    checkSessionsEmpty_();
}
/*
bool Server::onIsServerFullQuery() {
    REQUIRE_UI_THREAD();
    return (int)sessions_.size() >= globals->config->sessionLimit;
}

void Server::onPopupSessionOpen(shared_ptr<Session> session) {
    REQUIRE_UI_THREAD();

    REQUIRE(sessions_.emplace(session->id(), session).second);

    REQUIRE(state_ == Running || state_ == WaitSessions);
    if(state_ == WaitSessions) {
        session->close();
    }
}
*/

void Server::onSessionViewImageChanged(uint64_t id) {
    REQUIRE_UI_THREAD();
    REQUIRE(sessions_.count(id));

    viceCtx_->notifyWindowViewChanged(id);
}

void Server::afterConstruct_(shared_ptr<Server> self) {
    viceCtx_->start(self);
}
/*
void Server::handleClipboardRequest_(shared_ptr<HTTPRequest> request) {
    string method = request->method();
    if(method == "GET") {
        request->sendHTMLResponse(200, writeClipboardHTML, {""});
    } else if(method == "POST") {
        string mode = request->getFormParam("mode");
        if(mode == "get") {
            // Make sure that response is written even if paste callback is
            // dropped using custom destructor
            struct Responder {
                shared_ptr<HTTPRequest> request;

                void respond(string text) {
                    if(request) {
                        request->sendHTMLResponse(
                            200,
                            writeClipboardHTML,
                            {htmlEscapeString(text)}
                        );
                        request.reset();
                    }
                }

                ~Responder() {
                    if(request) {
                        request->sendHTMLResponse(200, writeClipboardHTML, {""});
                    }
                }
            };

            shared_ptr<Responder> responder = make_shared<Responder>();
            responder->request = request;

            globals->xWindow->pasteFromClipboard([responder](string text) {
                responder->respond(text);
            });
        } else if(mode == "set") {
            string text = request->getFormParam("text");
            globals->xWindow->copyToClipboard(text);
            request->sendHTMLResponse(
                200,
                writeClipboardHTML,
                {htmlEscapeString(text)}
            );
        } else {
            request->sendTextResponse(400, "ERROR: Invalid request parameters");
        }
    } else {
        request->sendTextResponse(400, "ERROR: Invalid request method");
    }
}
*/

void Server::checkSessionsEmpty_() {
    if(state_ == WaitSessions && sessions_.empty()) {
        state_ = WaitViceContext;
        viceCtx_->shutdown();
    }
}
