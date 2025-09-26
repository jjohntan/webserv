#!/usr/bin/env python3
import sys
import os

print("Content-Type: text/html\r\n\r\n", end="")
print("<html><head><title>Crash Test</title></head>")
print("<body><h1>This script should crash!</h1>")

# Force flush output
sys.stdout.flush()

print("<p>About to cause a crash...</p>")
sys.stdout.flush()

# This will cause a crash by calling sys.exit(1) without catching it
print("<p>Calling sys.exit(1) to simulate a crash...</p>")
sys.stdout.flush()

# This should cause the script to exit with code 1
sys.exit(1)

print("<p>This should never be reached</p>")
print("</body></html>")
