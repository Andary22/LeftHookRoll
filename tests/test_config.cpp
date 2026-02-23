#include <iostream>
#include <cassert>
#include <cstring>
#include <arpa/inet.h>
#include "AllowedMethods.hpp"
#include "LocationConf.hpp"
#include "ServerConf.hpp"
#include "ConfigParser.hpp"

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
// AllowedMethods tests
// ============================================================================

static void testAllowedMethods()
{
	std::cout << "\n-- AllowedMethods --\n";

	AllowedMethods am;
	check("default bitmap is 0",        am.getBitmap() == 0);
	check("GET not allowed by default", !am.isAllowed(GET));

	am.addMethod(GET);
	check("GET allowed after addMethod",  am.isAllowed(GET));
	check("POST still not allowed",       !am.isAllowed(POST));

	am.addMethod(POST);
	check("POST allowed after addMethod", am.isAllowed(POST));

	am.removeMethod(GET);
	check("GET removed",                  !am.isAllowed(GET));
	check("POST unaffected by removal",   am.isAllowed(POST));

	am.clear();
	check("bitmap cleared to 0",          am.getBitmap() == 0);

	// Copy
	am.addMethod(DELETE);
	AllowedMethods am2(am);
	check("copy ctor copies bitmap",      am2.isAllowed(DELETE));

	AllowedMethods am3;
	am3 = am;
	check("assignment copies bitmap",     am3.isAllowed(DELETE));
}

// ============================================================================
// LocationConf tests
// ============================================================================

static void testLocationConf()
{
	std::cout << "\n-- LocationConf --\n";

	LocationConf loc;
	check("default autoindex is false",   !loc.getAutoIndex());
	check("default path is empty",        loc.getPath().empty());

	loc.setPath("/api");
	check("setPath",                      loc.getPath() == "/api");

	loc.setRoot("/var/www");
	check("setRoot",                      loc.getRoot() == "/var/www");

	loc.setAutoIndex(true);
	check("setAutoIndex true",            loc.getAutoIndex());

	loc.setDefaultPage("index.html");
	check("setDefaultPage",               loc.getDefaultPage() == "index.html");

	loc.setStorageLocation("/tmp/up");
	check("setStorageLocation",           loc.getStorageLocation() == "/tmp/up");

	loc.setReturnCode("301");
	loc.setReturnURL("https://example.com");
	check("setReturnCode",                loc.getReturnCode() == "301");
	check("setReturnURL",                 loc.getReturnURL() == "https://example.com");

	loc.addAllowedMethod(GET);
	loc.addAllowedMethod(POST);
	check("GET allowed via addAllowedMethod",   loc.isMethodAllowed(GET));
	check("POST allowed via addAllowedMethod",  loc.isMethodAllowed(POST));
	check("DELETE not allowed",                 !loc.isMethodAllowed(DELETE));
}

// ============================================================================
// ServerConf tests
// ============================================================================

static void testServerConf()
{
	std::cout << "\n-- ServerConf --\n";

	ServerConf conf;
	check("default maxBodySize is 0",     conf.getMaxBodySize() == 0);
	check("default serverName is empty",  conf.getServerName().empty());

	conf.setServerName("example.com");
	check("setServerName",                conf.getServerName() == "example.com");

	conf.setMaxBodySize(1024);
	check("setMaxBodySize",               conf.getMaxBodySize() == 1024);

	conf.addErrorPage("404", "/404.html");
	check("addErrorPage + getErrorPagePath", conf.getErrorPagePath("404") == "/404.html");
	check("missing code returns empty",      conf.getErrorPagePath("500").empty());

	LocationConf loc;
	loc.setPath("/");
	conf.addLocation(loc);
	check("addLocation adds to vector",   conf.getLocations().size() == 1);
}

// ============================================================================
// ConfigParser tests
// ============================================================================

static void testConfigParser()
{
	std::cout << "\n-- ConfigParser --\n";

	ConfigParser parser("tests/webserv.conf");
	std::vector<ServerConf> servers = parser.parse();

	check("two server blocks parsed",      servers.size() == 2);

	// --- First server ---
	const ServerConf& s0 = servers[0];
	check("s0 server_name",                s0.getServerName() == "example.com");
	check("s0 maxBodySize (10M)",          s0.getMaxBodySize() == 10 * 1024 * 1024);
	check("s0 error_page 404",             s0.getErrorPagePath("404") == "/errors/404.html");
	check("s0 error_page 500",             s0.getErrorPagePath("500") == "/errors/500.html");
	check("s0 listen port 8080",           ntohs(s0.getInterfacePortPair().sin_port) == 8080);

	// Check IP was parsed (127.0.0.1)
	struct in_addr expected;
	inet_aton("127.0.0.1", &expected);
	check("s0 listen IP 127.0.0.1",
		s0.getInterfacePortPair().sin_addr.s_addr == expected.s_addr);

	check("s0 three location blocks",      s0.getLocations().size() == 3);

	const LocationConf& root = s0.getLocations()[0];
	check("s0 loc[0] path = /",            root.getPath() == "/");
	check("s0 loc[0] root",                root.getRoot() == "/var/www/html");
	check("s0 loc[0] GET allowed",         root.isMethodAllowed(GET));
	check("s0 loc[0] POST allowed",        root.isMethodAllowed(POST));
	check("s0 loc[0] DELETE not allowed",  !root.isMethodAllowed(DELETE));
	check("s0 loc[0] autoindex off",       !root.getAutoIndex());
	check("s0 loc[0] index = index.html",  root.getDefaultPage() == "index.html");

	const LocationConf& upload = s0.getLocations()[1];
	check("s0 loc[1] upload_store",        upload.getStorageLocation() == "/tmp/uploads");

	const LocationConf& redir = s0.getLocations()[2];
	check("s0 loc[2] return code 301",     redir.getReturnCode() == "301");
	check("s0 loc[2] return URL",          redir.getReturnURL() == "https://example.com/new");

	// --- Second server ---
	const ServerConf& s1 = servers[1];
	check("s1 server_name",                s1.getServerName() == "api.example.com");
	check("s1 maxBodySize (1K)",           s1.getMaxBodySize() == 1024);
	check("s1 listen port 9090",           ntohs(s1.getInterfacePortPair().sin_port) == 9090);

	const LocationConf& api = s1.getLocations()[0];
	check("s1 loc[0] GET",    api.isMethodAllowed(GET));
	check("s1 loc[0] POST",   api.isMethodAllowed(POST));
	check("s1 loc[0] DELETE", api.isMethodAllowed(DELETE));
}

// ============================================================================
// ConfigParser error tests
// ============================================================================

static void testConfigParserErrors()
{
	std::cout << "\n-- ConfigParser error handling --\n";

	{
		ConfigParser p("tests/nonexistent.conf");
		try { p.parse(); check("throws on missing file", false); }
		catch (const ConfigParser::ConfigException&) { check("throws on missing file", true); }
	}
}

// ============================================================================
// Entry point
// ============================================================================

int main()
{
	testAllowedMethods();
	testLocationConf();
	testServerConf();
	testConfigParser();
	testConfigParserErrors();

	std::cout << "\n===========================\n";
	std::cout << g_passed << " / " << g_total << " tests passed\n";
	std::cout << "===========================\n";

	return (g_passed == g_total) ? 0 : 1;
}
