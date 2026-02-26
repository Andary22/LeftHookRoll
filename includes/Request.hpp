/**
 * @file Request.hpp
 * @brief Parses and stores the entire HTTP request entity (headers, body).
 * Utilizes a State Machine to handle non-blocking, fragmented data streams.
 */
#pragma once

#include <string>
#include <map>
#include <sys/types.h>
#include "AllowedMethods.hpp"
#include "DataStore.hpp"

//like a time slice per parse iter, but in bytes 🤯😲
#define PARSE_BYTE_SLICE 8192

/**
 * @enum ReqState
 * @brief Represents the current network-reading phase of the HTTP request.
 */
enum ReqState
{
	REQ_HEADERS,	// Waiting for \r\n\r\n and parsing Request-Line & Headers
	REQ_BODY,	 	// Headers parsed, waiting for Content-Length bytes
	REQ_CHUNKED,	// Headers parsed, accumulating raw chunks until 0\r\n\r\n
	REQ_DONE,		// The entire request has been successfully received from the socket(often times for HTTP/0.9)
	REQ_ERROR		// A parsing error occurred
};

class Request {
public:
	// Canonical Form
	Request();
	Request(long long _maxBodySize);
	Request(const Request& other);
	Request& operator=(const Request& other);
	~Request();

	//  Core Parsing Behavior

	/**
	 * @brief Main entry point to parse the raw buffer from the Connection.
	 * Handles the entire state machine for parsing headers and body.
	 * @param rawBuffer The string buffer accumulated by the Connection.
	 * @return void. Updates internal state and data members accordingly.
	 */
	void parse(const std::string& rawBuffer);

	/**
	 * @brief Parses the raw buffer containing the HTTP Request-Line and Headers.
	 * Transitions state to REQ_BODY, REQ_CHUNKED, or REQ_DONE upon finding \r\n\r\n.
	 * @param rawBuffer The string buffer accumulated by the Connection.
	 * @return size_t The index at which the headers end (after \r\n\r\n).
	 * Useful for slicing leftover body data that recv() accidentally grabbed.
	 */
	size_t parseHeaders(const std::string& rawBuffer);

	/**
	 * @brief Checks if the incoming chunked data contains the terminal "0\r\n\r\n".
	 * Used to transition from REQ_CHUNKED to REQ_DONE.
	 * @param newData The latest chunk of data received from the socket.
	 * @return true if the terminal chunk is found, false otherwise.
	 */
	bool isChunkedDone(const std::string& newData) const;

	/**
	 * @brief Unified method to prepare the body for the Response/CGI phase.
	 * For Content-Length bodies, this does nothing and returns true immediately.
	 * For Chunked bodies, it decodes PARSE_BYTE_SLICE bytes per call to prevent blocking.
	 * @return true if the body is fully clean and ready, false if it needs more processing loops.
	 */
	bool processBodySlice();

	//  Getters

	HTTPMethod									getMethod() const;
	const std::string&							getURL() const;
	const std::string&							getProtocol() const;
	const std::string&							getQuery() const;
	const std::map<std::string, std::string>&	getHeaders() const;
	/**
	 * @brief Gets a specific header value.
	 * @return The value, or empty string if not found.
	 */
	std::string									getHeader(const std::string& key) const;
	/**
	 * @brief Returns a reference to the DataStore to allow direct writing from recv().
	 */
	DataStore&									getBodyStore();

	//  State Management Getters

	ReqState									getReqState() const;
	std::string									getStatusCode() const;
	size_t										getMaxBytesToRead() const;
	size_t										getTotalBytesRead() const;
	bool										isComplete() const;

private:
	//  Identity
	HTTPMethod						_methodName;
	std::string						_URL;
	std::string						_protocol;
	std::string						_query;
	long	long					_contentLength; // -1 for chunked requests.
	//  Data
	DataStore						 	_body;
	std::map<std::string, std::string>	_headers;

	//  State Management
	ReqState						 _reqState;
	std::string						 _statusCode; // e.g., 200, 400, 413
	size_t							 _maxBodySize; // filled from server config.
	size_t							 _totalBytesRead;// used to track against _contentLength and _maxBodySize

	//  Chunk Decoding State (filled per chunk)
	size_t							  _chunkSize;
	size_t							  _chunkDecodeOffset;
	bool								_isBodyProcessed;   //set on \r\n\r\n

	//  Private Parsing Helpers
	void _parseRequestLine(const std::string& line);// parses the  METHOD  URI PROTOCOL line.
	void _parseHeaderLine(const std::string& line);// parses the key value
	void _typeOfReq(const std::string& line);// know what type of request is it, chenked or content length
	void _extractQueryFromURL();
};