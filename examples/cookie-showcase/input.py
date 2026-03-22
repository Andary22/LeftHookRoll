#!/usr/bin/env python3
import sys
import os
import urllib.parse

# 1. Required HTTP Header
# The server needs to know what kind of content you are sending back.
print("Content-Type: text/html\n")

print("<html><body>")
print("<h1>CGI POST Test Results</h1>")

# 2. Check the Request Method
method = os.environ.get("REQUEST_METHOD", "GET")
print(f"<p>Method received: <b>{method}</b></p>")

if method == "POST":
    try:
        # 3. Get the size of the data
        content_length = int(os.environ.get("CONTENT_LENGTH", 0))
        
        # 4. Read the raw data from stdin
        post_data = sys.stdin.read(content_length)
        
        # 5. Parse the URL-encoded data into a dictionary
        fields = urllib.parse.parse_qs(post_data)
        
        print("<h3>Data Received:</h3>")
        print("<ul>")
        for key, value in fields.items():
            # parse_qs returns a list for each key
            print(f"<li>{key}: {value[0]}</li>")
        print("</ul>")
        
    except Exception as e:
        print(f"<p style='color:red;'>Error processing POST: {e}</p>")
else:
    print("<p>Please send a POST request to see the data processing in action.</p>")

print("</body></html>")