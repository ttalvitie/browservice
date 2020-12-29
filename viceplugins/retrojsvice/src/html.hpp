#pragma once

#include "common.hpp"

namespace retrojsvice {

struct NewWindowHTMLData {
    const string& pathPrefix;
};
void writeNewWindowHTML(ostream& out, const NewWindowHTMLData& data);

struct PreMainHTMLData {
    const string& pathPrefix;
};
void writePreMainHTML(ostream& out, const PreMainHTMLData& data);

struct MainHTMLData {
    const string& pathPrefix;
    uint64_t mainIdx;
    const string& nonCharKeyList;
};
void writeMainHTML(ostream& out, const MainHTMLData& data);

struct PrePrevHTMLData {
    const string& pathPrefix;
};
void writePrePrevHTML(ostream& out, const PrePrevHTMLData& data);

struct PrevHTMLData {
    const string& pathPrefix;
};
void writePrevHTML(ostream& out, const PrevHTMLData& data);

struct NextHTMLData {
    const string& pathPrefix;
};
void writeNextHTML(ostream& out, const NextHTMLData& data);

}
