1) Quick manual checks with curl (HTTP/1.1 keep-alive + Content-Length)
A) Basic 200 OK
- Expect: Connection: keep-alive and a correct Content-Length that matches the body bytes
curl -v --http1.1 http://127.0.0.1:8080/index.html -o /tmp/body.html -D /tmp/headers.txt
grep -i '^Connection:' /tmp/headers.txt
grep -i '^Content-Length:' /tmp/headers.txt
wc -c </tmp/body.html            # compare this byte count to the Content-Length

B) Reuse the same TCP connection (keep-alive)
- Expect to see curl say it reused the connection for the second URL.
curl -v --http1.1 \
  http://127.0.0.1:8080/index.html \
  http://127.0.0.1:8080/about.html >/dev/null
# Look in the verbose output for:
#   * Re-using existing connection! (#0) with host 127.0.0.1

C) Force close (Connection: close)
- Expect: Connection: close in the response and the socket closed after the response.
	curl -v --http1.1 -H 'Connection: close' http://127.0.0.1:8080/index.html -o /dev/null
- To inspect connection close, run this command on another terminal
	watch -n 0.2 "ss -tanp 'sport = :8080' | sed -n '1,3p;/:8080/p'"


D) HTTP/1.0 behavior (no keep-alive by default)
- Expect: Connection: close and the server closes the socket.
printf 'GET /index.html HTTP/1.0\r\nHost: 127.0.0.1:8080\r\n\r\n' | nc 127.0.0.1 8080

E) Error paths still correct
- 404: Expect proper Content-Length and the right Connection (keep-alive unless the client asked to close).
curl -v --http1.1 http://127.0.0.1:8080/does-not-exist -o /tmp/err.html -D /tmp/err.h
grep -i '^Connection:' /tmp/err.h
grep -i '^Content-Length:' /tmp/err.h
wc -c </tmp/err.html

F) 204 No Content
Expect: no body; your implementation won’t add Content-Length when body is empty (okay for 204).
# Delete a real file to get 204
curl -v --http1.1 -X DELETE http://127.0.0.1:8080/some-file -o /dev/null -D -


2) Single-socket test (prove two requests on ONE TCP connection)
	One-shot (two requests, one connection)
		printf 'GET /index.html HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\nGET /about.html HTTP/1.1\r\nHost: 127.0.0.1:8080\r\n\r\n' \
		| nc 127.0.0.1 8080

	You should see two full responses back-to-back.

3) Automated smoke test (Python, optional but handy)
This opens one socket, sends two requests, and validates Content-Length vs bytes read:
	import socket, sys

	HOST, PORT = "127.0.0.1", 8080

	def read_headers(sock):
		data = b""
		while b"\r\n\r\n" not in data:
			chunk = sock.recv(4096)
			if not chunk:
				raise RuntimeError("connection closed while reading headers")
			data += chunk
		head, rest = data.split(b"\r\n\r\n", 1)
		return head.decode("latin1"), rest

	def get_content_length(headers):
		for line in headers.split("\r\n"):
			if line.lower().startswith("content-length:"):
				return int(line.split(":",1)[1].strip())
		return None

	def do_get(sock, path):
		req = f"GET {path} HTTP/1.1\r\nHost: {HOST}:{PORT}\r\n\r\n".encode("ascii")
		sock.sendall(req)
		headers, remainder = read_headers(sock)
		cl = get_content_length(headers)
		# read body
		body = remainder
		if cl is not None:
			while len(body) < cl:
				chunk = sock.recv(4096)
				if not chunk:
					break
				body += chunk
			assert len(body) == cl, f"Body size {len(body)} != Content-Length {cl}"
		return headers, body

	s = socket.create_connection((HOST, PORT))
	h1, b1 = do_get(s, "/index.html")
	print("Resp1 Connection:", [l for l in h1.split("\r\n") if l.lower().startswith("connection:")])
	h2, b2 = do_get(s, "/about.html")
	print("Resp2 Connection:", [l for l in h2.split("\r\n") if l.lower().startswith("connection:")])
	s.close()
	print("OK: both requests over one socket and Content-Length matched.")

	python3 test_keepalive_cl.py


