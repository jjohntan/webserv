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

print("<html>")
print("<head><title>File Upload</title></head>")
print("<body>")
print("<h1>File Upload Test</h1>")

# Get environment variables for debugging
request_method = os.environ.get('REQUEST_METHOD', 'GET')
content_type = os.environ.get('CONTENT_TYPE', '')

# Handle POST request (file upload)
if request_method == 'POST':
    try:
        # Use cgi.FieldStorage for proper multipart form handling
        form = cgi.FieldStorage()
        
        # Check if file field exists
        if "file" in form:
            fileitem = form["file"]
            
            # Check if file was actually uploaded
            if fileitem.filename:
                # Create upload directory if it doesn't exist
                upload_path = "pages/upload"
                if not os.path.exists(upload_path):
                    os.makedirs(upload_path, mode=0o755)
                
                # Sanitize filename to prevent path traversal
                filename = os.path.basename(fileitem.filename)
                file_path = os.path.join(upload_path, filename)
                
                # Save the file
                with open(file_path, 'wb') as f:
                    f.write(fileitem.file.read())
                
                # Set proper file permissions
                os.chmod(file_path, 0o644)
                
                print(f"<p><strong>SUCCESS:</strong> File '{filename}' uploaded successfully!</p>")
                print(f"<p>File size: {os.path.getsize(file_path)} bytes</p>")
            else:
                print("<p><strong>ERROR:</strong> No file selected</p>")
        else:
            print("<p><strong>ERROR:</strong> No file field found in form data</p>")
            
    except Exception as e:
        print(f"<p><strong>ERROR:</strong> Error uploading file: {str(e)}</p>")
        import traceback
        print(f"<pre>Debug info:\n{traceback.format_exc()}</pre>")



# Debug information
print("<h3>Debug Information:</h3>")
print(f"<p>Request Method: {request_method}</p>")
print(f"<p>Content Type: {content_type}</p>")
print(f"<p>Content Length: {os.environ.get('CONTENT_LENGTH', 'Not set')}</p>")
print(f"<p>Script Path: {os.path.abspath(__file__)}</p>")
print(f"<p>Current Working Directory: {os.getcwd()}</p>")


print("<h2>Test Links:</h2>")
print("<ul>")
print("<li><a href='/'>Back to Home</a></li>")
print("<li><a href='/cgi_bin/hello.py'>CGI Test</a></li>")
print("<li><a href='/upload/'>Upload Directory</a></li>")
print("</ul>")

print("</body>")
print("</html>")