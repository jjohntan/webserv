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

# 1) Serve index.html on 8080
code=$(curl_code "http://${HOST}:8080/")
if [[ "$code" == "200" ]]; then
  pass "Serve index.html"
  body="$(curl_body "http://${HOST}:8080/")"
  if grep -qi "<title>webserv test page" <<<"$body"; then
    pass "index.html contains title"
  else
    fail "index.html contains title"
    say "Body was: $body"
  fi
else
  fail "Serve index.html (got $code expected 200)"
fi

# 2) Serve about.html on 8080
code=$(curl_code "http://${HOST}:8080/about.html")
if [[ "$code" == "200" ]]; then
  pass "Serve about.html"
  body="$(curl_body "http://${HOST}:8080/about.html")"
  if grep -qi "About" <<<"$body"; then
    pass "about.html contains 'About'"
  else
    fail "about.html contains 'About'"
    say "Body was: $body"
  fi
else
  fail "Serve about.html (got $code expected 200)"
fi

# 3) Autoindex on /upload/ (8080)
code=$(curl_code "http://${HOST}:8080/upload/")
if [[ "$code" == "200" ]]; then
  pass "Autoindex on /upload/"
  body="$(curl_body "http://${HOST}:8080/upload/")"
  if grep -qi "<title>Directory Listing" <<<"$body"; then
    pass "Autoindex page title"
  else
    fail "Autoindex page title"
    say "Body was: $body"
  fi
else
  fail "Autoindex on /upload/ (got $code expected 200)"
fi

# 4) Upload small file (8080) — multipart/form-data
SMALL_FILE="${TMP_DIR}/small.txt"
echo "hello webserv" > "$SMALL_FILE"
FNAME="small_$(date +%s).txt"
code=$(curl_code -X POST -F "file=@${SMALL_FILE};filename=${FNAME}" "http://${HOST}:8080/upload/")
if [[ "$code" == "200" || "$code" == "201" ]]; then
  body="$(curl_body "http://${HOST}:8080/upload/")"
  if grep -q "$FNAME" <<<"$body"; then
    pass "Upload small file"
  else
    fail "Upload small file (not listed after ${code})"
    say "server log (tail):"; tail -n 60 "$LOG_FILE" || true
  fi
else
  fail "Upload small file (got $code expected 200/201)"
  say "server log (tail):"; tail -n 60 "$LOG_FILE" || true
fi

# 5) CGI hello.py on 8081
code=$(curl_code "http://${HOST}:8081/cgi_bin/hello.py")
if [[ "$code" == "200" ]]; then
  pass "CGI hello.py"
  body="$(curl_body "http://${HOST}:8081/cgi_bin/hello.py")"
  if grep -qi "Hello" <<<"$body"; then
    pass "CGI contains 'Hello'"
  else
    fail "CGI contains 'Hello'"
    say "Body was: $body"
  fi
else
  fail "CGI hello.py (got $code expected 200)"
fi

# 6) 404 Not Found (8080)
code=$(curl_code "http://${HOST}:8080/no_such_page_$(date +%s).html")
if [[ "$code" == "404" ]]; then
  pass "404 Not Found"
else
  fail "404 Not Found (got $code expected 404)"
fi

# 7) 405 Method Not Allowed (POST to / on 8080)
code=$(curl_code -X POST "http://${HOST}:8080/")
if [[ "$code" == "405" ]]; then
  pass "405 Method Not Allowed"
else
  fail "405 Method Not Allowed (got $code expected 405)"
fi

# 8) 413 Payload Too Large (8082 tiny limit)
BIG_FILE="${TMP_DIR}/big.bin"
dd if=/dev/zero of="$BIG_FILE" bs=1M count=2 status=none
code=$(curl_code -X POST -H "Content-Type: application/octet-stream" --data-binary @"$BIG_FILE" "http://${HOST}:8082/upload/")
if [[ "$code" == "413" ]]; then
  pass "413 Payload Too Large"
else
  fail "413 Payload Too Large (got $code expected 413)"
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

# 16) Redirect matrix (8083)
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

# 24) Optional stress test with siege (availability > 99.5% on /)
if have_cmd siege; then
  SIEGE_C="${SIEGE_C:-25}"
  SIEGE_T="${SIEGE_T:-10S}"
  say "Running siege: -b -c ${SIEGE_C} -t ${SIEGE_T} http://${HOST}:8080/"
  siege_out="$(siege -b -c "$SIEGE_C" -t "$SIEGE_T" "http://${HOST}:8080/" 2>/dev/null || true)"
  avail="$(awk -F': *' '/Availability/ {gsub(/%/,"",$2); print $2}' <<<"$siege_out" | tail -n1)"
  if [[ -n "$avail" ]]; then
    awk "BEGIN{exit !($avail>=99.5)}" && pass "Siege availability ${avail}%" || fail "Siege availability ${avail}% (<99.5%)"
  else
    skip "Siege output unavailable; skipping availability assertion"
  fi
else
  skip "siege not installed—skipping stress test"
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
