/**
 * @file CGIManager.hpp
 * @brief Declares the CGIManager class responsible for executing CGI scripts and redirecting their input/output.
 */
#pragma once

#include <vector>
#include <string>
#include <map>
#include <unistd.h>
#include <sys/types.h>

class Request;
class CGIManager
{
	public:
		// Canonical Form
		CGIManager();
		CGIManager(const CGIManager& other);
		CGIManager& operator=(const CGIManager& other);
		~CGIManager();

		//Getters
		pid_t getPid() const;
		/**
		 * @return returns read end of the outpipe to add to epoll.
		 */
		int getOutputFd() const;

		//Behavior
		/**
		 * @brief Prepares the environment and arguments for executing a CGI script.
		 * @param request the parsed HTTP request.
		 * @param scriptPath the absolute path to the script to be executed.
		 */
		void prepare(const Request& request, const std::string& scriptPath);

		/**
		 * @brief Forks, redirs input file and outpipe, and executes the CGI script.
		 * @param inputFD the fd of the temp file(data store) containing the fully received request body.
		 * @warning this implementation requires that the request body is fully received before executing the CGI script,
		 * we handle large request bodies by writing them to a temp file and passing the fd to the CGI script,
		 * but this also means that we can't start executing the CGI script until we've fully received the request body,
		 * which is a tradeoff  we made for simplicity.
		 */
		 void execute(int inputFd);

		 /**
		 * @brief Checks if the child process has finished executing (Non-blocking); waitpid with WNOHANG.
		 * @return true if the process exited, false if it is still running.
		 */
		 bool isDone();

	private:
	//Identity
		pid_t	_pId;
		//Data
		int									_outPipe[2];
		std::string							_query;
		std::vector<std::string>			_scriptArgv;
		std::map<std::string, std::string>	_env;
		//Execve Arrays(RAII ts):
		char**								_execveEnvp;
		char**								_execveArgv;
		//Private Helpers
		void	_buildEnvMap(const Request& request, const std::string& scriptPath);
		void	_prepExecveArrays();
		void	_freeExecveArrays();
		void	_closePipes();
};