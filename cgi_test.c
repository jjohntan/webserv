#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("Content-Type: text/plain\r\n\r\n");
    printf("CGI Test Executable\n");
    printf("REQUEST_METHOD: %s\n", getenv("REQUEST_METHOD") ? getenv("REQUEST_METHOD") : "NOT SET");
    printf("QUERY_STRING: %s\n", getenv("QUERY_STRING") ? getenv("QUERY_STRING") : "NOT SET");
    printf("CONTENT_LENGTH: %s\n", getenv("CONTENT_LENGTH") ? getenv("CONTENT_LENGTH") : "NOT SET");
    printf("SCRIPT_NAME: %s\n", getenv("SCRIPT_NAME") ? getenv("SCRIPT_NAME") : "NOT SET");
    return 0;
}

