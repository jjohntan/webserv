#!/usr/bin/env python3

import os
import sys

# Print HTTP headers
print("Content-Type: text/html")
print()  # Empty line required to separate headers from body

# Get environment variables
request_method = os.environ.get('REQUEST_METHOD', 'GET')
script_name = os.environ.get('SCRIPT_NAME', '')
query_string = os.environ.get('QUERY_STRING', '')

print("<html>")
print("<head>")
print("<title>DELETE Test</title>")
print("<style>")
print("body { font-family: Arial, sans-serif; margin: 20px; }")
print(".success { background: #d4edda; border: 1px solid #c3e6cb; padding: 10px; border-radius: 5px; }")
print(".error { background: #f8d7da; border: 1px solid #f5c6cb; padding: 10px; border-radius: 5px; }")
print("</style>")
print("</head>")
print("<body>")

print("<h1>DELETE Method Test</h1>")

if request_method == 'DELETE':
    print("<div class='success'>")
    print("<h2>✅ DELETE Request Received Successfully!</h2>")
    print("<p><strong>Method:</strong>", request_method, "</p>")
    print("<p><strong>Script:</strong>", script_name, "</p>")
    print("<p><strong>Query String:</strong>", query_string or "(none)", "</p>")
    print("<p>In a real application, this would delete the resource at:", script_name, "</p>")
    print("</div>")
    
    # Simulate some deletion logic
    print("<h3>Simulated Deletion Process:</h3>")
    print("<ul>")
    print("<li>✅ Resource identified:", script_name, "</li>")
    print("<li>✅ Permission check passed</li>")
    print("<li>✅ Resource deleted successfully</li>")
    print("<li>✅ Response: 204 No Content</li>")
    print("</ul>")
    
else:
    print("<div class='error'>")
    print("<h2>❌ Not a DELETE Request</h2>")
    print("<p>Current method:", request_method, "</p>")
    print("<p>Use curl to test: <code>curl -X DELETE http://localhost:8081/cgi_bin/test_delete.py</code></p>")
    print("</div>")

print("<h2>Test Instructions:</h2>")
print("<ol>")
print("<li>Start your webserv server</li>")
print("<li>Run: <code>curl -X DELETE http://localhost:8081/cgi_bin/test_delete.py</code></li>")
print("<li>You should see a 200 OK response with HTML content</li>")
print("</ol>")

print("<h2>Environment Variables:</h2>")
print("<table border='1' style='border-collapse: collapse;'>")
print("<tr><th>Variable</th><th>Value</th></tr>")
for var in ['REQUEST_METHOD', 'SCRIPT_NAME', 'QUERY_STRING', 'SERVER_NAME', 'SERVER_PORT']:
    value = os.environ.get(var, '(not set)')
    print(f"<tr><td>{var}</td><td>{value}</td></tr>")
print("</table>")

print("</body>")
print("</html>")
