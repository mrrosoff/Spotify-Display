#pragma once

#include <string>
#include <vector>

namespace http {

// A persistent HTTP session that reuses a single curl handle. Reuse buys us
// keep-alive TCP, cached TLS session resumption, cached DNS, and HTTP/2
// multiplexing — collectively the difference between a cheap poll and a
// full-TLS-handshake-per-second.
//
// Not thread-safe. Each thread/caller should own its own Session, and
// connection reuse only applies to repeated calls to the same host.
class Session {
public:
    Session();
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    bool get(
        const std::string &url,
        long connect_timeout_s,
        long read_timeout_s,
        std::string *body,
        std::string *error
    );

    bool get_bearer(
        const std::string &url,
        const std::string &token,
        long connect_timeout_s,
        long read_timeout_s,
        std::string *body,
        long *http_code,
        std::string *error
    );

    bool post(
        const std::string &url,
        const std::vector<std::string> &headers,
        const std::string &request_body,
        long connect_timeout_s,
        long read_timeout_s,
        std::string *body,
        long *http_code,
        std::string *error
    );

private:
    void *handle_;  // CURL*
};

// One-shot helpers — fresh handle per call. Use Session for anything polled.
bool get(
    const std::string &url,
    long connect_timeout_s,
    long read_timeout_s,
    std::string *body,
    std::string *error
);

void global_init();
void global_cleanup();

}  // namespace http
