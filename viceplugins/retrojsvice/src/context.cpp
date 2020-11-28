#include "context.hpp"

namespace retrojsvice {

variant<shared_ptr<Context>, string> Context::init(
    vector<pair<string, string>> options
) {
    for(const pair<string, string>& option : options) {
        const string& name = option.first;
        const string& value = option.second;

        if(
            name != "default-quality" &&
            name != "http-listen-addr" &&
            name != "http-max-threads" &&
            name != "http-auth"
        ) {
            return "Unknown option '" + name + "'";
        }
        if(value == "") {
            return "Invalid value '" + value + "' for option '" + name + "'";
        }
    }
    return Context::create(CKey());
}

Context::Context(CKey, CKey) {
    INFO_LOG("Creating retrojsvice plugin context");
}

Context::~Context() {
    INFO_LOG("Destroying retrojsvice plugin context");
}

}
