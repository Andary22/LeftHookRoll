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
