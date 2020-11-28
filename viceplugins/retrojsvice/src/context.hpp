#include "common.hpp"

namespace retrojsvice {

// The implementation of the vice plugin context, exposed through the C API in
// vice_plugin_api.cpp.
class Context {
SHARED_ONLY_CLASS(Context);
public:
    // Returns either a successfully constructed context or an error message.
    static variant<shared_ptr<Context>, string> init(
        vector<pair<string, string>> options
    );

    // Private constructor.
    Context(CKey, CKey);
    ~Context();
};

}
