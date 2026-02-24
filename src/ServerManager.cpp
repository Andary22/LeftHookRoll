#include "../includes/ServerManager.hpp"
#include "../includes/FatalExceptions.hpp"

#include <iostream>
#include <cstring>
#include <cerrno>
#include <csignal>

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

// Defined in main.cpp — temp implementation.
extern volatile sig_atomic_t g_running;

// --- Canonical Form ---

ServerManager::ServerManager() : _epollFd(-1)
{
	_epollFd = epoll_create(1);
	if (_epollFd < 0)
		throw FatalException(std::string("epoll_create(): ") + strerror(errno));
	_eventBuffer.resize(64);
}

ServerManager::ServerManager(std::vector<ServerConf*> confs) : _epollFd(-1)
{
	_epollFd = epoll_create(1);
	if (_epollFd < 0)
		throw FatalException(std::string("epoll_create(): ") + strerror(errno));
	_eventBuffer.resize(64);
	for (size_t i = 0; i < confs.size(); ++i)
		addServer(confs[i]);
}

ServerManager::ServerManager(const ServerManager& other)
	: _interfacePortPairs(other._interfacePortPairs),
	  _epollFd(-1),
	  _eventBuffer(other._eventBuffer),
	  _fdEvents(),
	  _listenFds(other._listenFds)
{
	_epollFd = epoll_create(1);
	if (_epollFd < 0)
		throw FatalException(std::string("epoll_create(): ") + strerror(errno));
	for (std::map<int, uint32_t>::const_iterator it = other._fdEvents.begin();
		 it != other._fdEvents.end(); ++it)
	{
		addPollFd(it->first, it->second);
	}
}

ServerManager& ServerManager::operator=(const ServerManager& other)
{
	if (this != &other)
	{
		_closeAllFds();
		_interfacePortPairs = other._interfacePortPairs;
		_listenFds = other._listenFds;
		_eventBuffer = other._eventBuffer;
		_epollFd = epoll_create(1);
		if (_epollFd < 0)
			throw FatalException(std::string("epoll_create(): ") + strerror(errno));
		for (std::map<int, uint32_t>::const_iterator it = other._fdEvents.begin();
			 it != other._fdEvents.end(); ++it)
		{
			addPollFd(it->first, it->second);
		}
	}
	return *this;
}

ServerManager::~ServerManager()
{
	_closeAllFds();
}

// --- Public Interface ---

void ServerManager::addServer(const ServerConf* conf)
{
	struct sockaddr_in addr = conf->getInterfacePortPair();
	int fd = _createListeningSocket(addr);

	_interfacePortPairs[addr] = conf;
	_listenFds.insert(fd);
	addPollFd(fd, EPOLLIN);

	std::cout << "Listening on "
			  << inet_ntoa(addr.sin_addr) << ":"
			  << ntohs(addr.sin_port) << std::endl;
}

void ServerManager::addListenPort(int port)
{
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//change to specific IP once we have parsing up and ready.
	addr.sin_port = htons(port);

	int fd = _createListeningSocket(addr);
	_listenFds.insert(fd);
	addPollFd(fd, EPOLLIN);


	std::cout << "Listening on 0.0.0.0:" << port << std::endl;
}

void ServerManager::run()
{
	while (g_running)
	{
		int ready = epoll_wait(_epollFd, &_eventBuffer[0],
			static_cast<int>(_eventBuffer.size()), EPOLL_TIMEOUT_MS);
		if (ready < 0)
		{
			if (errno == EINTR)
				continue;
			throw FatalException(std::string("epoll_wait(): ") + strerror(errno));
		}
		if (ready == 0)
			continue;

		for (int i = 0; i < ready; ++i)
		{
			int fd = _eventBuffer[i].data.fd;
			uint32_t events = _eventBuffer[i].events;

			if (events & (EPOLLHUP | EPOLLERR))
			{
				if (!_listenFds.count(fd))
					_dropConnection(fd);
				continue;
			}

			if (_listenFds.count(fd))
				_acceptNewConnections(fd);
			else if ((events & EPOLLIN) && !_readAndPrint(fd))
				_dropConnection(fd);
		}
	}
	std::cout << "\nServer shut down." << std::endl;
}

void ServerManager::addPollFd(int fd, uint32_t events)
{
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;

	std::map<int, uint32_t>::iterator it = _fdEvents.find(fd);
	if (it != _fdEvents.end())
	{
		it->second = events;
		if (epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev) < 0)
			throw FatalException(std::string("epoll_ctl(MOD): ") + strerror(errno));
		return;
	}

	if (epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev) < 0)
		throw FatalException(std::string("epoll_ctl(ADD): ") + strerror(errno));
	_fdEvents[fd] = events;
}

const ServerConf* ServerManager::getServerConfForFd(int clientFd) const
{
	struct sockaddr_in localAddr;
	socklen_t len = sizeof(localAddr);
	if (getsockname(clientFd, reinterpret_cast<struct sockaddr*>(&localAddr), &len) < 0)
		return NULL;

	std::map<struct sockaddr_in, const ServerConf*, SockAddrCompare>::const_iterator it;
	it = _interfacePortPairs.find(localAddr);
	if (it != _interfacePortPairs.end())
		return it->second;
	return NULL;
}

// --- Private Helpers ---

int ServerManager::_createListeningSocket(const struct sockaddr_in& addr)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		throw FatalException(std::string("socket(): ") + strerror(errno));

	int yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) < 0)
	{
		close(fd);
		throw FatalException(std::string("setsockopt(): ") + strerror(errno));
	}

	if (bind(fd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)) < 0)
	{
		close(fd);
		throw FatalException(std::string("bind(): ") + strerror(errno));
	}

	if (listen(fd, BACKLOG) < 0)
	{
		close(fd);
		throw FatalException(std::string("listen(): ") + strerror(errno));
	}

	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(fd);
		throw FatalException(std::string("fcntl(): ") + strerror(errno));
	}

	return fd;
}

void ServerManager::_acceptNewConnections(int listenFd)
{
	while (true)
	{
		struct sockaddr_in clientAddr;
		socklen_t clientLen = sizeof(clientAddr);
		int clientFd = accept(listenFd,
			reinterpret_cast<struct sockaddr*>(&clientAddr), &clientLen);
		if (clientFd < 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			std::cerr << "accept(): " << strerror(errno) << std::endl;
			break;
		}

		if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
		{
			std::cerr << "fcntl(client): " << strerror(errno) << std::endl;
			close(clientFd);
			continue;
		}

		std::cout << "New connection from "
				  << inet_ntoa(clientAddr.sin_addr) << ":"
				  << ntohs(clientAddr.sin_port)
				  << " [fd " << clientFd << "]" << std::endl;

		addPollFd(clientFd, EPOLLIN);
	}
}

bool ServerManager::_readAndPrint(int fd)
{
	char buf[RECV_BUFFER_SIZE];
	ssize_t n = recv(fd, buf, sizeof(buf), 0);
	if (n > 0)
	{
		std::cout << "\n--- Received " << n << " bytes from fd " << fd << " ---\n";
		std::cout.write(buf, n);
		std::cout << "\n--- End fd " << fd << " ---" << std::endl;
		return true;
	}
	if (n == 0)
		std::cout << "Client [fd " << fd << "] disconnected." << std::endl;
	else
		std::cerr << "recv error on fd " << fd << std::endl;
	return false;
}

void ServerManager::_dropConnection(int fd)
{
	epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	_fdEvents.erase(fd);
}

void ServerManager::_closeAllFds()
{
	for (std::map<int, uint32_t>::iterator it = _fdEvents.begin();
		 it != _fdEvents.end(); ++it)
	{
		close(it->first);
	}
	_fdEvents.clear();
	_listenFds.clear();
	_eventBuffer.clear();
	if (_epollFd >= 0)
		close(_epollFd);
	_epollFd = -1;
}
