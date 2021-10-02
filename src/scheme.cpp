#include "scheme.hpp"

#include "bookmarks.hpp"

namespace browservice {

namespace {

class StaticResponseResourceHandler : public CefResourceHandler {
public:
    StaticResponseResourceHandler(int status, string statusText, string response) {
        status_ = status;
        statusText_ = move(statusText);
        response_ = move(response);
        pos_ = 0;
    }

    virtual bool Open(
        CefRefPtr<CefRequest> request,
        bool& handleRequest,
        CefRefPtr<CefCallback> callback
    ) override{
        handleRequest = true;
        return true;
    }

    virtual void GetResponseHeaders(
        CefRefPtr<CefResponse> response,
        int64_t& responseLength,
        CefString& redirectUrl
    ) override{
        responseLength = (int64_t)response_.size();
        response->SetStatus(status_);
        response->SetStatusText(statusText_);
        response->SetMimeType("text/html");
        response->SetCharset("UTF-8");
    }

    virtual bool Skip(
        int64_t bytesToSkip,
        int64_t& bytesSkipped,
        CefRefPtr<CefResourceSkipCallback> callback
    ) override {
        int64_t maxSkip = (int64_t)(response_.size() - pos_);
        int64_t skipCount = min(bytesToSkip, maxSkip);
        REQUIRE(skipCount >= (int64_t)0);

        if(skipCount > (int64_t)0) {
            bytesSkipped = skipCount;
            pos_ += (size_t)skipCount;
            return true;
        } else {
            bytesSkipped = -2;
            return false;
        }
    }

    virtual bool Read(
        void* dataOut,
        int bytesToRead,
        int& bytesRead,
        CefRefPtr<CefResourceReadCallback> callback
    ) override {
        int64_t maxRead = (int64_t)(response_.size() - pos_);
        int readCount = (int)min((int64_t)bytesToRead, maxRead);
        REQUIRE(readCount >= 0);

        if(readCount > 0) {
            bytesRead = readCount;
            memcpy(dataOut, response_.data() + pos_, (size_t)readCount);
            pos_ += (size_t)readCount;
            return true;
        } else {
            bytesRead = 0;
            return false;
        }
    }

    virtual void Cancel() override {
        response_.clear();
        pos_ = 0;
    }

private:
    int status_;
    string statusText_;
    string response_;
    size_t pos_;

    IMPLEMENT_REFCOUNTING(StaticResponseResourceHandler);
};

}

CefRefPtr<CefResourceHandler> BrowserviceSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request
) {
    CEF_REQUIRE_IO_THREAD();
    REQUIRE(request);

    int status = 404;
    string statusText = "Not Found";
    string response =
        "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"UTF-8\">"
        "<title>404 Not Found</title></head><body><h1>404 Not Found</h1></body></html>\n";
/*
    if(request->GetURL() == "browservice:bookmarks") {
        status = 200;
        statusText = "OK";
        response = handleBookmarksRequest(request);
    }
*/
    return new StaticResponseResourceHandler(status, move(statusText), move(response));
}

}
