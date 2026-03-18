#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "../includes/Response.hpp"
#include "../includes/Request.hpp"
#include "../includes/ServerConf.hpp"
#include "../includes/LocationConf.hpp"

static int g_total  = 0;
static int g_passed = 0;

static void check(const char* label, bool condition) {
    g_total++;
    if (condition) {
        g_passed++;
        std::cout << "  [PASS] " << label << "\n";
    } else {
        std::cout << "  [FAIL] " << label << "\n";
    }
}

static const std::string TEST_ROOT = "/tmp/webserv_test_root";
static const std::string UPLOAD_DIR = "/tmp/webserv_test_uploads";

static void mkdir_p(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

static void writeFile(const std::string& path, const std::string& content) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    write(fd, content.c_str(), content.size());
    close(fd);
}

static void writeFileBytes(const std::string& path, size_t size, char fill) {
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    const size_t chunk = 4096;
    std::vector<char> buf(chunk, fill);
    size_t written = 0;
    while (written < size) {
        size_t toWrite = std::min(chunk, size - written);
        write(fd, &buf[0], toWrite);
        written += toWrite;
    }
    close(fd);
}

static void removeFile(const std::string& path) {
    unlink(path.c_str());
}

static bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static std::string readFile(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return "";
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        out.append(buf, n);
    close(fd);
    return out;
}

static ServerConf makeConf(const std::string& root,
                            HTTPMethod method1 = GET,
                            HTTPMethod method2 = UNKNOWN_METHOD,
                            HTTPMethod method3 = UNKNOWN_METHOD,
                            bool autoIndex = false,
                            const std::string& defaultPage = "",
                            const std::string& storageDir  = "") {
    ServerConf conf;
    conf.setServerName("test");
    conf.setMaxBodySize(10 * 1024 * 1024);

    LocationConf loc;
    loc.setPath("/");
    loc.setRoot(root);
    loc.addAllowedMethod(method1);
    if (method2 != UNKNOWN_METHOD) loc.addAllowedMethod(method2);
    if (method3 != UNKNOWN_METHOD) loc.addAllowedMethod(method3);
    loc.setAutoIndex(autoIndex);
    if (!defaultPage.empty()) loc.setDefaultPage(defaultPage);
    if (!storageDir.empty())  loc.setStorageLocation(storageDir);
    conf.addLocation(loc);
    return conf;
}

static std::string drainResponse(Response& r) {
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0)
        return "";

    int MAX_ITERS = 100000;
    int iters = 0;
    bool done = false;
    while (!done && iters++ < MAX_ITERS)
        done = r.sendSlice(sv[0]);
    close(sv[0]);

    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = recv(sv[1], buf, sizeof(buf), 0)) > 0)
        out.append(buf, n);
    close(sv[1]);
    return out;
}

static std::string headerOf(const std::string& wire) {
    size_t sep = wire.find("\r\n\r\n");
    if (sep == std::string::npos) return wire;
    return wire.substr(0, sep);
}

static std::string bodyOf(const std::string& wire) {
    size_t sep = wire.find("\r\n\r\n");
    if (sep == std::string::npos) return "";
    return wire.substr(sep + 4);
}

static std::string headerValue(const std::string& headers, const std::string& key) {
    std::string search = key + ": ";
    size_t pos = headers.find(search);
    if (pos == std::string::npos) return "";
    size_t start = pos + search.size();
    size_t end   = headers.find("\r\n", start);
    return headers.substr(start, end - start);
}

static size_t toSizeT(const std::string& s) {
    std::istringstream ss(s);
    size_t n = 0;
    ss >> n;
    return n;
}

static size_t countOccurrences(const std::string& haystack, const std::string& needle) {
    if (needle.empty())
        return 0;
    size_t count = 0;
    size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

static Request makeRequest(const std::string& rawHTTP) {
    Request r;
    r.parseHeaders(rawHTTP);
    return r;
}

static void setupFixtures() {
    mkdir_p(TEST_ROOT);
    mkdir_p(TEST_ROOT + "/subdir");
    mkdir_p(UPLOAD_DIR);
    writeFile(TEST_ROOT + "/index.html", "<html><body>hello</body></html>");
    writeFile(TEST_ROOT + "/style.css",  "body { color: red; }");
    writeFile(TEST_ROOT + "/data.json",  "{\"key\":\"value\"}");
    writeFile(TEST_ROOT + "/subdir/page.html", "<p>subpage</p>");
    writeFileBytes(TEST_ROOT + "/large.bin", 64 * 1024, 'A');
}

static void cleanFixtures() {
    removeFile(TEST_ROOT + "/index.html");
    removeFile(TEST_ROOT + "/style.css");
    removeFile(TEST_ROOT + "/data.json");
    removeFile(TEST_ROOT + "/subdir/page.html");
    removeFile(TEST_ROOT + "/large.bin");
    rmdir((TEST_ROOT + "/subdir").c_str());
    rmdir(TEST_ROOT.c_str());
    rmdir(UPLOAD_DIR.c_str());
}

static void testCanonicalForm() {
    std::cout << "\n-- Canonical Form --\n";

    ServerConf conf = makeConf(TEST_ROOT);

    Response r1;
    r1.buildErrorPage("404", conf);
    std::string wire1 = drainResponse(r1);

    Response r2(r1);
    check("copy ctor preserves statusCode",
          r2.getStatusCode() == "404");
    check("copy ctor preserves responsePhrase",
          r2.getResponsePhrase() == "Not Found");

    Response r3;
    r3 = r1;
    check("assign preserves statusCode",  r3.getStatusCode() == "404");
    check("assign preserves phrase",      r3.getResponsePhrase() == "Not Found");

    {
        Response scoped;
        scoped.buildErrorPage("500", conf);
    }
    check("destructor runs without crash", true);
}

static void testBuildErrorPage() {
    std::cout << "\n-- buildErrorPage --\n";

    ServerConf conf = makeConf(TEST_ROOT);

    {
        Response r;
        r.buildErrorPage("404", conf);
        check("404 statusCode",  r.getStatusCode() == "404");
        check("404 phrase",      r.getResponsePhrase() == "Not Found");
        check("state is HEAD",   r.getResponseState() == SENDING_RES_HEAD);

        std::string wire   = drainResponse(r);
        std::string heads  = headerOf(wire);
        std::string body   = bodyOf(wire);

        check("status line starts with HTTP/1.0 404", heads.substr(0, 12) == "HTTP/1.0 404");
        check("Content-Type is text/html",  headerValue(heads, "Content-Type") == "text/html");
        check("body contains 404",          body.find("404") != std::string::npos);
        check("Content-Length matches body",
              toSizeT(headerValue(heads, "Content-Length")) == body.size());
    }

    {
        Response r;
        r.buildErrorPage("500", conf);
        std::string wire = drainResponse(r);
        check("500 status line", headerOf(wire).substr(0, 12) == "HTTP/1.0 500");
    }

    {
        std::string customPage = "/tmp/custom_404.html";
        writeFile(customPage, "<h1>Custom 404</h1>");

        ServerConf confWithCustom = makeConf(TEST_ROOT);
        confWithCustom.addErrorPage("404", customPage);

        Response r;
        r.buildErrorPage("404", confWithCustom);
        std::string wire = drainResponse(r);
        check("custom error page served",
              bodyOf(wire) == "<h1>Custom 404</h1>");
        removeFile(customPage);
    }

    {
        ServerConf confMissingCustom = makeConf(TEST_ROOT);
        confMissingCustom.addErrorPage("404", "/tmp/does_not_exist_at_all.html");

        Response r;
        r.buildErrorPage("404", confMissingCustom);
        std::string wire = drainResponse(r);
        check("missing custom page falls back to default HTML",
              bodyOf(wire).find("404") != std::string::npos);
    }

    {
        ServerConf conf2 = makeConf(TEST_ROOT);
        Response r;
        r.buildErrorPage("404", conf2);
        drainResponse(r);
        r.buildErrorPage("403", conf2);
        std::string wire = drainResponse(r);
        check("re-build clears previous state",
              headerOf(wire).substr(0, 12) == "HTTP/1.0 403");
    }
}

static void testRouting() {
    std::cout << "\n-- buildResponse routing --\n";

    {
        ServerConf conf = makeConf(TEST_ROOT);
        Request req = makeRequest("GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("valid GET dispatched, not an error",
              r.getStatusCode() == "200");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT);
        Request req;
        req.parseHeaders("BAD REQUEST\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("pre-set error status routes to error page",
              r.getStatusCode() != "200");
    }

    {
        ServerConf emptyConf;
        emptyConf.setServerName("test");
        Request req = makeRequest("GET /anything HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, emptyConf);
        check("no matching location yields 404",
              r.getStatusCode() == "404");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("DELETE /index.html HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("method not allowed yields 405",
              r.getStatusCode() == "405");
    }

    {
        ServerConf conf;
        conf.setServerName("test");
        LocationConf loc;
        loc.setPath("/old");
        loc.setReturnCode("301");
        loc.setReturnURL("https://new.example.com/");
        loc.addAllowedMethod(GET);
        conf.addLocation(loc);

        Request req = makeRequest("GET /old HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        std::string wire  = drainResponse(r);
        std::string heads = headerOf(wire);
        check("redirect yields 301",
              heads.substr(0, 12) == "HTTP/1.0 301");
        check("redirect has Location header",
              headerValue(heads, "Location") == "https://new.example.com/");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("PATCH /index.html HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("unknown method (PATCH/PUT/etc.) yields 501 from request parsing",
              r.getStatusCode() == "501");
    }
}

static void testGetServing() {
    std::cout << "\n-- GET serving --\n";

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);

        std::string wire  = drainResponse(r);
        std::string heads = headerOf(wire);
        std::string body  = bodyOf(wire);

        check("existing file - status 200",           r.getStatusCode() == "200");
        check("existing file - Content-Type html",    headerValue(heads, "Content-Type") == "text/html");
        check("existing file - body matches disk",    body == readFile(TEST_ROOT + "/index.html"));
        check("existing file - Content-Length matches",
              toSizeT(headerValue(heads, "Content-Length")) == body.size());
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("GET /style.css HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        std::string wire = drainResponse(r);
        check("css file - Content-Type",
              headerValue(headerOf(wire), "Content-Type") == "text/css");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("GET /data.json HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        std::string wire = drainResponse(r);
        check("json file - Content-Type",
              headerValue(headerOf(wire), "Content-Type") == "application/json");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("GET /ghost.html HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("non-existent file yields 404", r.getStatusCode() == "404");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET, UNKNOWN_METHOD, UNKNOWN_METHOD, false, "index.html");
        Request req = makeRequest("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        std::string body = bodyOf(drainResponse(r));
        check("directory with default page serves index.html",
              body == readFile(TEST_ROOT + "/index.html"));
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET, UNKNOWN_METHOD, UNKNOWN_METHOD, true);
        Request req = makeRequest("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        std::string body = bodyOf(drainResponse(r));
        check("directory with autoindex returns HTML listing",
              body.find("<a href=") != std::string::npos);
        check("autoindex listing contains known file",
              body.find("index.html") != std::string::npos);
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("directory with no autoindex and no default page yields 403",
              r.getStatusCode() == "403");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, GET);
        Request req = makeRequest("GET /subdir/page.html HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        std::string body = bodyOf(drainResponse(r));
        check("nested path served correctly",
              body == readFile(TEST_ROOT + "/subdir/page.html"));
    }
}

static void testLargeFileStreaming() {
    std::cout << "\n-- Large file streaming --\n";

    ServerConf conf = makeConf(TEST_ROOT, GET);
    Request req = makeRequest("GET /large.bin HTTP/1.1\r\nHost: x\r\n\r\n");
    Response r;
    r.buildResponse(req, conf);

    std::string wire  = drainResponse(r);
    std::string heads = headerOf(wire);
    std::string body  = bodyOf(wire);

    check("large file - status 200",  r.getStatusCode() == "200");
    check("large file - full 64KB received",  body.size() == 64 * 1024);
    check("large file - Content-Length matches",
          toSizeT(headerValue(heads, "Content-Length")) == body.size());
    check("large file - all bytes correct (filled with 'A')",
          body == std::string(64 * 1024, 'A'));
}

static void testPostUpload() {
    std::cout << "\n-- POST upload --\n";

    {
        ServerConf conf = makeConf(TEST_ROOT, POST, UNKNOWN_METHOD, UNKNOWN_METHOD,
                                   false, "", UPLOAD_DIR);
        Request req = makeRequest("POST /upload/photo.jpg HTTP/1.1\r\nHost: x\r\nContent-Length: 12\r\n\r\n");
        req.getBodyStore().append("JPEG_CONTENT");

        Response r;
        r.buildResponse(req, conf);

        check("POST upload - 201 Created",      r.getStatusCode() == "201");
        check("POST upload - file on disk",     fileExists(UPLOAD_DIR + "/photo.jpg"));
        check("POST upload - content correct",  readFile(UPLOAD_DIR + "/photo.jpg") == "JPEG_CONTENT");

        std::string wire  = drainResponse(r);
        std::string heads = headerOf(wire);
        check("POST upload - Location header set",
              headerValue(heads, "Location") == "/photo.jpg");

        removeFile(UPLOAD_DIR + "/photo.jpg");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, POST);
        Request req = makeRequest("POST /data HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("POST with no storageLocation yields 501",
              r.getStatusCode() == "501");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, POST, UNKNOWN_METHOD, UNKNOWN_METHOD,
                                   false, "", UPLOAD_DIR);
        Request req = makeRequest("POST /upload/ HTTP/1.1\r\nHost: x\r\n\r\n");
        req.getBodyStore().append("DATA");
        Response r;
        r.buildResponse(req, conf);
        check("POST with no filename in URL falls back to timestamp name",
              r.getStatusCode() == "201");

        struct dirent* entry;
        DIR* d = opendir(UPLOAD_DIR.c_str());
        if (d) {
            while ((entry = readdir(d)) != NULL) {
                std::string name(entry->d_name);
                if (name != "." && name != "..") {
                    removeFile(UPLOAD_DIR + "/" + name);
                }
            }
            closedir(d);
        }
    }
}

static void testDelete() {
    std::cout << "\n-- DELETE --\n";

    {
        std::string target = TEST_ROOT + "/todelete.txt";
        writeFile(target, "bye");

        ServerConf conf = makeConf(TEST_ROOT, DELETE);
        Request req = makeRequest("DELETE /todelete.txt HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);

        check("DELETE - 204 No Content",   r.getStatusCode() == "204");
        check("DELETE - file removed",     !fileExists(target));

        std::string wire  = drainResponse(r);
        std::string heads = headerOf(wire);
        check("DELETE - Content-Length is 0",
              headerValue(heads, "Content-Length") == "0");
        check("DELETE - no body",  bodyOf(wire).empty());
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, DELETE);
        Request req = makeRequest("DELETE /phantom.txt HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("DELETE non-existent file yields 404", r.getStatusCode() == "404");
    }

    {
        ServerConf conf = makeConf(TEST_ROOT, DELETE);
        Request req = makeRequest("DELETE / HTTP/1.1\r\nHost: x\r\n\r\n");
        Response r;
        r.buildResponse(req, conf);
        check("DELETE on a directory yields 403", r.getStatusCode() == "403");
    }
}

static void testWireFormat() {
    std::cout << "\n-- Wire format integrity --\n";

    ServerConf conf = makeConf(TEST_ROOT, GET);
    Request req = makeRequest("GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n");
    Response r;
    r.buildResponse(req, conf);
    std::string wire = drainResponse(r);

    check("wire starts with HTTP/1.0",     wire.substr(0, 8) == "HTTP/1.0");
    check("wire contains blank line sep",  wire.find("\r\n\r\n") != std::string::npos);

    std::string heads = headerOf(wire);
    check("has Date header",       headerValue(heads, "Date").size() > 0);
    check("has Connection close",  headerValue(heads, "Connection") == "close");

    size_t clValue = toSizeT(headerValue(heads, "Content-Length"));
    size_t bodyLen = bodyOf(wire).size();
    check("Content-Length == actual body bytes",  clValue == bodyLen);
    check("total wire = header + CRLF + body",
          wire.size() == heads.size() + 4 + bodyLen);
}

static void testSetCookieHeaders() {
	std::cout << "\n-- Set-Cookie headers --\n";

	{
		ServerConf conf = makeConf(TEST_ROOT, GET);
		Response r;
        Request req = makeRequest("GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n");
        r.buildResponse(req, conf);

		std::string wire = drainResponse(r);
		std::string heads = headerOf(wire);

		check("one Set-Cookie header line is emitted",
			countOccurrences(heads, "Set-Cookie: ") == 1);
		check("session_count initialized to 1",
			heads.find("Set-Cookie: session_count=1; Path=/") != std::string::npos);
	}

	{
		ServerConf conf = makeConf(TEST_ROOT, GET);
		Response r;
        Request req = makeRequest("GET /index.html HTTP/1.1\r\nHost: x\r\nCookie: session_count=41\r\n\r\n");
        r.buildResponse(req, conf);

		std::string wire = drainResponse(r);
		std::string heads = headerOf(wire);

		check("session_count increments existing value",
			heads.find("Set-Cookie: session_count=42; Path=/") != std::string::npos);
	}
}

int main() {
    setupFixtures();

    testCanonicalForm();
    testBuildErrorPage();
    testRouting();
    testGetServing();
    testLargeFileStreaming();
    testPostUpload();
    testDelete();
    testWireFormat();
    testSetCookieHeaders();

    cleanFixtures();

    std::cout << "\n===========================\n";
    std::cout << g_passed << " / " << g_total << " tests passed\n";
    std::cout << "===========================\n";
    return (g_passed == g_total) ? 0 : 1;
}
