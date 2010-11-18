#include "HttpConnection.h"
#include "HttpRequest.h"

#ifdef WIN32
#include <winsock2.h> // greebo: need to include winsock2 before curl/curl.h
#endif

#include <curl/curl.h>

namespace tdm
{

HttpConnection::HttpConnection()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

HttpConnection::~HttpConnection()
{
	// Clean up cURL
	curl_global_cleanup();
}

bool HttpConnection::HasProxy()
{
	return !_proxyHost.empty();
}

std::string HttpConnection::GetProxyHost()
{
	return _proxyHost;
}

std::string HttpConnection::GetProxyUsername()
{
	return _proxyUser;
}

std::string HttpConnection::GetProxyPassword()
{
	return _proxyPass;
}

void HttpConnection::SetProxyHost(const std::string& host)
{
	_proxyHost = host;
}

void HttpConnection::SetProxyUsername(const std::string& user)
{
	_proxyUser = user;
}

void HttpConnection::SetProxyPassword(const std::string& pass)
{
	_proxyPass = pass;
}

HttpRequestPtr HttpConnection::CreateRequest(const std::string& url)
{
	return HttpRequestPtr(new HttpRequest(*this, url));
}

HttpRequestPtr HttpConnection::CreateRequest(const std::string& url, const std::string& destFilename)
{
	return HttpRequestPtr(new HttpRequest(*this, url, destFilename));
}

}
