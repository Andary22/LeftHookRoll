/**
 * @file LocationConf.hpp
 * @brief Stores per-route configuration directives.
 * This class encapsulates the configuration for a specific location block
 * parsed from the webserv.conf file.
 */

#pragma once

#include <string>
#include "AllowedMethods.hpp"

class LocationConf
{
	public:
		// Canonical Form
		LocationConf();
		LocationConf(const LocationConf& other);
		LocationConf& operator=(const LocationConf& other);
		~LocationConf();

		// Getters

		const std::string&		getPath() const;
		const std::string&		getRoot() const;
		const AllowedMethods&	getAllowedMethods() const;
		const std::string&		getReturnURL() const;
		const std::string&		getReturnCode() const;
		bool					getAutoIndex() const;
		const std::string&		getDefaultPage() const;
		const std::string&		getStorageLocation() const;

		//  Setters

		void setPath(const std::string& path);
		void setRoot(const std::string& root);
		void addAllowedMethod(HTTPMethod method);
		void setReturnURL(const std::string& url);
		void setReturnCode(const std::string& code);
		void setAutoIndex(bool autoIndex);
		void setDefaultPage(const std::string& defaultPage);
		void setStorageLocation(const std::string& storageLocation);

		// Utility

		/**
		 * @brief Checks if a specific HTTP method is permitted in this location.
		 * @param method The method to check (e.g., GET, POST).
		 * @return true if allowed, false otherwise.
		 */
		bool isMethodAllowed(HTTPMethod method) const;

	private:
		//  Identity
		std::string	_path;
		//  Data
		std::string		 _root;							// Directory where the requested file should be located
		AllowedMethods	_allowedMethods;		// Bitmap wrapper of accepted HTTP methods
		std::string		 _returnURL;				 // Target URL for HTTP redirection
		std::string		 _returnCode;				// HTTP status code for redirection (e.g., "301")
		bool			_autoIndex;				 // Directory listing flag
		std::string		 _defaultPage;			 // Default file to serve (e.g., "index.html")
		std::string		 _storageLocation;	 // Directory where uploaded files are saved
};