#pragma once

#include "common.hpp"

namespace retrowebvice {

struct NewSessionHTMLData {
    uint64_t sessionID;
};
void writeNewSessionHTML(ostream& out, const NewSessionHTMLData& data);

struct MainHTMLData {
    uint64_t sessionID;
    uint64_t mainIdx;
    const string& nonCharKeyList;
};
void writeMainHTML(ostream& out, const MainHTMLData& data);

struct PreMainHTMLData {
    uint64_t sessionID;
};
void writePreMainHTML(ostream& out, const PreMainHTMLData& data);

struct PrePrevHTMLData {
    uint64_t sessionID;
};
void writePrePrevHTML(ostream& out, const PrePrevHTMLData& data);

struct PrevHTMLData {
    uint64_t sessionID;
};
void writePrevHTML(ostream& out, const PrevHTMLData& data);

struct NextHTMLData {
    uint64_t sessionID;
};
void writeNextHTML(ostream& out, const NextHTMLData& data);

struct PopupIframeHTMLData {
    uint64_t sessionID;
};
void writePopupIframeHTML(ostream& out, const PopupIframeHTMLData& data);

struct DownloadIframeHTMLData {
    uint64_t sessionID;
    uint64_t downloadIdx;
    string fileName;
};

void writeDownloadIframeHTML(ostream& out, const DownloadIframeHTMLData& data);

struct ClipboardIframeHTMLData {};
void writeClipboardIframeHTML(ostream& out, const ClipboardIframeHTMLData& data);

struct ClipboardHTMLData {
    string escapedText;
};
void writeClipboardHTML(ostream& out, const ClipboardHTMLData& data);

}
