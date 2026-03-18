#include <iostream>
#include <cassert>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../includes/CGIManager.hpp"
#include "../includes/Request.hpp"

// g++ ./test_cgi.cpp ../src/CGIManager.cpp ../src/Request.cpp ../src/DataStore.cpp ../src/AllowedMethods.cpp -o test_cgi
// ============================================================================
// Minimal test harness
// ============================================================================

static int  g_total  = 0;
static int  g_passed = 0;

static void check(const char* label, bool condition)
{
	g_total++;
	if (condition)
	{
		g_passed++;
		std::cout << "  [PASS] " << label << "\n";
	}
	else
	{
		std::cout << "  [FAIL] " << label << "\n";
	}
}

// ============================================================================
// Helper for creating a minimal Request object
// ============================================================================

static Request createTestRequest()
{
	Request req;
	// Parse a simple GET request
	std::string rawBuffer = "GET /index.html?foo=bar HTTP/1.1\r\n"
	                        "Host: localhost\r\n"
	                        "Content-Length: 0\r\n"
	                        "\r\n";
	req.parseHeaders(rawBuffer);
	return req;
}

static Request createPostRequest()
{
	Request req;
	// Parse a simple POST request
	std::string rawBuffer = "POST /cgi-bin/script.py HTTP/1.1\r\n"
	                        "Host: localhost\r\n"
	                        "Content-Type: application/x-www-form-urlencoded\r\n"
	                        "Content-Length: 13\r\n"
	                        "\r\n"
	                        "name=test&age=25";
	req.parseHeaders(rawBuffer);
	return req;
}

// ============================================================================
// CGIManager tests
// ============================================================================

static void testCGIManagerConstructor()
{
	std::cout << "\n-- CGIManager Constructor Tests --\n";

	CGIManager cgi;
	check("Default constructor initializes pid to -1", cgi.getPid() == -1);
	check("Default constructor initializes output fd to -1", cgi.getOutputFd() == -1);
}

static void testCGIManagerCopy()
{
	std::cout << "\n-- CGIManager Copy/Assignment Tests --\n";

	CGIManager cgi1;
	CGIManager cgi2 = cgi1;
	check("Copy constructor works", cgi2.getPid() == cgi1.getPid());

	CGIManager cgi3;
	cgi3 = cgi1;
	check("Assignment operator works", cgi3.getPid() == cgi1.getPid());
}

static void testPrepareWithGetRequest()
{
	std::cout << "\n-- CGIManager::prepare() with GET Request --\n";

	CGIManager cgi;
	Request req = createTestRequest();

	try {
		cgi.prepare(req, "/var/www/cgi-bin/script.py");
		check("prepare() succeeds with valid request and script path", true);
	} catch (const std::exception& e) {
		check("prepare() succeeds with valid request and script path", false);
		std::cerr << "  Exception: " << e.what() << "\n";
	}
}

static void testPrepareWithPostRequest()
{
	std::cout << "\n-- CGIManager::prepare() with POST Request --\n";

	CGIManager cgi;
	Request req = createPostRequest();

	try {
		cgi.prepare(req, "/var/www/cgi-bin/script.pl");
		check("prepare() succeeds with POST request", true);
	} catch (const std::exception& e) {
		check("prepare() succeeds with POST request", false);
		std::cerr << "  Exception: " << e.what() << "\n";
	}
}

static void testExecuteWithEchoScript()
{
	std::cout << "\n-- CGIManager::execute() with Simple Script --\n";

	CGIManager cgi;
	Request req = createTestRequest();

	try {
		// Use a simple shell script as the CGI script
		cgi.prepare(req, "/bin/echo");
		check("prepare() succeeds before execute", true);

		// Create a temporary input file
		int tempFd = open("/tmp/test_cgi_input", O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (tempFd < 0) {
			check("open temp file succeeds", false);
			return;
		}
		write(tempFd, "test input", 10);
		close(tempFd);

		// Open it for reading
		int readFd = open("/tmp/test_cgi_input", O_RDONLY);
		if (readFd < 0) {
			check("open temp file for reading succeeds", false);
			unlink("/tmp/test_cgi_input");
			return;
		}

		cgi.execute(readFd);
		close(readFd);
		check("execute() succeeds with valid fd", true);

		// Check if process was created
		check("getPid() returns valid pid after execute", cgi.getPid() > 0);

		// Check output fd
		int outFd = cgi.getOutputFd();
		check("getOutputFd() returns valid fd after execute", outFd >= 0);

		// Clean up the process
		int status;
		waitpid(cgi.getPid(), &status, 0);

		unlink("/tmp/test_cgi_input");
	} catch (const std::exception& e) {
		check("execute() succeeds with simple script", false);
		std::cerr << "  Exception: " << e.what() << "\n";
	}
}

static void testIsDone()
{
	std::cout << "\n-- CGIManager::isDone() --\n";

	CGIManager cgi;

	// Before any execution
	check("isDone() returns true for uninitialized process", cgi.isDone());

	Request req = createTestRequest();

	try {
		cgi.prepare(req, "/bin/true");

		// Create a temporary input file
		int tempFd = open("/tmp/test_cgi_isdone", O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (tempFd >= 0) {
			write(tempFd, "", 0);
			close(tempFd);

			tempFd = open("/tmp/test_cgi_isdone", O_RDONLY);
			cgi.execute(tempFd);
			close(tempFd);

			// Give the child some time to complete
			usleep(100000); // 100ms

			check("isDone() eventually returns true after process completes", cgi.isDone());

			unlink("/tmp/test_cgi_isdone");
		}
	} catch (const std::exception& e) {
		check("isDone() test executes without exception", false);
		std::cerr << "  Exception: " << e.what() << "\n";
	}
}

static void testPipeCreation()
{
	std::cout << "\n-- CGIManager Pipe Tests --\n";

	CGIManager cgi;
	Request req = createTestRequest();

	try {
		cgi.prepare(req, "/bin/cat");

		int tempFd = open("/tmp/test_cgi_pipe", O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (tempFd >= 0) {
			write(tempFd, "Hello from CGI\n", 15);
			close(tempFd);

			tempFd = open("/tmp/test_cgi_pipe", O_RDONLY);
			cgi.execute(tempFd);
			close(tempFd);

			// Read from the output pipe
			int outFd = cgi.getOutputFd();
			char buffer[256] = {0};
			ssize_t bytesRead = read(outFd, buffer, sizeof(buffer) - 1);

			check("Output pipe returns readable fd", bytesRead >= 0);
			check("Output pipe can read data from script", bytesRead > 0);

			// Clean up
			int status;
			waitpid(cgi.getPid(), &status, 0);

			unlink("/tmp/test_cgi_pipe");
		}
	} catch (const std::exception& e) {
		check("Pipe creation test executes without exception", false);
		std::cerr << "  Exception: " << e.what() << "\n";
	}
}

static void testMultipleCGIInstances()
{
	std::cout << "\n-- Multiple CGIManager Instances --\n";

	CGIManager cgi1, cgi2, cgi3;

	check("Multiple instances can be created", true);
	check("Instance 1 initializes correctly", cgi1.getPid() == -1);
	check("Instance 2 initializes correctly", cgi2.getPid() == -1);
	check("Instance 3 initializes correctly", cgi3.getPid() == -1);

	Request req1 = createTestRequest();
	Request req2 = createPostRequest();

	try {
		cgi1.prepare(req1, "/bin/echo");
		cgi2.prepare(req2, "/bin/sh");
		check("Multiple instances can be prepared independently", true);
	} catch (const std::exception& e) {
		check("Multiple instances can be prepared independently", false);
	}
}

// ============================================================================
// Main test runner
// ============================================================================

int main()
{
	std::cout << "========================================\n";
	std::cout << "   CGIManager Test Suite\n";
	std::cout << "========================================\n";

	testCGIManagerConstructor();
	testCGIManagerCopy();
	testPrepareWithGetRequest();
	testPrepareWithPostRequest();
	testExecuteWithEchoScript();
	testIsDone();
	testPipeCreation();
	testMultipleCGIInstances();

	std::cout << "\n========================================\n";
	std::cout << "Test Results: " << g_passed << "/" << g_total << " passed\n";
	std::cout << "========================================\n";

	return (g_passed == g_total) ? 0 : 1;
}
