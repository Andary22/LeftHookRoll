/**
 * @file Connection.hpp
 * @brief Will be spawned when we accept() a new connection, will handle reading from the socket, writing to the socket, and the state of the connection.
 */

#pragma once

#include <string>
#include <ctime>
#include <netinet/in.h>
#include <sys/types.h>

class Request;
class Response;
class ServerConf;
class LocationConf;

/**
 * @enum ConnectionState
 * @brief Represents the state of the CLIENT SOCKET.
 */
enum ConnectionState
{
	READING,			// Waiting for POLLIN on client socket. Accumulating request.
	WRITING,			// Waiting for POLLOUT on client socket. Draining the write buffer.
	PROCESSING,			// CPU-bound phase. Parsing, routing, checking permissions.
	WAITING_FOR_CGI,	// The socket is idle. We are waiting for the CGI pipe to give us data.
	FINISHED,			// Transaction complete. Ready to close socket.
};

class Connection
{
	public:
		// Canonical Form
		Connection();
		Connection(int fd, const struct sockaddr_in& ipa, const ServerConf* defaultConfig);
		Connection(const Connection& other);
		Connection& operator=(const Connection& other);
		~Connection();

		// State Machine Actions
		/**
		 * @brief Reads data from the client socket using recv() into _readBuffer.
		 * Transitions state to PROCESSING if the request is fully received.
		 */
		void handleRead();

		/**
		 * @brief Executes routing logic, instantiates the Response, and prepares data for sending.
		 * Transitions state to WRITING or handles CGI setup based on LocationConf.
		 */
		void process();

		/**
		 * @brief Writes data from the _writeBuffer (or Response) to the client socket using send().
		 * Transitions state to FINISHED when the transaction is completely sent.
		 */
		void handleWrite();

		// Error & Timeout Management
		/**
		 * @brief Checks if the connection has exceeded the defined timeout threshold.
		 * @param timeoutSeconds The maximum allowed idle time in seconds.
		 * @return true if timed out, false otherwise.
		 */
		bool hasTimedOut(int timeoutSeconds) const;

		/**
		 * @brief Forces the connection into an error state, bypassing normal processing.
		 * @param statusCode The HTTP status code to generate (e.g., 400, 408, 500).
		 */
		void triggerError(int statusCode);

		// Getters & Setters

		int				getFd() const;
		ConnectionState	getState() const;
		void			setState(ConnectionState state);
		Response*		getResponse() const;

		/**
		 * @brief Assigns the appropriate location block after parsing the request URI.
		 * @param conf The correctly matched location configuration.
		 */
		void setLocationConf(const LocationConf* conf);

	private:
		//  Identity
		int					 _acceptFD;
		struct sockaddr_in	  _IPA;
		time_t				  _lastActivity;

		//  Config
		const ServerConf* _serverConf;
		const LocationConf* _locationConf;

		//  Data
		size_t				  _readBufferSize;
		std::string			 _readBuffer;

		Request* _request;
		Response* _response;

		size_t				_writeBufferSize;
		std::string			_writeBuffer;

		//  Dynamic Data
		ConnectionState		 _state;
		size_t				  _totalBytesRead;

		//  Private Helpers
		/**
		 * @brief Updates the _lastActivity timestamp to current time.
		 */
		void _updateActivityTimer();
};