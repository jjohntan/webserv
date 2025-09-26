#!/usr/bin/env python3
import sys
import os

print("Content-Type: text/html\r\n\r\n", end="")
print("<html><head><title>Error Test</title></head>")
print("<body><h1>This script should cause an error!</h1>")

# Force flush output
sys.stdout.flush()

# Cause a division by zero error
try:
    result = 1 / 0
except ZeroDivisionError as e:
    print(f"<p>Caught division by zero: {e}</p>")
    sys.stdout.flush()

# Cause a file not found error
try:
    with open("/nonexistent/file.txt", "r") as f:
        content = f.read()
except FileNotFoundError as e:
    print(f"<p>Caught file not found: {e}</p>")
    sys.stdout.flush()

# Cause a syntax error by trying to execute invalid code
try:
    exec("invalid syntax here !!!")
except SyntaxError as e:
    print(f"<p>Caught syntax error: {e}</p>")
    sys.stdout.flush()

# Cause a system exit (should be handled gracefully)
try:
    print("<p>About to call sys.exit(1)...</p>")
    sys.stdout.flush()
    sys.exit(1)
except SystemExit as e:
    print(f"<p>Caught system exit: {e}</p>")
    sys.stdout.flush()

print("<p>Script completed successfully (this shouldn't be reached)</p>")
print("</body></html>")
