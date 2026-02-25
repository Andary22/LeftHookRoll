#include "../includes/AllowedMethods.hpp"

AllowedMethods::AllowedMethods() : _bitmap(0) {}

AllowedMethods::AllowedMethods(const AllowedMethods& other) : _bitmap(other._bitmap) {}

AllowedMethods& AllowedMethods::operator=(const AllowedMethods& other)
{
	if (this != &other)
		_bitmap = other._bitmap;
	return *this;
}

AllowedMethods::~AllowedMethods() {}

// --- Bitwise Operations ---

void AllowedMethods::addMethod(HTTPMethod method)
{
	_bitmap |= method;
}

void AllowedMethods::removeMethod(HTTPMethod method)
{
	_bitmap &= ~method;
}

bool AllowedMethods::isAllowed(HTTPMethod method) const
{
	return (_bitmap & method) != 0;
}

void AllowedMethods::clear()
{
	_bitmap = 0;
}

short AllowedMethods::getBitmap() const
{
	return _bitmap;
}

HTTPMethod AllowedMethods::stringToMethod(const std::string& methodStr)
{
	if (methodStr == "GET")
		return GET;
	else if (methodStr == "POST")
		return POST;
	else if (methodStr == "DELETE")
		return DELETE;
	else
		return UNKNOWN_METHOD;
}
