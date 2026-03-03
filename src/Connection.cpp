#include "../includes/Connection.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>

// --- Canonical Form ---

Connection::Connection()
	: _acceptFD(-1),
	  _lastActivity(time(NULL)),
	  _serverConf(NULL),
	  _locationConf(NULL),
	  _readBufferSize(MAX_HEADER_SIZE),
	  _request(NULL),
	  _response(NULL),
	  _writeBufferSize(0),
	  _state(READING),
	  _totalBytesRead(0)
{
	std::memset(&_IPA, 0, sizeof(_IPA));
	_request = new Request(0);
	_response = new Response();
}

Connection::Connection(int fd, const struct sockaddr_in& ipa, const ServerConf* defaultConfig)
	: _acceptFD(fd),
	  _IPA(ipa),
	  _lastActivity(time(NULL)),
	  _serverConf(defaultConfig),
	  _locationConf(NULL),
	  _readBufferSize(MAX_HEADER_SIZE),
	  _request(NULL),
	  _response(NULL),
	  _writeBufferSize(0),
	  _state(READING),
	  _totalBytesRead(0)
{
	long long maxBody = 0;
	if (_serverConf)
		maxBody = static_cast<long long>(_serverConf->getMaxBodySize());
	_request = new Request(maxBody);
	_response = new Response();
}

Connection::Connection(const Connection& other)
	: _acceptFD(other._acceptFD),
	  _IPA(other._IPA),
	  _lastActivity(other._lastActivity),
	  _serverConf(other._serverConf),
	  _locationConf(other._locationConf),
	  _readBufferSize(other._readBufferSize),
	  _readBuffer(other._readBuffer),
	  _request(NULL),
	  _response(NULL),
	  _writeBufferSize(other._writeBufferSize),
	  _writeBuffer(other._writeBuffer),
	  _state(other._state),
	  _totalBytesRead(other._totalBytesRead)
{
	_request = new Request(*other._request);
	_response = new Response(*other._response);
}

Connection& Connection::operator=(const Connection& other)
{
	if (this != &other)
	{
		_acceptFD = other._acceptFD;
		_IPA = other._IPA;
		_lastActivity = other._lastActivity;
		_serverConf = other._serverConf;
		_locationConf = other._locationConf;
		_readBufferSize = other._readBufferSize;
		_readBuffer = other._readBuffer;
		_writeBufferSize = other._writeBufferSize;
		_writeBuffer = other._writeBuffer;
		_state = other._state;
		_totalBytesRead = other._totalBytesRead;

		*_request = *other._request;
		*_response = *other._response;
	}
	return *this;
}

Connection::~Connection()
{
	delete _request;
	delete _response;
}

// --- State Machine Actions ---

void Connection::handleRead()
{
	if (_state != READING)
		return;

	char buf[MAX_HEADER_SIZE];
	ssize_t n = recv(_acceptFD, buf, sizeof(buf), 0);
	if (n <= 0)
	{
		if (n < 0)
			std::cerr << "recv error on client " << inet_ntoa(_IPA.sin_addr) <<
					": " << strerror(errno) << std::endl;
		_state = FINISHED;
		return;
	}

	_updateActivityTimer();
	_totalBytesRead += static_cast<size_t>(n);

	ReqState rState = _request->getReqState();
	size_t len = static_cast<size_t>(n);

	if (rState == REQ_HEADERS)
		_readHeaders(buf, len);
	else if (rState == REQ_BODY)
		_readBody(buf, len);
	else if (rState == REQ_CHUNKED)
		_readChunked(buf, len);
}

void Connection::_readHeaders(const char* buf, size_t n)
{
	_readBuffer.append(buf, n);
	if (_readBuffer.size() > MAX_HEADER_SIZE)
	{
		triggerError(431); // Request header too large.
		return;
	}

	size_t headerEnd = _request->parseHeaders(_readBuffer);
	if (_request->getReqState() == REQ_ERROR)
	{
		triggerError(400);
		return;
	}
	if (_request->getReqState() == REQ_HEADERS)
		return;

	//saving body data that made its way into the buffer.
	std::string leftover = _readBuffer.substr(headerEnd);
	_readBuffer.clear();

	ReqState rState = _request->getReqState();

	if (rState == REQ_DONE)
	{
		_state = PROCESSING;
		return;
	}

	if (!leftover.empty())
	{
		_request->getBodyStore().append(leftover);
		if (rState == REQ_CHUNKED && _request->isChunkedDone(leftover))
		{
			_state = PROCESSING;
			return;
		}
		if (rState == REQ_BODY && _request->getBodyStore().getSize() >= static_cast<size_t>(_request->getContentLength()))
		{
			_state = PROCESSING;
			return;
		}
	}
}

void Connection::_readBody(const char* buf, size_t n)
{
	_request->getBodyStore().append(buf, n);
	if (_request->getBodyStore().getSize() >= static_cast<size_t>(_request->getContentLength()))
		_state = PROCESSING;
}

void Connection::_readChunked(const char* buf, size_t n)
{
	std::string chunk(buf, n);
	_request->getBodyStore().append(chunk);
	if (_request->isChunkedDone(chunk))
		_state = PROCESSING;
}
