/**
 * @file ConfigParser.hpp
 * @brief Parses a webserv.conf file and produces a vector of ServerConf objects.
 *
 * Uses a simple two-pass approach:
 *   1. Tokenize the raw file content.
 *   2. Walk the token stream to populate ServerConf / LocationConf data structures.
 *
 * Throws ConfigParser::ConfigException (a FatalException subclass) on any
 * syntax or semantic error so the caller can exit cleanly via the normal
 * fatal-exception handler.
 */

#pragma once

#include <string>
#include <vector>
#include "FatalExceptions.hpp"
#include "ServerConf.hpp"

class ConfigParser
{
public:

	class ConfigException : public FatalException
	{
	public:
		explicit ConfigException(const std::string& msg);
		virtual ~ConfigException() throw();
		virtual const char* what() const throw();

	private:
		std::string _msg;
	};

	// Canonical form
	explicit ConfigParser(const std::string& filePath);
	~ConfigParser();

	// Public API 

	/**
	 * @brief Parses the config file and returns one ServerConf per server block.
	 * @throws ConfigException on any syntax / validation error.
	 */
	std::vector<ServerConf> parse();

private:
	std::string              _filePath;
	std::vector<std::string> _tokens;
	size_t                   _pos;

	// Tokenizer

	void _tokenize(const std::string& content);

	// Token stream helpers

	const std::string& _peek() const;
	const std::string& _consume();
	void               _expect(const std::string& token);
	bool               _atEnd() const;

	// Block parsers

	ServerConf   _parseServerBlock();
	LocationConf _parseLocationBlock(const std::string& path);

	// Server-level directive handlers

	void _parseListen(ServerConf& conf);
	void _parseServerName(ServerConf& conf);
	void _parseMaxBodySize(ServerConf& conf);
	void _parseErrorPage(ServerConf& conf);

	// Location-level directive handlers

	void _parseRoot(LocationConf& loc);
	void _parseMethods(LocationConf& loc);
	void _parseAutoIndex(LocationConf& loc);
	void _parseIndex(LocationConf& loc);
	void _parseUploadStore(LocationConf& loc);
	void _parseReturn(LocationConf& loc);

	// Validators / converters 

	struct sockaddr_in _parseSockAddr(const std::string& listenValue);
	size_t             _parseBodySize(const std::string& value);
	HTTPMethod         _parseMethodToken(const std::string& token);

	// Non-copyable
	ConfigParser(const ConfigParser&);
	ConfigParser& operator=(const ConfigParser&);
};
