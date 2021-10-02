#include "globals.hpp"
#include "server.hpp"
#include "scheme.hpp"
#include "vice.hpp"
#include "xvfb.hpp"

#include "text.hpp"

#include <csignal>
#include <cstdlib>

#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"

//#include <X11/Xlib.h>

namespace browservice {

namespace {

class AppServerEventHandler : public ServerEventHandler {
SHARED_ONLY_CLASS(AppServerEventHandler);
public:
    AppServerEventHandler(CKey) {}

    virtual void onServerShutdownComplete() override {
        INFO_LOG("Quitting CEF message loop");
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
        registrar->AddCustomScheme("browservice", CEF_SCHEME_OPTION_LOCAL | CEF_SCHEME_OPTION_DISPLAY_ISOLATED);
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
bool termSignalReceived = false;

void handleTermSignalSetFlag(int signalID) {
    INFO_LOG("Got signal ", signalID, ", initiating shutdown");
    termSignalReceived = true;
}

void handleTermSignalInApp(int signalID) {
    INFO_LOG("Got signal ", signalID, ", initiating shutdown");
    CefPostTask(TID_UI, base::Bind(&App::shutdown, app));
}

}

}

int APIENTRY wWinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine,
    int nCmdShow) {
    using namespace browservice;
    shared_ptr<TextRenderContext> textRenderContext = TextRenderContext::create();
/*
    CefMainArgs mainArgs(argc, argv);

    app = new App();

    int exitCode = CefExecuteProcess(mainArgs, app, nullptr);
    if(exitCode >= 0) {
        return exitCode;
    }

    signal(SIGINT, handleTermSignalSetFlag);
    signal(SIGTERM, handleTermSignalSetFlag);

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

    shared_ptr<Xvfb> xvfb;
    if(config->useDedicatedXvfb) {
        xvfb = Xvfb::create();
        xvfb->setupEnv();
    }

    globals = Globals::create(config);

    if(!termSignalReceived) {
        // Ignore non-fatal X errors
        XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
        XSetIOErrorHandler([](Display*) { return 0; });

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

        signal(SIGINT, handleTermSignalInApp);
        signal(SIGTERM, handleTermSignalInApp);

        if(termSignalReceived) {
            app->shutdown();
        }

        setRequireUIThreadEnabled(true);
        CefRunMessageLoop();
        setRequireUIThreadEnabled(false);

        signal(SIGINT, [](int) {});
        signal(SIGTERM, [](int) {});

        CefShutdown();

        app = nullptr;
    }

    globals.reset();
    xvfb.reset();

    return 0;
*/
}
