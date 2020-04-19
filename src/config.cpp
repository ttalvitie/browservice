#include "config.hpp"

#include <Poco/Net/HTTPServer.h>

namespace {

#define CONF_OPT_TYPE(var) \
    remove_const<remove_reference<decltype(declval<Config>().*(&Config::var))>::type>::type

template <typename T>
struct OptParser;

template <>
struct OptParser<string> {
    static optional<string> parse(const string& str) {
        return str;
    }
};

template <typename T, const T Config::* Var, typename S>
struct OptInfoBase {
    string defaultValStr() {
        return "default " + toString(static_cast<S*>(this)->defaultVal());
    }
};

template <typename T, const T Config::* Var>
struct OptInfo;

#define CONF_DEF_OPT_INFO(var) \
    template <> \
    struct OptInfo<CONF_OPT_TYPE(var), &Config::var> : \
        public OptInfoBase< \
            CONF_OPT_TYPE(var), \
            &Config::var, \
            OptInfo<CONF_OPT_TYPE(var), &Config::var> \
        >

#include "config_defs.hpp"

#define CONF_OPT_INFO(var) \
    OptInfo<CONF_OPT_TYPE(var), &Config::var>()

template <typename T, const T Config::* Var>
string helpLine(OptInfo<T, Var> info) {
    stringstream ss;
    ss << "  --" << info.name << "=" << info.valSpec << ' ';
    while(ss.tellp() < 32) {
        ss << ' ';
    }
    ss << info.desc << " [" << info.defaultValStr() << "]";
    return ss.str();
}

}

class Config::Src {
public:
    Src()
        : dummy_(0)
          #define CONF_FOREACH_OPT_ITEM(var) \
              ,var(CONF_OPT_INFO(var).defaultVal())
          CONF_FOREACH_OPT
          #undef CONF_FOREACH_OPT_ITEM
    {}

private:
    int dummy_;

public:
    #define CONF_FOREACH_OPT_ITEM(var) \
        CONF_OPT_TYPE(var) var;
    CONF_FOREACH_OPT
    #undef CONF_FOREACH_OPT_ITEM
};

Config::Config(CKey, Src& src)
    : dummy_(0)
      #define CONF_FOREACH_OPT_ITEM(var) \
          ,var(move(src.var))
      CONF_FOREACH_OPT
      #undef CONF_FOREACH_OPT_ITEM
{}

shared_ptr<Config> Config::read(int argc, char* argv[]) {
    CHECK(argc >= 1);

    Src src;

    map<string, function<bool(string)>> optHandlers;
    #define CONF_FOREACH_OPT_ITEM(var) \
        optHandlers[CONF_OPT_INFO(var).name] = [&src](const string& valStr) { \
            typedef CONF_OPT_TYPE(var) T; \
            optional<T> val = OptParser<T>::parse(valStr); \
            if(val && CONF_OPT_INFO(var).validate((const T&)*val)) { \
                src.var = move(*val); \
                return true; \
            } else { \
                return false; \
            } \
        };
    CONF_FOREACH_OPT
    #undef CONF_FOREACH_OPT_ITEM

    set<string> optsSeen;

    for(int argi = 1; argi < argc; ++argi) {
        string arg = argv[argi];
        if(arg == "--help") {
            cout << "USAGE: " << argv[0] << " [OPTION]...\n";
            cout << "\n";
            cout << "Supported options:\n";

            vector<string> lines;

            #define CONF_FOREACH_OPT_ITEM(var) \
                lines.push_back(helpLine(CONF_OPT_INFO(var)));
            CONF_FOREACH_OPT
            #undef CONF_FOREACH_OPT_ITEM
            lines.push_back("  --help                        show this help and exit");

            sort(lines.begin(), lines.end());
            for(const string& line : lines) {
                cout << line << '\n';
            }
            
            return {};
        }

        if(arg.substr(0, 2) == "--") {
            int eqSignPos = 2;
            while(eqSignPos < (int)arg.size() && arg[eqSignPos] != '=') {
                ++eqSignPos;
            }
            if(eqSignPos < (int)arg.size()) {
                string optName = arg.substr(2, eqSignPos - 2);
                auto it = optHandlers.find(optName);
                if(it != optHandlers.end()) {
                    if(!optsSeen.insert(optName).second) {
                        cerr << "ERROR: Option --" << optName << " specified multiple times\n";
                        return {};
                    }

                    string optVal = arg.substr(eqSignPos + 1);
                    bool result = it->second(optVal);
                    if(!result) {
                        cerr << "ERROR: Invalid value '" << optVal << "' given for option --" << optName << "\n";
                        cerr << "See '" << argv[0] << " --help' for more information\n";
                        return {};
                    }
                    continue;
                }
            }
        }

        if(arg.substr(0, 2) == "--" && optHandlers.count(arg.substr(2))) {
            cerr << "ERROR: Value missing for option " << arg << "\n";
        } else {
            cerr << "ERROR: Unrecognized option '" << arg << "'\n";
        }
        cerr << "Try '" << argv[0] << " --help' for list of supported options\n";
        return {};
    }
    return Config::create(src);
}
