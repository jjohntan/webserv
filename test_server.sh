#!/usr/bin/env bash
set -Eeuo pipefail

# -----------------------------
# Config (override via env vars)
# -----------------------------
CFG_FILE="${CFG_FILE:-testconfig/test.conf}"
LOG_FILE="${LOG_FILE:-server_test.log}"
BIN_CANDIDATES=( "${BIN:-}" ./webserv ./webserver ./a.out )
PORTS=(8080 8081 8082)
HOST="127.0.0.1"
STARTUP_WAIT_SECS=6       # how long to wait for ports
CURL_TIMEOUT=6
TMP_DIR="$(mktemp -d -t webserv-tests-XXXXXX)"
trap 'cleanup' EXIT INT TERM

cleanup() {
  set +e
  [[ -n "${SERVER_PID:-}" ]] && kill "${SERVER_PID}" 2>/dev/null || true
  sleep 0.2
  for p in "${PORTS[@]}"; do
    kill_port_listeners "$p" || true
  done
  rm -rf "$TMP_DIR" >/dev/null 2>&1 || true
}

say()   { printf "%s\n" "$*"; }
pass()  { say "[PASS] $*"; }
fail()  { say "[FAIL] $*"; ((FAILS++)) || true; }
fatal() { say "[FATAL] $*"; exit 1; }

# -----------------------------
# Helpers
# -----------------------------

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
    [[ -z "$pid" ]] && continue
    if [[ "${SERVER_PID:-}" == "$pid" ]]; then
      continue
    fi
    kill "$pid" 2>/dev/null && killed=1
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
  curl -sS -o /dev/null -m "$CURL_TIMEOUT" -w '%{http_code}' "$@" 2>/dev/null || echo 000
}
# Never crash the script if body fetch fails
curl_body() {
  curl -sS -m "$CURL_TIMEOUT" "$@" || true
}

need_build() {
  [[ -n "${BIN_PATH:-}" ]] && return 1
  [[ -f Makefile ]] || return 1
  return 0
}

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
curl -sS -m "$CURL_TIMEOUT" -o /dev/null -X POST -F "file=@${TMP_DIR}/${DEL_NAME};filename=${DEL_NAME}" "http://${HOST}:8080/upload/" || true
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
