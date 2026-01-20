#include "globals.hpp"
#include "server.hpp"
#include "scheme.hpp"
#include "vice.hpp"
#include "xvfb.hpp"

#include <csignal>
#include <cstdlib>

#include <iostream>

#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include "include/cef_sandbox_win.h"
#elif defined(__APPLE__)
// No X11 references for macOS
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

    void initialize(shared_ptr<ViceContext> viceCtx, CefRequestContextSettings requestContextSettings) {
        REQUIRE(!initialized_);

        initialized_ = true;
        requestContextSettings_ = requestContextSettings;
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

        // On Linux, use ANGLE/SwiftShader by maximize compatibility. On Windows, the Chromium
        // default should be reliable.
#if !defined(_WIN32) && !defined(__APPLE__)
        commandLine->AppendSwitchWithValue("use-gl", "angle");
        commandLine->AppendSwitchWithValue("use-angle", "swiftshader");
#endif

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

        CefRefPtr<CefRequestContext> requestContext = CefRequestContext::CreateContext(requestContextSettings_, nullptr);
        REQUIRE(requestContext);

        server_ = Server::create(serverEventHandler_, viceCtx_, requestContext);
        viceCtx_.reset();
        if(shutdown_) {
            server_->shutdown();
        }
    }
    virtual bool OnAlreadyRunningAppRelaunch(
        CefRefPtr<CefCommandLine> commandLine,
        const CefString& currentDirectory
    ) override {
        // Prevent default action.
        return true;
    }

private:
    bool initialized_;
    shared_ptr<Server> server_;
    CefRequestContextSettings requestContextSettings_;
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
            CefPostDelayedTask(TID_UI, base::BindOnce(pollTermSignal), 200);
        }
    }
}

}

}

#ifdef _WIN32
extern "C" CEF_BOOTSTRAP_EXPORT int RunConsoleMain(int, char*[], void* sandboxInfo, cef_version_info_t* versionInfo) {
    using namespace browservice;

    LPWSTR commandLineString = GetCommandLineW();
    if(commandLineString == nullptr) {
        cerr << "ERROR: Reading command line failed\n";
        return 1;
    }
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(commandLineString, &argc);
    if(argv == nullptr) {
        cerr << "ERROR: Reading command line failed\n";
        return 1;
    }

    CefMainArgs mainArgs(GetModuleHandle(nullptr));
#else
int main(int argc, char* argv[]) {
    using namespace browservice;

    void* sandboxInfo = nullptr;

    CefMainArgs mainArgs(argc, argv);
#endif

    std::cout << "Browservice starting..." << std::endl;
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

    initBrowserFontRenderMode(config->browserFontRenderMode);

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

#if !defined(_WIN32) && !defined(__APPLE__)
    shared_ptr<Xvfb> xvfb;
    if(config->useDedicatedXvfb) {
        xvfb = Xvfb::create();
        xvfb->setupEnv();
    }
#endif

    globals = Globals::create(config);

    if(!termSignalReceived.load()) {
#if !defined(_WIN32) && !defined(__APPLE__)
        // Ignore non-fatal X errors
        XSetErrorHandler([](Display*, XErrorEvent*) { return 0; });
        XSetIOErrorHandler([](Display*) { return 0; });
#endif

        CefSettings settings;
        settings.no_sandbox = true;
        settings.windowless_rendering_enabled = true;
        settings.command_line_args_disabled = true;

#ifdef __APPLE__
        // Helper app usage is handled by the bundle structure.
        // We do not need to explicitly set browser_subprocess_path if the helper is correctly placed.
#endif

        CefString(&settings.user_agent).FromString(globals->config->userAgent);

        CefRequestContextSettings requestContextSettings;
        requestContextSettings.persist_session_cookies = settings.persist_session_cookies;
        requestContextSettings.accept_language_list = settings.accept_language_list;
        requestContextSettings.cookieable_schemes_list = settings.cookieable_schemes_list;
        requestContextSettings.cookieable_schemes_exclude_defaults = settings.cookieable_schemes_exclude_defaults;

        if(globals->config->dataDir.empty()) {
            // Incognito mode.
            PathStr rootCachePath = globals->dotDirPath + PathSep + PATHSTR("cef");
#ifdef _WIN32
            CefString(&settings.root_cache_path).FromWString(rootCachePath);
            CefString(&settings.cache_path).FromWString(rootCachePath);
#else
            CefString(&settings.root_cache_path).FromString(rootCachePath);
            CefString(&settings.cache_path).FromString(rootCachePath);
#endif
            CefString(&requestContextSettings.cache_path).clear();
        } else {
            // Data dir specified by user.
            CefString(&settings.root_cache_path).FromString(globals->config->dataDir);
            CefString(&settings.cache_path).FromString(globals->config->dataDir);
            CefString(&requestContextSettings.cache_path).FromString(globals->config->dataDir);
        }

        app->initialize(viceCtx, requestContextSettings);
        viceCtx.reset();

        if(!CefInitialize(mainArgs, settings, app, sandboxInfo)) {
            if(CefGetExitCode() == CEF_RESULT_CODE_NORMAL_EXIT_PROCESS_NOTIFIED) {
                PANIC(
                    "Another Browservice instance is running. Close that instance or specify "
                    "different data directory using --data-dir for each instance to support "
                    "concurrent instances."
                );
            } else {
                PANIC("Initializing CEF failed");
            }
        }

        enablePanicUsingCEFFatalError();

        // Re-register termination handlers as CEF initialization may have
        // interfered with the previous registrations
        registerTermSignalHandler();

        CefPostTask(TID_UI, base::BindOnce(pollTermSignal));

        setRequireUIThreadEnabled(true);
        CefRunMessageLoop();
        setRequireUIThreadEnabled(false);

        REQUIRE(cefQuitMessageLoopCalled);

        CefShutdown();

        app = nullptr;
    }

    globals.reset();

#if !defined(_WIN32) && !defined(__APPLE__)
    xvfb.reset();
#endif

    return 0;
}
