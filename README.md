*This project has been created as part of the 42 curriculum by  Yaman Alrifai, Hamzah Beliah, and Yousef Kitaneh.*


---
# Instructions
## Configuration File

The server is configured via a text file following Nginx inspired syntax.

### Syntax Rules
- **Blocks** are enclosed in curly braces `{}`. Everything lives inside a `server` block.
- **Directives** end with a semicolon `;`.
- **Comments** start with `#` and extend to the end of the line.
- **Whitespaces** are ignored (spaces, tabs, newlines).
### Server Block

```nginx
server {
    # server directives here
}
```

| Directive | Syntax | Example |
|-----------|--------|---------|
| `listen` | `listen <port>;` or `listen <ip>:<port>;` | `listen 8080;` / `listen 127.0.0.1:8080;` |
| `server_name` | `server_name <name>;` | `server_name example.com;` |
| `client_max_body_size` | `client_max_body_size <size>;` | `client_max_body_size 10M;` |
| `error_page` | `error_page <code> <path>;` | `error_page 404 /errors/404.html;` |
| `location` | `location <path> { ... }` | `location /api { ... }` |


**`client_max_body_size` suffixes:** `k`/`K` (kilobytes), `m`/`M` (megabytes), `g`/`G` (gigabytes). Plain number = bytes.

### Location Block

Defined inside a `server` block. Matched by longest-prefix against the request URL.

```nginx
location /path {
    # location directives here
}
```

| Directive | Syntax | Example |
|-----------|--------|---------|
| `root` | `root <path>;` | `root /var/www/html;` |
| `methods` | `methods <METHOD> [METHOD ...];` | `methods GET POST DELETE;` |
| `autoindex` | `autoindex <on\|off>;` | `autoindex on;` |
| `index` | `index <file>;` | `index index.html;` |
| `upload_store` | `upload_store <path>;` | `upload_store /var/www/uploads;` |
| `return` | `return <code> <url>;` | `return 301 https://new-site.com;` |
| `cgi_interpreter` | `cgi_interpreter <path> <.ext>;` | `cgi_interpreter /usr/bin/python3 .py;` |

### CGI Configuration

CGI is configured per-location using the `cgi_interpreter` directive. Each directive maps a file extension to an interpreter binary. Multiple `cgi_interpreter` directives can be specified in a single location block to support different script types.


If the `cgi_interpreter` path is left as an empty string or the extension's interpreter is auto-detectable (`.py`, `.pl`, `.rb`, `.sh`), the server falls back to a limited built-in interpreter lookup.

```nginx
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 1M;

    error_page 404 /errors/404.html;
    error_page 500 /errors/500.html;

    location / {
        root /var/www/html;
        methods GET;
        index index.html;
        autoindex off;
    }

    location /uploads {
        root /var/www;
        methods GET POST DELETE;
        upload_store /var/www/uploads;
    }

    location /cgi-bin {
        root /var/www;
        methods GET POST;
        cgi_interpreter /usr/bin/python3 .py;
        cgi_interpreter /usr/bin/perl .pl;
        cgi_interpreter /bin/sh .sh;
    }

    location /old-page {
        return 301 https://new-site.com/new-page;
    }
}
```
