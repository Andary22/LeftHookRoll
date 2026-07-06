# Tutorial: Serving your website via LeftHookRoll

This guide covers compiling LeftHookRoll, defining server configurations, setting up routing, and executing the server.

## Prerequisites

leftHookRoll is a very lightweight project, you need only a linux system with a compiler that supports at least C++98

## Build Steps

Navigate to the root directory and run:
```
make
```

## Server Configuration

LeftHookRoll relies on a dedicated configuration file to define server blocks and routing logic. We provide a helper script to generate a basic configuration file. Run the following command to create a configuration file and provide the paths/methods etc when prompted.
```
./autoconf.sh
```

### Execution Example
Such config file example can look like:

```nginx
server {
    listen 127.16.16.0:8080;
    client_max_body_size 5M;
    error_page 404 /errors/404.html;

    # Serve static assets (read-only)
    location / {
        root /var/www/my_website;
        index index.html;
        methods GET;
    }

    # Handle backend CGI scripts for dynamic content
    location /api {
        root /var/www/api;
        methods GET POST;
        cgi_interpreter /usr/bin/python3 .py;
        cgi_interpreter /bin/bash .sh;
    }
}
```


## Running
You can then boot the server by passing the configuration file as the primary argument:

`./server server.conf`