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

//do we need a custom DNS inside the conf?
//cookies and session مانيجمينت? 🍪🍪🍪🍪:
//turn in checklist:
//rename executable to webserv
// write README
	// update README first line
// write confs, html pages, and CGI scripts to showcase and test on
//verify sigint handling
//remove all debugging disallowed functions (nm ts, o alternatively, comment .h and check errors):
	//: inet_ntoa
//verify we throw on: (-;
	//parsing errors
	//syscalls
//catch thrown exceptions )-:
//test CGI's and merge to main if it passes 🙏
//test cases (adding those as I think of them, if you have an idea feel free to add it to the list:)
	//mix n match interpreters (E.g. .py with bash)
	//script with no execute permissions
	//nonexistent script
	//nonexistent interpreter
	//omit interpreter to test auto-detection.
	//test huge request body handling with a CGI script that prints the body back out.
	//test that CGI timeout works by using a script that sleeps for longer than the timeout.
	//test client disconnect during CGI execution don't leave zombies
	//test redirects
	//test autoindex
	//test error pages (e.g. 404, 413, 500)
	//eval sheet test cases
	//test with the provided test suite once we have a stable implementation
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
