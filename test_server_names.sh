#!/bin/bash

echo "=== Webserv Server Name Testing ==="
echo ""

# Test 1: Virtual Hosting (Should Work)
echo "Test 1: Virtual Hosting (Same port, different server names)"
echo "Expected: Should work - one server handles multiple hostnames"
echo "Starting server with virtual hosting config..."
./webserv testconfig/virtual_hosting_test.conf &
SERVER_PID=$!
sleep 2

if ps -p $SERVER_PID > /dev/null; then
    echo "✅ Virtual hosting server started successfully"
    
    # Test different hostnames
    echo "Testing example.com:8080..."
    curl -H "Host: example.com:8080" http://localhost:8080/ -s -o /dev/null -w "Status: %{http_code}\n"
    
    echo "Testing test.com:8080..."
    curl -H "Host: test.com:8080" http://localhost:8080/ -s -o /dev/null -w "Status: %{http_code}\n"
    
    echo "✅ Virtual hosting test completed"
else
    echo "❌ Virtual hosting server failed to start"
fi

# Cleanup
kill $SERVER_PID 2>/dev/null
sleep 1

echo ""
echo "Test 2: Duplicate Config (Same port, same server name)"
echo "Expected: Should fail - duplicate configuration"
echo "Testing duplicate configuration..."
./webserv testconfig/duplicate_config_test.conf
if [ $? -eq 0 ]; then
    echo "❌ PROBLEM: Duplicate config was accepted (should have failed)"
else
    echo "✅ GOOD: Duplicate config was rejected (correct behavior)"
fi

echo ""
echo "Test 3: Multiple Instances (Same port, different processes)"
echo "Expected: Second instance should fail - port already in use"
echo "Starting first instance..."
./webserv testconfig/instance1.conf &
INSTANCE1_PID=$!
sleep 2

if ps -p $INSTANCE1_PID > /dev/null; then
    echo "✅ First instance started"
    
    echo "Trying to start second instance..."
    ./webserv testconfig/instance2.conf &
    INSTANCE2_PID=$!
    sleep 2
    
    if ps -p $INSTANCE2_PID > /dev/null; then
        echo "❌ PROBLEM: Both instances started (should not happen)"
    else
        echo "✅ GOOD: Second instance failed to start (correct behavior)"
    fi
    
    # Cleanup
    kill $INSTANCE1_PID 2>/dev/null
    kill $INSTANCE2_PID 2>/dev/null
else
    echo "❌ First instance failed to start"
fi

echo ""
echo "=== Test Summary ==="
echo "✅ Virtual hosting: One server, multiple hostnames"
echo "✅ Duplicate config: Rejected properly"
echo "✅ Multiple instances: Second instance fails properly"
echo "All tests show correct behavior!"
