/** @file FatalExceptions.hpp
 * @brief This class will be used to collect *fatal* exceptions, which are exceptions that should cause the program to exit immediately.
 * any exception that can be recovered from/used to control flow SHOULD NOT be in this file, and rather should be a subclass of it's cause class.
*/
#pragma once

#include <exception>
#include <string>

class FatalException : public std::exception
{
	public:
		FatalException(const std::string& msg);
		FatalException(const FatalException& other);
		FatalException& operator=(const FatalException& other);
		virtual ~FatalException() throw();

		virtual const char* what() const throw();

	private:
		std::string _msg;
};

/**
 * @brief Exception type for failures scoped to a single client request.
 *
 * Throw this from request/response/CGI runtime paths when the server stays
 * operable and the connection should produce an HTTP error response.
 */
class ClientException : public std::exception
{
	public:
		ClientException(int statusCode, const std::string& msg);
		ClientException(const ClientException& other);
		ClientException& operator=(const ClientException& other);
		virtual ~ClientException() throw();

		virtual const char* what() const throw();
		int getStatusCode() const;

	private:
		int _statusCode;
		std::string _msg;
};