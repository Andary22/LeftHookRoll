/**
 * @file HTTPMethods.hpp
 * @author Yaman Al-Rifai(you@domain.com)
 * @brief
 * @version 0.1
 * @date 2026-02-23
 *
 * @copyright Copyright (c) 2026
 *
 */
#pragma once

/**
 * @enum HTTPMethod
 * @brief Represents the HTTP methods supported by the server as a bitmask.
 */
enum HTTPMethod
{
	UNKNOWN_METHOD = 0,	//000
	GET			= 1 << 0, //001
	POST		   = 1 << 1, //010
	DELETE		 = 1 << 2  //100
};

/**
 * @class AllowedMethods
 * @brief A lightweight wrapper for a short bitmap to handle HTTP method validations securely.
 */
class AllowedMethods
{
	public:
		// Canonical Form
		AllowedMethods();
		AllowedMethods(const AllowedMethods& other);
		AllowedMethods& operator=(const AllowedMethods& other);
		~AllowedMethods();

		// Bitwise Operations

		/**
		 * @brief Adds an HTTP method to the allowed bitmap using bitwise OR.
		 */
		void addMethod(HTTPMethod method);

		/**
		 * @brief Removes an HTTP method from the allowed bitmap using bitwise AND NOT.
		 */
		void removeMethod(HTTPMethod method);

		/**
		 * @brief Checks if a specific HTTP method is permitted using bitwise AND.
		 */
		bool isAllowed(HTTPMethod method) const;

		/**
		 * @brief Clears all methods (resets to 0 / UNKNOWN_METHOD).
		 */
		void clear();

		/**
		 * @brief Returns the raw bitmap.
		 */
		short getBitmap() const;

	private:
		short _bitmap;
};