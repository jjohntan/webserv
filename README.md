# Webserv

A high-performance HTTP/1.1 server implemented in C++, designed to handle multiple client requests simultaneously using non-blocking I/O and event-driven architecture.

## Description

Webserv is a comprehensive HTTP server built from scratch. It supports core HTTP/1.1 features, including static file serving, CGI execution (e.g., Python scripts), file uploads, and custom error pages. The server is highly improved for performance using `poll()` for I/O multiplexing, allowing it to serve multiple clients concurrently without blocking.

## Features

-   **HTTP/1.1 Compliance**: Supports `GET`, `POST`, and `DELETE` methods.
-   **I/O Multiplexing**: Utilizes `poll()` to handle multiple connections efficiently.
-   **CGI Support**: Executes dynamic scripts (e.g., Python, PHP) via the Common Gateway Interface.
-   **Configuration Parsing**: Fully configurable via a `.conf` file (inspired by Nginx).
    -   Multiple server blocks/ports.
    -   Custom error pages.
    -   Client body size limits.
    -   Route-specific rules (redirects, autoindex, upload paths).
-   **Static File Serving**: Serves HTML, CSS, images, and other static assets.
-   **Directory Listing**: Automatic directory indexing (autoindex).
-   **File Uploads**: Supports `multipart/form-data` and chunked transfer encoding.
-   **HTTP Redirections**: Handles `3xx` redirection status codes.

## Configuration

The server is configured using a file (e.g., `default.conf`). verify directives include:

```nginx
server {
    listen 8080
    server_name localhost
    root ./www

    # Error pages
    error_page 404 /404.html

    # Routes
    location / {
        method GET POST
        autoindex on
        index index.html
    }

    location /cgi-bin {
        cgi_ext .py
    }
}
```

## Getting Started

### Prerequisites

-   `clang++` (C++98 standard)
-   `make`

### Installation & Usage

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/jjohntan/webserv.git
    cd webserv
    ```

2.  **Compile the server:**
    ```bash
    make
    ```

3.  **Run the server:**
    ```bash
    ./webserv [config_file]
    ```
    *If no config file is provided, it defaults to a standard configuration.*

    **Example:**
    ```bash
    ./webserv testconfig/default.conf
    ```
