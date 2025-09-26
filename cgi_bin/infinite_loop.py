#!/usr/bin/env python3
import sys
import time

print("Content-Type: text/html\r\n\r\n", end="")
print("<html><head><title>Infinite Loop Test</title></head>")
print("<body><h1>This script should timeout!</h1>")
print("<p>Starting infinite loop...</p>")

# Force flush output
sys.stdout.flush()

# Infinite loop - this should cause a timeout
counter = 0
while True:
    counter += 1
    print(f"<p>Loop iteration: {counter}</p>")
    sys.stdout.flush()
    time.sleep(1)  # Sleep for 1 second each iteration
    
    # Safety break after 200 iterations (in case timeout doesn't work)
    # This should be much longer than the CGI timeout (10 seconds)
    if counter > 200:
        print("<p>Safety break - timeout didn't work!</p>")
        break

print("</body></html>")
