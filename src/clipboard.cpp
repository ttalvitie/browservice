#include "clipboard.hpp"

namespace browservice {

// Accessor functions defined in patched CEF.
extern "C" char* cef_chromiumBrowserviceClipboardPaste();
extern "C" void cef_chromiumBrowserviceClipboardFreePasteResult(char* str);
extern "C" void cef_chromiumBrowserviceClipboardCopy(const char* str);

void copyToClipboard(string str) {
#ifdef USE_STD_CEF
	// When using standard CEF, we cannot access the internal clipboard functions.
    // For now, we will just warn or do nothing, as implementing full clipboard integration
    // without the patches requires a different approach (e.g. platform specific clipboard API).
    // TODO: Implement native clipboard support for macOS/Linux-std-cef
    WARNING_LOG("Clipboard copy not supported with standard CEF");
#else
	cef_chromiumBrowserviceClipboardCopy(str.c_str());
#endif
}

string pasteFromClipboard() {
#ifdef USE_STD_CEF
    WARNING_LOG("Clipboard paste not supported with standard CEF");
    return "";
#else
	char* buf = cef_chromiumBrowserviceClipboardPaste();
	REQUIRE(buf != nullptr);
	string str = buf;
	cef_chromiumBrowserviceClipboardFreePasteResult(buf);
	return str;
#endif
}

}
