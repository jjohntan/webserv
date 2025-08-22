#!/usr/bin/env python3

import os
import sys

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
print("<h1>CGI Test Page</h1>")

print("<h2>Request Information:</h2>")
print("<p><strong>Request Method:</strong>", request_method, "</p>")
print("<p><strong>Query String:</strong>", query_string or "(empty)", "</p>")
print("<p><strong>Content Length:</strong>", content_length, "</p>")

# Handle POST data
if request_method == 'POST' and content_length != '0':
    try:
        content_length = int(content_length)
        post_data = sys.stdin.read(content_length)
        print("<h2>POST Data:</h2>")
        print("<pre>", post_data, "</pre>")
    except:
        print("<p>Error reading POST data</p>")

print("<h2>Environment Variables:</h2>")
print("<ul>")
for key, value in sorted(os.environ.items()):
    if key.startswith(('HTTP_', 'REQUEST_', 'SERVER_', 'SCRIPT_', 'PATH_', 'QUERY_', 'CONTENT_', 'GATEWAY_', 'REMOTE_')):
        print("<li><strong>", key, ":</strong>", value, "</li>")
print("</ul>")

print("<h2>Test Links:</h2>")
print("<ul>")
print("<li><a href='/'>Back to Home</a></li>")
print("<li><a href='/cgi-bin/python/hello.py?name=World'>Test with parameter</a></li>")
print("<li><a href='/cgi-bin/python/upload.py'>File Upload</a></li>")
print("</ul>")

print("</body>")
print("</html>")