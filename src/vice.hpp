#pragma once

#include "common.hpp"

// Dynamically loaded view service (vice) plugin library that shows the browser
// view to the user and relays back the input events. Can be interacted with
// using a ViceContext.
class VicePlugin {
SHARED_ONLY_CLASS(VicePlugin);
private:
    struct APIFuncs;

public:
    // Returns an empty pointer if loading the plugin failed.
    static shared_ptr<VicePlugin> load(string filename);

    VicePlugin(CKey, CKey,
        string filename,
        void* lib,
        uint64_t apiVersion,
        unique_ptr<APIFuncs> apiFuncs
    );
    ~VicePlugin();

private:
    string filename_;
    void* lib_;
    uint64_t apiVersion_;

    unique_ptr<APIFuncs> apiFuncs_;
};
