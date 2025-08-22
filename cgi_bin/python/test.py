#!/usr/bin/env python3

import os
import sys
from datetime import datetime

# Print HTTP headers
print("Content-Type: text/html")
print()  # Empty line required to separate headers from body

# Get environment variables
request_method = os.environ.get('REQUEST_METHOD', 'GET')
query_string = os.environ.get('QUERY_STRING', '')
server_name = os.environ.get('SERVER_NAME', 'localhost')
server_port = os.environ.get('SERVER_PORT', '8080')

print("""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebServ CGI Test</title>
    <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            max-width: 600px;
            margin: 50px auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
        }
        .container {
            background: rgba(255,255,255,0.1);
            border-radius: 15px;
            padding: 30px;
            backdrop-filter: blur(10px);
        }
        h1 {
            text-align: center;
            margin-bottom: 30px;
            font-size: 2.5em;
        }
        .info-item {
            background: rgba(255,255,255,0.1);
            padding: 15px;
            margin: 10px 0;
            border-radius: 8px;
            border-left: 4px solid #48bb78;
        }
        .info-label {
            font-weight: bold;
            color: #e2e8f0;
            margin-bottom: 5px;
        }
        .info-value {
            color: #f7fafc;
            font-family: monospace;
        }
        .timestamp {
            text-align: center;
            margin-top: 30px;
            opacity: 0.8;
            font-style: italic;
        }
        .nav-link {
            display: inline-block;
            background: rgba(255,255,255,0.2);
            color: white;
            padding: 10px 20px;
            text-decoration: none;
            border-radius: 6px;
            margin: 10px 5px;
            transition: background 0.3s ease;
        }
        .nav-link:hover {
            background: rgba(255,255,255,0.3);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🧪 CGI Test</h1>
        
        <div style="text-align: center; margin-bottom: 30px;">
            <a href="/" class="nav-link">🏠 Home</a>
            <a href="/cgi-bin/python/hello.py" class="nav-link">🔧 Full Test</a>
            <a href="/cgi-bin/python/upload.py" class="nav-link">📁 Upload</a>
        </div>
""")

print(f"""
        <div class="info-item">
            <div class="info-label">Request Method</div>
            <div class="info-value">{request_method}</div>
        </div>
        
        <div class="info-item">
            <div class="info-label">Query String</div>
            <div class="info-value">{query_string or '(empty)'}</div>
        </div>
        
        <div class="info-item">
            <div class="info-label">Server</div>
            <div class="info-value">{server_name}:{server_port}</div>
        </div>
        
        <div class="info-item">
            <div class="info-label">CGI Script</div>
            <div class="info-value">test.py</div>
        </div>
        
        <div class="info-item">
            <div class="info-label">Status</div>
            <div class="info-value">✅ CGI Working!</div>
        </div>
""")

current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
print(f"""
        <div class="timestamp">
            ⏰ Generated at: {current_time}
        </div>
    </div>
</body>
</html>
""")
