#include "../includes/Connection.hpp"
#include "../includes/ServerConf.hpp"
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include "../includes/FatalExceptions.hpp"

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

// --- Getters & Setters ---
int Connection::getFd() const { return _acceptFD; }
ConnectionState Connection::getState() const { return _state; }
void Connection::setState(ConnectionState state) { _state = state; }
Response* Connection::getResponse() const { return _response; }
Request* Connection::getRequest() const { return _request; }
const ServerConf* Connection::getServerConf() const { return _serverConf; }
void Connection::setLocationConf(const LocationConf* conf) { _locationConf = conf; }

int Connection::getCgiPipeFd() const
{
	if (_response && _response->getCgiOutputFd() >= 0)
		return _response->getCgiOutputFd();
	return -1;
}
// --- Private Helpers ---
void Connection::_updateActivityTimer()
{
	_lastActivity = time(NULL);
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
		triggerError(std::atoi(_request->getStatusCode().c_str()));
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
			std::cerr << "recv error on client " << req_utils::ipv4ToString(_IPA) <<
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

void Connection::process()
{
	if (_state != PROCESSING)
		return;

	_updateActivityTimer();

	try
	{
		if (_request->getReqState() == REQ_CHUNKED)
		{
			if (!_request->processBodySlice())
				return; // still decoding chunked encoding.
		}
		//if anything went wrong at all;recheck that 200 is indeed the default in yaman's implemenation.
		if (_request->getStatusCode() != "200")
		{
			triggerError(std::atoi(_request->getStatusCode().c_str()));
			return;
		}

		if (_serverConf)
		{
			if (!_response->buildResponse(*_request, *_serverConf))
			{
				// Check if this is a CGI request that needs pipe monitoring
				if (_response->getBuildPhase() == BUILD_CGI_RUNNING)
				{
					_state = WAITING_FOR_CGI;
					return;
				}
				return; // still in round-robin (e.g. POST writing)
			}
		}
		else
		{
			//noconf fallback
			triggerError(500);
			return;
		}
		_state = WRITING;
	}
	catch (const ClientException& e)
	{
		std::cerr << "client runtime error on fd " << _acceptFD << ": " << e.what() << std::endl;
		triggerError(e.getStatusCode());
	}
	catch (const FatalException&)
	{
		throw;
	}
	catch (const std::exception& e)
	{
		std::cerr << "unexpected runtime error on fd " << _acceptFD << ": " << e.what() << std::endl;
		triggerError(500);
	}
}

void Connection::handleWrite()
{
	if (_state != WRITING)
		return;
	_updateActivityTimer();
	if (_response->sendSlice(_acceptFD))
		_state = FINISHED;
}

// --- Error & Timeout Management ---

bool Connection::hasTimedOut(int timeoutSeconds) const
{
	return (time(NULL) - _lastActivity) >= timeoutSeconds;
}

void Connection::triggerError(int statusCode)
{
	std::ostringstream oss;
	oss << statusCode;

	if (_serverConf)
		_response->buildErrorPage(oss.str(), *_serverConf);
	else
	{
		// noconf fallback
		_response->setStatusCode(oss.str());
		_response->setResponsePhrase("Error");
	}

	_state = WRITING;
}