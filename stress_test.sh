#!/bin/bash

# ==============================
# WebServer Stress Testing Script
# ==============================

echo "🚀 Starting WebServer Stress Tests"
echo "=================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVER_CONFIG="testconfig/test.conf"
SERVER_URL="http://127.0.0.1:8080"
CGI_URL="http://127.0.0.1:8081"
STRESS_URL="http://127.0.0.1:8082"
SERVER_PID=""

# Function to start server
start_server() {
    echo -e "${BLUE}📡 Starting webserver...${NC}"
    ./webserver $SERVER_CONFIG &
    SERVER_PID=$!
    sleep 2
    
    # Check if server started successfully
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}❌ Failed to start server${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✅ Server started (PID: $SERVER_PID)${NC}"
}

# Function to stop server
stop_server() {
    if [ ! -z "$SERVER_PID" ]; then
        echo -e "${YELLOW}🛑 Stopping server (PID: $SERVER_PID)...${NC}"
        kill $SERVER_PID 2>/dev/null
        wait $SERVER_PID 2>/dev/null
        echo -e "${GREEN}✅ Server stopped${NC}"
    fi
}

# Function to monitor memory usage
monitor_memory() {
    local duration=$1
    local interval=5
    local count=$((duration / interval))
    
    echo -e "${BLUE}📊 Monitoring memory usage for ${duration}s...${NC}"
    echo "Time,Memory(MB),RSS(MB)"
    
    for ((i=0; i<count; i++)); do
        if [ ! -z "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
            # Get memory usage
            local mem_info=$(ps -o pid,vsz,rss,comm -p $SERVER_PID 2>/dev/null | tail -1)
            if [ ! -z "$mem_info" ]; then
                local vsz=$(echo $mem_info | awk '{print $2}')
                local rss=$(echo $mem_info | awk '{print $3}')
                local vsz_mb=$((vsz / 1024))
                local rss_mb=$((rss / 1024))
                echo "$((i * interval)),${vsz_mb},${rss_mb}"
            fi
        fi
        sleep $interval
    done
}

# Function to check server health
check_server_health() {
    local url=$1
    local response=$(curl -s -o /dev/null -w "%{http_code}" "$url" 2>/dev/null)
    if [ "$response" = "200" ]; then
        echo -e "${GREEN}✅ Server responding (HTTP $response)${NC}"
        return 0
    else
        echo -e "${RED}❌ Server not responding (HTTP $response)${NC}"
        return 1
    fi
}

# Function to run basic siege test
run_basic_siege() {
    echo -e "${BLUE}🧪 Running basic siege test...${NC}"
    
    # Test 1: Simple GET request
    echo "Test 1: Simple GET (10 concurrent users, 10 seconds)"
    siege -b -c 10 -t 10s "$SERVER_URL/" 2>/dev/null | grep -E "(Availability|Transaction rate|Response time)"
    
    echo ""
    echo "Test 2: Simple GET (50 concurrent users, 30 seconds)"
    siege -b -c 50 -t 30s "$SERVER_URL/" 2>/dev/null | grep -E "(Availability|Transaction rate|Response time)"
}

# Function to run stress siege test
run_stress_siege() {
    echo -e "${BLUE}🔥 Running stress siege test...${NC}"
    
    # Test 3: High concurrency
    echo "Test 3: High concurrency (100 users, 60 seconds)"
    siege -b -c 100 -t 60s "$SERVER_URL/" 2>/dev/null | grep -E "(Availability|Transaction rate|Response time|Failed)"
    
    echo ""
    echo "Test 4: Maximum stress (200 users, 120 seconds)"
    siege -b -c 200 -t 120s "$SERVER_URL/" 2>/dev/null | grep -E "(Availability|Transaction rate|Response time|Failed)"
}

# Function to run CGI stress test
run_cgi_stress() {
    echo -e "${BLUE}🐍 Running CGI stress test...${NC}"
    
    echo "Test 5: CGI stress (50 users, 30 seconds)"
    siege -b -c 50 -t 30s "$CGI_URL/cgi_bin/hello.py" 2>/dev/null | grep -E "(Availability|Transaction rate|Response time|Failed)"
}

# Function to run indefinite test
run_indefinite_test() {
    echo -e "${BLUE}♾️  Running indefinite test (5 minutes)...${NC}"
    echo "This test runs for 5 minutes to check for memory leaks and hanging connections"
    
    # Start memory monitoring in background
    monitor_memory 300 > memory_log.csv &
    local monitor_pid=$!
    
    # Run siege indefinitely for 5 minutes
    timeout 300s siege -b -c 20 -t 300s "$SERVER_URL/" > siege_output.log 2>&1 &
    local siege_pid=$!
    
    # Wait for siege to complete
    wait $siege_pid
    local siege_exit_code=$?
    
    # Stop memory monitoring
    kill $monitor_pid 2>/dev/null
    
    # Analyze results
    echo -e "${BLUE}📈 Analyzing results...${NC}"
    
    if [ $siege_exit_code -eq 0 ]; then
        echo -e "${GREEN}✅ Indefinite test completed successfully${NC}"
        
        # Check availability
        local availability=$(grep "Availability" siege_output.log | tail -1 | grep -o '[0-9.]*%' | tr -d '%')
        if [ ! -z "$availability" ]; then
            if (( $(echo "$availability >= 99.5" | bc -l) )); then
                echo -e "${GREEN}✅ Availability: ${availability}% (>= 99.5%)${NC}"
            else
                echo -e "${RED}❌ Availability: ${availability}% (< 99.5%)${NC}"
            fi
        fi
        
        # Check for memory leaks
        if [ -f "memory_log.csv" ]; then
            local initial_mem=$(head -2 memory_log.csv | tail -1 | cut -d',' -f2)
            local final_mem=$(tail -1 memory_log.csv | cut -d',' -f2)
            local mem_increase=$((final_mem - initial_mem))
            
            if [ $mem_increase -lt 50 ]; then
                echo -e "${GREEN}✅ Memory usage stable (increase: ${mem_increase}MB)${NC}"
            else
                echo -e "${YELLOW}⚠️  Potential memory leak (increase: ${mem_increase}MB)${NC}"
            fi
        fi
        
    else
        echo -e "${RED}❌ Indefinite test failed${NC}"
    fi
}

# Function to test different endpoints
test_endpoints() {
    echo -e "${BLUE}🎯 Testing different endpoints...${NC}"
    
    local endpoints=(
        "$SERVER_URL/"
        "$SERVER_URL/about.html"
        "$SERVER_URL/images/test.jpg"
        "$CGI_URL/cgi_bin/hello.py"
        "$STRESS_URL/"
    )
    
    for endpoint in "${endpoints[@]}"; do
        echo "Testing: $endpoint"
        siege -b -c 10 -t 5s "$endpoint" 2>/dev/null | grep -E "(Availability|Transaction rate)" | head -1
        echo ""
    done
}

# Function to check for hanging connections
check_hanging_connections() {
    echo -e "${BLUE}🔍 Checking for hanging connections...${NC}"
    
    # Check server process
    if [ ! -z "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
        local connections=$(netstat -an 2>/dev/null | grep ":8080" | wc -l)
        echo "Active connections on port 8080: $connections"
        
        if [ $connections -gt 100 ]; then
            echo -e "${YELLOW}⚠️  High number of connections: $connections${NC}"
        else
            echo -e "${GREEN}✅ Connection count normal: $connections${NC}"
        fi
    fi
}

# Main execution
main() {
    # Trap to ensure server is stopped on exit
    trap stop_server EXIT
    
    # Start server
    start_server
    
    # Wait for server to be ready
    sleep 3
    
    # Check server health
    if ! check_server_health "$SERVER_URL/"; then
        echo -e "${RED}❌ Server health check failed${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}🎉 Starting comprehensive stress tests...${NC}"
    echo ""
    
    # Run tests
    run_basic_siege
    echo ""
    
    run_stress_siege
    echo ""
    
    run_cgi_stress
    echo ""
    
    test_endpoints
    echo ""
    
    run_indefinite_test
    echo ""
    
    check_hanging_connections
    echo ""
    
    echo -e "${GREEN}🏁 All stress tests completed!${NC}"
    echo "Check 'memory_log.csv' for memory usage data"
    echo "Check 'siege_output.log' for detailed siege results"
}

# Run main function
main "$@"
