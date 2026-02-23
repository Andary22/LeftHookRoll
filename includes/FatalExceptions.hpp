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