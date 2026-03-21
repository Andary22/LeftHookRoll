#include "../includes/ServerManager.hpp"
#include "../includes/FatalExceptions.hpp"
#include "../includes/CGIManager.hpp"

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

ServerManager::ServerManager(std::vector<ServerConf> confsCopy) : _epollFd(-1)
{
	_epollFd = epoll_create(1);
	if (_epollFd < 0)
		throw FatalException(std::string("epoll_create(): ") + strerror(errno));
	_eventBuffer.resize(64);

	try
	{
		for (size_t i = 0; i < confsCopy.size(); ++i)
		{
			_serverConfs.push_back(new ServerConf(confsCopy[i]));
			addServer(_serverConfs.back());
		}
	}
	catch (...)
	{
		for (size_t i = 0; i < _serverConfs.size(); ++i)
			delete _serverConfs[i];
		_serverConfs.clear();
		_closeAllFds();
		throw;
	}
}

ServerManager::ServerManager(const ServerManager& other)
	: _interfacePortPairs(other._interfacePortPairs),
	  _epollFd(-1),
	  _eventBuffer(other._eventBuffer),
	  _fdEvents(),
	  _listenFds(other._listenFds),
	  _listenFdToServerConf(other._listenFdToServerConf)
{
	_epollFd = epoll_create(1);
	if (_epollFd < 0)
		throw FatalException(std::string("epoll_create(): ") + strerror(errno));
	try
	{
		for (std::map<int, uint32_t>::const_iterator it = other._fdEvents.begin();
			 it != other._fdEvents.end(); ++it)
		{
			addPollFd(it->first, it->second);
		}
	}
	catch (...)
	{
		_closeAllFds();
		throw;
	}
}

ServerManager& ServerManager::operator=(const ServerManager& other)
{
	if (this != &other)
	{
		_closeAllFds();
		_interfacePortPairs = other._interfacePortPairs;
		_listenFds = other._listenFds;
		_listenFdToServerConf = other._listenFdToServerConf;
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
	for(size_t i = 0; i < _serverConfs.size(); i++)
		delete _serverConfs[i];
	_closeAllFds();
	CGIManager::cleanupAllProcesses();
}

// --- Public Interface ---

void ServerManager::addServer(const ServerConf* conf)
{
	struct sockaddr_in addr = conf->getInterfacePortPair();
	int fd = _createListeningSocket(addr);

	_interfacePortPairs[addr] = conf;
	_listenFds.insert(fd);
	_listenFdToServerConf[fd] = conf;
	addPollFd(fd, EPOLLIN);

	std::cout << "Server "<< conf->getServerName() << " Listening on "
			  << req_utils::ipv4ToString(addr) << ":"
			  << ntohs(addr.sin_port) << std::endl;
}

void ServerManager::addListenPort(int port)
{
	/**
	 * @brief listen without needing a conf file.
	 *
	 */
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	int fd = _createListeningSocket(addr);
	_listenFds.insert(fd);
	_listenFdToServerConf[fd] = NULL;
	addPollFd(fd, EPOLLIN);


	std::cout << "Listening on 0.0.0.0:" << port << std::endl;
}

void ServerManager::run()
{
	while (g_running)
	{
		_runRoundRobin();
		_sweepTimeouts();
		_sweepCgiTimeouts();

		// Use a finite timeout so periodic tasks like _sweepTimeouts() still run when idle.
		int ePollTimeOut = _processingQueue.empty() ? 1000 : 0; // ms; 0 = non-blocking when there is work.
		int ready = epoll_wait(_epollFd, &_eventBuffer[0],
			static_cast<int>(_eventBuffer.size()), ePollTimeOut);
		if (ready <= 0)
		{
			if (ready == 0 || errno == EINTR)
				continue;
			throw FatalException(std::string("epoll_wait(): ") + strerror(errno));
		}

		for (int i = 0; i < ready; ++i)
		{
			int			fd = _eventBuffer[i].data.fd;
			uint32_t	events = _eventBuffer[i].events;

			if (_cgiPipeToConn.count(fd))
			{
				_handleCgiPipeEvent(fd, events);
				continue;
			}
			if (events & (EPOLLHUP | EPOLLERR))
			{
				if (!_listenFds.count(fd))
					_dropConnection(fd);
				continue;
			}
			if (_listenFds.count(fd))
			{
				_acceptNewConnections(fd);
				continue;
			}
			std::map<int, Connection*>::iterator it = _connections.find(fd);
			if (it == _connections.end())
				continue;
			_handleConnection(it->second, events);
		}
	}
	std::cout << "\nServer shut down.\n";
}

void ServerManager::_handleConnection(Connection* conn, uint32_t events)
{
	int fd = conn->getFd();

	try
	{
		if (events & EPOLLIN)
		{
			conn->handleRead();
			if (conn->getState() == PROCESSING)
				_enqueueProcessing(conn);
		}
		if ((events & EPOLLOUT) && conn->getState() == WRITING)
			conn->handleWrite();
	}
	catch (const ClientException& e)
	{
		std::cerr << "client runtime error on fd " << fd << ": " << e.what() << std::endl;
		conn->triggerError(e.getStatusCode());
	}
	catch (const FatalException&)
	{
		throw;
	}
	catch (const std::exception& e)
	{
		std::cerr << "unexpected runtime error on fd " << fd << ": " << e.what() << std::endl;
		conn->triggerError(500);
	}

	//epoll based on final state for next iteration
	switch (conn->getState())
	{
		case READING:
			addPollFd(fd, EPOLLIN);
			break;
		case PROCESSING:
			addPollFd(fd, 0);
			break;
		case WRITING:
			addPollFd(fd, EPOLLIN | EPOLLOUT);
			break;
		case WAITING_FOR_CGI:
			// Client fd is idle while CGI runs; pipe fd handles I/O
			addPollFd(fd, 0);
			break;
		case FINISHED:
			_dropConnection(fd);
			break;
	}
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

		const ServerConf* conf = NULL;
		std::map<int, const ServerConf*>::const_iterator confIt = _listenFdToServerConf.find(listenFd);
		if (confIt != _listenFdToServerConf.end())
			conf = confIt->second;
		Connection* conn = new Connection(clientFd, clientAddr, conf);
		_connections[clientFd] = conn;
		addPollFd(clientFd, EPOLLIN);

		std::cout << "New connection from "
				  << req_utils::ipv4ToString(clientAddr) << ":"
				  << ntohs(clientAddr.sin_port)
				  << " [fd " << clientFd << "]" << std::endl;
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
	std::map<int, Connection*>::iterator it = _connections.find(fd);
	if (it != _connections.end())
	{
		// Clean up any associated CGI pipe
		int pipeFd = it->second->getCgiPipeFd();
		if (pipeFd >= 0)
			_unregisterCgiPipe(pipeFd);

		_dequeueProcessing(it->second);
		delete it->second;
		_connections.erase(it);
	}

	epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	_fdEvents.erase(fd);
}

void ServerManager::_enqueueProcessing(Connection* conn)
{
	if (_processingSet.insert(conn).second)
		_processingQueue.push_back(conn);
}

void ServerManager::_dequeueProcessing(Connection* conn)
{
	_processingSet.erase(conn);
}

void ServerManager::_runRoundRobin()
{
	size_t budget = BACKLOG / 2;
	size_t processed = 0;

	while (!_processingQueue.empty() && processed < budget)
	{
		Connection* conn = _processingQueue.front();
		_processingQueue.pop_front();

		//skip connections removed by _dropConnection
		if (_processingSet.find(conn) == _processingSet.end())
			continue;

		try
		{
			conn->process();
		}
		catch (const ClientException& e)
		{
			std::cerr << "client runtime error on fd " << conn->getFd() << ": " << e.what() << std::endl;
			conn->triggerError(e.getStatusCode());
		}
		catch (const FatalException&)
		{
			throw;
		}
		catch (const std::exception& e)
		{
			std::cerr << "unexpected runtime error on fd " << conn->getFd() << ": " << e.what() << std::endl;
			conn->triggerError(500);
		}
		++processed;

		if (conn->getState() == PROCESSING)
			_processingQueue.push_back(conn);
		else
		{
			_processingSet.erase(conn);
			_finalizeProcessed(conn);
		}
	}
}

void ServerManager::_finalizeProcessed(Connection* conn)
{
	int fd = conn->getFd();
	switch (conn->getState())
	{
		case WRITING:
			addPollFd(fd, EPOLLIN | EPOLLOUT);
			break;
		case WAITING_FOR_CGI:
			// Unsubscribe client fd from epoll (it's idle during CGI)
			addPollFd(fd, 0);
			// Register the CGI pipe fd in epoll for reading
			_registerCgiPipe(conn);
			break;
		case FINISHED:
			_dropConnection(fd);
			break;
		default:
			break;
	}
}

void ServerManager::_registerCgiPipe(Connection* conn)
{
	int pipeFd = conn->getCgiPipeFd();
	if (pipeFd < 0)
		return;
	_cgiPipeToConn[pipeFd] = conn;
	_cgiStartTimes[pipeFd] = time(NULL);
	addPollFd(pipeFd, EPOLLIN);
}

void ServerManager::_unregisterCgiPipe(int pipeFd)
{
	if (pipeFd < 0)
		return;
	epoll_ctl(_epollFd, EPOLL_CTL_DEL, pipeFd, NULL);
	_fdEvents.erase(pipeFd);
	_cgiPipeToConn.erase(pipeFd);
	_cgiStartTimes.erase(pipeFd);
}

void ServerManager::_handleCgiPipeEvent(int pipeFd, uint32_t events)
{
	std::map<int, Connection*>::iterator it = _cgiPipeToConn.find(pipeFd);
	if (it == _cgiPipeToConn.end())
		return;

	Connection* conn = it->second;
	Response* resp = conn->getResponse();

	bool done = false;
	try
	{
		if (events & (EPOLLHUP | EPOLLERR))
			done = true;
		else if (events & EPOLLIN)
			done = resp->readCgiOutput();
	}
	catch (const ClientException& e)
	{
		std::cerr << "client CGI runtime error on fd " << conn->getFd() << ": " << e.what() << std::endl;
		conn->triggerError(e.getStatusCode());
		_unregisterCgiPipe(pipeFd);
		addPollFd(conn->getFd(), EPOLLIN | EPOLLOUT);
		return;
	}
	catch (const FatalException&)
	{
		throw;
	}
	catch (const std::exception& e)
	{
		std::cerr << "unexpected CGI runtime error on fd " << conn->getFd() << ": " << e.what() << std::endl;
		conn->triggerError(500);
		_unregisterCgiPipe(pipeFd);
		addPollFd(conn->getFd(), EPOLLIN | EPOLLOUT);
		return;
	}

	if (done)
	{
		_unregisterCgiPipe(pipeFd);
		resp->finalizeCgiResponse();
		conn->setState(WRITING);
		addPollFd(conn->getFd(), EPOLLIN | EPOLLOUT);
	}
}

void ServerManager::_sweepCgiTimeouts()
{
	if (_cgiPipeToConn.empty())
		return;

	time_t now = time(NULL);
	std::vector<int> toKill;

	for (std::map<int, time_t>::iterator it = _cgiStartTimes.begin();
		 it != _cgiStartTimes.end(); ++it)
	{
		if (now - it->second >= CGI_TIMEOUT_S)
			toKill.push_back(it->first);
	}

	for (size_t i = 0; i < toKill.size(); ++i)
	{
		int pipeFd = toKill[i];
		std::map<int, Connection*>::iterator it = _cgiPipeToConn.find(pipeFd);
		if (it == _cgiPipeToConn.end())
			continue;

		Connection* conn = it->second;
		Response* resp = conn->getResponse();
		const ServerConf* conf = conn->getServerConf();

		_unregisterCgiPipe(pipeFd);

		if (conf)
			resp->cgiTimeout(*conf);
		else
		{
			resp->setStatusCode("504");
			resp->setResponsePhrase("Gateway Timeout");
		}
		conn->setState(WRITING);
		addPollFd(conn->getFd(), EPOLLIN | EPOLLOUT);
	}
}

void ServerManager::_sweepTimeouts()
{
	static time_t lastSweep = 0;
	time_t now = time(NULL);
	if (now - lastSweep < 5) // sweep every 5 seconds at most.
		return;
	lastSweep = now;

	std::vector<int> toDrop;
	for (std::map<int, Connection*>::iterator it = _connections.begin();
		 it != _connections.end(); ++it)
	{
		if (it->second->hasTimedOut(CONNECTION_TIMEOUT_S))
			toDrop.push_back(it->first);
	}
	for (size_t i = 0; i < toDrop.size(); ++i)
	{
		std::map<int, Connection*>::iterator it = _connections.find(toDrop[i]);
		if (it != _connections.end())
		{
			it->second->triggerError(408); // Request Timeout
			_dequeueProcessing(it->second);
			//sets state as WRITING and mods epoll.
			_finalizeProcessed(it->second);
		}
	}
}

void ServerManager::_closeAllFds()
{
	_processingQueue.clear();
	_processingSet.clear();
	_cgiPipeToConn.clear();
	_cgiStartTimes.clear();

	for(std::map<int, Connection*>::iterator it = _connections.begin();
		it != _connections.end(); ++it)
	{
		delete it->second;
	}
	_connections.clear();

	for (std::map<int, uint32_t>::iterator it = _fdEvents.begin();
		 it != _fdEvents.end(); ++it)
	{
		close(it->first);
	}
	_fdEvents.clear();
	_listenFds.clear();
	_listenFdToServerConf.clear();
	_eventBuffer.clear();
	if (_epollFd >= 0)
		close(_epollFd);
	_epollFd = -1;
}
