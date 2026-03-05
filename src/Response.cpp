#include "../includes/Response.hpp"
#include "../includes/LocationConf.hpp"
#include "../includes/CGIManager.hpp"

#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>

static const size_t RESPONSE_SEND_CHUNK = 16384;

namespace {

const LocationConf* matchLocation(const std::string& url, const ServerConf& config)
{
    const std::vector<LocationConf>& locations = config.getLocations();
    const LocationConf* best = NULL;
    size_t longestMatch = 0;

    for (size_t i = 0; i < locations.size(); ++i) 
    {
        const std::string& lpath = locations[i].getPath();
        bool matches = false;

        if (lpath == "/") 
            matches = true; 
        else if (url == lpath)
            matches = true;
        else if (url.size() > lpath.size() && url[lpath.size()] == '/')
            matches = (url.substr(0, lpath.size()) == lpath);

        if (matches && lpath.size() >= longestMatch) 
        {
            longestMatch = lpath.size();
            best = &locations[i];
        }
    }
    return best;
}

std::string detectContentType(const std::string& path) 
{
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos)
        return "application/octet-stream";

    std::string ext = path.substr(dotPos + 1);
    for (size_t i = 0; i < ext.size(); ++i)
        ext[i] = static_cast<char>(tolower(static_cast<unsigned char>(ext[i])));

    if (ext == "html" || ext == "htm")
        return "text/html";
    if (ext == "css")                  
        return "text/css";
    if (ext == "js")                   
        return "application/javascript";
    if (ext == "json")                 
        return "application/json";
    if (ext == "xml")                  
        return "application/xml";
    if (ext == "txt")                  
        return "text/plain";
    if (ext == "png")                  
        return "image/png";
    if (ext == "jpg" || ext == "jpeg") 
        return "image/jpeg";
    if (ext == "gif")                  
        return "image/gif";
    if (ext == "svg")                  
        return "image/svg+xml";
    if (ext == "ico")                  
        return "image/x-icon";
    if (ext == "pdf")                  
        return "application/pdf";
    if (ext == "zip")                  
        return "application/zip";
    return "application/octet-stream";
}

std::string sizeToString(size_t n) {
    std::ostringstream ss;
    ss << n;
    return ss.str();
}

std::string currentHttpDate() {
    time_t now = time(NULL);
    struct tm* gmt = gmtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmt);
    return std::string(buf);
}

std::string buildAutoIndexPage(const std::string& url, const std::string& dirPath) 
{
    DIR* dir = opendir(dirPath.c_str());
    if (!dir)
        return "";

    std::string html;
    html  = "<!DOCTYPE html>\r\n<html>\r\n<head><meta charset=\"utf-8\"><title>Index of ";
    html += url;
    html += "</title></head>\r\n<body>\r\n<h1>Index of ";
    html += url;
    html += "</h1>\r\n<hr>\r\n<pre>\r\n";

    if (url != "/")
        html += "<a href=\"../\">../</a>\r\n";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) 
    {
        std::string name(entry->d_name);
        if (name == "." || name == "..")
            continue;

        std::string fullPath = dirPath + "/" + name;
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            name += "/";
        html += "<a href=\"" + name + "\">" + name + "</a>\r\n";
    }
    closedir(dir);
    html += "</pre>\r\n<hr>\r\n</body>\r\n</html>";
    return html;
}

std::string buildDefaultErrorHtml(const std::string& code, const std::string& phrase) 
{
    std::string html;
    html  = "<!DOCTYPE html>\r\n<html>\r\n<head><meta charset=\"utf-8\"><title>";
    html += code + " " + phrase;
    html += "</title></head>\r\n<body>\r\n<h1>";
    html += code + " " + phrase;
    html += "</h1>\r\n<p>LeftHookRoll/1.0</p>\r\n</body>\r\n</html>";
    return html;
}

bool isPathSafe(const std::string& root, const std::string& resolved) 
{
    if (resolved.size() < root.size())
        return false;
    return resolved.substr(0, root.size()) == root;
}

std::string extractFilenameFromUrl(const std::string& url) 
{
    size_t pos = url.rfind('/');
    if (pos == std::string::npos || pos == url.size() - 1)
        return "";
    return url.substr(pos + 1);
}

}

Response::Response()
    : _statusCode("200"),
      _version("HTTP/1.1"),
      _response_phrase("OK"),
      _writeBufferSize(RESPONSE_SEND_CHUNK),
      _responseDataStore(),
      _totalBytesSent(0),
      _headers(),
      _fileFd(-1),
      _fileSize(0),
      _streamBuf(),
      _streamBufLen(0),
      _streamBufSent(0),
      _cgiInstance(NULL),
      _currentChunkSize(0),
      _responseState(SENDING_RES_HEAD),
      _headerBuffer()
{}

Response::Response(const Response& other)
    : _statusCode(other._statusCode),
      _version(other._version),
      _response_phrase(other._response_phrase),
      _writeBufferSize(other._writeBufferSize),
      _responseDataStore(other._responseDataStore),
      _totalBytesSent(other._totalBytesSent),
      _headers(other._headers),
      _fileFd(-1),
      _fileSize(other._fileSize),
      _streamBuf(other._streamBuf),
      _streamBufLen(other._streamBufLen),
      _streamBufSent(other._streamBufSent),
      _cgiInstance(NULL),
      _currentChunkSize(other._currentChunkSize),
      _responseState(other._responseState),
      _headerBuffer(other._headerBuffer)
{}

Response& Response::operator=(const Response& other) {
    if (this != &other) {
        _statusCode        = other._statusCode;
        _version           = other._version;
        _response_phrase   = other._response_phrase;
        _writeBufferSize   = other._writeBufferSize;
        _responseDataStore = other._responseDataStore;
        _totalBytesSent    = other._totalBytesSent;
        _headers           = other._headers;
        if (_fileFd != -1)
        {
            close(_fileFd);
            _fileFd = -1;
        }
        _fileSize      = other._fileSize;
        _streamBuf     = other._streamBuf;
        _streamBufLen  = other._streamBufLen;
        _streamBufSent = other._streamBufSent;
        _currentChunkSize = other._currentChunkSize;
        _responseState    = other._responseState;
        _headerBuffer     = other._headerBuffer;
        delete _cgiInstance;
        _cgiInstance = NULL;
    }
    return *this;
}

Response::~Response() {
    if (_fileFd != -1)
        close(_fileFd);
    delete _cgiInstance;
}


void Response::buildResponse(Request& req, const ServerConf& config) 
{
    if (req.getStatusCode() != "200") 
    {
        buildErrorPage(req.getStatusCode(), config);
        return;
    }

    const LocationConf* loc = matchLocation(req.getURL(), config);
    if (!loc) 
    {
        buildErrorPage("404", config);
        return;
    }

    if (!loc->isMethodAllowed(req.getMethod())) 
    {
        buildErrorPage("405", config);
        return;
    }

    if (!loc->getReturnCode().empty()) 
    {
        _statusCode      = loc->getReturnCode();
        _response_phrase = _lookupReasonPhrase(_statusCode);
        if (!loc->getReturnURL().empty())
            addHeader("Location", loc->getReturnURL());
        addHeader("Content-Length", "0");
        addHeader("Date", currentHttpDate());
        addHeader("Connection", "close");
        _headerBuffer  = _generateHeaderString();
        _responseState = SENDING_RES_HEAD;
        return;
    }

    if (req.getMethod() == GET)
        _handleGet(req, *loc, config);
    else if (req.getMethod() == POST)
        _handlePost(req, *loc, config);
    else if (req.getMethod() == DELETE)
        _handleDelete(req, *loc, config);
    else
        buildErrorPage("501", config);
}

void Response::buildErrorPage(const std::string& code, const ServerConf& config) 
{
    _responseDataStore.clear();
    _totalBytesSent = 0;
    _headers.clear();
    _headerBuffer.clear();
    if (_fileFd != -1)
    {
        close(_fileFd);
        _fileFd = -1;
    }
    _fileSize     = 0;
    _streamBufLen = 0;
    _streamBufSent = 0;

    _statusCode      = code;
    _response_phrase = _lookupReasonPhrase(code);

    std::string customPath = config.getErrorPagePath(code);
    if (!customPath.empty()) 
    {
        int fd = open(customPath.c_str(), O_RDONLY);
        if (fd >= 0) 
        {
            char buf[4096];
            ssize_t n;
            while ((n = read(fd, buf, sizeof(buf))) > 0)
                _responseDataStore.append(buf, static_cast<size_t>(n));
            close(fd);
            _finalizeSuccess("text/html");
            return;
        }
    }

    std::string body = buildDefaultErrorHtml(code, _response_phrase);
    _responseDataStore.append(body);
    _finalizeSuccess("text/html");
}

bool Response::sendSlice(int fd) 
{
    if (_responseState == SENDING_RES_HEAD) 
    {
        size_t headerSize = _headerBuffer.size();
        size_t remaining  = headerSize - _totalBytesSent;
        size_t toSend     = std::min(remaining, _writeBufferSize);

        ssize_t sent = send(fd, _headerBuffer.c_str() + _totalBytesSent, toSend, MSG_DONTWAIT);
        if (sent < 0) 
        {
            return true;
        }
        _totalBytesSent += static_cast<size_t>(sent);

        if (_totalBytesSent < headerSize)
            return false;

        if (_responseDataStore.getSize() == 0 && _fileFd == -1)
            return true;

        _responseState = SENDING_BODY_STATIC;
    }

    if (_responseState == SENDING_BODY_STATIC) 
    {
        if (_fileFd != -1)
        {
            if (_streamBufLen == 0)
            {
                _streamBuf.resize(_writeBufferSize);
                ssize_t bytesRead = read(_fileFd, &_streamBuf[0], _streamBuf.size());
                if (bytesRead <= 0)
                {
                    close(_fileFd);
                    _fileFd = -1;
                    return true;
                }
                _streamBufLen  = static_cast<size_t>(bytesRead);
                _streamBufSent = 0;
            }

            ssize_t sent = send(fd, &_streamBuf[0] + _streamBufSent,
                                _streamBufLen - _streamBufSent, MSG_DONTWAIT);
            if (sent < 0)
            {
                close(_fileFd);
                _fileFd = -1;
                return true;
            }
            _streamBufSent  += static_cast<size_t>(sent);
            _totalBytesSent += static_cast<size_t>(sent);

            if (_streamBufSent == _streamBufLen)
                _streamBufLen = 0;

            if (_totalBytesSent - _headerBuffer.size() >= _fileSize)
            {
                close(_fileFd);
                _fileFd = -1;
                return true;
            }
            return false;
        }

        size_t bodyOffset    = _totalBytesSent - _headerBuffer.size();
        size_t bodySize      = _responseDataStore.getSize();
        size_t bodyRemaining = bodySize - bodyOffset;

        if (bodyRemaining == 0)
            return true;

        size_t toSend = std::min(bodyRemaining, _writeBufferSize);
        ssize_t sent  = 0;

        if (_responseDataStore.getMode() == RAM) 
        {
            const std::vector<char>& vec = _responseDataStore.getVector();
            sent = send(fd, &vec[0] + bodyOffset, toSend, MSG_DONTWAIT);
        }
        else 
        {
            int storeFd = _responseDataStore.getFd();
            if (lseek(storeFd, static_cast<off_t>(bodyOffset), SEEK_SET) < 0)
                return true;

            char buf[RESPONSE_SEND_CHUNK];
            ssize_t bytesRead = read(storeFd, buf, toSend);
            if (bytesRead <= 0)
                return true;

            sent = send(fd, buf, static_cast<size_t>(bytesRead), MSG_DONTWAIT);
        }

        if (sent < 0) 
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return false;
            return true;
        }
        _totalBytesSent += static_cast<size_t>(sent);

        if (_totalBytesSent == _headerBuffer.size() + bodySize)
            return true;

        return false;
    }

    return false;
}


void Response::_handleGet(const Request& req, const LocationConf& loc, const ServerConf& config) 
{
    const std::string& root = loc.getRoot();
    const std::string  url  = req.getURL();

    std::string resolvedPath = root + url;
    if (resolvedPath.size() > 1 && resolvedPath[resolvedPath.size() - 1] == '/')
        resolvedPath = resolvedPath.substr(0, resolvedPath.size() - 1);

    if (!isPathSafe(root, resolvedPath)) 
    {
        buildErrorPage("400", config);
        return;
    }

    struct stat st;
    if (stat(resolvedPath.c_str(), &st) != 0) 
    {
        buildErrorPage("404", config);
        return;
    }

    if (S_ISDIR(st.st_mode)) 
    {
        if (!loc.getDefaultPage().empty()) 
        {
            std::string indexPath = resolvedPath + "/" + loc.getDefaultPage();
            struct stat ist;
            if (stat(indexPath.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) 
            {
                _serveFile(indexPath, config);
                return;
            }
        }

        if (loc.getAutoIndex()) 
        {
            std::string dirUrl = url;
            if (dirUrl.empty() || dirUrl[dirUrl.size() - 1] != '/')
                dirUrl += '/';

            std::string listing = buildAutoIndexPage(dirUrl, resolvedPath);
            if (listing.empty()) 
            {
                buildErrorPage("500", config);
                return;
            }
            _responseDataStore.append(listing);
            _statusCode      = "200";
            _response_phrase = "OK";
            _finalizeSuccess("text/html");
            return;
        }

        buildErrorPage("403", config);
        return;
    }

    if (!S_ISREG(st.st_mode)) 
    {
        buildErrorPage("403", config);
        return;
    }

    _serveFile(resolvedPath, config);
}

void Response::_handlePost(Request& req, const LocationConf& loc, const ServerConf& config) {
    const std::string& storageDir = loc.getStorageLocation();

    if (storageDir.empty()) 
    {
        buildErrorPage("501", config);
        return;
    }

    std::string filename = extractFilenameFromUrl(req.getURL());
    if (filename.empty()) 
    {
        std::ostringstream ss;
        ss << "upload_" << time(NULL);
        filename = ss.str();
    }

    std::string destPath = storageDir + "/" + filename;

    int outFd = open(destPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outFd < 0) 
    {
        buildErrorPage("500", config);
        return;
    }

    DataStore& body = req.getBodyStore();
    if (body.getMode() == RAM) 
    {
        const std::vector<char>& vec = body.getVector();
        if (!vec.empty()) 
        {
            ssize_t written = write(outFd, &vec[0], vec.size());
            if (written < 0 || static_cast<size_t>(written) != vec.size()) 
            {
                close(outFd);
                buildErrorPage("500", config);
                return;
            }
        }
    } 
    else 
    {
        int bodyFd = body.getFd();
        if (lseek(bodyFd, 0, SEEK_SET) < 0) 
        {
            close(outFd);
            buildErrorPage("500", config);
            return;
        }
        char buf[4096];
        ssize_t n;
        while ((n = read(bodyFd, buf, sizeof(buf))) > 0) 
        {
            if (write(outFd, buf, static_cast<size_t>(n)) != n) 
            {
                close(outFd);
                buildErrorPage("500", config);
                return;
            }
        }
        if (n < 0) 
        {
            close(outFd);
            buildErrorPage("500", config);
            return;
        }
    }
    close(outFd);

    std::string responseBody = "<!DOCTYPE html>\r\n<html><body><p>File uploaded successfully.</p></body></html>";
    _responseDataStore.append(responseBody);
    _statusCode      = "201";
    _response_phrase = "Created";
    addHeader("Location", "/" + filename);
    _finalizeSuccess("text/html");
}

void Response::_handleDelete(const Request& req, const LocationConf& loc, const ServerConf& config) 
{
    const std::string& root = loc.getRoot();
    const std::string  url  = req.getURL();

    std::string resolvedPath = root + url;
    if (resolvedPath.size() > 1 && resolvedPath[resolvedPath.size() - 1] == '/')
        resolvedPath = resolvedPath.substr(0, resolvedPath.size() - 1);

    if (!isPathSafe(root, resolvedPath)) 
    {
        buildErrorPage("400", config);
        return;
    }

    struct stat st;
    if (stat(resolvedPath.c_str(), &st) != 0) 
    {
        buildErrorPage("404", config);
        return;
    }

    if (!S_ISREG(st.st_mode)) 
    {
        buildErrorPage("403", config);
        return;
    }

    if (unlink(resolvedPath.c_str()) != 0) 
    {
        if (errno == EACCES || errno == EPERM)
            buildErrorPage("403", config);
        else
            buildErrorPage("500", config);
        return;
    }

    _statusCode      = "204";
    _response_phrase = "No Content";
    addHeader("Content-Length", "0");
    addHeader("Date", currentHttpDate());
    addHeader("Connection", "close");
    _headerBuffer  = _generateHeaderString();
    _responseState = SENDING_RES_HEAD;
}

void Response::_finalizeSuccess(const std::string& contentType) 
{
    addHeader("Content-Type", contentType);
    addHeader("Content-Length", sizeToString(_responseDataStore.getSize()));
    addHeader("Date", currentHttpDate());
    addHeader("Connection", "close");
    _headerBuffer  = _generateHeaderString();
    _responseState = SENDING_RES_HEAD;
}

void Response::_serveFile(const std::string& path, const ServerConf& config) 
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0)
    {
        buildErrorPage("404", config);
        return;
    }

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        buildErrorPage("404", config);
        return;
    }

    _fileFd        = fd;
    _fileSize      = static_cast<size_t>(st.st_size);
    _streamBufLen  = 0;
    _streamBufSent = 0;
    _streamBuf.resize(_writeBufferSize);

    _statusCode      = "200";
    _response_phrase = "OK";
    addHeader("Content-Type", detectContentType(path));
    addHeader("Content-Length", sizeToString(_fileSize));
    addHeader("Date", currentHttpDate());
    addHeader("Connection", "close");
    _headerBuffer  = _generateHeaderString();
    _responseState = SENDING_RES_HEAD;
}


const std::string& Response::getStatusCode() const      { return _statusCode; }
const std::string& Response::getVersion() const         { return _version; }
const std::string& Response::getResponsePhrase() const  { return _response_phrase; }
ResponseState      Response::getResponseState() const   { return _responseState; }

void Response::setStatusCode(const std::string& code) 
{
    _statusCode      = code;
    _response_phrase = _lookupReasonPhrase(code);
}

void Response::setResponsePhrase(const std::string& phrase) 
{
    _response_phrase = phrase;
}

void Response::addHeader(const std::string& key, const std::string& value)
{
    _headers[key] = value;
}


std::string Response::_generateHeaderString() 
{
    std::string result;
    result += _version + " " + _statusCode + " " + _response_phrase + "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
         it != _headers.end(); ++it) 
    {
        result += it->first + ": " + it->second + "\r\n";
    }
    result += "\r\n";
    return result;
}

std::string Response::_lookupReasonPhrase(const std::string& code) {
    if (code == "200") 
        return "OK";
    if (code == "201") 
        return "Created";
    if (code == "204") 
        return "No Content";
    if (code == "301") 
        return "Moved Permanently";
    if (code == "302") 
        return "Found";
    if (code == "303") 
        return "See Other";
    if (code == "307") 
        return "Temporary Redirect";
    if (code == "308") 
        return "Permanent Redirect";
    if (code == "400") 
        return "Bad Request";
    if (code == "403") 
        return "Forbidden";
    if (code == "404") 
        return "Not Found";
    if (code == "405") 
        return "Method Not Allowed";
    if (code == "408") 
        return "Request Timeout";
    if (code == "411") 
        return "Length Required";
    if (code == "413") 
        return "Payload Too Large";
    if (code == "414") 
        return "URI Too Long";
    if (code == "500") 
        return "Internal Server Error";
    if (code == "501") 
        return "Not Implemented";
    if (code == "502") 
        return "Bad Gateway";
    if (code == "503") 
        return "Service Unavailable";
    if (code == "504") 
        return "Gateway Timeout";
    return "Unknown";
}
