#!/usr/bin/env python3

import os
import sys
import urllib.parse

# Print HTTP headers
print("Content-Type: text/html")
print()  # Empty line required to separate headers from body

# Get environment variables
request_method = os.environ.get('REQUEST_METHOD', 'GET')
query_string = os.environ.get('QUERY_STRING', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')
script_name = os.environ.get('SCRIPT_NAME', '')

print("<html>")
print("<head>")
print("<title>WebServ CGI Test</title>")
print("<style>")
print("body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f8ff; }")
print("h1 { color: #2c3e50; border-bottom: 2px solid #3498db; }")
print(".method { background: #ecf0f1; padding: 10px; margin: 10px 0; border-radius: 5px; }")
print(".success { background: #d4edda; border: 1px solid #c3e6cb; padding: 10px; border-radius: 5px; }")
print(".error { background: #f8d7da; border: 1px solid #f5c6cb; padding: 10px; border-radius: 5px; }")
print("table { border-collapse: collapse; width: 100%; }")
print("th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }")
print("th { background-color: #f2f2f2; }")
print("</style>")
print("</head>")
print("<body>")

print("<h1>WebServ CGI Test Page</h1>")

# Show current request info
print("<div class='method'>")
print("<h2>Current Request:</h2>")
print("<p><strong>Method:</strong>", request_method, "</p>")
print("<p><strong>Script:</strong>", script_name, "</p>")
print("<p><strong>Query String:</strong>", query_string or "(none)", "</p>")
print("</div>")

# Handle different HTTP methods
if request_method == 'GET':
    print("<div class='success'>")
    print("<h2>GET Request Successful</h2>")
    
    # Parse query string
    if query_string:
        params = urllib.parse.parse_qs(query_string)
        print("<h3>Query Parameters:</h3>")
        print("<table>")
        for key, values in params.items():
            print("<tr><td><strong>" + key + "</strong></td><td>" + ", ".join(values) + "</td></tr>")
        print("</table>")
    else:
        print("<p>No query parameters provided.</p>")
    print("</div>")

elif request_method == 'POST':
    print("<div class='success'>")
    print("<h2>POST Request Successful</h2>")
    
    try:
        if content_length != '0':
            content_length = int(content_length)
            post_data = sys.stdin.read(content_length)
            print("<h3>POST Data Received:</h3>")
            print("<pre>" + post_data + "</pre>")
            
            # Try to parse as form data
            if 'application/x-www-form-urlencoded' in os.environ.get('CONTENT_TYPE', ''):
                params = urllib.parse.parse_qs(post_data)
                print("<h3>Parsed Form Data:</h3>")
                print("<table>")
                for key, values in params.items():
                    print("<tr><td><strong>" + key + "</strong></td><td>" + ", ".join(values) + "</td></tr>")
                print("</table>")
        else:
            print("<p>No POST data received.</p>")
    except Exception as e:
        print("<div class='error'>")
        print("<p>Error processing POST data: " + str(e) + "</p>")
        print("</div>")
    
    print("</div>")

elif request_method == 'DELETE':
    print("<div class='success'>")
    print("<h2>DELETE Request Successful</h2>")
    print("<p>DELETE method is working. In a real application, this would delete a resource.</p>")
    print("<p><strong>Note:</strong> No actual deletion performed in this test.</p>")
    print("</div>")

else:
    print("<div class='error'>")
    print("<h2>Unsupported Method: " + request_method + "</h2>")
    print("<p>This CGI script supports GET, POST, and DELETE methods.</p>")
    print("</div>")

# Show environment variables
print("<h2>CGI Environment Variables:</h2>")
print("<table>")
cgi_vars = ['REQUEST_METHOD', 'QUERY_STRING', 'CONTENT_LENGTH', 'CONTENT_TYPE', 
           'SERVER_NAME', 'SERVER_PORT', 'SCRIPT_NAME', 'PATH_INFO', 
           'SERVER_PROTOCOL', 'GATEWAY_INTERFACE', 'REMOTE_ADDR']

for var in cgi_vars:
    value = os.environ.get(var, '(not set)')
    print("<tr><td><strong>" + var + "</strong></td><td>" + value + "</td></tr>")
print("</table>")

# Test forms
print("<h2>Test Forms:</h2>")

print("<h3>GET Test:</h3>")
print("<form method='GET'>")
print("Name: <input type='text' name='name' value='test'>")
print("Value: <input type='text' name='value' value='123'>")
print("<input type='submit' value='Test GET'>")
print("</form>")

print("<h3>POST Test:</h3>")
print("<form method='POST'>")
print("Username: <input type='text' name='username' value='testuser'>")
print("Message: <textarea name='message'>Hello from POST!</textarea>")
print("<input type='submit' value='Test POST'>")
print("</form>")

print("<h3>File Upload Test:</h3>")
print("<form method='POST' action='/cgi_bin/upload.py' enctype='multipart/form-data'>")
print("Select file: <input type='file' name='file' required>")
print("<input type='submit' value='Upload File'>")
print("</form>")

print("<h3>DELETE Test:</h3>")
print("<p>Use curl to test DELETE:</p>")
print("<pre>curl -X DELETE http://localhost:8081/cgi_bin/hello.py</pre>")

print("<h2>Navigation:</h2>")
print("<ul>")
print("<li><a href='/'>Back to Home</a></li>")
print("<li><a href='/upload/'>Upload Directory</a></li>")
print("</ul>")

print("</body>")
print("</html>")