#include "secrets.hpp"

namespace retrojsvice {

namespace {

bool passwordsEqual_(const void* a, const void* b, size_t size) {
    const unsigned char* x = (const unsigned char*)a;
    const unsigned char* y = (const unsigned char*)b;
    volatile unsigned char agg = 0;
    for(volatile size_t i = 0; i < size; ++i) {
        agg |= x[i] ^ y[i];
    }
    return !agg;
}

}

bool passwordsEqual(string a, string b) {
    if(a.size() != b.size()) {
        return false;
    }
    return passwordsEqual_(a.data(), b.data(), a.size());
}

SecretGenerator::SecretGenerator(CKey) {
    REQUIRE_API_THREAD();

    random_device rdev;
    const int SeedSize = 624;
    unsigned seed[SeedSize];
    for(int i = 0; i < SeedSize; ++i) {
        seed[i] = rdev();
    }
    seed_seq seedSeq(seed, seed + SeedSize);
    rng_.seed(seedSeq);
}

string SecretGenerator::generateCSRFToken() {
    REQUIRE_API_THREAD();

    string token;
    for(int i = 0; i < 20; ++i) {
        int t = uniform_int_distribution<int>(0, 61)(rng_);
        if(t < 10) {
            REQUIRE(t >= 0);
            token.push_back('0' + t);
        } else {
            t -= 10;
            if(t < 26) {
                token.push_back('a' + t);
            } else {
                t -= 26;
                REQUIRE(t < 26);
                token.push_back('A' + t);
            }
        }
    }

    return token;
}

vector<int> SecretGenerator::generateSnakeOilCipherKey() {
    REQUIRE_API_THREAD();

    vector<int> key(uniform_int_distribution<int>(5000, 6000)(rng_));
    for(int& val : key) {
        val = uniform_int_distribution<int>(0, 255)(rng_);
    }
    return key;
}

}
