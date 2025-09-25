#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os, sys, urllib.parse

status = "302 Found"  # or "303 See Other" if you want POST→GET
qs = os.environ.get("QUERY_STRING", "")
params = urllib.parse.parse_qs(qs, keep_blank_values=True)
q = params.get("q", [""])[0].strip()

# Default: Google homepage
if q:
    dest = "https://www.google.com/search?" + urllib.parse.urlencode({"q": q})
else:
    dest = "https://www.google.com/"

sys.stdout.write(f"Status: {status}\r\n")
sys.stdout.write(f"Location: {dest}\r\n")
sys.stdout.write("Content-Type: text/html; charset=utf-8\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(f"""<!doctype html>
<html><head>
  <meta http-equiv="refresh" content="0; url={dest}">
  <title>Redirecting…</title>
</head>
<body>
  <p>Redirecting to <a href="{dest}">{dest}</a></p>
</body></html>""")
