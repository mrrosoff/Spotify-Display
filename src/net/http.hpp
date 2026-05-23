#pragma once

#include <string>
#include <vector>

namespace http {

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
    long *http_code,  // optional; may be null
    std::string *error
);

// POST with a list of "Header: value" strings and a raw request body.
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

void global_init();
void global_cleanup();

}  // namespace http
