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
