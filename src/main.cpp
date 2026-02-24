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

#define DEFAULT_PORT 8080

/*TEMP IMPLEMENTATION SO WE CAN CTRLC*/
volatile sig_atomic_t g_running = 1;

static void signalHandler(int sig)
{
	(void)sig;
	g_running = 0;
}
/*TEMP IMPLEMENTATION SO WE CAN CTRLC*/

int main(int argc, char** argv)
{
	if (argc > 2)
	{
		std::cerr << "Usage: " << argv[0] << " [configuration file]" << std::endl;
		return EXIT_FAILURE;
	}

	signal(SIGINT, signalHandler);
	signal(SIGPIPE, SIG_IGN);

	// std::string configPath = (argc == 2) ? argv[1] : "default.conf";
	// TODO: parse config file into std::vector<ServerConf>

	try
	{
		ServerManager manager;

		// Temporary: listen on a hardcoded port until config parsing is implemented.
		(void)argv;
		int port = DEFAULT_PORT;
		manager.addListenPort(port);// replace this with the vector of serverconfs later.

		manager.run();
	}
	catch (const FatalException& e)
	{
		std::cerr << "Fatal: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
