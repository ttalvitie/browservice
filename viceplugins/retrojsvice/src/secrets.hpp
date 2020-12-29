#pragma once

#include "common.hpp"

namespace retrojsvice {

bool passwordsEqual(string a, string b);

class SecretGenerator {
SHARED_ONLY_CLASS(SecretGenerator);
public:
    SecretGenerator(CKey);

    // 20 characters from [A-Za-z0-9]
    string generateCSRFToken();

private:
    mt19937 rng_;
};

}
