#pragma once

#include "common.hpp"

namespace retrojsvice {

bool passwordsEqual(string a, string b);

class SecretGenerator {
SHARED_ONLY_CLASS(SecretGenerator);
public:
    SecretGenerator(CKey);

    // 32 characters from [A-Za-z0-9]
    string generateCSRFToken();

    // 2000..2500 integers in range 0..255
    vector<int> generateSnakeOilCipherKey();

private:
    mt19937 rng_;
};

}
