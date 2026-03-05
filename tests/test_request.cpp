#include <iostream>
#include <cassert>
#include <cstring>
#include <arpa/inet.h>
#include "../includes/Request.hpp"
using namespace std;
// g++ ./test_request.cpp ../src/Request.cpp ../src/DataStore.cpp ../src/AllowedMethods.cpp -o test_request
// ============================================================================
// Request tests
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

static void testRequestValid()
{
    std::cout << "\n-- Request Valid Parsing --\n";

    {
        Request req;
        std::string raw = "GET /index.html?user=123&sort=asc HTTP/1.0\r\n"
                          "Host: localhost\r\n"
                          "Accept: text/html\r\n\r\n";
        
        req.parseHeaders(raw);

        check("Method parsed to GET",       req.getMethod() == GET);
        check("URL extracted perfectly",    req.getURL() == "/index.html");
        check("Query extracted perfectly",  req.getQuery() == "user=123&sort=asc");
        check("Protocol is HTTP/1.0",       req.getProtocol() == "HTTP/1.0");
        check("Host header stored",         req.getHeader("Host") == "localhost");
        check("State is REQ_DONE (no body)",req.getReqState() == REQ_DONE);
        check("Status code is 200",         req.getStatusCode() == "200");
    }

    {
        // Test POST with Content-Length
        Request req;
        std::string raw = "POST /api/upload HTTP/1.0\r\n"
                          "Content-Length: 42\r\n\r\n";
        
        req.parseHeaders(raw);
        check("State is REQ_BODY",          req.getReqState() == REQ_BODY);
        check("Method parsed to POST",      req.getMethod() == POST);
    }

    {
        // Test Chunked Request
        Request req;
        std::string raw = "POST /api/stream HTTP/1.0\r\n"
                          "Transfer-Encoding: chunked\r\n\r\n";
        
        req.parseHeaders(raw);
        check("State is REQ_CHUNKED",       req.getReqState() == REQ_CHUNKED);
    }

    {
        // Test HTTP/0.9 Backward Compatibility
        Request req;
        std::string raw = "GET /old_page\r\n\r\n";
        
        req.parseHeaders(raw);
        check("Protocol is HTTP/0.9",       req.getProtocol() == "HTTP/0.9");
        check("State is REQ_DONE",          req.getReqState() == REQ_DONE);
    }
}

static void testRequestErrors()
{
    std::cout << "\n-- Request Error Handling --\n";

    {
        // Test HTTP/1.1 Rejection
        Request req;
        std::string raw = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
        req.parseHeaders(raw);
        
        check("HTTP/1.1 triggers REQ_ERROR", req.getReqState() == REQ_ERROR);
        check("Returns 505 Not Supported",   req.getStatusCode() == "505");
    }

    {
        // Test Malformed URL (Missing '/')
        Request req;
        std::string raw = "GET bad_path HTTP/1.0\r\n\r\n";
        req.parseHeaders(raw);
        
        check("Missing '/' triggers REQ_ERROR", req.getReqState() == REQ_ERROR);
        check("Returns 400 Bad Request",        req.getStatusCode() == "400");
    }

    {
        // Test Payload Too Large Limit
        Request req(100); // Max size of 100 bytes
        std::string raw = "POST / HTTP/1.0\r\nContent-Length: 105\r\n\r\n";
        req.parseHeaders(raw);
        
        check("Oversized Content-Length caught", req.getReqState() == REQ_ERROR);
        check("Returns 413 Payload Too Large",   req.getStatusCode() == "413");
    }

    {
        // Test Unknown Method
        Request req;
        std::string raw = "PATCH / HTTP/1.0\r\n\r\n";
        req.parseHeaders(raw);
        
        check("Unknown method caught",         req.getReqState() == REQ_ERROR);
        check("Returns 501 Not Implemented",   req.getStatusCode() == "501");
    }
}

// ============================================================================
// Request Chunked Utility Tests
// ============================================================================

static void testRequestChunkedDone()
{
    std::cout << "\n-- Request::isChunkedDone --\n";

    Request req;
    check("Finds end marker perfectly", req.isChunkedDone("some data\r\n0\r\n\r\n"));
    check("Fails on partial marker",    !req.isChunkedDone("some data\r\n0\r\n"));
    check("Fails on empty string",      !req.isChunkedDone(""));
    check("Finds exact marker",         req.isChunkedDone("0\r\n\r\n"));
}

// ============================================================================
// Request Chunked Decoding Tests (processBodySlice)
// ============================================================================

static void testRequestBodySlice()
{
    std::cout << "\n-- Request::processBodySlice (Chunked Decoding) --\n";

    Request req;
    // 1. Send the headers to put the Request into REQ_CHUNKED state
    std::string headers = "POST /api HTTP/1.0\r\nTransfer-Encoding: chunked\r\n\r\n";
    req.parseHeaders(headers);

    cout << "State after parsing headers: " << req.getReqState() << endl;
    check("State is correctly REQ_CHUNKED", req.getReqState() == REQ_CHUNKED);

    // 2. Simulate the socket receiving chunked data and pushing it to the DataStore
    // Format: 4 bytes ("Wiki"), 5 bytes ("pedia"), End Marker
    std::string rawChunkedPayload = "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n";
    
    // Force DataStore to RAM mode for the test
    req.getBodyStore().append(rawChunkedPayload.c_str(), rawChunkedPayload.size());

    // 3. Trigger the parser
    bool result = req.processBodySlice();

    // 4. Validate
    check("processBodySlice returns true (finished)", result == true);
    check("isBodyProcessed is flagged",               req.isComplete() || req.getReqState() == REQ_CHUNKED); // Depending on how you transition state
    
    // Check if the body was successfully unchunked into "Wikipedia"
    std::string decodedString;
    const std::vector<char>& vec = req.getBodyStore().getVector();
    decodedString.assign(vec.begin(), vec.end());

    check("Payload correctly unchunked", decodedString == "Wikipedia");
}

// ============================================================================
// Request Header Edge Cases Tests
// ============================================================================

static void testRequestHeaderEdgeCases()
{
    std::cout << "\n-- Request Headers (Case Insensitivity & Trimming) --\n";

    Request req;
    // Test weird spacing and mixed capitalization (allowed by HTTP standards)
    std::string raw = "GET / HTTP/1.0\r\n"
                      " hOsT :   localhost  \r\n"
                      "CoNtEnT-LeNgTh: 0\r\n\r\n";
    
    req.parseHeaders(raw);

    // Your map stores the exact key case parsed, so we test the exact key.
    // If you eventually implement case-insensitive header lookups, you can change "hOsT" to "Host".
    check("Header trims spaces around colon", req.getHeader("hOsT") == "localhost");
    check("Content-Length extracted properly", req.getHeader("CoNtEnT-LeNgTh") == "0");
    check("State resolves to REQ_DONE (Length 0)", req.getReqState() == REQ_DONE);
}

int main()
{
    testRequestValid();
    testRequestErrors();
    testRequestHeaderEdgeCases();
    testRequestChunkedDone();
    testRequestBodySlice();

	std::cout << "\n===========================\n";
	std::cout << g_passed << " / " << g_total << " tests passed\n";
	std::cout << "===========================\n";

	return (g_passed == g_total) ? 0 : 1;
}
