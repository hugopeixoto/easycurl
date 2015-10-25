#ifndef _easycurl_h_
#define _easycurl_h_

#include <iostream>
#include <string>
#include "result.h"

#include <curl/curl.h>

#define USERAGENT_STR "Opera/12.80 (Windows NT 5.1; U; en) Presto/2.10.289 Version/12.02"
#define ISPRINT_LOCALE "pt_PT"
#define DOWNLOAD_SIZE (5*1024)

class EasyCurl {
  public:
    typedef std::string Error;

  private:
    CURL* curl;
    CURLcode curlCode;

    int bufferTotal;

    bool determineIfHtml();

    static int bodyWriter(char *data, size_t size, size_t nmemb, EasyCurl* instance);
    bool instanceBodyWriter(char*data, size_t bytes);

    static int headerWriter(char *data, size_t size, size_t nmemb, EasyCurl* instance);
    bool instanceHeaderWriter(char*data, size_t bytes);

    Result<CURL*, EasyCurl::Error> curlSetup();

    bool extractContentType();
    void extractTitle();
    void extractImageURL();
    bool extractMetadata();

    void fail();

  public:
    bool requestWentOk;
    bool isHtml;

    std::string error_message;

    std::string request_url;
    std::string redirect_count;
    std::string response_content_type;
    std::string response_content_length;
    std::string response_code;
    std::string response_body;
    std::string html_title;
    std::string prntscr_url;

    EasyCurl(std::string url);
    ~EasyCurl();
};

#endif
