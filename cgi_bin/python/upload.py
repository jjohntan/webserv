#!/usr/bin/env python3

import os
import sys
import cgi

# Print HTTP headers
print("Content-Type: text/html")
print()  # Empty line required to separate headers from body

# Get environment variables
request_method = os.environ.get('REQUEST_METHOD', 'GET')
query_string = os.environ.get('QUERY_STRING', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')

print("<html>")
print("<head><title>File Upload</title></head>")
print("<body>")
print("<h1>File Upload Test</h1>")

# Handle POST request (file upload)
if request_method == 'POST' and content_length != '0':
    try:
        content_length = int(content_length)
        post_data = sys.stdin.read(content_length)
        
        # Simple multipart parsing for file upload
        if 'multipart/form-data' in os.environ.get('CONTENT_TYPE', ''):
            # Extract boundary
            content_type = os.environ.get('CONTENT_TYPE', '')
            if 'boundary=' in content_type:
                boundary = content_type.split('boundary=')[1]
                boundary = '--' + boundary
                
                # Split by boundary
                parts = post_data.split(boundary)
                
                for part in parts:
                    if 'filename=' in part and 'Content-Type:' in part:
                        # Extract filename
                        lines = part.split('\r\n')
                        filename = None
                        for line in lines:
                            if 'filename=' in line:
                                filename_start = line.find('filename="') + 10
                                filename_end = line.find('"', filename_start)
                                if filename_start > 9 and filename_end > filename_start:
                                    filename = line[filename_start:filename_end]
                                break
                        
                        if filename:
                            # Find the start of file content (after headers)
                            content_start = part.find('\r\n\r\n')
                            if content_start != -1:
                                content_start += 4
                                # Find the end of file content
                                content_end = part.find('\r\n--', content_start)
                                if content_end == -1:
                                    content_end = len(part)
                                
                                file_content = part[content_start:content_end]
                                
                                # Save file
                                upload_path = "./pages/upload"
                                if not os.path.exists(upload_path):
                                    os.makedirs(upload_path)
                                
                                file_path = os.path.join(upload_path, filename)
                                with open(file_path, 'wb') as f:
                                    f.write(file_content.encode('latin1'))
                                
                                print("<p><strong>SUCCESS:</strong> File '", filename, "' uploaded successfully!</p>")
                                break
            else:
                print("<p><strong>ERROR:</strong> No boundary found in content type</p>")
        else:
            print("<p><strong>ERROR:</strong> Invalid content type for file upload</p>")
    except Exception as e:
        print("<p><strong>ERROR:</strong> Error uploading file:", str(e), "</p>")

# Display upload form
print("<h2>Upload File:</h2>")
print("<form method='POST' enctype='multipart/form-data'>")
print("  <p>Select file: <input type='file' name='file' required></p>")
print("  <p><input type='submit' value='Upload File'></p>")
print("</form>")

# Display uploaded files
print("<h2>Uploaded Files:</h2>")
upload_path = "./pages/upload"
try:
    if os.path.exists(upload_path):
        files = os.listdir(upload_path)
        if files:
            print("<ul>")
            for filename in files:
                file_path = os.path.join(upload_path, filename)
                if os.path.isfile(file_path):
                    file_size = os.path.getsize(file_path)
                    print("<li>", filename, " (", file_size, " bytes) - <a href='/upload/", filename, "'>Download</a></li>")
            print("</ul>")
        else:
            print("<p>No files uploaded yet.</p>")
    else:
        print("<p>Upload directory does not exist.</p>")
except Exception as e:
    print("<p>Error reading upload directory:", str(e), "</p>")

print("<h2>Test Links:</h2>")
print("<ul>")
print("<li><a href='/'>Back to Home</a></li>")
print("<li><a href='/cgi-bin/python/hello.py'>CGI Test</a></li>")
print("<li><a href='/upload/'>Upload Directory</a></li>")
print("</ul>")

print("</body>")
print("</html>")
