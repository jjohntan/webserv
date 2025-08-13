#!/usr/bin/env python3

import os
import sys
import cgi
import cgitb

# Enable CGI error reporting
cgitb.enable()

# Print HTTP headers
print("Content-Type: text/html")
print()  # Empty line required to separate headers from body

# Get environment variables
request_method = os.environ.get('REQUEST_METHOD', 'GET')
query_string = os.environ.get('QUERY_STRING', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')

print("<html>")
print("<head><title>CGI Test</title></head>")
print("<body>")
print("<h1>CGI Test Script</h1>")
print(f"<p><strong>Request Method:</strong> {request_method}</p>")
print(f"<p><strong>Query String:</strong> {query_string}</p>")

# Display environment variables
print("<h2>Environment Variables:</h2>")
print("<ul>")
for key, value in sorted(os.environ.items()):
    if key.startswith(('HTTP_', 'REQUEST_', 'SERVER_', 'SCRIPT_', 'PATH_', 'QUERY_', 'CONTENT_', 'GATEWAY_', 'REMOTE_')):
        print(f"<li><strong>{key}:</strong> {value}</li>")
print("</ul>")

# Handle POST data
if request_method == 'POST' and content_length != '0':
    try:
        content_length = int(content_length)
        post_data = sys.stdin.read(content_length)
        print(f"<h2>POST Data:</h2>")
        print(f"<p>{post_data}</p>")
    except:
        print("<p>Error reading POST data</p>")

print("</body>")
print("</html>")