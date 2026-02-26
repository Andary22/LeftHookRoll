/**
 * @file Request.cpp
 * @brief Implementation of the Request class HTTP parser state machine.
 */

#include "../includes/Request.hpp"

// Canonical Form

Request::Request(): _methodName(UNKNOWN_METHOD), _contentLength(-1), _reqState(REQ_HEADERS), _statusCode("200"), _maxBodySize(0), _totalBytesRead(0), _chunkSize(0), _chunkDecodeOffset(0), _isBodyProcessed(false)
{}

Request::Request(long long maxBodySize): _methodName(UNKNOWN_METHOD), _contentLength(-1), _reqState(REQ_HEADERS), _statusCode("200"), _maxBodySize(static_cast<size_t>(maxBodySize)), _totalBytesRead(0), _chunkSize(0), _chunkDecodeOffset(0), _isBodyProcessed(false)
{}

Request::Request(const Request& other): _methodName(other._methodName), _URL(other._URL), _protocol(other._protocol), _query(other._query), _contentLength(other._contentLength), _body(other._body), _headers(other._headers), _reqState(other._reqState), _statusCode(other._statusCode), _maxBodySize(other._maxBodySize), _totalBytesRead(other._totalBytesRead), _chunkSize(other._chunkSize), _chunkDecodeOffset(other._chunkDecodeOffset), _isBodyProcessed(other._isBodyProcessed)
{
}

Request& Request::operator=(const Request& other)
{
	if (this != &other)
	{
		_methodName = other._methodName;
		_URL = other._URL;
		_protocol = other._protocol;
		_query = other._query;
		_contentLength = other._contentLength;
		_body = other._body;
		_headers = other._headers;
		_reqState = other._reqState;
		_statusCode = other._statusCode;
		_maxBodySize = other._maxBodySize;
		_totalBytesRead = other._totalBytesRead;
		_chunkSize = other._chunkSize;
		_chunkDecodeOffset = other._chunkDecodeOffset;
		_isBodyProcessed = other._isBodyProcessed;
	}
	return *this;
}

Request::~Request() {}

// Getters

HTTPMethod Request::getMethod() const {
    return _methodName;
}

const std::string& Request::getURL() const {
    return _URL;
}

const std::string& Request::getProtocol() const {
    return _protocol;
}

const std::string& Request::getQuery() const {
    return _query;
}

const std::map<std::string, std::string>& Request::getHeaders() const {
    return _headers;
}

//  Core Parsing Behavior
void Request::_parseRequestLine(const std::string& line)
{
	size_t firstSpace = line.find(' ');
	if (firstSpace == std::string::npos)
	{
		_reqState = REQ_ERROR;
		_statusCode = "400"; // Bad Request
		return;
	}
	std::string methodStr = line.substr(0, firstSpace);
	_methodName = AllowedMethods::stringToMethod(methodStr);
	if (_methodName == UNKNOWN_METHOD)
	{
		_reqState = REQ_ERROR;
		_statusCode = "501"; // Not Implemented
		return;
	}
	size_t secondSpace = line.find(' ', firstSpace + 1);
	if (secondSpace == std::string::npos)
	{
		// HTTP/0.9: "GET /path"
		_URL = line.substr(firstSpace + 1);
        if (_URL.empty() || _URL[0] != '/')
        {
            _reqState = REQ_ERROR;
            _statusCode = "400"; // Bad Request
            return;
        }
		_protocol = "HTTP/0.9";
		_extractQueryFromURL();
		_reqState = REQ_DONE;
		return;
	}
	_URL = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	_protocol = line.substr(secondSpace + 1);
	if (_protocol != "HTTP/1.0")
	{
		_reqState = REQ_ERROR;
		_statusCode = "505"; // HTTP Version Not Supported
		return;
	}
	if (_URL.empty() || _URL[0] != '/')
	{
		_reqState = REQ_ERROR;
		_statusCode = "400"; // Bad Request
		return;
	}
	_extractQueryFromURL();
}

void Request::_extractQueryFromURL()
{
	size_t queryPos = _URL.find('?');

	if (queryPos != std::string::npos)
	{
		_query = _URL.substr(queryPos + 1);
		_URL = _URL.substr(0, queryPos);
	}
}

size_t Request::parseHeaders(const std::string& rawBuffer)
{
	size_t headerEnd = rawBuffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos)
		return 0;

	std::string headerSection = rawBuffer.substr(0, headerEnd);
	size_t lineStart = 0;
	bool firstLine = true;

	while (lineStart < headerSection.size())
	{
		size_t lineEnd = headerSection.find("\r\n", lineStart);
		if (lineEnd == std::string::npos)
			lineEnd = headerSection.size();

		std::string line = headerSection.substr(lineStart, lineEnd - lineStart);
		if (!line.empty())
		{
			if (firstLine)
			{
				_parseRequestLine(line);
				firstLine = false;
			}
			else
				_parseHeaderLine(line);
		}
		lineStart = lineEnd + 2;
	}
	return headerEnd + 4;
}

void Request::_typeOfReq(const std::string& line)
{
    // determine body transfer mode from the parsed headers
	std::string transferEncoding = getHeader("Transfer-Encoding");
	std::string contentLengthStr = getHeader("Content-Length");

	std::string teLower = transferEncoding;
    for (size_t i = 0; i < teLower.size(); ++i)
        teLower[i] = std::tolower(static_cast<unsigned char>(teLower[i]));
	if (teLower.find("chunked") != std::string::npos)
	{
		_contentLength = -1;
		_reqState = REQ_CHUNKED;
	}
	else if (!contentLengthStr.empty())
	{
		char* endPtr = NULL;
		long long cl = strtoll(contentLengthStr.c_str(), &endPtr, 10);
		if (*endPtr != '\0' || cl < 0)
		{
			_reqState = REQ_ERROR;
			_statusCode = "400";
			return ;
		}
		_contentLength = cl;
		if (_maxBodySize > 0 && static_cast<size_t>(_contentLength) > _maxBodySize)
		{
			_reqState = REQ_ERROR;
			_statusCode = "413";
			return ;
		}
		_reqState = (_contentLength == 0) ? REQ_DONE : REQ_BODY;
	}
	else
	{
		_contentLength = 0;
		_reqState = REQ_DONE;
	}
}

std::string trim(const std::string& s) {
    int start = 0;
    int end = static_cast<int>(s.size()) - 1;
    while (start <= end && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    while (end >= start && std::isspace(static_cast<unsigned char>(s[end]))) {
        --end;
    }
    if (start >= end) {
        return "";
    }
    return s.substr(start, end - start + 1);
}

std::string Request::getHeader(const std::string& key) const
{
	if (_headers.count(key))
        return _headers.find(key)->second;
	return "";
}

void Request::_parseHeaderLine(const std::string& line)
{
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos)
		return;

	std::string key = trim(line.substr(0, colonPos));
	std::string value = trim(line.substr(colonPos + 1));

	if (!key.empty())
		_headers[key] = value;
}
