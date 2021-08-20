#pragma once

#include "common.hpp"

#include "include/cef_app.h"

namespace browservice {

class BrowserviceSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
    virtual CefRefPtr<CefResourceHandler> Create(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& scheme_name,
        CefRefPtr<CefRequest> request
    ) override;

private:
    IMPLEMENT_REFCOUNTING(BrowserviceSchemeHandlerFactory);
};

}
