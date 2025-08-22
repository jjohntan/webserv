# WebServ - HTTP Server

A high-performance HTTP server built in C++98 for the 42 School curriculum.

## 🚀 Features

- **HTTP/1.1 Compliant**: Full HTTP/1.1 protocol support
- **CGI Support**: Execute Python scripts with proper environment setup
- **File Upload**: Multipart form data handling for file uploads
- **Multiple Servers**: Support for multiple server configurations
- **NGINX-style Config**: Configuration files similar to NGINX
- **Non-blocking I/O**: Uses poll() for efficient concurrent connections
- **Error Handling**: Custom error pages and graceful error handling
- **Directory Listing**: Auto-generated directory listings
- **Method Support**: GET, POST, DELETE methods

## 🛠️ Technology Stack

- **C++98**: Core server implementation
- **HTTP/1.1**: Protocol compliance
- **CGI**: Common Gateway Interface for dynamic content
- **Python**: CGI script execution
- **Socket Programming**: Network communication
- **poll()**: Non-blocking I/O multiplexing

## 📁 Project Structure

```
webserv/
├── cgi_bin/
│   └── python/
│       ├── hello.py          # CGI test script
│       ├── upload.py         # File upload handler
│       └── test.py           # Simple CGI test
├── cgi_handler/
│   ├── cgi.cpp              # CGI implementation
│   └── cgi.hpp              # CGI header
├── config_files/
│   ├── config.cpp           # Configuration parser
│   └── config.hpp           # Configuration header
├── http/
│   ├── HTTPRequest/         # HTTP request handling
│   └── HTTPResponse/        # HTTP response handling
├── pages/
│   ├── error/               # Error pages
│   ├── upload/              # Upload directory
│   └── www/                 # Web root
├── testconfig/
│   └── test.conf            # Configuration file
├── server.cpp               # Main server implementation
├── server.hpp               # Server header
├── main.cpp                 # Entry point
└── Makefile                 # Build configuration
```

## 🏗️ Building the Project

```bash
# Clone the repository
git clone <repository-url>
cd webserv

# Make CGI scripts executable
chmod +x cgi_bin/python/*.py

# Build the project
make

# Run the server
./webserver testconfig/test.conf
```

## ⚙️ Configuration

The server uses NGINX-style configuration files. Example configuration:

```nginx
server {
    listen 127.0.0.1:8080;
    server_name localhost example.com;
    
    root ./pages/www;
    client_max_body_size 10485760;  # 10MB
    
    error_page 400 /pages/error/400.html;
    error_page 404 /pages/error/404.html;
    error_page 408 /pages/error/408.html;
    error_page 500 /pages/error/500.html;
    
    location / {
        index index.html;
        allowed_methods GET POST DELETE;
        autoindex on;
    }
    
    location /cgi-bin/ {
        root ./cgi_bin;
        allowed_methods GET POST;
        cgi_extension .py /usr/bin/python3;
    }
    
    location /upload/ {
        root ./pages/upload;
        allowed_methods GET POST DELETE;
        upload_path ./pages/upload;
        autoindex on;
    }
}
```

## 🌐 Usage

### Starting the Server

```bash
./webserver [configuration_file]
```

If no configuration file is provided, the server will use a default configuration.

### Accessing the Server

- **Homepage**: http://localhost:8080/
- **CGI Test**: http://localhost:8080/cgi-bin/python/hello.py
- **File Upload**: http://localhost:8080/cgi-bin/python/upload.py
- **Upload Directory**: http://localhost:8080/upload/

### CGI Scripts

The server supports Python CGI scripts. Example CGI script:

```python
#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>Hello from CGI!</h1>")
print(f"<p>Request Method: {os.environ.get('REQUEST_METHOD', 'Unknown')}</p>")
print("</body></html>")
```

### File Upload

The server supports file uploads through CGI scripts. The upload system includes:

- File upload interface
- File management (view, delete)
- Size validation
- Error handling

## 🔧 Development

### Compilation Flags

The project is compiled with:
- `-std=c++98`: C++98 standard compliance
- `-Wall -Wextra -Werror`: Strict warning handling

### Makefile Targets

- `make`: Build the project
- `make clean`: Remove object files
- `make fclean`: Remove all compiled files
- `make re`: Rebuild the project

## 🧪 Testing

### Manual Testing

1. **Basic HTTP**: Visit http://localhost:8080/
2. **CGI Scripts**: Test http://localhost:8080/cgi-bin/python/hello.py
3. **File Upload**: Use http://localhost:8080/cgi-bin/python/upload.py
4. **Error Pages**: Try accessing non-existent files

### Automated Testing

```bash
# Test with curl
curl -X GET http://localhost:8080/
curl -X POST http://localhost:8080/cgi-bin/python/hello.py
curl -X DELETE http://localhost:8080/upload/test.txt
```

## 📋 Requirements Compliance

### Mandatory Features

- ✅ HTTP/1.1 protocol support
- ✅ GET, POST, DELETE methods
- ✅ CGI support (Python)
- ✅ File upload capability
- ✅ Multiple server configurations
- ✅ Error pages
- ✅ Non-blocking I/O with poll()
- ✅ NGINX-style configuration
- ✅ C++98 standard compliance

### Technical Requirements

- ✅ No external libraries (except standard C++98)
- ✅ Proper error handling
- ✅ Memory management
- ✅ Socket programming
- ✅ Process management (fork/exec for CGI)

## 🐛 Troubleshooting

### Common Issues

1. **Permission Denied**: Make sure CGI scripts are executable
   ```bash
   chmod +x cgi_bin/python/*.py
   ```

2. **Port Already in Use**: Change the port in the configuration file

3. **CGI Scripts Not Working**: Check Python path in configuration

4. **File Upload Fails**: Verify upload directory permissions

### Debug Mode

The server includes debug output. Check console output for:
- Request parsing information
- CGI execution details
- File path resolution
- Error messages

## 📄 License

This project is part of the 42 School curriculum.

## 👥 Contributors

- Built for 42 School WebServ project
- C++98 implementation
- HTTP/1.1 compliance
- CGI support with Python

---

**Note**: This server is designed for educational purposes and should not be used in production without additional security measures.
