#!/bin/bash

echo "Testing Virtual Hosting with Server Names"
echo "=========================================="

# Test 1: example.com should serve index.html
echo "Test 1: Requesting example.com:8080"
curl -H "Host: example.com:8080" http://localhost:8080/ -s -o /dev/null -w "Status: %{http_code}\n"

# Test 2: test.com should serve about.html  
echo "Test 2: Requesting test.com:8080"
curl -H "Host: test.com:8080" http://localhost:8080/ -s -o /dev/null -w "Status: %{http_code}\n"

# Test 3: localhost:8081 should serve index.html
echo "Test 3: Requesting localhost:8081"
curl -H "Host: localhost:8081" http://localhost:8081/ -s -o /dev/null -w "Status: %{http_code}\n"

# Test 4: Unknown hostname should fallback to first server on port
echo "Test 4: Requesting unknown.com:8080 (should fallback)"
curl -H "Host: unknown.com:8080" http://localhost:8080/ -s -o /dev/null -w "Status: %{http_code}\n"

echo "=========================================="
echo "Virtual hosting test completed!"
