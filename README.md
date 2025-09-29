# WebServ

A non-blocking HTTP/1.1 web server written in C++98 that implements CGI support, file serving, and multiple server configurations.

## What it does

This project implements a complete HTTP server that:
- Serves static websites and handles file uploads
- Executes CGI scripts (Python, PHP, etc.) in separate processes
- Supports GET, POST, and DELETE methods
- Handles multiple server configurations with different ports and hosts
- Uses non-blocking I/O with poll()/select() for all operations
- Never crashes or hangs, even under stress
- Provides proper error handling and timeout protection

## Key Features

- **Non-blocking I/O**: Uses poll()/select() for all read/write operations
- **CGI Support**: Executes external scripts with proper environment setup
- **File Operations**: Static file serving, uploads, and directory listing
- **Multi-server**: Listen on multiple ports with different configurations
- **Error Handling**: Comprehensive error pages and status codes
- **Timeout Protection**: Prevents hanging on slow CGI scripts

## Building

```bash
make
```



