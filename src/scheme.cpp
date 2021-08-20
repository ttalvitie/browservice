#include "scheme.hpp"

namespace browservice {

namespace {

class StaticResponseResourceHandler : public CefResourceHandler {
public:
    StaticResponseResourceHandler(vector<char> response) {
        response_ = std::move(response);
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
        response->SetStatus(200);
        response->SetStatusText("OK");
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
    vector<char> response_;
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
    string responseStr = "TEST RESPONSE";
    return new StaticResponseResourceHandler(vector<char>(responseStr.begin(), responseStr.end()));
}

}
