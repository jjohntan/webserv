#!/bin/bash

# ==============================
# Focused Siege Stress Test
# ==============================

echo "🚀 WebServer Siege Stress Test"
echo "==============================="

# Configuration
SERVER_CONFIG="testconfig/test.conf"
SERVER_URL="http://127.0.0.1:8080"
SERVER_PID=""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Function to start server
start_server() {
    echo -e "${BLUE}📡 Starting webserver...${NC}"
    ./webserver $SERVER_CONFIG > server.log 2>&1 &
    SERVER_PID=$!
    sleep 3
    
    if ! kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${RED}❌ Failed to start server${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✅ Server started (PID: $SERVER_PID)${NC}"
}

# Function to stop server
stop_server() {
    if [ ! -z "$SERVER_PID" ]; then
        echo -e "${YELLOW}🛑 Stopping server...${NC}"
        kill $SERVER_PID 2>/dev/null
        wait $SERVER_PID 2>/dev/null
    fi
}

# Function to check server health
check_health() {
    local response=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/" 2>/dev/null)
    if [ "$response" = "200" ]; then
        echo -e "${GREEN}✅ Server responding (HTTP $response)${NC}"
        return 0
    else
        echo -e "${RED}❌ Server not responding (HTTP $response)${NC}"
        return 1
    fi
}

# Function to monitor memory
monitor_memory() {
    local duration=$1
    echo -e "${BLUE}📊 Monitoring memory for ${duration}s...${NC}"
    
    for ((i=0; i<duration; i+=5)); do
        if [ ! -z "$SERVER_PID" ] && kill -0 $SERVER_PID 2>/dev/null; then
            local mem=$(ps -o rss -p $SERVER_PID 2>/dev/null | tail -1)
            if [ ! -z "$mem" ]; then
                local mem_mb=$((mem / 1024))
                echo "Time: ${i}s, Memory: ${mem_mb}MB"
            fi
        fi
        sleep 5
    done
}

# Function to run siege test
run_siege_test() {
    local test_name="$1"
    local concurrent="$2"
    local duration="$3"
    local url="$4"
    
    echo -e "${BLUE}🧪 $test_name${NC}"
    echo "Concurrent users: $concurrent, Duration: $duration"
    
    # Run siege and capture results
    siege -b -c $concurrent -t ${duration}s "$url" > siege_result.tmp 2>&1
    
    # Extract key metrics
    local availability=$(grep "Availability" siege_result.tmp | grep -o '[0-9.]*%' | tr -d '%')
    local transaction_rate=$(grep "Transaction rate" siege_result.tmp | grep -o '[0-9.]*' | head -1)
    local response_time=$(grep "Response time" siege_result.tmp | grep -o '[0-9.]*' | head -1)
    local failed=$(grep "Failed" siege_result.tmp | grep -o '[0-9]*' | head -1)
    
    echo "Results:"
    echo "  Availability: ${availability}%"
    echo "  Transaction rate: ${transaction_rate} trans/sec"
    echo "  Response time: ${response_time} secs"
    echo "  Failed: ${failed}"
    
    # Check availability requirement
    if [ ! -z "$availability" ]; then
        if (( $(echo "$availability >= 99.5" | bc -l 2>/dev/null || echo "0") )); then
            echo -e "${GREEN}✅ Availability >= 99.5%${NC}"
        else
            echo -e "${RED}❌ Availability < 99.5%${NC}"
        fi
    fi
    
    echo ""
}

# Function to run indefinite test
run_indefinite_test() {
    echo -e "${BLUE}♾️  Running indefinite test (5 minutes)...${NC}"
    
    # Start memory monitoring in background
    monitor_memory 300 > memory.log &
    local monitor_pid=$!
    
    # Run siege indefinitely
    echo "Starting indefinite siege test..."
    timeout 300s siege -b -c 20 -t 300s "$SERVER_URL/" > indefinite_siege.log 2>&1 &
    local siege_pid=$!
    
    # Wait for completion
    wait $siege_pid
    local siege_exit_code=$?
    
    # Stop monitoring
    kill $monitor_pid 2>/dev/null
    
    # Analyze results
    if [ $siege_exit_code -eq 0 ]; then
        echo -e "${GREEN}✅ Indefinite test completed${NC}"
        
        # Check final availability
        local final_availability=$(grep "Availability" indefinite_siege.log | tail -1 | grep -o '[0-9.]*%' | tr -d '%')
        if [ ! -z "$final_availability" ]; then
            echo "Final availability: ${final_availability}%"
            if (( $(echo "$final_availability >= 99.5" | bc -l 2>/dev/null || echo "0") )); then
                echo -e "${GREEN}✅ Final availability >= 99.5%${NC}"
            else
                echo -e "${RED}❌ Final availability < 99.5%${NC}"
            fi
        fi
        
        # Check memory stability
        if [ -f "memory.log" ]; then
            local initial_mem=$(head -1 memory.log | grep -o '[0-9]*' | tail -1)
            local final_mem=$(tail -1 memory.log | grep -o '[0-9]*' | tail -1)
            if [ ! -z "$initial_mem" ] && [ ! -z "$final_mem" ]; then
                local mem_increase=$((final_mem - initial_mem))
                echo "Memory increase: ${mem_increase}MB"
                if [ $mem_increase -lt 50 ]; then
                    echo -e "${GREEN}✅ Memory usage stable${NC}"
                else
                    echo -e "${YELLOW}⚠️  Potential memory leak${NC}"
                fi
            fi
        fi
        
    else
        echo -e "${RED}❌ Indefinite test failed${NC}"
    fi
}

# Main execution
main() {
    # Trap to ensure cleanup
    trap stop_server EXIT
    
    # Start server
    start_server
    
    # Check health
    if ! check_health; then
        echo -e "${RED}❌ Server health check failed${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}🎉 Starting siege stress tests...${NC}"
    echo ""
    
    # Test 1: Basic test
    run_siege_test "Basic Test" 10 10 "$SERVER_URL/"
    
    # Test 2: Medium load
    run_siege_test "Medium Load" 50 30 "$SERVER_URL/"
    
    # Test 3: High load
    run_siege_test "High Load" 100 60 "$SERVER_URL/"
    
    # Test 4: Indefinite test
    run_indefinite_test
    
    echo -e "${GREEN}🏁 All tests completed!${NC}"
    echo "Check 'memory.log' for memory usage"
    echo "Check 'indefinite_siege.log' for detailed results"
}

# Run main function
main "$@"
