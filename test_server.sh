#!/usr/bin/env bash
set -Eeuo pipefail

# -----------------------------
# Helper functions (define first)
# -----------------------------
say()   { printf "%s\n" "$*"; }
skip()  { say "[SKIP] $*"; }
pass()  { say "[PASS] $*"; }
fail()  { say "[FAIL] $*"; ((FAILS++)) || true; }
fatal() { say "[FATAL] $*"; exit 1; }

have_cmd() { command -v "$1" >/dev/null 2>&1; }

port_pids_lsof() {
  local port="$1"
  if have_cmd lsof; then
    lsof -t -iTCP:"$port" -sTCP:LISTEN 2>/dev/null || true
  fi
}
port_pids_ss() {
  local port="$1"
  if have_cmd ss; then
    ss -ltnp 2>/dev/null | awk -v p=":${port}" '
      $4 ~ p {
        m=match($0,/pid=([0-9]+)/,a);
        if (m) print a[1];
      }' || true
  fi
}

kill_port_listeners() {
  local port="$1" pid
  local killed=0
  while read -r pid; do
    [[ -z "${pid}" ]] && continue
    if [[ "${SERVER_PID:-}" == "${pid}" ]]; then
      continue
    fi
    kill "${pid}" 2>/dev/null && killed=1
  done < <( { port_pids_lsof "$port"; port_pids_ss "$port"; } | sort -u )
  [[ $killed -eq 1 ]] && sleep 0.1
  if curl -sS -m 0.3 "http://${HOST}:${port}/" >/dev/null 2>&1; then
    return 1
  fi
  return 0
}

wait_port_up() {
  local port="$1" deadline=$((SECONDS+STARTUP_WAIT_SECS))
  while (( SECONDS < deadline )); do
    if curl -sS -m 0.3 "http://${HOST}:${port}/" >/dev/null 2>&1; then
      return 0
    fi
    sleep 0.15
  done
  return 1
}

# Always return a code, even if curl errors (prints 000)
curl_code() {
  curl -sS -o /dev/null -m "${CURL_TIMEOUT}" -w '%{http_code}' "$@" 2>/dev/null || echo 000
}
# Never crash the script if body fetch fails
curl_body() {
  curl -sS -m "${CURL_TIMEOUT}" "$@" || true
}

# Single raw HTTP TCP shot (requires nc)
raw_http_once() {
  local host="$1" port="$2" payload="$3"
  if ! have_cmd nc; then
    echo "__NO_NC__"
    return 0
  fi
  printf "%b" "$payload" | nc -w 2 "$host" "$port" 2>/dev/null || true
}

need_build() {
  [[ -n "${BIN_PATH:-}" ]] && return 1
  [[ -f Makefile ]] || return 1
  return 0
}

# -----------------------------
# Config (override via env vars)
# -----------------------------
CFG_FILE="${CFG_FILE:-testconfig/test.conf}"
LOG_FILE="${LOG_FILE:-server_test.log}"
BIN_CANDIDATES=( "${BIN:-}" ./webserv ./webserver ./a.out )
PORTS=(8080 8081 8082 8083)
HOST="127.0.0.1"
STARTUP_WAIT_SECS=6
CURL_TIMEOUT=6
TMP_DIR="$(mktemp -d -t webserv-tests-XXXXXX)"

# -----------------------------
# Cleanup + trap (set trap AFTER vars)
# -----------------------------
cleanup() {
  set +e
  [[ -n "${SERVER_PID:-}" ]] && kill "${SERVER_PID}" 2>/dev/null || true
  [[ -n "${NC_TMP:-}" ]] && rm -f "$NC_TMP" 2>/dev/null || true
  sleep 0.2
  for p in "${PORTS[@]}"; do
    kill_port_listeners "$p" || true
  done
  rm -rf "$TMP_DIR" >/dev/null 2>&1 || true
}
trap 'cleanup' EXIT INT TERM

# -----------------------------
# Locate binary
# -----------------------------
BIN_PATH=""
for c in "${BIN_CANDIDATES[@]}"; do
  [[ -z "$c" ]] && continue
  if [[ -x "$c" ]]; then BIN_PATH="$c"; break; fi
done
if [[ -z "${BIN_PATH}" ]] && [[ -f Makefile ]]; then
  say "No binary found; running: make"
  make -j || fatal "Build failed."
  for c in "${BIN_CANDIDATES[@]}"; do
    [[ -z "$c" ]] && continue
    if [[ -x "$c" ]]; then BIN_PATH="$c"; break; fi
  done
fi
[[ -x "${BIN_PATH:-}" ]] || fatal "Could not find server binary (expected ./webserv, ./webserver, or ./a.out)."
[[ -f "$CFG_FILE" ]] || fatal "Config file not found: ${CFG_FILE}"

# -----------------------------
# Free ports up-front
# -----------------------------
for p in "${PORTS[@]}"; do
  kill_port_listeners "$p" || true
done

# -----------------------------
# Start server
# -----------------------------
: > "$LOG_FILE"
say "=== Starting regression tests ==="
say "started ${BIN_PATH} (with ${CFG_FILE})"

# Ensure CGI scripts are executable before starting tests
if compgen -G "cgi_bin/*.py" > /dev/null; then
  chmod +x cgi_bin/*.py 2>/dev/null || true
fi

"${BIN_PATH}" "${CFG_FILE}" >>"$LOG_FILE" 2>&1 &
SERVER_PID=$!

for p in "${PORTS[@]}"; do
  if ! wait_port_up "$p"; then
    say "Port ${p} did not open in time. Server output (tail):"
    tail -n 60 "$LOG_FILE" || true
    fatal "Startup failed (port ${p})."
  fi
done

# -----------------------------
# Tests
# -----------------------------
FAILS=0

# ===========================================
# PORT 8080 TESTS - Static Website & Basic HTTP
# ===========================================
say ""
say "=== TESTING PORT 8080 (Static Website) ==="

# 1) Serve index.html on 8080
code=$(curl_code "http://${HOST}:8080/")
if [[ "$code" == "200" ]]; then
  pass "Port 8080: Serve index.html"
  body="$(curl_body "http://${HOST}:8080/")"
  if grep -qi "<title>webserv test page" <<<"$body"; then
    pass "Port 8080: index.html contains title"
  else
    fail "Port 8080: index.html contains title"
    say "Body was: $body"
  fi
else
  fail "Port 8080: Serve index.html (got $code expected 200)"
fi

# 2) Serve about.html on 8080
code=$(curl_code "http://${HOST}:8080/about.html")
if [[ "$code" == "200" ]]; then
  pass "Port 8080: Serve about.html"
  body="$(curl_body "http://${HOST}:8080/about.html")"
  if grep -qi "About" <<<"$body"; then
    pass "Port 8080: about.html contains 'About'"
  else
    fail "Port 8080: about.html contains 'About'"
    say "Body was: $body"
  fi
else
  fail "Port 8080: Serve about.html (got $code expected 200)"
fi

# 3) Autoindex ON on /upload/ (8080)
code=$(curl_code "http://${HOST}:8080/upload/")
if [[ "$code" == "200" ]]; then
  pass "Port 8080: Autoindex ON on /upload/"
  body="$(curl_body "http://${HOST}:8080/upload/")"
  if grep -qi "<title>Directory Listing" <<<"$body"; then
    pass "Port 8080: Autoindex page title"
  else
    fail "Port 8080: Autoindex page title"
    say "Body was: $body"
  fi
else
  fail "Port 8080: Autoindex ON on /upload/ (got $code expected 200)"
fi

# 4) Autoindex OFF on / (8080) - should serve index.html instead
code=$(curl_code "http://${HOST}:8080/")
if [[ "$code" == "200" ]]; then
  body="$(curl_body "http://${HOST}:8080/")"
  if grep -qi "<title>webserv test page" <<<"$body"; then
    pass "Port 8080: Autoindex OFF on / (serves index.html)"
  else
    fail "Port 8080: Autoindex OFF on / (should serve index.html)"
  fi
else
  fail "Port 8080: Autoindex OFF on / (got $code expected 200)"
fi

# 5) GET method test (8080)
code=$(curl_code "http://${HOST}:8080/")
if [[ "$code" == "200" ]]; then
  pass "Port 8080: GET method works"
else
  fail "Port 8080: GET method (got $code expected 200)"
fi

# 6) POST method test (8080) - upload file
SMALL_FILE="${TMP_DIR}/small.txt"
echo "hello webserv" > "$SMALL_FILE"
FNAME="small_$(date +%s).txt"
code=$(curl_code -X POST -F "file=@${SMALL_FILE};filename=${FNAME}" "http://${HOST}:8080/upload/")
if [[ "$code" == "200" || "$code" == "201" ]]; then
  body="$(curl_body "http://${HOST}:8080/upload/")"
  if grep -q "$FNAME" <<<"$body"; then
    pass "Port 8080: POST method (file upload)"
  else
    fail "Port 8080: POST method (file not listed after ${code})"
    say "server log (tail):"; tail -n 60 "$LOG_FILE" || true
  fi
else
  fail "Port 8080: POST method (got $code expected 200/201)"
  say "server log (tail):"; tail -n 60 "$LOG_FILE" || true
fi

# 7) DELETE method test (8080) - delete uploaded file
if [[ -n "${FNAME:-}" ]]; then
  code=$(curl_code -X DELETE "http://${HOST}:8080/upload/${FNAME}")
  if [[ "$code" == "200" || "$code" == "204" ]]; then
    pass "Port 8080: DELETE method (file deletion)"
    # Verify file is actually deleted
    body="$(curl_body "http://${HOST}:8080/upload/")"
    if ! grep -q "$FNAME" <<<"$body"; then
      pass "Port 8080: DELETE method (file actually removed)"
    else
      fail "Port 8080: DELETE method (file still exists)"
    fi
  else
    fail "Port 8080: DELETE method (got $code expected 200/204)"
  fi
fi

# 8) Error Pages - 404 Not Found (8080)
code=$(curl_code "http://${HOST}:8080/no_such_page_$(date +%s).html")
if [[ "$code" == "404" ]]; then
  pass "Port 8080: 404 Not Found error page"
  body="$(curl_body "http://${HOST}:8080/no_such_page_$(date +%s).html")"
  if grep -qi "404\|not found" <<<"$body"; then
    pass "Port 8080: 404 error page contains expected content"
  else
    fail "Port 8080: 404 error page missing expected content"
  fi
else
  fail "Port 8080: 404 Not Found (got $code expected 404)"
fi



# 10) Error Pages - 405 Method Not Allowed (DELETE to / on 8080)
code=$(curl_code -X DELETE "http://${HOST}:8080/")
if [[ "$code" == "405" ]]; then
  pass "Port 8080: 405 Method Not Allowed (DELETE to /)"
else
  fail "Port 8080: 405 Method Not Allowed DELETE (got $code expected 405)"
fi

# ===========================================
# PORT 8081 TESTS - CGI Functionality
# ===========================================
say ""
say "=== TESTING PORT 8081 (CGI) ==="

# 1) CGI hello.py on 8081 - GET method
code=$(curl_code "http://${HOST}:8081/cgi_bin/hello.py")
if [[ "$code" == "200" ]]; then
  pass "Port 8081: CGI hello.py (GET)"
  body="$(curl_body "http://${HOST}:8081/cgi_bin/hello.py")"
  if grep -qi "Hello" <<<"$body"; then
    pass "Port 8081: CGI contains 'Hello'"
  else
    fail "Port 8081: CGI contains 'Hello'"
    say "Body was: $body"
  fi
else
  fail "Port 8081: CGI hello.py (got $code expected 200)"
fi

# 2) CGI hello.py on 8081 - POST method
code=$(curl_code -X POST -H "Content-Type: application/x-www-form-urlencoded" -d "name=TestUser&message=Hello" "http://${HOST}:8081/cgi_bin/hello.py")
if [[ "$code" == "200" ]]; then
  pass "Port 8081: CGI hello.py (POST)"
  body="$(curl_body -X POST -H "Content-Type: application/x-www-form-urlencoded" -d "name=TestUser&message=Hello" "http://${HOST}:8081/cgi_bin/hello.py")"
  if grep -qi "TestUser\|Hello" <<<"$body"; then
    pass "Port 8081: CGI POST contains form data"
  else
    fail "Port 8081: CGI POST missing form data"
  fi
else
  fail "Port 8081: CGI hello.py POST (got $code expected 200)"
fi

# 3) CGI upload.py on 8081 - file upload
UPLOAD_FILE="${TMP_DIR}/upload_test.txt"
echo "CGI upload test content" > "$UPLOAD_FILE"
UPLOAD_FNAME="upload_test_$(date +%s).txt"
code=$(curl_code -X POST -F "file=@${UPLOAD_FILE};filename=${UPLOAD_FNAME}" "http://${HOST}:8081/cgi_bin/upload.py")
if [[ "$code" == "200" || "$code" == "201" ]]; then
  pass "Port 8081: CGI upload.py (file upload)"
  body="$(curl_body -X POST -F "file=@${UPLOAD_FILE};filename=${UPLOAD_FNAME}" "http://${HOST}:8081/cgi_bin/upload.py")"
  if grep -qi "upload\|success\|${UPLOAD_FNAME}" <<<"$body"; then
    pass "Port 8081: CGI upload contains success message"
  else
    fail "Port 8081: CGI upload missing success message"
  fi
else
  fail "Port 8081: CGI upload.py (got $code expected 200/201)"
fi

# 4) Autoindex on 8081
code=$(curl_code "http://${HOST}:8081/upload/")
if [[ "$code" == "200" ]]; then
  pass "Port 8081: Autoindex on /upload/"
  body="$(curl_body "http://${HOST}:8081/upload/")"
  if grep -qi "<title>Directory Listing" <<<"$body"; then
    pass "Port 8081: Autoindex page title"
  else
    fail "Port 8081: Autoindex page title"
  fi
else
  fail "Port 8081: Autoindex on /upload/ (got $code expected 200)"
fi

# 5) CGI error_script.py - should handle errors gracefully
code=$(curl_code "http://${HOST}:8081/cgi_bin/error_script.py")
if [[ "$code" == "200" ]]; then
  pass "Port 8081: CGI error_script.py (error handling)"
  body="$(curl_body "http://${HOST}:8081/cgi_bin/error_script.py")"
  if grep -qi "error\|exception\|handled" <<<"$body"; then
    pass "Port 8081: CGI error script handles errors properly"
  else
    fail "Port 8081: CGI error script missing error handling"
  fi
else
  fail "Port 8081: CGI error_script.py (got $code expected 200)"
fi

# 6) CGI syntax_error.py - should return 500 or handle syntax error
code=$(curl_code "http://${HOST}:8081/cgi_bin/syntax_error.py")
if [[ "$code" == "500" ]]; then
  pass "Port 8081: CGI syntax_error.py (500 Internal Server Error)"
else
  fail "Port 8081: CGI syntax_error.py (got $code expected 500)"
fi

# 7) CGI crash_script.py - should return 500 or handle crash
code=$(curl_code "http://${HOST}:8081/cgi_bin/crash_script.py")
if [[ "$code" == "500" ]]; then
  pass "Port 8081: CGI crash_script.py (500 Internal Server Error)"
else
  fail "Port 8081: CGI crash_script.py (got $code expected 500)"
fi

# 8) CGI infinite_loop.py - should timeout and return 504
code=$(curl_code --max-time 15 "http://${HOST}:8081/cgi_bin/infinite_loop.py")
if [[ "$code" == "504" ]]; then
  pass "Port 8081: CGI infinite_loop.py (504 Gateway Timeout)"
else
  fail "Port 8081: CGI infinite_loop.py (got $code expected 504, timeout should prevent hanging)"
fi

# 9) CGI with large input - should be rejected if over body size limit (5MB for port 8081)
LARGE_CGI_FILE="${TMP_DIR}/large_cgi_input.txt"
python3 -c "print('A' * 6 * 1024 * 1024)" > "$LARGE_CGI_FILE" 2>/dev/null || dd if=/dev/zero bs=1M count=6 status=none | tr '\0' 'A' > "$LARGE_CGI_FILE"
code=$(curl_code -X POST -H "Content-Type: application/x-www-form-urlencoded" --data-binary @"$LARGE_CGI_FILE" "http://${HOST}:8081/cgi_bin/hello.py")
if [[ "$code" == "413" ]]; then
  pass "Port 8081: CGI with large input rejected (413)"
else
  fail "Port 8081: CGI with large input (got $code expected 413)"
fi

# 10) CGI with input within body size limit (10KB < 5MB limit for port 8081)
MEDIUM_CGI_FILE="${TMP_DIR}/medium_cgi_input.txt"
python3 -c "print('A' * 10 * 1024)" > "$MEDIUM_CGI_FILE" 2>/dev/null || dd if=/dev/zero bs=10K count=1 status=none | tr '\0' 'A' > "$MEDIUM_CGI_FILE"
code=$(curl_code -X POST -H "Content-Type: application/x-www-form-urlencoded" --data-binary @"$MEDIUM_CGI_FILE" "http://${HOST}:8081/cgi_bin/hello.py")
if [[ "$code" == "200" ]]; then
  pass "Port 8081: CGI with medium input accepted (within 5MB limit)"
else
  fail "Port 8081: CGI with medium input (got $code expected 200)"
fi

# ===========================================
# PORT 8082 TESTS - Client Max Body Size
# ===========================================
say ""
say "=== TESTING PORT 8082 (Client Max Body Size) ==="

# 1) 413 Payload Too Large (8082 tiny limit)
BIG_FILE="${TMP_DIR}/big.bin"
dd if=/dev/zero of="$BIG_FILE" bs=1M count=2 status=none
code=$(curl_code -X POST -H "Content-Type: application/octet-stream" --data-binary @"$BIG_FILE" "http://${HOST}:8082/upload/")
if [[ "$code" == "413" ]]; then
  pass "Port 8082: 413 Payload Too Large (2MB file)"
else
  fail "Port 8082: 413 Payload Too Large (got $code expected 413)"
fi

# 8.1) Port 8082 - Small file upload (should work under 1MB limit)
SMALL_8082_FILE="${TMP_DIR}/small_8082.txt"
echo "Small file for 8082 test" > "$SMALL_8082_FILE"
FNAME_8082="small_8082_$(date +%s).txt"
code=$(curl_code -X POST -F "file=@${SMALL_8082_FILE};filename=${FNAME_8082}" "http://${HOST}:8082/upload/")
if [[ "$code" == "200" || "$code" == "201" ]]; then
  pass "Port 8082: Small file upload (under 1MB limit)"
  body="$(curl_body "http://${HOST}:8082/upload/")"
  if grep -q "$FNAME_8082" <<<"$body"; then
    pass "Port 8082: Uploaded file appears in directory listing"
  else
    fail "Port 8082: Uploaded file not listed after ${code}"
  fi
else
  fail "Port 8082: Small file upload (got $code expected 200/201)"
fi



# 8.3) Port 8082 - File upload just over 1MB (should fail)
OVER_1MB_FILE="${TMP_DIR}/over_1mb.bin"
dd if=/dev/zero of="$OVER_1MB_FILE" bs=1M count=1 status=none
# Add 1 extra byte to exceed 1MB
echo "X" >> "$OVER_1MB_FILE"
FNAME_OVER="over_1mb_$(date +%s).bin"
code=$(curl_code -X POST -F "file=@${OVER_1MB_FILE};filename=${FNAME_OVER}" "http://${HOST}:8082/upload/")
if [[ "$code" == "413" ]]; then
  pass "Port 8082: File just over 1MB rejected (413)"
else
  fail "Port 8082: File just over 1MB (got $code expected 413)"
fi

# 8.4) Port 8082 - Large POST body (should fail)
LARGE_POST_FILE="${TMP_DIR}/large_post.txt"
python3 -c "print('A' * 2 * 1024 * 1024)" > "$LARGE_POST_FILE" 2>/dev/null || dd if=/dev/zero bs=1M count=2 status=none | tr '\0' 'A' > "$LARGE_POST_FILE"
code=$(curl_code -X POST -H "Content-Type: text/plain" --data-binary @"$LARGE_POST_FILE" "http://${HOST}:8082/")
if [[ "$code" == "413" ]]; then
  pass "Port 8082: Large POST body rejected (413)"
else
  fail "Port 8082: Large POST body (got $code expected 413)"
fi

# 8.5) Port 8082 - Verify 413 error page content
code=$(curl_code -X POST -H "Content-Type: application/octet-stream" --data-binary @"$BIG_FILE" "http://${HOST}:8082/upload/")
if [[ "$code" == "413" ]]; then
  body="$(curl_body -X POST -H "Content-Type: application/octet-stream" --data-binary @"$BIG_FILE" "http://${HOST}:8082/upload/")"
  if grep -qi "413\|Payload Too Large\|exceeds the maximum size" <<<"$body"; then
    pass "Port 8082: 413 error page contains appropriate content"
  else
    fail "Port 8082: 413 error page missing expected content"
  fi
else
  fail "Port 8082: 413 error page test (got $code expected 413)"
fi

# 8.6) Port 8082 - CGI with large input (should fail)
code=$(curl_code -X POST -H "Content-Type: application/x-www-form-urlencoded" --data-binary @"$LARGE_POST_FILE" "http://${HOST}:8082/cgi_bin/hello.py")
if [[ "$code" == "413" ]]; then
  pass "Port 8082: CGI with large input rejected (413)"
else
  fail "Port 8082: CGI with large input (got $code expected 413)"
fi

# 8.7) Port 8082 - Normal CGI request (should work)
code=$(curl_code "http://${HOST}:8082/cgi_bin/hello.py")
if [[ "$code" == "200" ]]; then
  pass "Port 8082: Normal CGI request works"
  body="$(curl_body "http://${HOST}:8082/cgi_bin/hello.py")"
  if grep -qi "Hello\|WebServ CGI Test" <<<"$body"; then
    pass "Port 8082: CGI returns expected content"
  else
    fail "Port 8082: CGI content unexpected"
  fi
else
  fail "Port 8082: Normal CGI request (got $code expected 200)"
fi

# 8.8) Port 8082 - Server name verification
code=$(curl_code -H "Host: stress.localhost" "http://${HOST}:8082/")
if [[ "$code" == "200" ]]; then
  pass "Port 8082: Server name 'stress.localhost' works"
else
  fail "Port 8082: Server name 'stress.localhost' (got $code expected 200)"
fi

# 9) DELETE file (8080)
DEL_NAME="tmp_delete_$(date +%s).txt"
echo "please delete me" > "${TMP_DIR}/${DEL_NAME}"
curl -sS -m "${CURL_TIMEOUT}" -o /dev/null -X POST -F "file=@${TMP_DIR}/${DEL_NAME};filename=${DEL_NAME}" "http://${HOST}:8080/upload/" || true
code=$(curl_code -X DELETE "http://${HOST}:8080/upload/${DEL_NAME}")
if [[ "$code" == "204" ]]; then
  pass "DELETE file"
  code2=$(curl_code "http://${HOST}:8080/upload/${DEL_NAME}")
  if [[ "$code2" == "404" ]]; then
    pass "DELETE file (file removed)"
  else
    fail "DELETE file (still accessible, got $code2 expected 404)"
  fi
else
  fail "DELETE file (got $code expected 204)"
fi

# 10) UNKNOWN method should not crash; expect 405 or 501
code=$(curl_code -X FOO "http://${HOST}:8080/")
if [[ "$code" == "405" || "$code" == "501" ]]; then
  pass "UNKNOWN method gets 405/501 (no crash)"
else
  fail "UNKNOWN method (got $code expected 405/501)"
fi

# 11) HEAD method on static should return headers and no body (Content-Length honored)
head_headers="$(curl -sS -D - -o /dev/null -m "${CURL_TIMEOUT}" -X HEAD "http://${HOST}:8080/")" || true
if grep -qi '^HTTP/1\.[01] 200' <<<"$head_headers"; then
  if grep -qi '^content-length:' <<<"$head_headers"; then
    pass "HEAD returns 200 with Content-Length"
  else
    fail "HEAD missing Content-Length"
  fi
else
  fail "HEAD did not return 200"
fi

# 12) Keep-Alive: two requests over one TCP connection (HTTP/1.1)
NC_TMP="$(mktemp -t ws-keepalive-XXXXXX)" || true
ka_req=$'GET / HTTP/1.1\r\nHost: '"${HOST}"$'\r\nConnection: keep-alive\r\n\r\nGET /about.html HTTP/1.1\r\nHost: '"${HOST}"$'\r\nConnection: close\r\n\r\n'
ka_resp="$(raw_http_once "$HOST" 8080 "$ka_req")"
if [[ "$ka_resp" == "__NO_NC__" ]]; then
  skip "Keep-Alive (nc not installed)—skipping raw two-requests test"
else
  cnt=$(grep -cE '^HTTP/1\.[01] 200' <<<"$ka_resp" || true)
  if [[ "$cnt" -ge 2 ]]; then
    pass "Keep-Alive: multiple responses on one connection"
  else
    fail "Keep-Alive: expected two 200 responses on one connection"
  fi
fi

# 13) Directory listing OFF on "/" (expect index served, not listing)
body_root="$(curl_body "http://${HOST}:8080/")"
if grep -qi "<title>webserv test page" <<<"$body_root"; then
  pass "Autoindex OFF on / (index served)"
else
  skip "Autoindex OFF heuristic check (custom index?)"
fi

# 14) Method-limited route returns 405 for disallowed verb (DELETE /)
code=$(curl_code -X DELETE "http://${HOST}:8080/")
if [[ "$code" == "405" || "$code" == "403" ]]; then
  pass "Method control: DELETE / disallowed"
else
  skip "Method control (DELETE /) expected 405/403; got $code (config-dependent)"
fi

# 15) Default error pages: ensure error bodies are HTML-ish
code=$(curl_code "http://${HOST}:8080/this_path_should_not_exist_$(date +%s)")
err_body="$(curl_body "http://${HOST}:8080/this_path_should_not_exist_$(date +%s)")"
if [[ "$code" == "404" ]]; then
  if grep -qiE "<html|<body|</html>|</body>" <<<"$err_body"; then
    pass "Default error page (404) is HTML-like"
  else
    fail "Default error page (404) not HTML-like"
  fi
else
  fail "Default error page: expected 404, got $code"
fi

# ===========================================
# PORT 8083 TESTS - Redirect Functionality
# ===========================================
say ""
say "=== TESTING PORT 8083 (Redirects) ==="

# 1) 301 Moved Permanently redirect
code=$(curl_code -L "http://${HOST}:8083/moved")
if [[ "$code" == "200" ]]; then
  pass "Port 8083: 301 Moved Permanently redirect"
else
  fail "Port 8083: 301 Moved Permanently redirect (got $code expected 200 after redirect)"
fi

# 2) 302 Found redirect
code=$(curl_code -L "http://${HOST}:8083/found")
if [[ "$code" == "200" ]]; then
  pass "Port 8083: 302 Found redirect"
else
  fail "Port 8083: 302 Found redirect (got $code expected 200 after redirect)"
fi

# 3) 303 See Other redirect
code=$(curl_code -L "http://${HOST}:8083/see-other")
if [[ "$code" == "200" ]]; then
  pass "Port 8083: 303 See Other redirect"
else
  fail "Port 8083: 303 See Other redirect (got $code expected 200 after redirect)"
fi



# 5) 308 Permanent Redirect
code=$(curl_code -L "http://${HOST}:8083/perm")
if [[ "$code" == "200" ]]; then
  pass "Port 8083: 308 Permanent Redirect"
else
  fail "Port 8083: 308 Permanent Redirect (got $code expected 200 after redirect)"
fi

# 6) Verify redirect headers (without following)
redirect_headers="$(curl -sS -D - -o /dev/null -m "${CURL_TIMEOUT}" "http://${HOST}:8083/moved")" || true
if grep -qi '^HTTP/1\.[01] 301' <<<"$redirect_headers" && grep -qi '^Location:' <<<"$redirect_headers"; then
  pass "Port 8083: 301 redirect returns proper headers"
else
  fail "Port 8083: 301 redirect missing proper headers"
fi

# 7) Verify redirect headers (without following) - 302
redirect_headers="$(curl -sS -D - -o /dev/null -m "${CURL_TIMEOUT}" "http://${HOST}:8083/found")" || true
if grep -qi '^HTTP/1\.[01] 302' <<<"$redirect_headers" && grep -qi '^Location:' <<<"$redirect_headers"; then
  pass "Port 8083: 302 redirect returns proper headers"
else
  fail "Port 8083: 302 redirect missing proper headers"
fi

# 8) Verify redirect headers (without following) - 303
redirect_headers="$(curl -sS -D - -o /dev/null -m "${CURL_TIMEOUT}" "http://${HOST}:8083/see-other")" || true
if grep -qi '^HTTP/1\.[01] 303' <<<"$redirect_headers" && grep -qi '^Location:' <<<"$redirect_headers"; then
  pass "Port 8083: 303 redirect returns proper headers"
else
  fail "Port 8083: 303 redirect missing proper headers"
fi

# 9) Verify redirect headers (without following) - 307
redirect_headers="$(curl -sS -D - -o /dev/null -m "${CURL_TIMEOUT}" "http://${HOST}:8083/temp")" || true
if grep -qi '^HTTP/1\.[01] 307' <<<"$redirect_headers" && grep -qi '^Location:' <<<"$redirect_headers"; then
  pass "Port 8083: 307 redirect returns proper headers"
else
  fail "Port 8083: 307 redirect missing proper headers"
fi

# 10) Verify redirect headers (without following) - 308
redirect_headers="$(curl -sS -D - -o /dev/null -m "${CURL_TIMEOUT}" "http://${HOST}:8083/perm")" || true
if grep -qi '^HTTP/1\.[01] 308' <<<"$redirect_headers" && grep -qi '^Location:' <<<"$redirect_headers"; then
  pass "Port 8083: 308 redirect returns proper headers"
else
  fail "Port 8083: 308 redirect missing proper headers"
fi

# ===========================================
# DUPLICATE CONFIGURATION TESTS
# ===========================================
say ""
say "=== TESTING DUPLICATE CONFIGURATIONS ==="

# 1) Test duplicate port with same server name (should fail)
say "Testing duplicate port 8083 with same server name..."
DUPLICATE_CONFIG="${TMP_DIR}/duplicate_test.conf"
cat > "$DUPLICATE_CONFIG" << 'EOF'
server {
    listen 127.0.0.1:8083;
    server_name duplicate.localhost;
    root ./pages/www;
    client_max_body_size 10M;
}

server {
    listen 127.0.0.1:8083;
    server_name duplicate.localhost;
    root ./pages/www;
    client_max_body_size 5M;
}
EOF

# Try to start server with duplicate config
timeout 5s ./webserver "$DUPLICATE_CONFIG" >/dev/null 2>&1 &
DUPLICATE_PID=$!
sleep 2
if kill -0 "$DUPLICATE_PID" 2>/dev/null; then
  kill "$DUPLICATE_PID" 2>/dev/null || true
  fail "Duplicate port/server name should cause startup failure"
else
  pass "Duplicate port/server name correctly rejected"
fi

# 2) Test duplicate port with different server names (should work)
say "Testing duplicate port 8085 with different server names..."
DIFFERENT_CONFIG="${TMP_DIR}/different_test.conf"
cat > "$DIFFERENT_CONFIG" << 'EOF'
server {
    listen 127.0.0.1:8085;
    server_name server1.localhost;
    root ./pages/www;
    client_max_body_size 10M;
}

server {
    listen 127.0.0.1:8085;
    server_name server2.localhost;
    root ./pages/www;
    client_max_body_size 5M;
}
EOF

# Try to start server with different server names
timeout 5s ./webserver "$DIFFERENT_CONFIG" >/dev/null 2>&1 &
DIFFERENT_PID=$!
sleep 2
if kill -0 "$DIFFERENT_PID" 2>/dev/null; then
  kill "$DIFFERENT_PID" 2>/dev/null || true
  pass "Duplicate port with different server names works"
else
  fail "Duplicate port with different server names should work"
fi

# 3) Test multiple terminal launch (same config)
say "Testing multiple terminal launch with same config..."
# Start a second instance in background
timeout 10s ./webserver testconfig/test.conf >/dev/null 2>&1 &
SECOND_PID=$!
sleep 3

# Check if second instance is still running (should fail to bind)
if kill -0 "$SECOND_PID" 2>/dev/null; then
  kill "$SECOND_PID" 2>/dev/null || true
  fail "Second instance should fail to bind to same ports"
else
  pass "Second instance correctly rejected (port already in use)"
fi

# 4) Test multiple terminal launch (different config, same ports)
say "Testing multiple terminal launch with different config but same ports..."
# Create config with same ports but different server names
MULTI_CONFIG="${TMP_DIR}/multi_test.conf"
cat > "$MULTI_CONFIG" << 'EOF'
server {
    listen 127.0.0.1:8080;
    server_name multi1.localhost;
    root ./pages/www;
    client_max_body_size 10M;
}

server {
    listen 127.0.0.1:8081;
    server_name multi1.localhost;
    root ./pages/www;
    client_max_body_size 10M;
}

server {
    listen 127.0.0.1:8082;
    server_name multi1.localhost;
    root ./pages/www;
    client_max_body_size 1M;
}

server {
    listen 127.0.0.1:8083;
    server_name multi1.localhost;
    root ./pages/www;
    client_max_body_size 10M;
}
EOF

# Try to start second instance
timeout 10s ./webserver "$MULTI_CONFIG" >/dev/null 2>&1 &
MULTI_PID=$!
sleep 3

# Check if second instance is still running (should fail to bind)
if kill -0 "$MULTI_PID" 2>/dev/null; then
  kill "$MULTI_PID" 2>/dev/null || true
  fail "Second instance with same ports should fail to bind"
else
  pass "Second instance with same ports correctly rejected"
fi

# Clean up test configs
rm -f "$DUPLICATE_CONFIG" "$DIFFERENT_CONFIG" "$MULTI_CONFIG" 2>/dev/null || true

# ===========================================
# ADDITIONAL COMPREHENSIVE TESTS
# ===========================================
say ""
say "=== ADDITIONAL COMPREHENSIVE TESTS ==="

# 16) Redirect matrix (8083) - Original tests
REDIR_HOST_PORT="http://${HOST}:8083"
declare -a REDIR_CASES=(
  "/moved|301|/"
  "/found|302|/images/test.jpg"
  "/see-other|303|/"
  "/temp|307|/upload/"
  "/perm|308|/"
)
ran_any=0
for case in "${REDIR_CASES[@]}"; do
  IFS='|' read -r path exp_code exp_loc <<<"$case"
  url="${REDIR_HOST_PORT}${path}"
  code=$(curl_code -I "${url}")
  if [[ "$code" == "$exp_code" ]]; then
    ran_any=1
    loc=$(curl -sSI "${url}" | awk -F': ' '/^Location:/ {print $2}' | tr -d '\r')
    if [[ -z "$loc" ]]; then
      fail "Redirect ${path} → ${code} but missing Location header"
      continue
    fi
    if [[ "$loc" != "$exp_loc" && "$loc" != "${REDIR_HOST_PORT}${exp_loc}" ]]; then
      fail "Redirect ${path}: expected Location '${exp_loc}', got '${loc}'"
    else
      pass "Redirect ${path} → ${code} (Location: ${loc})"
    fi
  else
    fail "Redirect ${path}: expected ${exp_code}, got ${code}"
  fi
done
if [[ "${ran_any}" -eq 0 ]]; then
  skip "Redirect matrix: none matched (is redirect server on :8083 configured and running?)"
fi

# 17) Server names / virtual hosts on same port (config-dependent)
if [[ -n "${VHOST:-}" ]]; then
  vhost_code=$(curl_code --resolve "${VHOST}:8080:${HOST}" -H "Host: ${VHOST}" "http://${VHOST}:8080/")
  if [[ "$vhost_code" == "200" ]]; then
    pass "Virtual host (${VHOST}) on 8080 responds"
  else
    fail "Virtual host (${VHOST}) expected 200, got $vhost_code"
  fi
else
  skip "Virtual host check skipped (set VHOST=your.server.name)"
fi

# 18) CGI POST (form) on 8081
cgip_code=$(curl_code -X POST -F "name=webserv" "http://${HOST}:8081/cgi_bin/hello.py")
if [[ "$cgip_code" == "200" ]]; then
  pass "CGI POST returns 200"
else
  fail "CGI POST (got $cgip_code expected 200)"
fi

# 19) Upload then GET back the exact file and verify bytes echo
UP_FILE="${TMP_DIR}/echo.bin"
printf "WS-echo-%s" "$(date +%s)" > "$UP_FILE"
UP_NAME="echo_$(date +%s).bin"
up_code=$(curl_code -X POST -F "file=@${UP_FILE};filename=${UP_NAME}" "http://${HOST}:8080/upload/")
if [[ "$up_code" == "200" || "$up_code" == "201" ]]; then
  got="$(curl -sS -m "${CURL_TIMEOUT}" "http://${HOST}:8080/upload/${UP_NAME}" || true)"
  if [[ "$got" == "$(cat "$UP_FILE")" ]]; then
    pass "Upload+GET back exact bytes"
  else
    fail "Uploaded bytes mismatch on download"
  fi
else
  fail "Upload for echo file (got $up_code expected 200/201)"
fi

# 20) Body limit boundary (config-dependent)
skip "Additional body-limit boundary case (config-dependent)—skipped"

# 21) Directory traversal protection
tr_code=$(curl_code "http://${HOST}:8080/../test.conf")
if [[ "$tr_code" == "403" || "$tr_code" == "404" || "$tr_code" == "400" ]]; then
  pass "Directory traversal blocked (${tr_code})"
else
  fail "Directory traversal not blocked (got $tr_code)"
fi

# 22) Connection close honored when client asks
close_hdrs="$(curl -sS -D - -o /dev/null -m "${CURL_TIMEOUT}" -H 'Connection: close' "http://${HOST}:8080/")" || true
if grep -qi '^connection: close' <<<"$close_hdrs"; then
  pass "Connection: close honored"
else
  fail "Connection: close not present in response headers"
fi

# 23) Static DELETE forbidden outside allowed route
code=$(curl_code -X DELETE "http://${HOST}:8080/about.html")
if [[ "$code" == "405" || "$code" == "403" ]]; then
  pass "DELETE protected for non-upload resource"
else
  fail "DELETE should be disallowed on /about.html (got $code)"
fi

# ===========================================
# SIEGE STRESS TEST
# ===========================================
say ""
say "=== SIEGE STRESS TEST ==="

# Function to install siege if not available
install_siege() {
  say "Installing siege..."
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update -qq && sudo apt-get install -y siege >/dev/null 2>&1
  elif command -v yum >/dev/null 2>&1; then
    sudo yum install -y siege >/dev/null 2>&1
  elif command -v brew >/dev/null 2>&1; then
    brew install siege >/dev/null 2>&1
  elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --noconfirm siege >/dev/null 2>&1
  else
    say "Cannot install siege automatically on this system"
    return 1
  fi
}

# Check if siege is available, install if not
if ! have_cmd siege; then
  say "Siege not found. Attempting to install..."
  if install_siege; then
    say "Siege installed successfully"
  else
    skip "Could not install siege—skipping stress test"
  fi
fi

# Run stress tests if siege is available
if have_cmd siege; then
  # Test 1: Basic stress test on index page (availability > 99.5%)
  say "Running basic stress test on index page..."
  SIEGE_C="${SIEGE_C:-25}"
  SIEGE_T="${SIEGE_T:-30S}"
  say "Command: siege -b -c ${SIEGE_C} -t ${SIEGE_T} http://${HOST}:8080/"
  
  siege_out="$(siege -b -c "$SIEGE_C" -t "$SIEGE_T" "http://${HOST}:8080/" 2>/dev/null || true)"
  
  # Try to parse JSON format first (newer siege versions)
  if echo "$siege_out" | grep -q '"availability"'; then
    # JSON format parsing
    avail="$(echo "$siege_out" | grep -o '"availability":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
    trans="$(echo "$siege_out" | grep -o '"transactions":[[:space:]]*[0-9]*' | grep -o '[0-9]*')"
    elapsed="$(echo "$siege_out" | grep -o '"elapsed_time":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
    data_trans="$(echo "$siege_out" | grep -o '"data_transferred":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
    response_time="$(echo "$siege_out" | grep -o '"response_time":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
    trans_rate="$(echo "$siege_out" | grep -o '"transaction_rate":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
    throughput="$(echo "$siege_out" | grep -o '"throughput":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
  else
    # Legacy text format parsing
  avail="$(awk -F': *' '/Availability/ {gsub(/%/,"",$2); print $2}' <<<"$siege_out" | tail -n1)"
    trans="$(awk -F': *' '/Transactions/ {print $2}' <<<"$siege_out" | tail -n1)"
    elapsed="$(awk -F': *' '/Elapsed time/ {print $2}' <<<"$siege_out" | tail -n1)"
    data_trans="$(awk -F': *' '/Data transferred/ {print $2}' <<<"$siege_out" | tail -n1)"
    response_time="$(awk -F': *' '/Response time/ {print $2}' <<<"$siege_out" | tail -n1)"
    trans_rate="$(awk -F': *' '/Transaction rate/ {print $2}' <<<"$siege_out" | tail -n1)"
    throughput="$(awk -F': *' '/Throughput/ {print $2}' <<<"$siege_out" | tail -n1)"
  fi
  
  if [[ -n "$avail" ]]; then
    if awk "BEGIN{exit !($avail>=99.5)}"; then
      pass "Siege stress test: Availability ${avail}% (>= 99.5%)"
    else
      fail "Siege stress test: Availability ${avail}% (< 99.5%)"
    fi
    
    say "Siege Results:"
    say "  Transactions: ${trans:-N/A}"
    say "  Elapsed time: ${elapsed:-N/A}"
    say "  Data transferred: ${data_trans:-N/A}"
    say "  Response time: ${response_time:-N/A}"
    say "  Transaction rate: ${trans_rate:-N/A}"
    say "  Throughput: ${throughput:-N/A}"
  else
    fail "Siege stress test: Could not parse availability from output"
    say "Siege output: $siege_out"
  fi
  
  # Test 2: Stress test on different endpoints
  say "Running stress test on multiple endpoints..."
  siege_multi_out="$(siege -b -c 10 -t 10S "http://${HOST}:8080/" "http://${HOST}:8080/about.html" "http://${HOST}:8081/cgi_bin/hello.py" 2>/dev/null || true)"
  
  # Parse availability for multi-endpoint test
  if echo "$siege_multi_out" | grep -q '"availability"'; then
    multi_avail="$(echo "$siege_multi_out" | grep -o '"availability":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
  else
    multi_avail="$(awk -F': *' '/Availability/ {gsub(/%/,"",$2); print $2}' <<<"$siege_multi_out" | tail -n1)"
  fi
  
  if [[ -n "$multi_avail" ]]; then
    if awk "BEGIN{exit !($multi_avail>=95.0)}"; then
      pass "Siege multi-endpoint test: Availability ${multi_avail}% (>= 95.0%)"
    else
      fail "Siege multi-endpoint test: Availability ${multi_avail}% (< 95.0%)"
    fi
  else
    skip "Siege multi-endpoint test: Could not parse availability"
  fi
  
  # Test 3: Concurrent connection test
  say "Running concurrent connection test..."
  siege_conc_out="$(siege -b -c 50 -t 15S "http://${HOST}:8080/" 2>/dev/null || true)"
  
  # Parse availability for concurrent test
  if echo "$siege_conc_out" | grep -q '"availability"'; then
    conc_avail="$(echo "$siege_conc_out" | grep -o '"availability":[[:space:]]*[0-9.]*' | grep -o '[0-9.]*')"
  else
    conc_avail="$(awk -F': *' '/Availability/ {gsub(/%/,"",$2); print $2}' <<<"$siege_conc_out" | tail -n1)"
  fi
  
  if [[ -n "$conc_avail" ]]; then
    if awk "BEGIN{exit !($conc_avail>=98.0)}"; then
      pass "Siege concurrent test (50 users): Availability ${conc_avail}% (>= 98.0%)"
    else
      fail "Siege concurrent test (50 users): Availability ${conc_avail}% (< 98.0%)"
    fi
  else
    skip "Siege concurrent test: Could not parse availability"
  fi
  
else
  skip "Siege not available—skipping stress tests"
fi

# -----------------------------
# Summary
# -----------------------------
if (( FAILS == 0 )); then
  say "All tests passed ✅"
else
  say "${FAILS} test(s) failed ❌"
  say "server log (tail):"
  tail -n 100 "$LOG_FILE" || true
fi
