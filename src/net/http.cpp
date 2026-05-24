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

struct SlistDeleter {
    void operator()(curl_slist *p) const {
        if (p) curl_slist_free_all(p);
    }
};
using SlistHandle = unique_ptr<curl_slist, SlistDeleter>;

SlistHandle build_slist(const vector<string> &headers) {
    curl_slist *list = nullptr;
    for (const auto &h : headers) list = curl_slist_append(list, h.c_str());
    return SlistHandle{list};
}

void apply_common(CURL *curl, const string &url, long ct, long rt, string *body, char *errbuf) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, ct);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, ct + rt);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "spotify-display/1.0");
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
}

// Run a configured request and translate curl + HTTP status into bool/error.
bool finish(CURL *curl, const char *errbuf, long *http_code_out, string *error) {
    const CURLcode rc = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
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

}  // namespace

void global_init() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}
void global_cleanup() {
    curl_global_cleanup();
}

Session::Session() : handle_(curl_easy_init()) {}

Session::~Session() {
    if (handle_) curl_easy_cleanup(static_cast<CURL *>(handle_));
}

bool Session::get(
    const string &url, long ct, long rt, string *body, string *error
) {
    return get_bearer(url, {}, ct, rt, body, nullptr, error);
}

bool Session::get_bearer(
    const string &url,
    const string &token,
    long ct,
    long rt,
    string *body,
    long *http_code,
    string *error
) {
    auto *curl = static_cast<CURL *>(handle_);
    if (!curl) {
        if (error) *error = "session has no curl handle";
        return false;
    }
    curl_easy_reset(curl);
    body->clear();
    char errbuf[CURL_ERROR_SIZE] = {0};
    apply_common(curl, url, ct, rt, body, errbuf);
    SlistHandle headers;
    if (!token.empty()) {
        headers = build_slist({"Authorization: Bearer " + token});
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    }
    return finish(curl, errbuf, http_code, error);
}

bool Session::post(
    const string &url,
    const vector<string> &headers_in,
    const string &request_body,
    long ct,
    long rt,
    string *body,
    long *http_code,
    string *error
) {
    auto *curl = static_cast<CURL *>(handle_);
    if (!curl) {
        if (error) *error = "session has no curl handle";
        return false;
    }
    curl_easy_reset(curl);
    body->clear();
    char errbuf[CURL_ERROR_SIZE] = {0};
    apply_common(curl, url, ct, rt, body, errbuf);
    SlistHandle headers = build_slist(headers_in);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.c_str());
    curl_easy_setopt(
        curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_body.size())
    );
    return finish(curl, errbuf, http_code, error);
}

bool get(const string &url, long ct, long rt, string *body, string *error) {
    Session s;
    return s.get(url, ct, rt, body, error);
}

}  // namespace http
