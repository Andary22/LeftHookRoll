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

