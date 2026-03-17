#include "../includes/FatalExceptions.hpp"

FatalException::FatalException(const std::string& msg) : _msg(msg) {}

FatalException::FatalException(const FatalException& other) : std::exception(other), _msg(other._msg) {}

FatalException& FatalException::operator=(const FatalException& other)
{
	if (this != &other)
		_msg = other._msg;
	return *this;
}

FatalException::~FatalException() throw() {}

const char* FatalException::what() const throw()
{
	return _msg.c_str();
}

ClientException::ClientException(int statusCode, const std::string& msg)
	: _statusCode(statusCode), _msg(msg) {}

ClientException::ClientException(const ClientException& other)
	: std::exception(other), _statusCode(other._statusCode), _msg(other._msg) {}

ClientException& ClientException::operator=(const ClientException& other)
{
	if (this != &other)
	{
		_statusCode = other._statusCode;
		_msg = other._msg;
	}
	return *this;
}

ClientException::~ClientException() throw() {}

const char* ClientException::what() const throw()
{
	return _msg.c_str();
}

int ClientException::getStatusCode() const
{
	return _statusCode;
}
