#include "globals.hpp"
#include "server.hpp"

#include <csignal>
#include <cstdlib>

#include "include/wrapper/cef_closure_task.h"
#include "include/cef_app.h"

namespace {

class AppServerEventHandler : public ServerEventHandler {
SHARED_ONLY_CLASS(AppServerEventHandler);
public:
    AppServerEventHandler(CKey) {}

    virtual void onServerShutdownComplete() override {
        LOG(INFO) << "Server shutdown complete, quitting CEF message loop";
        CefQuitMessageLoop();
    }
};

class App :
    public CefApp,
    public CefBrowserProcessHandler
{
public:
    App() {
        serverEventHandler_ = AppServerEventHandler::create();
        shutdown_ = false;
    }

    void shutdown() {
        CEF_REQUIRE_UI_THREAD();

        if(server_) {
            server_->shutdown();
        } else {
            shutdown_ = true;
        }
    }

    // CefApp:
    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }
    virtual void OnBeforeCommandLineProcessing(
        const CefString& processType,
        CefRefPtr<CefCommandLine> commandLine
    ) override {
        commandLine->AppendSwitch("disable-smooth-scrolling");
        commandLine->AppendSwitchWithValue("use-gl", "desktop");
    }

    // CefBrowserProcessHandler:
    virtual void OnContextInitialized() override {
        CEF_REQUIRE_UI_THREAD();
        CHECK(!server_);

        server_ = Server::create(serverEventHandler_);
        if(shutdown_) {
            server_->shutdown();
        }
    }

private:
    shared_ptr<Server> server_;
    shared_ptr<AppServerEventHandler> serverEventHandler_;
    bool shutdown_;

    IMPLEMENT_REFCOUNTING(App);
};

CefRefPtr<App> app;

void handleTermSignal(int signalID) {
    LOG(INFO) << "Got signal " << signalID << ", initiating shutdown";
    CefPostTask(TID_UI, base::Bind(&App::shutdown, app));
}

}

int main(int argc, char* argv[]) {
    CefMainArgs mainArgs(argc, argv);

    int exitCode = CefExecuteProcess(mainArgs, nullptr, nullptr);
    if(exitCode >= 0) {
        return exitCode;
    }

    shared_ptr<Config> config = Config::read(argc, argv);
    if(!config) {
        return 1;
    }
    globals = Globals::create(config);

    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.command_line_args_disabled = true;
    if(!globals->config->userAgent.empty()) {
        CefString(&settings.user_agent).FromString(globals->config->userAgent);
    }

    app = new App;

    CefInitialize(mainArgs, settings, app, nullptr);

    signal(SIGINT, handleTermSignal);
    signal(SIGTERM, handleTermSignal);

    CefRunMessageLoop();

    signal(SIGINT, [](int) {});
    signal(SIGTERM, [](int) {});

    CefShutdown();

    app = nullptr;
    globals.reset();

    return 0;
}
