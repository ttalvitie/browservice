#include "common.hpp"

namespace retrowebvice {

class Context {
public:
    // Returns either a successfully constructed context or an error message.
    static variant<unique_ptr<Context>, string> create(
        vector<pair<string, string>> options,
        function<void(string, string)> panicCallback,
        function<void(string, string)> infoLogCallback,
        function<void(string, string)> warningLogCallback,
        function<void(string, string)> errorLogCallback
    );

    Context(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(const Context&) = delete;
    Context& operator=(Context&&) = delete;

    ~Context();

    // Returns documentation for supported options in create as
    // (name, valSpec, desc, defaultValStr)-tuples.
    static vector<tuple<string, string, string, string>> supportedOptionDocs();

private:
    Context() {}

    function<void(string, string)> panicCallback_;
    function<void(string, string)> infoLogCallback_;
    function<void(string, string)> warningLogCallback_;
    function<void(string, string)> errorLogCallback_;
};

}
