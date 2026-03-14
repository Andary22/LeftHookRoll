#!/usr/bin/env python3
import os

def get_cookie_value(cookie_str, name):
    if not cookie_str:
        return None
    cookies = cookie_str.split('; ')
    for c in cookies:
        if c.startswith(f"{name}="):
            return c.split('=')[1]
    return None

# 1. Read existing cookie from environment variables
# Your C++ CGIManager should set 'HTTP_COOKIE' from the Request headers
raw_cookies = os.environ.get('HTTP_COOKIE', '')
current_count = get_cookie_value(raw_cookies, 'session_count')

# 2. Increment logic
count = int(current_count) + 1 if current_count and current_count.isdigit() else 1

# 3. Build the Set-Cookie header
# IMPORTANT: http_only must be False so your session.html JS can see it
cookie_header = f"Set-Cookie: session_count={count}; Path=/; Max-Age=3600; SameSite=Lax"

# 4. Output Headers
print("Content-Type: text/html")
print(cookie_header)
print("")  # Critical: Headers/Body separator

# 5. Output Body (Redirect or show info)
print("<html><head>")
print("<meta http-equiv='refresh' content='2;url=/session.html'>")
print("</head><body>")
print(f"<h1>CGI Session Updated!</h1>")
print(f"<p>New Count: <b>{count}</b></p>")
print("<p>Redirecting you back to the dashboard in 2 seconds...</p>")
print("</body></html>")