#!/bin/bash

# The mandatory blank line after the HTTP headers
printf "Content-Type: text/plain\r\n\r\n"

echo "=== CGI Variable & Query String Test (Bash) ==="
echo ""

# Explicitly check the most critical CGI variables
# The ${VAR:-default} syntax prints "Not Set" if the variable is empty or undefined
echo "QUERY_STRING    : ${QUERY_STRING:-Not Set}"
echo "REQUEST_METHOD  : ${REQUEST_METHOD:-Not Set}"
echo "SERVER_PROTOCOL : ${SERVER_PROTOCOL:-Not Set}"

echo ""
echo "--- All Environment Variables ---"
# Print everything passed to the child process, sorted alphabetically
env | sort