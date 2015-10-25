#include "html_parser.h"
#include "stripper.h"

#include <boost/regex.hpp>
#include <string>

extern "C" size_t decode_html_entities_utf8(char *dest, const char *src);

static std::string decode_html_entities(const std::string &str) {
  char buff[str.length() + 1];
  decode_html_entities_utf8(buff, str.c_str());
  return buff;
}

static Optional<std::string> extract(const std::string &buffer,
                                   const std::string &expr, int match_no) {
  boost::regex re;
  boost::cmatch matches;
  re.assign(expr, boost::regex_constants::icase);

  try {
    if (boost::regex_match(buffer.c_str(), matches, re, boost::match_not_eol)) {
      return Optional<std::string>::some(matches[match_no]);
    }
  } catch (...) {
  }

  return Optional<std::string>::none();
}

Optional<std::string> html_parser::image_url(const std::string &buffer) {
  return extract(buffer,
                 ".*<meta name=\"twitter:image:src\" content=\"([^\"]*)\".*", 1)
      .map(decode_html_entities)
      .map(stripWhitespace);
}

Optional<std::string> html_parser::title(const std::string &buffer) {
  return extract(buffer, ".*(<title>|<title .+>)(.*)</title>.*", 2)
      .map(decode_html_entities)
      .map(stripWhitespace);
}
