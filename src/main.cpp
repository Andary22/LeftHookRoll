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
			parsedConfs.push_back(ServerConf());
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
