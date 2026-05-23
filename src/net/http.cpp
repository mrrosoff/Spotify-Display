#include "net/http.hpp"

#include <curl/curl.h>

#include <memory>

using namespace std;

namespace http {

namespace {

size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *out = static_cast<string *>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

struct CurlDeleter {
    void operator()(CURL *p) const {
        if (p) curl_easy_cleanup(p);
    }
};
struct SlistDeleter {
    void operator()(curl_slist *p) const {
        if (p) curl_slist_free_all(p);
    }
};
using CurlHandle = unique_ptr<CURL, CurlDeleter>;
using SlistHandle = unique_ptr<curl_slist, SlistDeleter>;

bool perform(
    const string &url,
    curl_slist *headers,
    const string *post_body,
    long connect_timeout_s,
    long read_timeout_s,
    string *body,
    long *http_code_out,
    string *error
) {
    CurlHandle curl{curl_easy_init()};
    if (!curl) {
        if (error) *error = "curl_easy_init failed";
        return false;
    }
    body->clear();
    char errbuf[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, body);
    curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, connect_timeout_s);
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, connect_timeout_s + read_timeout_s);
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, "spotify-display/1.0");
    curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, errbuf);
    if (headers) curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers);
    if (post_body) {
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, post_body->c_str());
        curl_easy_setopt(
            curl.get(),
            CURLOPT_POSTFIELDSIZE,
            static_cast<long>(post_body->size())
        );
    }

    const CURLcode rc = curl_easy_perform(curl.get());
    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code_out) *http_code_out = http_code;

    if (rc != CURLE_OK) {
        if (error) *error = errbuf[0] ? errbuf : curl_easy_strerror(rc);
        return false;
    }
    if (http_code < 200 || http_code >= 300) {
        // 204 (No Content) is a success status — Spotify uses it for "nothing playing".
        if (http_code == 204) return true;
        if (error) *error = "HTTP " + to_string(http_code);
        return false;
    }
    return true;
}

SlistHandle build_slist(const vector<string> &headers) {
    curl_slist *list = nullptr;
    for (const auto &h : headers) list = curl_slist_append(list, h.c_str());
    return SlistHandle{list};
}

}  // namespace

void global_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}
void global_cleanup() {
    curl_global_cleanup();
}

bool get(
    const string &url,
    long connect_timeout_s,
    long read_timeout_s,
    string *body,
    string *error
) {
    return perform(
        url, nullptr, nullptr, connect_timeout_s, read_timeout_s, body, nullptr, error
    );
}

bool get_bearer(
    const string &url,
    const string &token,
    long connect_timeout_s,
    long read_timeout_s,
    string *body,
    long *http_code,
    string *error
) {
    const auto headers = build_slist({"Authorization: Bearer " + token});
    return perform(
        url,
        headers.get(),
        nullptr,
        connect_timeout_s,
        read_timeout_s,
        body,
        http_code,
        error
    );
}

bool post(
    const string &url,
    const vector<string> &headers,
    const string &request_body,
    long connect_timeout_s,
    long read_timeout_s,
    string *body,
    long *http_code,
    string *error
) {
    const auto h = build_slist(headers);
    return perform(
        url,
        h.get(),
        &request_body,
        connect_timeout_s,
        read_timeout_s,
        body,
        http_code,
        error
    );
}

}  // namespace http
