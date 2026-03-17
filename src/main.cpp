/**
 * @file main.cpp - Entry point for LeftHookRoll web server.
 * @brief validates args, spins up parser ServerManager, and enters main event loop.
 *
 */
#include <iostream>
#include <cstdlib>
#include <csignal>

#include "../includes/ServerManager.hpp"
#include "../includes/FatalExceptions.hpp"
#include "../includes/ConfigParser.hpp"

//turn in checklist:
//rename executable to webserv
// write README
	// update README first line
// write confs, html pages, and CGI scripts to showcase and test on
//remove all debugging disallowed functions (nm ts, o alternatively, comment .h and check errors):
	//: inet_ntoa
	//: inet_aton
	//: usleep --> waitpid
//verify we throw on: (-;
	//parsing errors
	//syscalls
//catch thrown exceptions )-:
//test cases (adding those as I think of them, if you have an idea feel free to add it to the list:)
	//mix n match interpreters (E.g. .py with bash)
	//script with no execute permissions
	//nonexistent script
	//nonexistent interpreter
	//omit interpreter to test auto-detection.
	//test huge request body handling with a CGI script that prints the body back out.
	//eval sheet test cases
volatile sig_atomic_t g_running = 1;

static void signalHandler(int sig)
{
	(void)sig;
	g_running = 0;
}

int main(int argc, char** argv)
{
	signal(SIGINT, signalHandler);
	if (argc > 2)
	{
		std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
		return 1;
	}
	try
	{
		std::vector<ServerConf> parsedConfs;
		if (argc == 1)
		{
			ServerConf defaultConf;
			defaultConf.setDefaults();
			parsedConfs.push_back(defaultConf);
		}
		else
		{
			ConfigParser parser(argv[1]);
			parsedConfs = parser.parse();
		}
		ServerManager manager(parsedConfs);
		manager.run();
	}
	catch (const FatalException& e)
	{
		std::cerr << "Fatal: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
