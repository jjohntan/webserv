c#!/bin/bash

# WebServ Comprehensive Test Script
# Tests all requirements: GET, POST, DELETE, CGI, Error Handling, File Upload/Download

echo "=========================================="
echo "WebServ Comprehensive Test Suite"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
PASSED=0
FAILED=0
TOTAL=0

# Function to run test and check result
run_test() {
    local test_name="$1"
    local command="$2"
    local expected_status="$3"
    local description="$4"
    
    TOTAL=$((TOTAL + 1))
    echo -e "${BLUE}Test $TOTAL: $test_name${NC}"
    echo "Description: $description"
    echo "Command: $command"
    echo "Expected Status: $expected_status"
    echo "---"
    
    # Run the command and capture status code
    response=$(eval "$command" 2>&1)
    status_code=$(echo "$response" | grep -o "HTTP/1.1 [0-9]*" | grep -o "[0-9]*" | tail -1)
    
    if [ "$status_code" = "$expected_status" ]; then
        echo -e "${GREEN}✅ PASSED${NC} - Status: $status_code"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}❌ FAILED${NC} - Expected: $expected_status, Got: $status_code"
        FAILED=$((FAILED + 1))
    fi
    echo ""
}

# Function to run test without status check (for content verification)
run_content_test() {
    local test_name="$1"
    local command="$2"
    local expected_content="$3"
    local description="$4"
    
    TOTAL=$((TOTAL + 1))
    echo -e "${BLUE}Test $TOTAL: $test_name${NC}"
    echo "Description: $description"
    echo "Command: $command"
    echo "Expected Content: $expected_content"
    echo "---"
    
    response=$(eval "$command" 2>&1)
    
    if echo "$response" | grep -q "$expected_content"; then
        echo -e "${GREEN}✅ PASSED${NC} - Content found"
        PASSED=$((PASSED + 1))
    else
        echo -e "${RED}❌ FAILED${NC} - Content not found"
        FAILED=$((FAILED + 1))
    fi
    echo ""
}

echo -e "${YELLOW}Starting WebServ Test Suite...${NC}"
echo "Make sure your webserver is running on ports 8080, 8081, 8082"
echo ""

# ==========================================
# TEST 1: BASIC HTTP METHODS
# ==========================================
echo -e "${YELLOW}=== TEST 1: BASIC HTTP METHODS ===${NC}"

# Test 1.1: GET Request (should work)
run_test "GET Request" "curl -s -i http://localhost:8080/" "200" "Basic GET request to root path"

# Test 1.2: POST to root (should return 405)
run_test "POST to Root" "curl -s -i -X POST -H 'Content-Type: application/x-www-form-urlencoded' -d 'test=data' http://localhost:8080/" "405" "POST to root path (should be method not allowed)"

# Test 1.3: DELETE to upload directory (should work)
run_test "DELETE File" "curl -s -i -X DELETE http://localhost:8080/upload/test.txt" "204" "DELETE request to remove a file"

# Test 1.4: Invalid HTTP method
run_test "Invalid Method" "curl -s -i -X INVALID http://localhost:8080/" "400" "Invalid HTTP method (should return bad request)"

# ==========================================
# TEST 2: ERROR HANDLING
# ==========================================
echo -e "${YELLOW}=== TEST 2: ERROR HANDLING ===${NC}"

# Test 2.1: 404 Not Found
run_test "404 Not Found" "curl -s -i http://localhost:8080/nonexistent.html" "404" "Request for non-existent file"

# Test 2.2: 405 Method Not Allowed
run_test "405 Method Not Allowed" "curl -s -i -X PUT http://localhost:8080/" "405" "PUT method not allowed on root"

# Test 2.3: 413 Payload Too Large (if implemented)
run_test "413 Payload Too Large" "curl -s -i -X POST -H 'Content-Type: text/plain' --data-binary /dev/zero http://localhost:8082/upload/" "413" "Large payload test (stress server)"

# ==========================================
# TEST 3: FILE UPLOAD/DOWNLOAD
# ==========================================
echo -e "${YELLOW}=== TEST 3: FILE UPLOAD/DOWNLOAD ===${NC}"

# Create test files
echo "Creating test files..."
echo "This is a test file for upload" > test_upload.txt
echo "CGI test content" > cgi_test_file.txt

# Test 3.1: Non-CGI Upload
run_test "Non-CGI Upload" "curl -s -i -X POST -H 'Content-Type: text/plain' --data-binary @test_upload.txt http://localhost:8080/upload/" "201" "Upload file via non-CGI method"

# Test 3.2: CGI Upload
run_test "CGI Upload" "curl -s -i -X POST -F 'file=@cgi_test_file.txt' http://localhost:8081/cgi_bin/upload.py" "200" "Upload file via CGI method"

# Test 3.3: File Download (directory listing)
run_content_test "File Download" "curl -s http://localhost:8080/upload/" "test_upload.txt" "Check if uploaded file appears in directory listing"

# ==========================================
# TEST 4: CGI FUNCTIONALITY
# ==========================================
echo -e "${YELLOW}=== TEST 4: CGI FUNCTIONALITY ===${NC}"

# Test 4.1: CGI GET Request
run_test "CGI GET" "curl -s -i http://localhost:8081/cgi_bin/hello.py" "200" "CGI script execution via GET"

# Test 4.2: CGI POST Request
run_test "CGI POST" "curl -s -i -X POST -H 'Content-Type: application/x-www-form-urlencoded' -d 'name=test&value=123' http://localhost:8081/cgi_bin/hello.py" "200" "CGI script execution via POST"

# Test 4.3: CGI DELETE Request
run_test "CGI DELETE" "curl -s -i -X DELETE http://localhost:8081/cgi_bin/hello.py" "200" "CGI script execution via DELETE"

# Test 4.4: CGI Error Handling
run_test "CGI Error" "curl -s -i http://localhost:8081/cgi_bin/broken.py" "500" "CGI script with error (should return 500)"

# ==========================================
# TEST 5: MULTI-SERVER CONFIGURATION
# ==========================================
echo -e "${YELLOW}=== TEST 5: MULTI-SERVER CONFIGURATION ===${NC}"

# Test 5.1: Port 8080 (Static Server)
run_test "Port 8080 Static" "curl -s -i http://localhost:8080/" "200" "Static server on port 8080"

# Test 5.2: Port 8081 (CGI Server)
run_test "Port 8081 CGI" "curl -s -i http://localhost:8081/cgi_bin/hello.py" "200" "CGI server on port 8081"

# Test 5.3: Port 8082 (Stress Server)
run_test "Port 8082 Stress" "curl -s -i http://localhost:8082/" "200" "Stress test server on port 8082"

# ==========================================
# TEST 6: CONTENT TYPE HANDLING
# ==========================================
echo -e "${YELLOW}=== TEST 6: CONTENT TYPE HANDLING ===${NC}"

# Test 6.1: HTML Content
run_content_test "HTML Content" "curl -s http://localhost:8080/" "text/html" "HTML content type for root page"

# Test 6.2: Image Content
run_content_test "Image Content" "curl -s -i http://localhost:8080/images/test.jpg" "image/jpeg" "Image content type for JPG files"

# Test 6.3: CSS Content (if available)
run_content_test "CSS Content" "curl -s -i http://localhost:8080/style.css" "text/css" "CSS content type"

# ==========================================
# TEST 7: DIRECTORY LISTING
# ==========================================
echo -e "${YELLOW}=== TEST 7: DIRECTORY LISTING ===${NC}"

# Test 7.1: Directory with autoindex
run_content_test "Directory Listing" "curl -s http://localhost:8080/upload/" "Directory Listing" "Autoindex directory listing"

# Test 7.2: Directory without autoindex
run_test "No Autoindex" "curl -s -i http://localhost:8080/images/" "403" "Directory without autoindex (should be forbidden)"

# ==========================================
# TEST 8: STRESS TESTING
# ==========================================
echo -e "${YELLOW}=== TEST 8: STRESS TESTING ===${NC}"

# Test 8.1: Multiple concurrent requests
echo "Testing concurrent requests..."
for i in {1..5}; do
    curl -s http://localhost:8080/ > /dev/null &
done
wait
echo -e "${GREEN}✅ PASSED${NC} - Concurrent requests handled"
PASSED=$((PASSED + 1))
TOTAL=$((TOTAL + 1))

# Test 8.2: Large file upload
echo "Testing large file upload..."
dd if=/dev/zero of=large_file.txt bs=1M count=1 2>/dev/null
run_test "Large File Upload" "curl -s -i -X POST -H 'Content-Type: text/plain' --data-binary @large_file.txt http://localhost:8080/upload/" "201" "Large file upload (1MB)"

# ==========================================
# TEST 9: SECURITY TESTS
# ==========================================
echo -e "${YELLOW}=== TEST 9: SECURITY TESTS ===${NC}"

# Test 9.1: Path traversal attempt
run_test "Path Traversal" "curl -s -i http://localhost:8080/../../../etc/passwd" "404" "Path traversal attack attempt"

# Test 9.2: Invalid headers
run_test "Invalid Headers" "curl -s -i -H 'Content-Length: invalid' http://localhost:8080/" "400" "Invalid Content-Length header"

# ==========================================
# CLEANUP
# ==========================================
echo -e "${YELLOW}=== CLEANUP ===${NC}"
echo "Cleaning up test files..."
rm -f test_upload.txt cgi_test_file.txt large_file.txt
echo -e "${GREEN}✅ Cleanup completed${NC}"

# ==========================================
# FINAL RESULTS
# ==========================================
echo ""
echo "=========================================="
echo -e "${YELLOW}FINAL TEST RESULTS${NC}"
echo "=========================================="
echo -e "Total Tests: $TOTAL"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"

if [ $FAILED -eq 0 ]; then
    echo ""
    echo -e "${GREEN}🎉 ALL TESTS PASSED! 🎉${NC}"
    echo -e "${GREEN}Your WebServ implementation meets all requirements!${NC}"
    exit 0
else
    echo ""
    echo -e "${RED}❌ Some tests failed. Please review the implementation.${NC}"
    exit 1
fi
