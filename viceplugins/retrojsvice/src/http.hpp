#pragma once

#include "common.hpp"

namespace retrojsvice {

class SocketAddress {
public:
    // Create socket address from string representation of type "ADDRESS:PORT",
    // such as "127.0.0.1:8080". Returns an empty optional if parsing the
    // representation failed.
    static optional<SocketAddress> parse(string repr);

private:
    SocketAddress();

    struct Impl;
    shared_ptr<Impl> impl_;

    friend ostream& operator<<(ostream& out, SocketAddress addr);
};

ostream& operator<<(ostream& out, SocketAddress addr);

}
