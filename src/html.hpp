#pragma once

#include "common.hpp"

struct NewSessionHTMLData {
    uint64_t sessionID;
};
void writeNewSessionHTML(ostream& out, const NewSessionHTMLData& data);

struct MainHTMLData {
    uint64_t sessionID;
    uint64_t mainIdx;
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
