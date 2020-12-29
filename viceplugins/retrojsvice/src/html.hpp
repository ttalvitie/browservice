#pragma once

#include "common.hpp"

namespace retrojsvice {

struct NewWindowHTMLData {
    uint64_t handle;
};
void writeNewWindowHTML(ostream& out, const NewWindowHTMLData& data);

struct PreMainHTMLData {
    uint64_t handle;
};
void writePreMainHTML(ostream& out, const PreMainHTMLData& data);

struct MainHTMLData {
    uint64_t handle;
    uint64_t mainIdx;
    const string& nonCharKeyList;
};
void writeMainHTML(ostream& out, const MainHTMLData& data);

struct PrePrevHTMLData {
    uint64_t handle;
};
void writePrePrevHTML(ostream& out, const PrePrevHTMLData& data);

struct PrevHTMLData {
    uint64_t handle;
};
void writePrevHTML(ostream& out, const PrevHTMLData& data);

struct NextHTMLData {
    uint64_t handle;
};
void writeNextHTML(ostream& out, const NextHTMLData& data);

}
