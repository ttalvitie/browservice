import os
import platform
import shutil
import subprocess
import sys
import urllib.request
from base64 import b64encode

DEFAULT_BRANCH = "5304"
DEFAULT_COMMIT = "26c0b5e"

# Created at the end of the file.
PATCHER_SCRIPT = None

def fail(msg):
    print("FAIL: " + msg, file=sys.stderr)
    sys.exit(1)

def log(msg):
    print("--- " + msg, file=sys.stderr)

def run():
    if len(sys.argv) not in [4, 6]:
        fail("Usage: python build_cef.py <build directory> <output file> <architecture> [<branch> <commit>]")

    build_dir = sys.argv[1]
    output_path = sys.argv[2]
    arch = sys.argv[3]

    branch = DEFAULT_BRANCH
    commit = DEFAULT_COMMIT
    if len(sys.argv) > 4:
        branch = sys.argv[4]
        commit = sys.argv[5]

    for (name, value, chars, length) in [("branch", branch, "0123456789", 4), ("commit", commit, "0123456789abcdef", 7)]:
        for c in value:
            if c not in chars:
                fail(f"Only characters '{chars}' are allowed in the {name} argument")
                sys.exit()
        if len(value) != length:
            fail(f"The length of the '{name}' argument must be {length}")

    build_dir = os.path.abspath(build_dir)

    if os.path.exists(build_dir):
        fail(f"Given build dir path '{build_dir}' already exists")
    if len(build_dir) > 20:
        fail(f"Given build dir path '{build_dir}' is longer than 20 characters, which may cause issues")

    if os.path.exists(output_path):
        fail(f"Given output file path '{output_path}' already exists")
    output_path_parent = os.path.dirname(output_path)
    if output_path_parent == "":
        output_path_parent = "."
    if not os.path.isdir(output_path_parent):
        fail(f"The parent path '{output_path_parent}' of given output file path '{output_path}' is not a directory")

    os_type = platform.system()
    if os_type not in ["Windows", "Linux"]:
        fail(f"Platform '{os_type}' not supported")

    supported_archs = ["windows32", "windows64"] if os_type == "Windows" else ["x86_64", "armhf", "aarch64"]
    if arch not in supported_archs:
        fail(f"Architecture '{arch}' not supported (supported values: {', '.join(supported_archs)})")

    log(f"Creating build dir '{build_dir}'")
    os.mkdir(build_dir)

    automate_git_path = os.path.join(build_dir, "automate-git.py")
    log(f"Downloading CEF automate-git.py to '{automate_git_path}'")
    urllib.request.urlretrieve("https://bitbucket.org/chromiumembedded/cef/raw/master/tools/automate/automate-git.py", automate_git_path)

    patcher_path = os.path.join(build_dir, "browservice_cef_patcher.py")
    log(f"Installing script for applying Browservice-specific CEF/Chromium patches to '{patcher_path}'")
    with open(patcher_path, "w") as fp:
        fp.write(PATCHER_SCRIPT)

    patched_automate_git_path = os.path.join(build_dir, "patched-automate-git.py")
    log(f"Creating patched automate-git.py script '{patched_automate_git_path}' that will apply browservice_cef_patcher.py before building CEF")
    with open(automate_git_path) as fp:
        automate_git_script = fp.read()
    patched_automate_git_script = automate_git_script.replace(
        "# Build using Ninja.",
        "import browservice_cef_patcher; browservice_cef_patcher.run(cef_src_dir)\n\n# Build using Ninja."
    )
    if patched_automate_git_script == automate_git_script:
        fail("Patching automate-git.py script failed")
    with open(patched_automate_git_path, "w") as fp:
        fp.write(patched_automate_git_script)

    cmd = [
        sys.executable,
        patched_automate_git_path,
        "--download-dir=" + build_dir,
        "--branch=" + branch,
        "--checkout=" + commit,
        "--force-clean",
        "--build-target=cefsimple",
        "--no-debug-build"
    ]
    env = {
        "CEF_ARCHIVE_FORMAT": "tar.bz2"
    }

    if os_type == "Windows":
        if arch == "windows64":
            cmd.append("--x64-build")
        cmd.append("--with-pgo-profiles")
        env["GN_DEFINES"] = "is_official_build=true"
        env["GYP_MSVS_VERSION"] = "2019"
    elif os_type == "Linux":
        if arch == "x86_64":
            cmd.append("--x64-build")
            cmd.append("--with-pgo-profiles")
            env["GN_DEFINES"] = "is_official_build=true use_sysroot=true symbol_level=1 is_cfi=false"
        elif arch == "armhf":
            cmd.append("--arm-build")
            env["GN_DEFINES"] = "is_official_build=true use_sysroot=true symbol_level=1 is_cfi=false use_thin_lto=false chrome_pgo_phase=0 use_vaapi=false"
            env["CEF_INSTALL_SYSROOT"] = "arm"
        elif arch == "aarch64":
            cmd.append("--arm64-build")
            env["GN_DEFINES"] = "is_official_build=true use_sysroot=true symbol_level=1 is_cfi=false use_thin_lto=false chrome_pgo_phase=0"
            env["CEF_INSTALL_SYSROOT"] = "arm64"
        else:
            assert False
    else:
        assert False

    log(f"Building CEF using command {cmd} and environment variables {env}")
    subprocess.check_call(cmd, env={**os.environ, **env})

    binary_distrib_dir = os.path.join(build_dir, "chromium", "src", "cef", "binary_distrib")
    log(f"Build successful, locating output file in '{binary_distrib_dir}'")
    output_filename_matches = []
    for filename in os.listdir(binary_distrib_dir):
        if not filename.startswith("cef_binary"): continue
        if not filename.endswith(".tar.bz2"): continue
        if "release_symbols" in filename: continue
        if f"+g{commit}+" not in filename: continue
        if not os.path.isfile(os.path.join(binary_distrib_dir, filename)): continue
        output_filename_matches.append(filename)
    if len(output_filename_matches) != 1:
        fail(f"Could not find exactly one output filename matching criteria in directory '{binary_distrib_dir}', found matches: {output_filename_matches}")

    output_src_path = os.path.join(binary_distrib_dir, output_filename_matches[0])
    log(f"Copying output file from '{output_src_path}' to '{output_path}'")
    shutil.copyfile(output_src_path, output_path)

    log(f"CEF built successfully, output saved in '{output_path}'")
    log(f"NOTE: The build directory '{build_dir}' was not removed!")
    log("Success")
    return 0

CLIPBOARD_IMPLEMENTATION_H_CODE = b"""\
// Native clipboard implementation replaced by Browservice non-backed clipboard; see .cc file.
"""

CLIPBOARD_IMPLEMENTATION_CC_CODE = b"""\
// Native clipboard implementation replaced by Browservice non-backed clipboard.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "base/strings/utf_string_conversions.h"

namespace {
    std::mutex browserviceClipboardMutex;
    std::string browserviceClipboardText;
    uint64_t browserviceClipboardSeqNum;
}

#ifdef USE_OZONE
#define BROWSERVICE_EXPORT __attribute__((visibility("default")))
#else
#define BROWSERVICE_EXPORT __declspec(dllexport)
#endif

BROWSERVICE_EXPORT extern "C" char* cef_chromiumBrowserviceClipboardPaste() {
    std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
    const char* src = browserviceClipboardText.c_str();
    size_t count = browserviceClipboardText.size() + 1;
    char* str = new char[count];
    std::copy(src, src + count, str);
    return str;
}
BROWSERVICE_EXPORT extern "C" void cef_chromiumBrowserviceClipboardFreePasteResult(char* str) {
    delete [] str;
}
BROWSERVICE_EXPORT extern "C" void cef_chromiumBrowserviceClipboardCopy(const char* str) {
    std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
    browserviceClipboardText = str;
    ++browserviceClipboardSeqNum;
}

namespace ui {

class ClipboardBrowservice : public Clipboard {
public:
    ClipboardBrowservice(const ClipboardBrowservice&) = delete;
    ClipboardBrowservice& operator=(const ClipboardBrowservice&) = delete;

private:
    mutable struct {
        uint64_t sequence_number;
        ClipboardSequenceNumberToken token;
    } clipboard_sequence_;

    ClipboardBrowservice() {}
    ~ClipboardBrowservice() override {}

    void OnPreShutdown() override {}
    DataTransferEndpoint* GetSource(ClipboardBuffer buffer) const override {
        return nullptr;
    }
    const ClipboardSequenceNumberToken& GetSequenceNumber(ClipboardBuffer buffer) const override {
        uint64_t sequence_number;
        {
            std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
            sequence_number = browserviceClipboardSeqNum;
        }
        if(sequence_number != clipboard_sequence_.sequence_number) {
            clipboard_sequence_ = {sequence_number, ClipboardSequenceNumberToken()};
        }
        return clipboard_sequence_.token;
    }
    std::vector<std::u16string> GetStandardFormats(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst) const override {
        std::vector<std::u16string> types;
        types.push_back(base::UTF8ToUTF16(kMimeTypeText));
        return types;
    }
    bool IsFormatAvailable(const ClipboardFormatType& format, ClipboardBuffer buffer, const DataTransferEndpoint* data_dst) const override {
        return format == ClipboardFormatType::PlainTextType();
    }
    void Clear(ClipboardBuffer buffer) override {
        std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
        browserviceClipboardText.clear();
        ++browserviceClipboardSeqNum;
    }
    void ReadAvailableTypes(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, std::vector<std::u16string>* types) const override {
        if(!types) return;
        types->clear();
        *types = GetStandardFormats(buffer, data_dst);
    }
    void ReadText(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, std::u16string* result) const override {
        if(!result) return;
        std::string text;
        {
            std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
            text = browserviceClipboardText;
        }
        *result = base::UTF8ToUTF16(text);
    }
    void ReadAsciiText(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, std::string* result) const override {
        if(!result) return;
        std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
        *result = browserviceClipboardText;
    }
    void ReadHTML(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, std::u16string* markup, std::string* src_url, uint32_t* fragment_start, uint32_t* fragment_end) const override {
        if(markup) markup->clear();
        if(src_url) src_url->clear();
        if(fragment_start) *fragment_start = 0;
        if(fragment_end) *fragment_end = 0;
    }
    void ReadSvg(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, std::u16string* result) const override {
        if(result) result->clear();
    }
    void ReadRTF(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, std::string* result) const override {
        if(result) result->clear();
    }
    void ReadPng(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, ReadPngCallback callback) const override {
        std::vector<uint8_t> data;
        std::move(callback).Run(data);
    }
    void ReadCustomData(ClipboardBuffer buffer, const std::u16string& type, const DataTransferEndpoint* data_dst, std::u16string* result) const override {
        if(result) result->clear();
    }
    void ReadFilenames(ClipboardBuffer buffer, const DataTransferEndpoint* data_dst, std::vector<ui::FileInfo>* result) const override {
        if(result) result->clear();
    }
    void ReadBookmark(const DataTransferEndpoint* data_dst, std::u16string* title, std::string* url) const override {
        if(title) title->clear();
        if(url) url->clear();
    }
    void ReadData(const ClipboardFormatType& format, const DataTransferEndpoint* data_dst, std::string* result) const override {
        if(result) result->clear();
    }
    void WritePortableAndPlatformRepresentations(ClipboardBuffer buffer, const ObjectMap& objects, std::vector<Clipboard::PlatformRepresentation> platform_representations, std::unique_ptr<DataTransferEndpoint> data_src) override {
        {
            std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
            browserviceClipboardText.clear();
            ++browserviceClipboardSeqNum;
        }

        DispatchPlatformRepresentations(std::move(platform_representations));
        for(const auto& object : objects) {
            DispatchPortableRepresentation(object.first, object.second);
        }
    }
    void WriteText(const char* text_data, size_t text_len) override {
        std::lock_guard<std::mutex> lock(browserviceClipboardMutex);
        browserviceClipboardText.assign(text_data, text_len);
        ++browserviceClipboardSeqNum;
    }
    void WriteHTML(const char* markup_data, size_t markup_len, const char* url_data, size_t url_len) override {}
    void WriteSvg(const char* markup_data, size_t markup_len) override {}
    void WriteRTF(const char* rtf_data, size_t data_len) override {}
    void WriteFilenames(std::vector<ui::FileInfo> filenames) override {}
    void WriteBookmark(const char* title_data, size_t title_len, const char* url_data, size_t url_len) override {}
    void WriteWebSmartPaste() override {}
    void WriteBitmap(const SkBitmap& bitmap) override {}
    void WriteData(const ClipboardFormatType& format, const char* data_data, size_t data_len) override {}

#ifdef USE_OZONE
    bool IsSelectionBufferAvailable() const override {
        return false;
    }
#endif

    friend class Clipboard;
};

Clipboard* Clipboard::Create() {
    return new ClipboardBrowservice;
}

}
"""

CLIPBOARD_FACTORY_OZONE_CC_CODE = b"""\
// Native clipboard implementation replaced by Browservice non-backed clipboard; see .cc file.
"""

def embed(data):
    return "b64decode(\"" + b64encode(data).decode("ASCII") + "\")"

PATCHER_SCRIPT = """\
import os
import sys
from base64 import b64decode

def fail(msg):
    print("FAIL: " + msg, file=sys.stderr)
    sys.exit(1)

def log(msg):
    print("--- " + msg, file=sys.stderr)

def run(cef_src_dir):
    log(f"Applying Browservice-specific CEF/Chromium patches to '{cef_src_dir}' and its parent directory")

    for platform in ["win", "ozone"]:
        clipboard_h_path = os.path.join(cef_src_dir, "..", "ui", "base", "clipboard", "clipboard_" + platform + ".h")
        log(f"Replacing '{clipboard_h_path}' with the Browservice clipboard implementation")
        with open(clipboard_h_path, "wb") as fp:
            fp.write(""" + embed(CLIPBOARD_IMPLEMENTATION_H_CODE) + """)

        clipboard_cc_path = os.path.join(cef_src_dir, "..", "ui", "base", "clipboard", "clipboard_" + platform + ".cc")
        log(f"Replacing '{clipboard_cc_path}' with the Browservice clipboard implementation")
        with open(clipboard_cc_path, "wb") as fp:
            fp.write(""" + embed(CLIPBOARD_IMPLEMENTATION_CC_CODE) + """)

    clipboard_factory_ozone_cc_path = os.path.join(cef_src_dir, "..", "ui", "base", "clipboard", "clipboard_factory_ozone.cc")
    log(f"Replacing '{clipboard_factory_ozone_cc_path}' with the Browservice clipboard implementation")
    with open(clipboard_factory_ozone_cc_path, "wb") as fp:
        fp.write(""" + embed(CLIPBOARD_FACTORY_OZONE_CC_CODE) + """)

    log("Browservice-specific CEF/Chromium patches applied successfully")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        fail("Usage: python browservice_cef_patcher.py <CEF src dir>")
    run(sys.argv[1])
"""

if __name__ == "__main__":
    sys.exit(run())
