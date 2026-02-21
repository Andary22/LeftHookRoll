
/** @file FatalExceptions.hpp
 * @brief This class will be used to collect *fatal* exceptions, which are exceptions that should cause the program to exit immediately.
 * any exception that can be recovered from/used to control flow SHOULD NOT be in this file, and rather should be a subclass of it's cause class.
*/
#pragma once