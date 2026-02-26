#include <cstring>
#include <iostream>
#include "../includes/ServerConf.hpp"
#include <arpa/inet.h>

ServerConf::ServerConf()
	:	_serverName("LeftHookRoll"),
		_interfacePortPair(),
		_maxBodySize(1024*1024),
		_locations(),
		_errorPages()
{
	memset(&_interfacePortPair, 0, sizeof(_interfacePortPair));
	_interfacePortPair.sin_family = AF_INET;
	_interfacePortPair.sin_addr.s_addr = INADDR_ANY;
	_interfacePortPair.sin_port = htons(8080);
	std::cout << "Default " << _serverName << "Listening on "
			  << inet_ntoa(_interfacePortPair.sin_addr) << ":"
			  << ntohs(_interfacePortPair.sin_port) << std::endl;
}

ServerConf::ServerConf(const ServerConf& other)
	: _serverName(other._serverName),
	  _interfacePortPair(other._interfacePortPair),
	  _maxBodySize(other._maxBodySize),
	  _locations(other._locations),
	  _errorPages(other._errorPages)
{}

ServerConf& ServerConf::operator=(const ServerConf& other)
{
	if (this != &other)
	{
		_serverName         = other._serverName;
		_interfacePortPair  = other._interfacePortPair;
		_maxBodySize        = other._maxBodySize;
		_locations          = other._locations;
		_errorPages         = other._errorPages;
	}
	return *this;
}

ServerConf::~ServerConf() {}


const std::string& ServerConf::getServerName() const
{
	return _serverName;
}

const struct sockaddr_in& ServerConf::getInterfacePortPair() const
{
	return _interfacePortPair;
}

size_t ServerConf::getMaxBodySize() const
{
	return _maxBodySize;
}

const std::vector<LocationConf>& ServerConf::getLocations() const
{
	return _locations;
}

const std::map<std::string, std::string>& ServerConf::getErrorPages() const
{
	return _errorPages;
}

void ServerConf::setServerName(const std::string& name)
{
	_serverName = name;
}

void ServerConf::setInterfacePortPair(const struct sockaddr_in& address)
{
	_interfacePortPair = address;
}

void ServerConf::setMaxBodySize(size_t size)
{
	_maxBodySize = size;
}

void ServerConf::addLocation(const LocationConf& location)
{
	_locations.push_back(location);
}

void ServerConf::addErrorPage(const std::string& errorCode, const std::string& errorPagePath)
{
	_errorPages[errorCode] = errorPagePath;
}

std::string ServerConf::getErrorPagePath(const std::string& errorCode) const
{
	std::map<std::string, std::string>::const_iterator it = _errorPages.find(errorCode);
	if (it != _errorPages.end())
		return it->second;
	return "";
}
