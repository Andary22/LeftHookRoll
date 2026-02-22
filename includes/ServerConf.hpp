/**
 * @file ServerConf.hpp
 * @brief Stores per-server configuration directives.
 * This class encapsulates the configuration for a specific server block
 * parsed from the webserv.conf file.
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <netinet/in.h>
#include "LocationConf.hpp"

class ServerConf
{
	public:
		//  Canonical Form
		ServerConf();
		ServerConf(const ServerConf& other);
		ServerConf& operator=(const ServerConf& other);
		~ServerConf();

		//  Getters

		const std::string&							getServerName() const;
		const struct sockaddr_in&					getInterfacePortPair() const;
		size_t										getMaxBodySize() const;
		const std::vector<LocationConf>&			getLocations() const;
		const std::map<std::string, std::string>&	getErrorPages() const;

		//  Setters
		void setServerName(const std::string& name);
		void setInterfacePortPair(const struct sockaddr_in& address);
		void setMaxBodySize(size_t size);

		/**
		 * @brief Adds a parsed LocationConf block to this server.
		 */
		void addLocation(const LocationConf& location);

		/**
		 * @brief Adds a custom error page mapping (e.g., "404" -> "/errors/404.html").
		 */
		void addErrorPage(const std::string& errorCode, const std::string& errorPagePath);

		//  Utils

		/**
		 * @brief Retrieves the path to a custom error page if one exists.
		 * @param errorCode The HTTP error code as a string (e.g., "404").
		 * @return The path to the custom error page, or an empty string if none is defined.
		 */
		std::string getErrorPagePath(const std::string& errorCode) const;

	private:
		//  Identity
		std::string			_serverName;
		struct sockaddr_in	_interfacePortPair;

		//  Data
		size_t								_maxBodySize;
		std::vector<LocationConf>			_locations;
		std::map<std::string, std::string>	_errorPages;
};