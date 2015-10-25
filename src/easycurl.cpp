////////////////////////////////////
//////
// EasyMode for libcurl
// now using libcurl sucks less :-)
/////
////////////////////////////////////

#include "easycurl.h"
#include "stripper.h"
#include "html_parser.h"
#include "result.h"

#include <sstream>
#include <cstring>
#include <algorithm>

using namespace std;

static bool is_not_printable(char c) {
  // locale l(ISPRINT_LOCALE);
  // return !isprint(c, l);
  return !((unsigned int)c > 31);
}

static std::string filterUnprintables(const std::string &str) {
  std::string filtered = str;
  filtered.erase(remove_if(filtered.begin(), filtered.end(), is_not_printable),
                 filtered.end());
  return filtered;
}

struct Dummy {};
static Optional<Dummy> from_bool(bool ycao) {
  if (ycao) {
    return Optional<Dummy>::some(Dummy());
  } else {
    return Optional<Dummy>::none();
  }
}

static Optional<std::string> html_body(const EasyCurl &response) {
  return from_bool(response.isHtml)
      .and_(Optional<std::string>::some(response.response_body));
}

Result<CURL *, EasyCurl::Error> curl_result(CURL *curl, CURLcode code) {
  typedef Result<CURL *, EasyCurl::Error> R;

  return from_bool(code == CURLE_OK || code == CURLE_WRITE_ERROR)
      .and_(Optional<CURL *>::some(curl))
      .ok_or_else([code]() { return std::string(curl_easy_strerror(code)); });
}

template <typename T>
static Result<T, EasyCurl::Error> easycurl_getinfo(CURL *curl, CURLINFO info) {
  T result;

  return curl_result(curl, curl_easy_getinfo(curl, info, &result))
      .map([&result](CURL *) { return result; });
}

template <typename T> static std::string to_s(const T &value) {
  std::ostringstream oss;
  oss.precision(20);
  oss << value;
  return oss.str();
}

static bool is_html_content_type(const std::string &content_type) {
  static std::string valid[] = {"text/html", "application/xhtml+xml"};

  for (const auto &ct : valid) {
    if (content_type.compare(0, ct.length(), ct) == 0) {
      return true;
    }
  }

  return false;
}

int EasyCurl::headerWriter(char *data, size_t size, size_t nmemb,
                           EasyCurl *instance) {
  return instance->instanceHeaderWriter(data, size * nmemb) ? size * nmemb : 0;
}

bool EasyCurl::instanceHeaderWriter(char *data, size_t bytes) {
  if (!memcmp(data, "\r\n", std::min(bytes, (size_t)2U))) {
    if (extractContentType() && !isHtml) {
      // early abort if not html
      return false;
    }
  }

  this->bufferTotal += bytes;

  // Continue if max size is not exceeded
  return (this->bufferTotal <= DOWNLOAD_SIZE);
}

int EasyCurl::bodyWriter(char *data, size_t size, size_t nmemb,
                         EasyCurl *instance) {
  return instance->instanceBodyWriter(data, size * nmemb) ? size * nmemb : 0;
}

bool EasyCurl::instanceBodyWriter(char *data, size_t bytes) {
  // Append the data to the buffer
  this->response_body.append(data, bytes);

  this->bufferTotal += bytes;

  // Continue if max size is not exceeded
  return (this->bufferTotal <= DOWNLOAD_SIZE);
}

static Optional<CURL *> final_request(CURL *curl) {
  auto not_redirect =
      [](long code) { return from_bool(code != 301 && code != 302); };

  return easycurl_getinfo<long>(curl, CURLINFO_RESPONSE_CODE)
      .ok()
      .and_then(not_redirect)
      .and_(Optional<CURL *>::some(curl));
}

static Optional<std::string> content_type(CURL *curl) {
  return easycurl_getinfo<const char *>(curl, CURLINFO_CONTENT_TYPE)
      .map(to_s<const char *>)
      .ok();
}

bool EasyCurl::extractContentType() {
  auto type =
      final_request(this->curl).and_then(content_type).map(filterUnprintables);

  this->response_content_type = type.value_or("");
  this->isHtml = type.map(is_html_content_type).value_or(false);

  return type.is_some();
}

static Result<CURL *, EasyCurl::Error> curlRequest(CURL *curl) {
  // Attempt to retrieve the remote page
  return curl_result(curl, curl_easy_perform(curl));
}

EasyCurl::EasyCurl(string url) {
  this->request_url = url;

  Result<CURL *, Error> result = curlSetup().and_then(curlRequest);

  this->requestWentOk = result.is_ok();
  this->error_message = result.err().value_or("");

  if (result.is_ok()) {
    extractMetadata();

    extractTitle();
    extractImageURL();
  }
}

EasyCurl::~EasyCurl() {
  if (this->curl) {
    curl_easy_cleanup(this->curl);
  }
}

void EasyCurl::extractImageURL() {
  this->prntscr_url = html_body(*this)
                          .and_then(html_parser::image_url)
                          .map(filterUnprintables)
                          .value_or("");
}

static auto validate_title(const std::string &title) {
  return from_bool(title != "").and_(Optional<std::string>::some(title));
}

void EasyCurl::extractTitle() {
  this->html_title = html_body(*this)
                         .and_then(html_parser::title)
                         .map(filterUnprintables)
                         .and_then(validate_title)
                         .value_or("N/A");
}

Result<CURL *, EasyCurl::Error> EasyCurl::curlSetup() {
  // Write all expected data in here
  this->curl = curl_easy_init();

  this->bufferTotal = 0;

  if (!curl)
    return Result<CURL *, EasyCurl::Error>::from_error("initialization error");

  curl_easy_setopt(this->curl, CURLOPT_URL, this->request_url.c_str());
  curl_easy_setopt(this->curl, CURLOPT_USERAGENT, USERAGENT_STR);

  // I'm not sure how CURL_TIMEOUT works
  curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, 10);
  curl_easy_setopt(this->curl, CURLOPT_MAXREDIRS, 10);
  curl_easy_setopt(this->curl, CURLOPT_HEADER, 0);
  curl_easy_setopt(this->curl, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(this->curl, CURLOPT_ENCODING, "identity");
  curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1);

  curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, (void *)this);
  curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, EasyCurl::bodyWriter);

  curl_easy_setopt(this->curl, CURLOPT_HEADERDATA, (void *)this);
  curl_easy_setopt(this->curl, CURLOPT_HEADERFUNCTION, EasyCurl::headerWriter);

  return Result<CURL *, EasyCurl::Error>::from_result(this->curl);
}

bool EasyCurl::extractMetadata() {
  this->request_url =
      easycurl_getinfo<const char *>(curl, CURLINFO_EFFECTIVE_URL)
          .map(to_s<const char *>)
          .value_or("");

  this->response_content_length =
      easycurl_getinfo<double>(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD)
          .map(to_s<double>)
          .value_or("");

  this->response_code = easycurl_getinfo<long>(curl, CURLINFO_RESPONSE_CODE)
                            .map(to_s<long>)
                            .value_or("");

  this->redirect_count = easycurl_getinfo<long>(curl, CURLINFO_REDIRECT_COUNT)
                             .map(to_s<long>)
                             .value_or("");

  return true;
}
