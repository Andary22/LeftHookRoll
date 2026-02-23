#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <arpa/inet.h>
#include "ConfigParser.hpp"

// ConfigException

ConfigParser::ConfigException::ConfigException(const std::string& msg)
	: _msg("webserv: config error: " + msg) {}

ConfigParser::ConfigException::~ConfigException() throw() {}

const char* ConfigParser::ConfigException::what() const throw()
{
	return _msg.c_str();
}

// Canonical form

ConfigParser::ConfigParser(const std::string& filePath)
	: _filePath(filePath), _pos(0) {}

ConfigParser::~ConfigParser() {}

// Public API

std::vector<ServerConf> ConfigParser::parse()
{
	std::ifstream file(_filePath.c_str());
	if (!file.is_open())
		throw ConfigException("cannot open file: " + _filePath);

	std::ostringstream ss;
	ss << file.rdbuf();
	if (file.bad())
		throw ConfigException("read error on file: " + _filePath);
	
    _tokenize(ss.str());

	std::vector<ServerConf> servers;
	while (!_atEnd())
	{
		if (_peek() != "server")
			throw ConfigException("expected 'server' block, got: '" + _peek() + "'");
		_consume();
		_expect("{");
		servers.push_back(_parseServerBlock());
	}

	if (servers.empty())
		throw ConfigException("config contains no server blocks");

	return servers;
}


void ConfigParser::_tokenize(const std::string& content)
{
	size_t i = 0;
	while (i < content.size())
	{
		const char c = content[i];

		if (std::isspace(c))
        {
            i++; continue; 
        }

		if (c == '#')
		{
			while (i < content.size() && content[i] != '\n')
				i++;
			continue;
		}

		if (c == '{' || c == '}' || c == ';')
		{
			_tokens.push_back(std::string(1, c));
			i++;
			continue;
		}

		size_t start = i;
		while (i < content.size()
			&& !std::isspace(content[i])
			&& content[i] != '{'
			&& content[i] != '}'
			&& content[i] != ';'
			&& content[i] != '#')
		{
			i++;
		}
		_tokens.push_back(content.substr(start, i - start));
	}
}

bool ConfigParser::_atEnd() const
{
	return _pos >= _tokens.size();
}

const std::string& ConfigParser::_peek() const
{
	if (_atEnd())
		throw ConfigException("unexpected end of config file");
	return _tokens[_pos];
}

const std::string& ConfigParser::_consume()
{
	const std::string& t = _peek();
	_pos++;
	return t;
}

void ConfigParser::_expect(const std::string& token)
{
	const std::string got = _consume();
	if (got != token)
		throw ConfigException("expected '" + token + "', got '" + got + "'");
}


ServerConf ConfigParser::_parseServerBlock()
{
	ServerConf conf;

	while (!_atEnd() && _peek() != "}")
	{
		const std::string directive = _consume();

		if      (directive == "listen")
            _parseListen(conf);
		else if (directive == "server_name")          
            _parseServerName(conf);
		else if (directive == "client_max_body_size")
            _parseMaxBodySize(conf);
		else if (directive == "error_page")
            _parseErrorPage(conf);
		else if (directive == "location")
		{
			const std::string path = _consume();
			_expect("{");
			conf.addLocation(_parseLocationBlock(path));
		}
		else
			throw ConfigException("unknown server directive: '" + directive + "'");
	}
	_expect("}");
	return conf;
}

LocationConf ConfigParser::_parseLocationBlock(const std::string& path)
{
	LocationConf loc;
	loc.setPath(path);

	while (!_atEnd() && _peek() != "}")
	{
		const std::string directive = _consume();

		if      (directive == "root")
            _parseRoot(loc);
		else if (directive == "methods")
            _parseMethods(loc);
		else if (directive == "autoindex")
            _parseAutoIndex(loc);
		else if (directive == "index")
            _parseIndex(loc);
		else if (directive == "upload_store")
            _parseUploadStore(loc);
		else if (directive == "return")       
            _parseReturn(loc);
		else
			throw ConfigException("unknown location directive: '" + directive + "'");
	}
	_expect("}");
	return loc;
}


void ConfigParser::_parseListen(ServerConf& conf)
{
	const std::string value = _consume();
	_expect(";");
	conf.setInterfacePortPair(_parseSockAddr(value));
}

void ConfigParser::_parseServerName(ServerConf& conf)
{
	const std::string name = _consume();
	_expect(";");
	conf.setServerName(name);
}

void ConfigParser::_parseMaxBodySize(ServerConf& conf)
{
	const std::string value = _consume();
	_expect(";");
	conf.setMaxBodySize(_parseBodySize(value));
}

void ConfigParser::_parseErrorPage(ServerConf& conf)
{
	const std::string code = _consume();
	const std::string path = _consume();
	_expect(";");

	if (code.size() != 3 || code.find_first_not_of("0123456789") != std::string::npos)
		throw ConfigException("invalid error_page code: '" + code + "'");

	conf.addErrorPage(code, path);
}

void ConfigParser::_parseRoot(LocationConf& loc)
{
	const std::string root = _consume();
	_expect(";");
	loc.setRoot(root);
}

void ConfigParser::_parseMethods(LocationConf& loc)
{
	if (_peek() == ";")
		throw ConfigException("'methods' directive requires at least one method");

	while (!_atEnd() && _peek() != ";")
		loc.addAllowedMethod(_parseMethodToken(_consume()));

	_expect(";");
}

void ConfigParser::_parseAutoIndex(LocationConf& loc)
{
	const std::string value = _consume();
	_expect(";");

	if (value == "on")
		loc.setAutoIndex(true);
	else if (value == "off")
		loc.setAutoIndex(false);
	else
		throw ConfigException("autoindex must be 'on' or 'off', got: '" + value + "'");
}

void ConfigParser::_parseIndex(LocationConf& loc)
{
	const std::string page = _consume();
	_expect(";");
	loc.setDefaultPage(page);
}

void ConfigParser::_parseUploadStore(LocationConf& loc)
{
	const std::string dir = _consume();
	_expect(";");
	loc.setStorageLocation(dir);
}

void ConfigParser::_parseReturn(LocationConf& loc)
{
	const std::string code = _consume();
	const std::string url  = _consume();
	_expect(";");

	if (code.find_first_not_of("0123456789") != std::string::npos)
		throw ConfigException("invalid return code: '" + code + "'");

	loc.setReturnCode(code);
	loc.setReturnURL(url);
}

struct sockaddr_in ConfigParser::_parseSockAddr(const std::string& listenValue)
{
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;

	size_t colon = listenValue.find(':');
	if (colon != std::string::npos)
	{
		const std::string ipStr   = listenValue.substr(0, colon);
		const std::string portStr = listenValue.substr(colon + 1);

		if (inet_aton(ipStr.c_str(), &addr.sin_addr) == 0)
			throw ConfigException("invalid IP address in listen: '" + ipStr + "'");

		if (portStr.empty() || portStr.find_first_not_of("0123456789") != std::string::npos)
			throw ConfigException("invalid port in listen: '" + portStr + "'");

		const int port = std::atoi(portStr.c_str());
		if (port <= 0 || port > 65535)
			throw ConfigException("port out of range in listen: '" + portStr + "'");

		addr.sin_port = htons(static_cast<uint16_t>(port));
	}
	else
	{
		if (listenValue.empty() || listenValue.find_first_not_of("0123456789") != std::string::npos)
			throw ConfigException("invalid listen value: '" + listenValue + "'");

		const int port = std::atoi(listenValue.c_str());
		if (port <= 0 || port > 65535)
			throw ConfigException("port out of range in listen: '" + listenValue + "'");

		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		addr.sin_port        = htons(static_cast<uint16_t>(port));
	}
	return addr;
}

size_t ConfigParser::_parseBodySize(const std::string& value)
{
	if (value.empty())
		throw ConfigException("empty client_max_body_size value");
	const char   suffix = value[value.size() - 1];
	size_t       multiplier = 1;
	std::string  numStr = value;

	if (suffix == 'k' || suffix == 'K')
	{
		multiplier = 1024;
		numStr = value.substr(0, value.size() - 1);
	}
	else if (suffix == 'm' || suffix == 'M')
	{
		multiplier = 1024 * 1024;
		numStr = value.substr(0, value.size() - 1);
	}
	else if (suffix == 'g' || suffix == 'G')
	{
		multiplier = 1024UL * 1024 * 1024;
		numStr = value.substr(0, value.size() - 1);
	}

	if (numStr.empty() || numStr.find_first_not_of("0123456789") != std::string::npos)
		throw ConfigException("invalid client_max_body_size value: '" + value + "'");

	return static_cast<size_t>(std::atol(numStr.c_str())) * multiplier;
}

HTTPMethod ConfigParser::_parseMethodToken(const std::string& token)
{
	if (token == "GET")
        return GET;
	if (token == "POST")
        return POST;
	if (token == "DELETE")
        return DELETE;
	throw ConfigException("unknown HTTP method: '" + token + "'");
}
