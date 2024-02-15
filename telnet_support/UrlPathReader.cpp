#include <string>

#include "UrlPathReader.h"
#include "SimpleNetMT.h"
//#include "SFSlib/parse/split.h"
//#include "SFSlib/parse/text.h"
//#include "SFSlib/parse/parse.h"

namespace simplenet
{

static const char GET[] = "GET";
static const char HTTP11[] = "HTTP/1.1";

std::pair<std::string, bool> TryReadUrlPath(SimpleNetMT::Connection &a_connection)
{
	std::string url = a_connection.ReadLine();
	const size_t getPos = url.find(std::string{GET});
	const size_t httpPos = url.rfind(std::string{HTTP11});
	if ((getPos != 0) || (httpPos != url.size() - (sizeof(HTTP11) - sizeof('\0'))))
	{
		return std::make_pair(url, false); // not an http request
	}

	std::string popped;
	do 
	{
		popped = a_connection.ReadLine();
	} while (popped != "\n");

	return std::make_pair(url.substr(sizeof(GET), url.size() - sizeof(GET) - sizeof(HTTP11)), true);
}

void EmitHttpResponse(SimpleNetMT::Connection &a_connection, const std::string& a_completeResponse)
{
	SimpleNetMT::Connection& c = a_connection;
	c.Write("HTTP/1.1 200 OK\n");
	c.Write("Content-Type: text/plain\n");
	c.Write("Server: SimpleNet (Win32)\n");
	c.Write("Content-Length: " + std::to_string(a_completeResponse.size()) + "\n");
	c.Write("Connection: Closed\n");
	c.Write("\n");
	c.Write(a_completeResponse);
}

	/*namespace p = sfs::parse;
	using HttpHeader = p::split<std::tuple<p::itext<GET>, std::string, p::itext<HTTP11>>, ' '>;
	HttpHeader::result_type result = p::parse<HttpHeader>(url);*/
	// get rid of headers

} // simplenet
