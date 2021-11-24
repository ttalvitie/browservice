#include "globals.hpp"
#include "server.hpp"
#include "scheme.hpp"
#include "vice.hpp"
#include "xvfb.hpp"

#include <csignal>
#include <cstdlib>

#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"

#ifdef _WIN32
#include <windows.h>
#pragma comment(lib, "cef_sandbox.lib")
#include "include/cef_sandbox_win.h"
#else
#include <X11/Xlib.h>
#endif

namespace browservice {

namespace {

bool cefQuitMessageLoopCalled = false;

class AppServerEventHandler : public ServerEventHandler {
SHARED_ONLY_CLASS(AppServerEventHandler);
public:
    AppServerEventHandler(CKey) {}

    virtual void onServerShutdownComplete() override {
        INFO_LOG("Quitting CEF message loop");
        cefQuitMessageLoopCalled = true;
        CefQuitMessageLoop();
    }
};

class App :
    public CefApp,
    public CefBrowserProcessHandler
{
public:
    App() {
        initialized_ = false;
    }

    void initialize(shared_ptr<ViceContext> viceCtx) {
        REQUIRE(!initialized_);

        initialized_ = true;
        serverEventHandler_ = AppServerEventHandler::create();
        shutdown_ = false;
        viceCtx_ = viceCtx;
    }

    void shutdown() {
        REQUIRE_UI_THREAD();
        REQUIRE(initialized_);

        if(server_) {
            server_->shutdown();
        } else {
            shutdown_ = true;
        }
    }

    // CefApp (may be used with initialized_ = false in other processes):
    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }
    virtual void OnBeforeCommandLineProcessing(
        const CefString& processType,
        CefRefPtr<CefCommandLine> commandLine
    ) override {
        if(!initialized_) {
            return;
        }

        commandLine->AppendSwitch("disable-smooth-scrolling");
        commandLine->AppendSwitchWithValue("use-gl", "desktop");

        for(const pair<string, optional<string>>& arg : globals->config->chromiumArgs) {
            if(arg.second) {
                commandLine->AppendSwitchWithValue(arg.first, *arg.second);
            } else {
                commandLine->AppendSwitch(arg.first);
            }
        }
    }
    virtual void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
        registrar->AddCustomScheme(
            "browservice",
            CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_LOCAL | CEF_SCHEME_OPTION_DISPLAY_ISOLATED
        );
    }

    // CefBrowserProcessHandler (may be used with initialized_ = false in other processes):
    virtual void OnContextInitialized() override {
        if(!initialized_) {
            return;
        }

        REQUIRE_UI_THREAD();
        REQUIRE(!server_);

        CefRefPtr<BrowserviceSchemeHandlerFactory> schemeHandlerFactory = new BrowserviceSchemeHandlerFactory();
        CefRegisterSchemeHandlerFactory("browservice", "", schemeHandlerFactory);

        server_ = Server::create(serverEventHandler_, viceCtx_);
        viceCtx_.reset();
        if(shutdown_) {
            server_->shutdown();
        }
    }

private:
    bool initialized_;
    shared_ptr<Server> server_;
    shared_ptr<AppServerEventHandler> serverEventHandler_;
    bool shutdown_;
    shared_ptr<ViceContext> viceCtx_;

    IMPLEMENT_REFCOUNTING(App);
};

CefRefPtr<App> app;
atomic<bool> termSignalReceived = false;

#ifdef _WIN32
BOOL WINAPI handleTermSignal(DWORD dwCtrlType) {
    INFO_LOG("Got control signal ", dwCtrlType, ", initiating shutdown");
    termSignalReceived.store(true);
    return TRUE;
}
#else
void handleTermSignal(int signalID) {
    INFO_LOG("Got signal ", signalID, ", initiating shutdown");
    termSignalReceived.store(true);
}
#endif

void registerTermSignalHandler() {
#ifdef _WIN32
    REQUIRE(SetConsoleCtrlHandler(handleTermSignal, TRUE));
#else
    signal(SIGINT, handleTermSignal);
    signal(SIGTERM, handleTermSignal);
#endif
}

void pollTermSignal() {
    REQUIRE_UI_THREAD();

    if(app) {
        if(termSignalReceived.load()) {
            app->shutdown();
        } else {
            CefPostDelayedTask(TID_UI, base::Bind(pollTermSignal), 200);
        }
    }
}

}

}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
    using namespace browservice;

    CefScopedSandboxInfo scoped_sandbox;
    void* sandboxInfo = scoped_sandbox.sandbox_info();

    CefMainArgs mainArgs(GetModuleHandle(nullptr));
#else
int main(int argc, char* argv[]) {
    using namespace browservice;

    void* sandboxInfo = nullptr;

    CefMainArgs mainArgs(argc, argv);
#endif

    app = new App();

    int exitCode = CefExecuteProcess(mainArgs, app, sandboxInfo);
    if(exitCode >= 0) {
        return exitCode;
    }

    registerTermSignalHandler();

    shared_ptr<Config> config = Config::read(argc, argv);
    if(!config) {
        return 1;
    }

    INFO_LOG("Loading vice plugin ", config->vicePlugin);
    shared_ptr<VicePlugin> vicePlugin = VicePlugin::load(config->vicePlugin);
    if(!vicePlugin) {
        cerr << "ERROR: Loading vice plugin " << config->vicePlugin << " failed\n";
        return 1;
    }

    INFO_LOG("Initializing vice plugin ", config->vicePlugin);
    shared_ptr<ViceContext> viceCtx =
        ViceContext::init(vicePlugin, config->viceOpts);
    if(!viceCtx) {
        return 1;
    }

    vicePlugin.reset();

#ifndef _WIN32
    shared_ptr<Xvfb> xvfb;
    if(config->useDedicatedXvfb) {
        xvfb = Xvfb::create();
        xvfb->setupEnv();
    }
#endif

    globals = Globals::create(config);

    if(!termSignalReceived.load()) {
#ifndef _WIN32
        // Ignore non-fatal X errors
        XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
        XSetIOErrorHandler([](Display*) { return 0; });
#endif

        app->initialize(viceCtx);
        viceCtx.reset();

        CefSettings settings;
        settings.windowless_rendering_enabled = true;
        settings.command_line_args_disabled = true;
        CefString(&settings.cache_path).FromString(globals->config->dataDir);
        CefString(&settings.user_agent).FromString(globals->config->userAgent);

        if(!CefInitialize(mainArgs, settings, app, nullptr)) {
            PANIC("Initializing CEF failed");
        }

        enablePanicUsingCEFFatalError();

        // Re-register termination handlers as CEF initialization may have
        // interfered with the previous registrations
        registerTermSignalHandler();

        CefPostTask(TID_UI, base::Bind(pollTermSignal));

        setRequireUIThreadEnabled(true);
        CefRunMessageLoop();
        setRequireUIThreadEnabled(false);

        REQUIRE(cefQuitMessageLoopCalled);

        CefShutdown();

        app = nullptr;
    }

    globals.reset();

#ifndef _WIN32
    xvfb.reset();
#endif

    return 0;
}
