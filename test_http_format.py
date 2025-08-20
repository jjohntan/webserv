#!/usr/bin/env python3

# This simulates what your webserver should return for a CGI request
cgi_output = """Content-Type: text/plain

Hello from CGI!
This is a simple CGI test."""

# This is what your webserver should wrap it with:
http_response = """HTTP/1.1 200 OK
Server: webserv/1.0
Content-Type: text/plain
Content-Length: 44

Hello from CGI!
This is a simple CGI test."""

print("=== Raw CGI Output ===")
print(cgi_output)
print("\n=== Full HTTP Response ===")
print(http_response)
