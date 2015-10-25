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

static Optional<std::string> html_body(const EasyCurl &response) {
  if (response.isHtml) {
    return Optional<std::string>::some(response.response_body);
  } else {
    return Optional<std::string>::none();
  }
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
  long response;

  if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response) != CURLE_OK) {
    return Optional<CURL *>::none();
  }

  if (response == 301 || response == 302) {
    return Optional<CURL *>::none();
  }

  return Optional<CURL *>::some(curl);
}

static Optional<std::string> content_type(CURL *curl) {
  const char *ct;

  if (curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &ct) != CURLE_OK) {
    return Optional<std::string>::none();
  }

  return Optional<std::string>::some(std::string(ct));
}

bool EasyCurl::extractContentType() {
  auto type =
      final_request(this->curl).and_then(content_type).map(filterUnprintables);

  this->response_content_type = type.value_or("");
  this->isHtml = type.map(is_html_content_type).value_or(false);

  return type.is_some();
}

EasyCurl::EasyCurl(string url) {
  this->request_url = url;

  Optional<Error> error =
      curlSetup().or_else([&]() { return this->curlRequest(); });

  this->requestWentOk = error.is_none();
  this->error_message = error.value_or("");

  if (error.is_none()) {
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

static std::string default_title(const std::string &title) {
  if (title == "") {
    return "N/A";
  } else {
    return title;
  }
}

void EasyCurl::extractTitle() {
  this->html_title = default_title(html_body(*this)
                                       .and_then(html_parser::title)
                                       .map(filterUnprintables)
                                       .value_or(""));
}

Optional<EasyCurl::Error> EasyCurl::curlSetup() {
  // Write all expected data in here
  this->curl = curl_easy_init();

  this->bufferTotal = 0;

  if (!curl)
    return Optional<EasyCurl::Error>::some("initialization error");

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

  return Optional<EasyCurl::Error>::none();
}

Optional<EasyCurl::Error> EasyCurl::curlRequest() {
  // Attempt to retrieve the remote page
  this->curlCode = curl_easy_perform(this->curl);

  if ((this->curlCode != CURLE_OK) && (this->curlCode != CURLE_WRITE_ERROR)) {
    return Optional<EasyCurl::Error>::some(curl_easy_strerror(this->curlCode));
  }

  return Optional<EasyCurl::Error>::none();
}

template <typename T> static std::string to_s(const T &value) {
  std::ostringstream oss;
  oss.precision(20);
  oss << value;
  return oss.str();
}

bool EasyCurl::extractMetadata() {
  char *effective_url;
  double content_length;
  long response_code;
  long redirect_count;

  // Along with the request get some information
  curl_easy_getinfo(this->curl, CURLINFO_EFFECTIVE_URL, &effective_url);
  curl_easy_getinfo(this->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                    &content_length);
  curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &response_code);
  curl_easy_getinfo(this->curl, CURLINFO_REDIRECT_COUNT, &redirect_count);

  this->request_url = effective_url;
  this->response_content_length = to_s(content_length);
  this->response_code = to_s(response_code);
  this->redirect_count = to_s(redirect_count);

  return true;
}
