#include "net/http.hpp"

#include <curl/curl.h>

#include <chrono>
#include <cstdio>
#include <ctime>
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

int debug_cb(CURL *, curl_infotype type, char *data, size_t size, void *) {
    if (type != CURLINFO_TEXT) return 0;
    const auto now = chrono::system_clock::now();
    const auto t = chrono::system_clock::to_time_t(now);
    const auto ms = chrono::duration_cast<chrono::milliseconds>(
                        now.time_since_epoch()
                    )
                        .count() %
                    1000;
    tm tmv{};
    localtime_r(&t, &tmv);
    char ts[16];
    strftime(ts, sizeof(ts), "%H:%M:%S", &tmv);
    string line(data, size);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.pop_back();
    fprintf(stderr, "[%s.%03ld] curl: %s\n", ts, static_cast<long>(ms), line.c_str());
    return 0;
}

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
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    // Force HTTP/1.1. Default would ALPN-negotiate HTTP/2 against
    // Cloudflare-fronted endpoints (like accounts.spotify.com), and HTTP/2's
    // stricter stream timing causes the server to close on us mid-write when
    // the matrix RT thread is starving the kernel. Python's requests is
    // HTTP/1.1-only and works on this hardware; matching that.
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    // Force IPv4. Brother's network's IPv6 path is unreliable: TCP connects
    // and TLS completes, but the socket dies before we can write the HTTP
    // request, surfacing as 'getpeername(): Transport endpoint is not
    // connected' / 'Send failure: Broken pipe'. accounts.spotify.com has
    // AAAA records that curl will prefer otherwise.
    curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    // libcurl will otherwise add 'Expect: 100-continue' on POSTs with HTTP/1.1
    // and stall up to 1s waiting for the server's 100 response before sending
    // the body. Python's requests doesn't do this; matching that behavior
    // avoids the stall on starved-CPU TLS handshakes where the 100 response
    // is delayed long enough that some servers drop the connection.
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "spotify-display/1.0");
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, debug_cb);
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

Session::Session(std::chrono::seconds max_age)
    : handle_(curl_easy_init()),
      created_at_(std::chrono::steady_clock::now()),
      max_age_(max_age) {}

Session::~Session() {
    if (handle_) curl_easy_cleanup(static_cast<CURL *>(handle_));
}

void Session::reset() {
    if (handle_) curl_easy_cleanup(static_cast<CURL *>(handle_));
    handle_ = curl_easy_init();
    created_at_ = std::chrono::steady_clock::now();
}

void Session::ensure_fresh() {
    if (max_age_.count() <= 0) return;
    if (std::chrono::steady_clock::now() - created_at_ < max_age_) return;
    reset();
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
    ensure_fresh();
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
    ensure_fresh();
    auto *curl = static_cast<CURL *>(handle_);
    if (!curl) {
        if (error) *error = "session has no curl handle";
        return false;
    }
    curl_easy_reset(curl);
    body->clear();
    char errbuf[CURL_ERROR_SIZE] = {0};
    apply_common(curl, url, ct, rt, body, errbuf);
    vector<string> all_headers = headers_in;
    all_headers.emplace_back("Expect:");  // suppress Expect: 100-continue
    SlistHandle headers = build_slist(all_headers);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
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
