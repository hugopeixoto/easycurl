////////////////////////////////////
//////
// EasyMode for libcurl
// now using libcurl sucks less :-)
/////
////////////////////////////////////

#include "curl.h"


bool EasyCurl::is_not_printable(char c) {
  locale l(ISPRINT_LOCALE);
  return !isprint(c, l);
}

int EasyCurl::writer(char *data, size_t size, size_t nmemb, EasyCurl* instance) {
  if (instance != NULL) {
    return instance->instanceWriter(data, size, nmemb);
  }
  return 0;
}
    
int EasyCurl::instanceWriter(char*data, size_t size, size_t nmemb) {

  // Append the data to the buffer
  this->response_body.append(data, size * nmemb);
  bufferTotal += size * nmemb;
  
  //Stop if max size exceeded by returning 0
  if (bufferTotal < DOWNLOAD_SIZE) {
    return size * nmemb;
  } else {
    return 0;
  }
}
    
bool EasyCurl::determineIfHtml() {
  string valid[] = {"text/html", "application/xhtml+xml"};
  
  for (int i = 0; i < 2; i++)
    if (this->response_content_type.compare(0, valid[i].length(), valid[i]) == 0)
    return true;
  
  return false;
}
    
string EasyCurl::parseFor(string expr, int match_no) {
  boost::regex re;
  boost::cmatch matches;
  re.assign(expr, boost::regex_constants::icase);
  if (boost::regex_match(this->response_body.c_str(), matches, re)) {
    try {
      return matches[match_no];
    } catch(...) {
      return "N/A";
    }
  } else {
    return "N/A";
  }
}
    
EasyCurl::EasyCurl(string url) {
  this->request_url = url;
  int result;
  
  result = this->curlSetup(false);
  if (result == -1) {
    this->requestWentOk = false;
    return;
  }
  
  result = this->curlRequest();
  if (result == -1) {
    this->requestWentOk = false;
    this->error_message = curl_easy_strerror(this->curlCode);
    return;
  }

  this->isHtml = this->determineIfHtml();
  if (this->isHtml) {
    result = this->curlSetup(true);
    if (result == -1) {
      this->requestWentOk = false;
      return;
    }
      
    result = this->curlRequest();
    if (result == -1) { 
      this->requestWentOk = false;
      this->error_message = curl_easy_strerror(this->curlCode);
      return;
    }

    // Obtain HTML Title
    string html_title_regexp = ".*(<title>|<title .+>)(.*)</title>.*";
    this->html_title = parseFor(html_title_regexp, 2);

  }
  
  this->requestWentOk = true;
}
    
int EasyCurl::curlSetup(bool getBody) {
  // Write all expected data in here

  this->curl = curl_easy_init();
  char errorBuffer[CURL_ERROR_SIZE];
  

  if (!curl)
    return -1;
    
  curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, errorBuffer);
  curl_easy_setopt(this->curl, CURLOPT_URL, this->request_url.c_str());
  curl_easy_setopt(this->curl, CURLOPT_USERAGENT, USERAGENT_STR);
  //I'm not sure how CURL_TIMEOUT works
  curl_easy_setopt(this->curl, CURLOPT_TIMEOUT, 5); 
  curl_easy_setopt(this->curl, CURLOPT_MAXREDIRS, 10);
  curl_easy_setopt(this->curl, CURLOPT_HEADER, 0);
  curl_easy_setopt(this->curl, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt(this->curl, CURLOPT_ENCODING, "identity");
  curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1);
  
  if (getBody) {
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, (void*)this);
    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, EasyCurl::writer);
    curl_easy_setopt(this->curl, CURLOPT_NOBODY, 0);
  } else {
    curl_easy_setopt(this->curl, CURLOPT_WRITEHEADER, (void*)this);
    curl_easy_setopt(this->curl, CURLOPT_HEADERFUNCTION, EasyCurl::writer);
    curl_easy_setopt(this->curl, CURLOPT_NOBODY, 1);
  }
  
  return 0;
}
 
int EasyCurl::curlRequest() {

  char* effective_url;
  char* content_type;
  double content_length;
  long response_code;
  long redirect_count;

  // (re)initialize the buffer
  this->response_body = "";

  // Attempt to retrieve the remote page
  this->curlCode = curl_easy_perform(this->curl);


  if (this->curlCode != CURLE_OK) {
    curl_easy_cleanup(this->curl);
    return -1;
  }

  remove_if(this->response_body.begin(), this->response_body .end(), EasyCurl::is_not_printable);
        
  // Along with the request get some information
  curl_easy_getinfo(this->curl, CURLINFO_EFFECTIVE_URL, &effective_url);
  curl_easy_getinfo(this->curl, CURLINFO_CONTENT_TYPE, &content_type);

  curl_easy_getinfo(this->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);
  curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &response_code);
  curl_easy_getinfo(this->curl, CURLINFO_REDIRECT_COUNT, &redirect_count);

  ostringstream oss;
  string s;

  oss << effective_url;
  this->request_url = oss.str();
  oss.str("");

  oss << content_type;
  s = oss.str();
  remove_if(s.begin(), s.end(), EasyCurl::is_not_printable);
  this->response_content_type = s;
  oss.str("");

  oss << content_length;
  this->response_content_length = oss.str();
  oss.str("");

  oss << response_code;
  this->response_code = oss.str();
  oss.str("");

  oss << redirect_count;
  this->redirect_count = oss.str();
  oss.str("");

  curl_easy_cleanup(this->curl);

  return 0;
}