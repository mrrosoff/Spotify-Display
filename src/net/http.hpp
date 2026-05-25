#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace http {

// A persistent HTTP session that reuses a single curl handle. Reuse buys us
// keep-alive TCP, cached TLS session resumption, and cached DNS —
// collectively the difference between a cheap poll and a full-TLS-handshake-
// per-second.
//
// The handle is recycled (destroyed + re-created) after `max_age` to avoid
// long-lived state going stale: middleboxes / load balancers may rotate or
// silently kill connections we still think are alive, leading to wedged
// state that survives across requests. Periodic recycling forces a fresh
// connection, DNS lookup, and TLS handshake.
//
// Not thread-safe. Each thread/caller should own its own Session, and
// connection reuse only applies to repeated calls to the same host.
class Session {
public:
    explicit Session(
        std::chrono::seconds max_age = std::chrono::minutes{30}
    );
    ~Session();
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    // Destroy the underlying curl handle and create a fresh one. Callers
    // can use this after an error to defensively clear any wedged state.
    void reset();

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
    std::chrono::steady_clock::time_point created_at_;
    std::chrono::seconds max_age_;
    void ensure_fresh();
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
