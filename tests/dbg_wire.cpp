#include <iostream>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>
#include "Response.hpp"
#include "ServerConf.hpp"
#include "LocationConf.hpp"

static std::string drain(Response& r) {
    int sv[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    bool done = false;
    for (int i = 0; !done && i < 100000; i++) done = r.sendSlice(sv[0]);
    close(sv[0]);
    std::string out; char buf[4096]; ssize_t n;
    while ((n = recv(sv[1], buf, sizeof(buf), 0)) > 0) out.append(buf, n);
    close(sv[1]);
    return out;
}

int main() {
    ServerConf conf;
    conf.setServerName("test");
    conf.setMaxBodySize(10*1024*1024);
    LocationConf loc;
    loc.setPath("/");
    loc.setRoot("/tmp");
    loc.addAllowedMethod(GET);
    conf.addLocation(loc);

    Response r;
    r.buildErrorPage("404", conf);
    std::string wire = drain(r);
    std::printf("wire size: %zu\n", wire.size());
    std::printf("first 50 chars: [%s]\n", wire.substr(0, 50).c_str());
    std::printf("hex: ");
    for (size_t i = 0; i < 20 && i < wire.size(); i++)
        std::printf("%02x ", (unsigned char)wire[i]);
    std::printf("\n");
    return 0;
}
