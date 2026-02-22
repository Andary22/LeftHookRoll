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
#include <map>

class CGIManager;

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

class Response {
public:
	// Canonical Form
	Response();
	Response(const Response& other);
	Response& operator=(const Response& other);
	~Response();

	//  Core Behavior
	/**
	 * @brief Analyzes the Request and Location settings to prepare the response.
	 * Sets the status code, phrase, and loads the DataStore with content.
	 */
	void buildResponse(Request& req, const ServerConf& config);

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

	//  Getters & Setters

	const std::string&	  getStatusCode() const;
	const std::string&	  getVersion() const;
	const std::string&	  getResponsePhrase() const;
	ResponseState		   getResponseState() const;

	void setStatusCode(const std::string& code);
	void setResponsePhrase(const std::string& phrase);

	/**
	* @brief Adds a header to the response (e.g., "Content-Type", "text/html").
	*/
	void addHeader(const std::string& key, const std::string& value);

private:
	//  Identity
	std::string			 _statusCode;	  // e.g., "200"
	std::string			 _version;		 // e.g., "HTTP/1.1"
	std::string			 _response_phrase; // e.g., "OK"

	//  Data
	size_t								_writeBufferSize; // Max bytes to send per send() call
	DataStore							_responseDataStore;
	size_t								_totalBytesSent;  // Tracks progress through the DataStore
	std::map<std::string, std::string>	_headers;
	// CGI
	CGIManager*				_cgiInstance;
	size_t					_currentChunkSize;

	//  State Management
	ResponseState	_responseState;

	//  Private Helpers
	/**
	 * @brief Formulates the header string based on headers map.
	 */
	std::string _generateHeaderString();

	/**
	 * @brief Looks up the standard Reason Phrase for a status code.
	 */
	std::string _lookupReasonPhrase(const std::string& code);
};