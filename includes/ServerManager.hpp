/**
 * @file ServerManager.hpp
 * @brief Owns the event loop and all listening sockets.
 * Accepts new connections, dispatches I/O events, and routes
 * each accepted fd to the correct ServerConf via getsockname().
 */

#pragma once

#include <map>
#include <set>
#include <deque>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "ServerConf.hpp"
#include "Connection.hpp"

#define BACKLOG 128
#define RECV_BUFFER_SIZE 4096// keep this smaller than read buffer size in Connection.!
#define EPOLL_TIMEOUT_MS 2500

/**
 * @struct SockAddrCompare
 * @brief Custom comparator for sockaddr_in to allow its use as a key in std::map.
 */
struct SockAddrCompare
{
	bool operator()(const struct sockaddr_in& lhs, const struct sockaddr_in& rhs) const
	{
		if (lhs.sin_addr.s_addr != rhs.sin_addr.s_addr)
			return lhs.sin_addr.s_addr < rhs.sin_addr.s_addr;
		return lhs.sin_port < rhs.sin_port;
	}
};

class ServerManager {
public:
	// Canonical Form
	ServerManager();
	ServerManager(std::vector<ServerConf> confs);// this will be the main constructer to use once we have parsing up and ready!
	ServerManager(const ServerManager& other);
	ServerManager& operator=(const ServerManager& other);
	~ServerManager();

	/**
	 * @brief Creates a listening socket for the given ServerConf's IP:Port and
	 * registers the mapping.
	 * @param conf Pointer to the configuration block (pointer used to avoid heavy copying).
	 * @throws FatalException if socket setup fails.
	 */
	void addServer(const ServerConf* conf);

	/**
	 * @brief Temporary overload for early development: creates a listening socket
	 * on the given port bound to INADDR_ANY, with no ServerConf mapping.
	 */
	void addListenPort(int port);

	/**
	 * @brief Enters the main epoll() event loop. Blocks until g_running becomes false.
	 */
	void run();

	/**
	 * @brief Adds or updates an fd in the epoll list (e.g., CGI output pipe).
	 * @param fd The file descriptor to monitor.
	 * @param events EPOLLIN/EPOLLOUT mask to apply.
	 */
	void addPollFd(int fd, uint32_t events);

	/**
	 * @brief Uses getsockname() to find the local address of a client fd,
	 * then returns the matching ServerConf.
	 * @param clientFd The file descriptor returned by accept().
	 * @return const ServerConf* Pointer to the matching configuration, or NULL if not found.
	 */
	const ServerConf* getServerConfForFd(int clientFd) const;

private:
	// conf to address mapping for quick lookup on accept():
	std::map<struct sockaddr_in, const ServerConf*, SockAddrCompare>	_interfacePortPairs;
	std::vector<ServerConf*>											_serverConfs;
	// Round-robin processing scheduler
	std::deque<Connection*>		_processingQueue;
	std::set<Connection*>		_processingSet;
	//connection to fd mapping on epoll events.
	std::map<int, Connection*>	_connections;
	// Event loop state
	int									_epollFd;
	std::vector<struct epoll_event>		_eventBuffer;
	std::map<int, uint32_t>				_fdEvents;
	std::set<int>						_listenFds;

	// Private helpers
	/**
	 * @brief Creates, binds, listens, and sets O_NONBLOCK on a socket.
	 * @return The listening fd.
	 * @throws FatalException if any socket operation fails, with the error message.
	 */
	int _createListeningSocket(const struct sockaddr_in& addr);

	/**
	 * @brief Accepts all pending connections on a listening fd.
	 * Sets each client fd to O_NONBLOCK and adds it to _pollfds.
	 */
	void _acceptNewConnections(int listenFd);

	/**
	 * @brief Reads from a client fd and prints the raw data.
	 * @return false if the client disconnected or errored, true otherwise.
	 */
	bool _readAndPrint(int fd);

	/**
	 * @brief Dispatches epoll events to the correct Connection handler
	 * and drives state transitions. Called from run() for every client event.
	 */
	void _handleConnection(Connection* conn, uint32_t events);

	/**
	 * @brief Runs a budgeted round-robin pass over _processingQueue.
	 * Calls process() once per connection, re-enqueues if still PROCESSING,
	 * budgeted to BACKLOG/2 per tick, which is arbitrary, tune as needed.
	 * @warning tribute to Prof.waleed al-maqableh.
	 */
	void _runRoundRobin();

	/**
	 * @brief Closes a client fd and removes it from epoll and _connections.
	 */
	void _dropConnection(int fd);

	/**
	 * @brief Adds a connection to the processing queue if not already queued.
	 */
	void _enqueueProcessing(Connection* conn);

	/**
	 * @brief Removes a connection from the processing set (queue cleanup is lazy).
	 */
	void _dequeueProcessing(Connection* conn);

	/**
	 * @brief Handles state transition for a connection that just left PROCESSING.
	 */
	void _finalizeProcessed(Connection* conn);

	/**
	 * @brief Closes all fds (listeners + clients) during shutdown.
	 */
	void _closeAllFds();
};