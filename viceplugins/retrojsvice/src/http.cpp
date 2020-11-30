#include "http.hpp"

#include <Poco/Net/SocketAddress.h>

namespace retrojsvice {

struct SocketAddress::Impl {
    Poco::Net::SocketAddress addr;
    string addrStr;
};

optional<SocketAddress> SocketAddress::parse(string repr) {
    SocketAddress ret;
    ret.impl_ = make_shared<Impl>();
    try {
        ret.impl_->addr = Poco::Net::SocketAddress(repr);
        ret.impl_->addrStr = ret.impl_->addr.toString();
    } catch(...) {
        return {};
    }
    return ret;
}

SocketAddress::SocketAddress() {}

ostream& operator<<(ostream& out, SocketAddress addr) {
    out << addr.impl_->addrStr;
    return out;
}

}
