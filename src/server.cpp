#include "server.hpp"

#include "globals.hpp"
#include "html.hpp"
#include "quality.hpp"
#include "xwindow.hpp"

namespace {

regex sessionPathRegex("/([0-9]+)/.*");

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

}

Server::Server(CKey, weak_ptr<ServerEventHandler> eventHandler) {
    requireUIThread();
    eventHandler_ = eventHandler;
    state_ = Running;
    // Setup is finished in afterConstruct_
}

void Server::shutdown() {
    requireUIThread();

    if(state_ == Running) {
        state_ = ShutdownPending;
        LOG(INFO) << "Shutting down server";

        httpServer_->shutdown();

        map<uint64_t, shared_ptr<Session>> sessions = sessions_;
        for(pair<uint64_t, shared_ptr<Session>> p : sessions) {
            p.second->close();
        }
    }
}

void Server::onHTTPServerRequest(shared_ptr<HTTPRequest> request) {
    requireUIThread();

    string method = request->method();
    string path = request->path();

    if(method == "GET" && path == "/") {
        shared_ptr<Session> session = Session::create(
            shared_from_this(), hasPNGSupport(request->userAgent())
        );
        sessions_[session->id()] = session;

        request->sendHTMLResponse(200, writeNewSessionHTML, {session->id()});
        return;
    }

    if(path == "/clipboard/") {
        handleClipboardRequest_(request);
        return;
    }

    smatch match;
    if(regex_match(path, match, sessionPathRegex)) {
        CHECK(match.size() == 2);
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
    requireUIThread();
    checkShutdownStatus_();
}

void Server::onSessionClosed(uint64_t id) {
    requireUIThread();

    auto it = sessions_.find(id);
    CHECK(it != sessions_.end());
    sessions_.erase(it);

    checkShutdownStatus_();
}

void Server::onPopupSessionOpen(shared_ptr<Session> session) {
    requireUIThread();

    CHECK(sessions_.emplace(session->id(), session).second);

    if(state_ == ShutdownPending) {
        session->close();
    }
}

void Server::afterConstruct_(shared_ptr<Server> self) {
    httpServer_ = HTTPServer::create(self, globals->config->httpListenAddr);
}

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

void Server::checkShutdownStatus_() {
    if(
        state_ == ShutdownPending &&
        httpServer_->isShutdownComplete() &&
        sessions_.empty()
    ) {
        state_ = ShutdownComplete;
        LOG(INFO) << "Server shutdown complete";
        postTask(eventHandler_, &ServerEventHandler::onServerShutdownComplete);
    }
}
