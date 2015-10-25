#ifndef HTML_PARSER_H_
#define HTML_PARSER_H_

#include <string>
#include "result.h"

namespace html_parser {
Optional<std::string> image_url(const std::string &buffer);
Optional<std::string> title(const std::string &buffer);
}

#endif
