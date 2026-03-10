/**
 * @file Response.hpp
 * @brief Formulates the HTTP response and manages non-blocking data transmission.
 * * This class decides the course of action (uploading/creating a file,CGI,static message) and
 * manages the state of the outgoing data stream.
 */
#pragma once

#include <string>
#include <sys/types.h>
#include "DataStore.hpp"
#include "ServerConf.hpp"
#include "Request.hpp"
#include "CGIManager.hpp"
#include <map>


/**
 * @enum ResponseState
 * @brief Tracks the progress of sending the response to the client.
 */
enum ResponseState
{
	SENDING_RES_HEAD,		// Sending the Status-Line and Headers
	SENDING_BODY_STATIC,	// Sending a static file from the DataStore
	SENDING_BODY_CHUNKED	// Sending CGI output using Chunked Transfer Coding
};

/**
 * @enum BuildPhase
 * @brief Tracks the incremental construction of the response body.
 * exclusivly for POST.
 */
enum BuildPhase
{
	BUILD_IDLE,
	BUILD_POST_WRITING,
	BUILD_DONE
};

class Response {
public:
	Response();
	Response(const Response& other);
	Response& operator=(const Response& other);
	~Response();

	/**
	 * @brief Analyzes the Request and Location settings to prepare the response.
	 * Sets the status code, phrase, and loads the DataStore with content.
	 * @return true if the response is finished, false if not.
	 */
	bool buildResponse(Request& req, const ServerConf& config);

	/**
	 * @brief Fast-tracks the response to an error state.
	 * Loads the appropriate error page from config or default HTML.
	 * @param code The HTTP status code (e.g., "404", "500").
	 */
	void buildErrorPage(const std::string& code, const ServerConf& config);

	/**
	 * @brief Sends a slice of the response to the client socket.
	 * To be called during a POLLOUT event.
	 * @param fd The client socket file descriptor.
	 * @return true if the entire response is finished, false if more data remains.
	 */
	bool sendSlice(int fd);


	const std::string&	getStatusCode() const;
	const std::string&	getVersion() const;
	const std::string&	getResponsePhrase() const;
	ResponseState		getResponseState() const;

	void				setStatusCode(const std::string& code);
	void				setResponsePhrase(const std::string& phrase);

	/**
	* @brief Adds a header to the response (e.g., "Content-Type", "text/html").
	*/
	void addHeader(const std::string& key, const std::string& value);

private:
	std::string			 _statusCode;	  // e.g., "200"
	std::string			 _version;		 // e.g., "HTTP/1.1"
	std::string			 _response_phrase; // e.g., "OK"

	size_t								_writeBufferSize;
	DataStore							_responseDataStore;
	size_t								_totalBytesSent;
	std::map<std::string, std::string>	_headers;
	int									_fileFd;		  // Open FD for the file being streamed; -1 when not in use
	size_t								_fileSize;		// Total byte count from stat(); used for Content-Length and end detection
	std::vector<char>					_streamBuf;	   // Holds the latest chunk read from _fileFd, retained across EAGAIN
	size_t								_streamBufLen;	// How many bytes are currently valid in _streamBuf
	size_t								_streamBufSent;  // How many of those bytes have been sent to the socket so far
	CGIManager*							_cgiInstance;
	size_t								_currentChunkSize;

	// concurrent POST state.
	BuildPhase							_buildPhase;
	const ServerConf*					_cachedConfig;
	int									_postOutFd;
	size_t								_postWritePos;
	std::string							_postFilename;

	ResponseState	_responseState;

	//  Private Helpers
	std::string _generateHeaderString();
	std::string _lookupReasonPhrase(const std::string& code);

	void _handleGet(const Request& req, const LocationConf& loc, const ServerConf& config);
	bool _handlePost(Request& req, const LocationConf& loc, const ServerConf& config);
	bool _continuePostWrite(Request& req);
	void _handleDelete(const Request& req, const LocationConf& loc, const ServerConf& config);

	void _finalizeSuccess(const std::string& contentType);
	void _serveFile(const std::string& path, const ServerConf& config);

	bool _sendHeader(int fd);
	bool _sendBodyStatic(int fd);
	bool _sendBodyFile(int fd);
	bool _sendBodyDataStore(int fd);

	//  Serialized header line (Status-Line + Headers + blank line) cached after build
	std::string _headerBuffer;
};