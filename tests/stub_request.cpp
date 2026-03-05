#include "../includes/Request.hpp"
#include <sstream>

Request::Request()
    : _methodName(UNKNOWN_METHOD),
      _URL("/"),
      _protocol("HTTP/1.1"),
      _query(),
      _contentLength(0),
      _body(),
      _headers(),
      _reqState(REQ_DONE),
      _statusCode("200"),
      _maxBodySize(1024 * 1024),
      _totalBytesRead(0),
      _chunkSize(0),
      _chunkDecodeOffset(0),
      _isBodyProcessed(true)
{}

Request::Request(long long maxBodySize)
    : _methodName(UNKNOWN_METHOD),
      _URL("/"),
      _protocol("HTTP/1.1"),
      _query(),
      _contentLength(0),
      _body(),
      _headers(),
      _reqState(REQ_DONE),
      _statusCode("200"),
      _maxBodySize(static_cast<size_t>(maxBodySize)),
      _totalBytesRead(0),
      _chunkSize(0),
      _chunkDecodeOffset(0),
      _isBodyProcessed(true)
{}

Request::Request(const Request& other)
    : _methodName(other._methodName),
      _URL(other._URL),
      _protocol(other._protocol),
      _query(other._query),
      _contentLength(other._contentLength),
      _body(other._body),
      _headers(other._headers),
      _reqState(other._reqState),
      _statusCode(other._statusCode),
      _maxBodySize(other._maxBodySize),
      _totalBytesRead(other._totalBytesRead),
      _chunkSize(other._chunkSize),
      _chunkDecodeOffset(other._chunkDecodeOffset),
      _isBodyProcessed(other._isBodyProcessed)
{}

Request& Request::operator=(const Request& other) {
    if (this != &other) {
        _methodName        = other._methodName;
        _URL               = other._URL;
        _protocol          = other._protocol;
        _query             = other._query;
        _contentLength     = other._contentLength;
        _body              = other._body;
        _headers           = other._headers;
        _reqState          = other._reqState;
        _statusCode        = other._statusCode;
        _maxBodySize       = other._maxBodySize;
        _totalBytesRead    = other._totalBytesRead;
        _chunkSize         = other._chunkSize;
        _chunkDecodeOffset = other._chunkDecodeOffset;
        _isBodyProcessed   = other._isBodyProcessed;
    }
    return *this;
}

Request::~Request() {}

void Request::_extractQueryFromURL() {
    size_t qpos = _URL.find('?');
    if (qpos != std::string::npos) {
        _query = _URL.substr(qpos + 1);
        _URL   = _URL.substr(0, qpos);
    }
}

void Request::_parseRequestLine(const std::string& line) {
    size_t sp1 = line.find(' ');
    if (sp1 == std::string::npos) { _statusCode = "400"; _reqState = REQ_ERROR; return; }
    size_t sp2 = line.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) { _statusCode = "400"; _reqState = REQ_ERROR; return; }

    std::string method = line.substr(0, sp1);
    _URL      = line.substr(sp1 + 1, sp2 - sp1 - 1);
    _protocol = line.substr(sp2 + 1);

    _extractQueryFromURL();

    if      (method == "GET")    _methodName = GET;
    else if (method == "POST")   _methodName = POST;
    else if (method == "DELETE") _methodName = DELETE;
    else                         _methodName = UNKNOWN_METHOD;
}

void Request::_parseHeaderLine(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return;
    std::string key   = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    size_t start = value.find_first_not_of(" \t");
    if (start != std::string::npos)
        value = value.substr(start);
    _headers[key] = value;
}

size_t Request::parseHeaders(const std::string& rawBuffer) {
    size_t headerEnd = rawBuffer.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        _statusCode = "400";
        _reqState   = REQ_ERROR;
        return 0;
    }

    std::string headerSection = rawBuffer.substr(0, headerEnd);
    size_t lineStart = 0;
    size_t firstLine = headerSection.find("\r\n");

    _parseRequestLine(headerSection.substr(0, firstLine));
    if (_reqState == REQ_ERROR) return 0;

    lineStart = firstLine + 2;
    while (lineStart < headerSection.size()) {
        size_t lineEnd = headerSection.find("\r\n", lineStart);
        if (lineEnd == std::string::npos)
            lineEnd = headerSection.size();
        _parseHeaderLine(headerSection.substr(lineStart, lineEnd - lineStart));
        lineStart = lineEnd + 2;
    }

    _statusCode = "200";
    _reqState   = REQ_DONE;
    return headerEnd + 4;
}

bool Request::isChunkedDone(const std::string& newData) const {
    return newData.find("0\r\n\r\n") != std::string::npos;
}

bool Request::processBodySlice() { return true; }

HTTPMethod                                  Request::getMethod()       const { return _methodName; }
const std::string&                          Request::getURL()          const { return _URL; }
const std::string&                          Request::getProtocol()     const { return _protocol; }
const std::string&                          Request::getQuery()        const { return _query; }
const std::map<std::string, std::string>&   Request::getHeaders()      const { return _headers; }
DataStore&                                  Request::getBodyStore()          { return _body; }
ReqState                                    Request::getReqState()     const { return _reqState; }
std::string                                 Request::getStatusCode()   const { return _statusCode; }
size_t                                      Request::getMaxBytesToRead() const { return _maxBodySize; }
size_t                                      Request::getTotalBytesRead() const { return _totalBytesRead; }
bool                                        Request::isComplete()      const { return _reqState == REQ_DONE; }

std::string Request::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    return (it != _headers.end()) ? it->second : "";
}
