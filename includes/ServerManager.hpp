/**
 * @file ServerManager.hpp
 * @brief Matches the fd provided by accept() with the correct ServerConf to instantiate a Connection.
 * pretty much a getsockname() wrapper.
 */

#pragma once

#include <map>
#include <netinet/in.h>
#include <sys/socket.h>
#include "ServerConf.hpp"

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
	ServerManager(const ServerManager& other);
	ServerManager& operator=(const ServerManager& other);
	~ServerManager();

	/**
	 * @brief Links a specific IP:Port combination to a parsed ServerConf block.
=	 * @param conf Pointer to the configuration block (pointer used to avoid heavy copying).
	 */
	void addServer(const ServerConf* conf);

	/**
	 * @brief ,ap
	 * interface/port the client connected to, then returns the matching ServerConf.
	 * * @param clientFd The file descriptor returned by accept().
	 * @return const ServerConf* Pointer to the matching configuration, or NULL if not found.
	 */
	const ServerConf* getServerConfForFd(int clientFd) const;

private:
	// Data
	std::map<struct sockaddr_in, const ServerConf*, SockAddrCompare> _interfacePortPairs;
};