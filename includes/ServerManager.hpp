/**
 * @file ServerManager.hpp
 * @brief Owns the event loop and all listening sockets.
 * Accepts new connections, dispatches I/O events, and routes
 * each accepted fd to the correct ServerConf via getsockname().
 */

#pragma once

#include <map>
#include <set>
#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "ServerConf.hpp"

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
	// Config mapping
	std::map<struct sockaddr_in, const ServerConf*, SockAddrCompare> _interfacePortPairs;
	std::vector<ServerConf*> _serverConfs;
	// Event loop state
	int								_epollFd;
	std::vector<struct epoll_event>	_eventBuffer;
	std::map<int, uint32_t>			_fdEvents;
	std::set<int>					_listenFds;

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
	 * @brief Closes a client fd and removes it from _pollfds.
	 */
	void _dropConnection(int fd);

	/**
	 * @brief Closes all fds (listeners + clients) during shutdown.
	 */
	void _closeAllFds();
};