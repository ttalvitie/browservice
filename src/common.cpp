#include "common.hpp"

#include "include/wrapper/cef_closure_task.h"
#include "include/base/cef_callback.h"

namespace browservice {

namespace {

template <typename A, typename B>
void sanitizeUTF8StringImpl(const string& str, A byteHandler, B pointHandler) {
    for(size_t i = 0; i < str.size(); ++i) {
        int ch = (int)(uint8_t)str[i];
        if(ch == 0) {
            continue;
        }
        if(ch < 128) {
            byteHandler(str.data() + i, 1);
            pointHandler(ch);
            continue;
        }

        size_t length;
        if((ch & 0xE0) == 0xC0) {
            length = 2;
        } else if((ch & 0xF0) == 0xE0) {
            length = 3;
        } else if((ch & 0xF8) == 0xF0) {
            length = 4;
        } else {
            continue;
        }

        if(i + length > str.size()) {
            continue;
        }

        bool ok = true;
        int point = ch & (0x7F >> length);
        for(size_t j = 1; j < length; ++j) {
            int ch2 = (int)(uint8_t)str[i + j];
            if((ch2 & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            point = (point << 6) | (ch2 & 0x3F);
        }
        if(!ok) {
            continue;
        }

        if(
            (length == (size_t)2 && point >= 0x80 && point <= 0x7FF) ||
            (length == (size_t)3 && (
                (point >= 0x800 && point <= 0xD7FF) ||
                (point >= 0xE000 && point <= 0xFFFF)
            )) ||
            (length == (size_t)4 && point >= 0x10000 && point <= 0x10FFFF)
        ) {
            byteHandler(str.data() + i, length);
            pointHandler(point);
            i += length - 1;
        }
    }
}

}

string sanitizeUTF8String(string str) {
    string ret;
    sanitizeUTF8StringImpl(
        str,
        [&](const char* bytes, size_t count) {
            ret.insert(ret.end(), bytes, bytes + count);
        },
        [](int) {}
    );
    return ret;
}

vector<int> sanitizeUTF8StringToCodePoints(string str) {
    vector<int> ret;
    sanitizeUTF8StringImpl(
        str,
        [](const char*, size_t) {},
        [&](int point) {
            ret.push_back(point);
        }
    );
    return ret;
}

mt19937 createRNG() {
    static_assert(sizeof(random_device::result_type) == 4);
    static_assert(mt19937::word_size == 32);

    random_device dev;
    array<uint32_t, mt19937::state_size> seedData;
    for(uint32_t& val : seedData) {
        val = dev();
    }
    seed_seq seedSeq(seedData.begin(), seedData.end());
    return mt19937(seedSeq);
}

static atomic<bool> panicUsingCEFFatalError_(false);

void Panicker::panic_(string msg) {
    stringstream output;
    output << "PANIC @ " << location_;
    if(!msg.empty()) {
        output << ": " << msg;
    }
    output << "\n";

    cerr << output.str();
    cerr.flush();

    if(panicUsingCEFFatalError_.load()) {
        LOG(FATAL);
    }

    abort();
}

void enablePanicUsingCEFFatalError() {
    panicUsingCEFFatalError_.store(true);
}

void postTask(function<void()> func) {
    void (*call)(function<void()>) = [](function<void()> func) {
        func();
    };
    CefPostTask(TID_UI, base::BindOnce(call, func));
}

atomic<bool> requireUIThreadEnabled_(false);


}

// Compatibility fix for missing symbol in builds where DCHECK_IS_ON() doesn't match libcef_dll_wrapper
#include "include/base/cef_ref_counted.h"

namespace base {
namespace cef_subtle {
#if DCHECK_IS_ON()
RefCountedThreadSafeBase::~RefCountedThreadSafeBase() {}
#endif
}
}
