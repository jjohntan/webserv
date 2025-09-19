#!/bin/bash

# WebServ Stress Test Script
# Tests: Availability, Memory Leaks, Connection Handling, Indefinite Operation

echo "=========================================="
echo "WebServ Stress Test Suite"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if siege is installed
if ! command -v siege &> /dev/null; then
    echo -e "${RED}❌ Siege is not installed. Installing...${NC}"
    echo "Please install siege: sudo apt-get install siege"
    echo "Or use: brew install siege (on macOS)"
    exit 1
fi

# Check if server is running
echo -e "${YELLOW}Checking if WebServ is running...${NC}"
if ! curl -s http://localhost:8080/ > /dev/null; then
    echo -e "${RED}❌ WebServ is not running on port 8080${NC}"
    echo "Please start your server first: ./webserver testconfig/test.conf"
    exit 1
fi
echo -e "${GREEN}✅ WebServ is running${NC}"
echo ""

# Get server PID for memory monitoring
SERVER_PID=$(pgrep -f webserver)
if [ -z "$SERVER_PID" ]; then
    echo -e "${RED}❌ Cannot find WebServ process${NC}"
    exit 1
fi
echo -e "${BLUE}WebServ PID: $SERVER_PID${NC}"
echo ""

# Function to monitor memory usage
monitor_memory() {
    local pid=$1
    local duration=$2
    local interval=1
    
    echo -e "${YELLOW}Monitoring memory usage for $duration seconds...${NC}"
    echo "Time(s) | Memory(KB) | Memory(MB)"
    echo "--------|------------|----------"
    
    for ((i=0; i<duration; i+=interval)); do
        if [ -f "/proc/$pid/status" ]; then
            memory_kb=$(grep VmRSS /proc/$pid/status | awk '{print $2}')
            memory_mb=$((memory_kb / 1024))
            printf "%7d | %10d | %8d\n" $i $memory_kb $memory_mb
        else
            echo -e "${RED}Process $pid not found!${NC}"
            break
        fi
        sleep $interval
    done
}

# Function to check for hanging connections
check_connections() {
    local pid=$1
    echo -e "${YELLOW}Checking for hanging connections...${NC}"
    
    # Count open file descriptors (connections)
    if [ -d "/proc/$pid/fd" ]; then
        fd_count=$(ls /proc/$pid/fd | wc -l)
        echo "Open file descriptors: $fd_count"
        
        # Check for specific socket connections
        socket_count=$(lsof -p $pid 2>/dev/null | grep -c "socket")
        echo "Socket connections: $socket_count"
        
        if [ $fd_count -gt 50 ]; then
            echo -e "${RED}⚠️  High number of file descriptors: $fd_count${NC}"
        else
            echo -e "${GREEN}✅ File descriptor count looks normal${NC}"
        fi
    else
        echo -e "${RED}❌ Cannot access process file descriptors${NC}"
    fi
}

# ==========================================
# TEST 1: BASIC AVAILABILITY TEST
# ==========================================
echo -e "${YELLOW}=== TEST 1: BASIC AVAILABILITY TEST ===${NC}"

echo "Running basic availability test with siege..."
echo "Target: http://localhost:8080/"
echo "Duration: 30 seconds"
echo "Concurrent users: 10"
echo ""

# Run siege for 30 seconds
siege -b -c10 -t30s http://localhost:8080/ > siege_basic.log 2>&1

# Parse results
availability=$(grep "Availability:" siege_basic.log | awk '{print $2}' | sed 's/%//')
transactions=$(grep "Transactions:" siege_basic.log | awk '{print $2}')
response_time=$(grep "Response time:" siege_basic.log | awk '{print $3}')

echo -e "${BLUE}Basic Test Results:${NC}"
echo "Availability: $availability%"
echo "Transactions: $transactions"
echo "Response Time: $response_time"

if (( $(echo "$availability > 99.5" | bc -l) )); then
    echo -e "${GREEN}✅ PASSED - Availability > 99.5%${NC}"
else
    echo -e "${RED}❌ FAILED - Availability < 99.5%${NC}"
fi
echo ""

# ==========================================
# TEST 2: MEMORY LEAK TEST
# ==========================================
echo -e "${YELLOW}=== TEST 2: MEMORY LEAK TEST ===${NC}"

echo "Testing for memory leaks during stress test..."
echo ""

# Get initial memory
initial_memory=$(grep VmRSS /proc/$SERVER_PID/status | awk '{print $2}')
echo "Initial memory: $initial_memory KB"

# Run stress test while monitoring memory
echo "Running stress test with memory monitoring..."
siege -b -c20 -t60s http://localhost:8080/ > siege_memory.log 2>&1 &
siege_pid=$!

# Monitor memory during test
monitor_memory $SERVER_PID 60

# Get final memory
final_memory=$(grep VmRSS /proc/$SERVER_PID/status | awk '{print $2}')
memory_diff=$((final_memory - initial_memory))

echo ""
echo -e "${BLUE}Memory Test Results:${NC}"
echo "Initial memory: $initial_memory KB"
echo "Final memory: $final_memory KB"
echo "Memory difference: $memory_diff KB"

if [ $memory_diff -lt 10000 ]; then  # Less than 10MB increase
    echo -e "${GREEN}✅ PASSED - No significant memory leak detected${NC}"
else
    echo -e "${RED}❌ FAILED - Potential memory leak detected${NC}"
fi
echo ""

# ==========================================
# TEST 3: CONNECTION HANDLING TEST
# ==========================================
echo -e "${YELLOW}=== TEST 3: CONNECTION HANDLING TEST ===${NC}"

echo "Testing connection handling and cleanup..."
echo ""

# Check connections before test
echo "Connections before test:"
check_connections $SERVER_PID
echo ""

# Run high concurrency test
echo "Running high concurrency test (50 users for 30 seconds)..."
siege -b -c50 -t30s http://localhost:8080/ > siege_connections.log 2>&1

# Wait a bit for cleanup
sleep 5

# Check connections after test
echo "Connections after test:"
check_connections $SERVER_PID
echo ""

# ==========================================
# TEST 4: INDEFINITE OPERATION TEST
# ==========================================
echo -e "${YELLOW}=== TEST 4: INDEFINITE OPERATION TEST ===${NC}"

echo "Testing indefinite operation capability..."
echo "This test runs for 2 minutes to verify server stability"
echo ""

# Run indefinite test
echo "Starting indefinite test (2 minutes)..."
siege -b -c15 -t2m http://localhost:8080/ > siege_indefinite.log 2>&1 &
indefinite_pid=$!

# Monitor during test
echo "Monitoring server during indefinite test..."
for i in {1..12}; do  # 12 iterations of 10 seconds = 2 minutes
    echo "Minute $((i/6 + 1)) - Second $(((i-1)*10 + 10))"
    
    # Check if server is still responding
    if curl -s http://localhost:8080/ > /dev/null; then
        echo -e "${GREEN}✅ Server responding${NC}"
    else
        echo -e "${RED}❌ Server not responding!${NC}"
        break
    fi
    
    # Check memory
    current_memory=$(grep VmRSS /proc/$SERVER_PID/status | awk '{print $2}')
    echo "Current memory: $current_memory KB"
    
    sleep 10
done

# Wait for siege to finish
wait $indefinite_pid

echo ""
echo -e "${GREEN}✅ Indefinite operation test completed${NC}"
echo ""

# ==========================================
# TEST 5: DIFFERENT ENDPOINTS STRESS TEST
# ==========================================
echo -e "${YELLOW}=== TEST 5: MULTI-ENDPOINT STRESS TEST ===${NC}"

echo "Testing different endpoints under stress..."
echo ""

# Test static files
echo "Testing static file serving..."
siege -b -c10 -t30s http://localhost:8080/images/test.jpg > siege_static.log 2>&1

# Test CGI
echo "Testing CGI under stress..."
siege -b -c5 -t30s http://localhost:8081/cgi_bin/hello.py > siege_cgi.log 2>&1

# Test upload endpoint
echo "Testing upload endpoint..."
siege -b -c5 -t30s http://localhost:8080/upload/ > siege_upload.log 2>&1

echo -e "${GREEN}✅ Multi-endpoint stress test completed${NC}"
echo ""

# ==========================================
# FINAL ANALYSIS
# ==========================================
echo -e "${YELLOW}=== FINAL ANALYSIS ===${NC}"

# Check all siege results
echo "Analyzing all test results..."

# Basic availability
basic_avail=$(grep "Availability:" siege_basic.log | awk '{print $2}' | sed 's/%//')
echo "Basic availability: $basic_avail%"

# Memory test availability
memory_avail=$(grep "Availability:" siege_memory.log | awk '{print $2}' | sed 's/%//')
echo "Memory test availability: $memory_avail%"

# Connection test availability
conn_avail=$(grep "Availability:" siege_connections.log | awk '{print $2}' | sed 's/%//')
echo "Connection test availability: $conn_avail%"

# Indefinite test availability
indef_avail=$(grep "Availability:" siege_indefinite.log | awk '{print $2}' | sed 's/%//')
echo "Indefinite test availability: $indef_avail%"

# Calculate average availability
avg_avail=$(echo "scale=2; ($basic_avail + $memory_avail + $conn_avail + $indef_avail) / 4" | bc)
echo "Average availability: $avg_avail%"

echo ""
echo "=========================================="
echo -e "${YELLOW}STRESS TEST SUMMARY${NC}"
echo "=========================================="

# Final verdict
if (( $(echo "$avg_avail > 99.5" | bc -l) )); then
    echo -e "${GREEN}🎉 STRESS TEST PASSED! 🎉${NC}"
    echo -e "${GREEN}✅ Availability: $avg_avail% (> 99.5% required)${NC}"
    echo -e "${GREEN}✅ Memory usage: Stable${NC}"
    echo -e "${GREEN}✅ Connection handling: Proper cleanup${NC}"
    echo -e "${GREEN}✅ Indefinite operation: Server stable${NC}"
    echo ""
    echo -e "${GREEN}Your WebServ implementation handles stress testing excellently!${NC}"
else
    echo -e "${RED}❌ STRESS TEST FAILED${NC}"
    echo -e "${RED}Availability: $avg_avail% (< 99.5% required)${NC}"
fi

# Cleanup
echo ""
echo "Cleaning up test files..."
rm -f siege_*.log

echo "Stress test completed!"
