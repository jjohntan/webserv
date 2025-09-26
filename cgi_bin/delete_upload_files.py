#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os, sys, html, urllib.parse

def env(key, default=""):
    return os.environ.get(key, default)

# Allow overriding upload directory via env. Fallback to ./pages/upload relative to cwd.
DEFAULT_UPLOAD_DIR = os.path.abspath(os.path.join(os.getcwd(), "pages", "upload"))
UPLOAD_DIR = os.path.abspath(env("UPLOAD_DIR", DEFAULT_UPLOAD_DIR))

# Basic security: ensure UPLOAD_DIR exists and is a directory we can access.
if not os.path.isdir(UPLOAD_DIR):
    # Print early error page if misconfigured
    print("Content-Type: text/html\r\n\r\n", end="")
    print(f"""<!doctype html>
<html><head><meta charset="utf-8"><title>Delete Uploads • Error</title></head>
<body style="font-family: ui-sans-serif, system-ui; background:#f7fafc; color:#1c2540;">
  <div style="max-width:780px;margin:40px auto;background:#fff;border:1px solid #e6ecf2;border-radius:18px;padding:22px;">
    <h1 style="margin:0 0 10px 0;font-size:22px;color:#304c89;">Directory Management • Delete Files</h1>
    <p>Upload directory not found or not a directory:</p>
    <pre style="background:#f8fafc;border:1px dashed #e6ecf2;padding:10px;border-radius:10px;">{html.escape(UPLOAD_DIR)}</pre>
    <p>Set <code>UPLOAD_DIR</code> in CGI env or create the default path above.</p>
    <div style="display:flex;justify-content:center;gap:10px;margin-top:12px;">
      <a class="btn" href="/index2.html" style="appearance:none;border:1px solid #e6ecf2;background:#fff;color:#1c2540;padding:10px 14px;border-radius:12px;text-decoration:none;font-weight:700;box-shadow:0 6px 20px rgba(20,32,60,.06);">Back to Home</a>
    </div>
  </div>
</body></html>""")
    sys.exit(0)

def sanitize_filename(name: str) -> str:
    """
    Allow only plain file names. Reject path separators and traversal.
    """
    name = urllib.parse.unquote_plus(name).strip()
    if not name or "/" in name or "\\" in name or (name.startswith(".") and name not in (".htaccess",)):
        return ""
    base = os.path.basename(name)
    if ".." in base:
        return ""
    return base

def list_upload_files():
    try:
        entries = sorted(os.listdir(UPLOAD_DIR))
    except Exception:
        entries = []
    return [e for e in entries if os.path.isfile(os.path.join(UPLOAD_DIR, e))]

def parse_post_body():
    ctype = env("CONTENT_TYPE", "")
    clen = env("CONTENT_LENGTH", "")
    data = b""
    if clen.isdigit():
        remaining = int(clen)
        while remaining > 0:
            chunk = sys.stdin.buffer.read(min(remaining, 65536))
            if not chunk: break
            data += chunk
            remaining -= len(chunk)
    if "application/x-www-form-urlencoded" in ctype:
        try:
            return urllib.parse.parse_qs(data.decode("utf-8", "replace"))
        except Exception:
            return {}
    return {}

def delete_selected(selected_names):
    deleted, failed = [], []
    for raw in selected_names:
        fn = sanitize_filename(raw)
        if not fn:
            failed.append(raw)
            continue
        path = os.path.abspath(os.path.join(UPLOAD_DIR, fn))
        if not path.startswith(UPLOAD_DIR + os.sep):
            failed.append(fn)
            continue
        try:
            os.remove(path)
            deleted.append(fn)
        except Exception:
            failed.append(fn)
    return deleted, failed

def header_html(title="Delete Uploads"):
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>{html.escape(title)}</title>
  <style>
    :root{{
      --bg:#f7fafc; --card:#ffffff; --txt:#1c2540; --muted:#64748b; --border:#e6ecf2;
      --radius:18px; --shadow:0 14px 40px rgba(20,32,60,.08); --shadow-sm:0 6px 20px rgba(20,32,60,.06);
      --focus:0 0 0 3px rgba(79,140,255,.25);
    }}
    *{{box-sizing:border-box}}
    body{{
      margin:0; font-family:ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, "Helvetica Neue", Arial;
      color:var(--txt);
      background:
        radial-gradient(1200px 800px at -10% -10%, #e9f2ff 0%, transparent 70%) no-repeat,
        radial-gradient(1200px 800px at 110% 10%, #eafff9 0%, transparent 70%) no-repeat,
        var(--bg);
      line-height:1.65; padding:28px 18px;
    }}
    .wrap{{max-width:940px;margin:0 auto}}
    .card{{background:var(--card); border:1px solid var(--border); border-radius:var(--radius); box-shadow:var(--shadow); padding:22px}}
    h1{{margin:0 0 12px 0; font-size:22px; letter-spacing:.2px; color:#304c89}}
    .muted{{color:var(--muted); font-size:14px}}
    .row{{display:flex; gap:10px; flex-wrap:wrap; align-items:center; margin-top:14px; justify-content:center}}
    .btn{{
      appearance:none; border:1px solid var(--border); background:#ffffff; color:#1c2540;
      padding:10px 14px; border-radius:12px; cursor:pointer; text-decoration:none; font-weight:700; box-shadow:var(--shadow-sm);
      transition:transform .08s ease, box-shadow .15s ease, border-color .15s ease, background .15s ease; font-size:14px;
    }}
    .btn:hover{{transform:translateY(-1px); border-color:#d8e4f2}}
    .btn.danger{{background:#fff2f2; border-color:#ffd6d6; color:#b01616}}
    .filegrid{{display:grid; grid-template-columns:repeat(auto-fill, minmax(240px,1fr)); gap:10px; margin-top:10px}}
    .file{{border:1px solid var(--border); border-radius:12px; padding:10px; background:#fff}}
    .file label{{display:flex; align-items:center; gap:10px; cursor:pointer}}
    .msg{{margin:8px 0; padding:10px 12px; border-radius:12px; font-size:14px}}
    .msg.ok{{background:#effdf8; border:1px solid #d6f4ec; color:#0f8c76}}
    .msg.err{{background:#fff2f2; border:1px solid #ffd6d6; color:#b01616}}
    footer{{margin-top:22px; text-align:center; font-size:13px; color:#64748b}}
  </style>
</head>
<body>
  <div class="wrap">
    <section class="card">
      <h1>Directory Management • Delete Files</h1>
      <div class="muted">Upload path: <code>{html.escape(UPLOAD_DIR)}</code></div>
"""

def footer_html():
    # footer kept minimal; no extra buttons to avoid duplication
    return """    </section>
    <footer>WebServ • CGI • Delete</footer>
  </div>
</body>
</html>"""

def render_list_page(message_ok=None, message_err=None):
    files = list_upload_files()
    print("Content-Type: text/html\r\n\r\n", end="")
    print(header_html(), end="")
    if message_ok:
        print(f'<div class="msg ok">{html.escape(message_ok)}</div>')
    if message_err:
        print(f'<div class="msg err">{html.escape(message_err)}</div>')
    if not files:
        print('<p class="muted">No files found in the upload directory.</p>')
        # Still show centered Back to Home for convenience
        print('<div class="row"><a class="btn" href="/index2.html">Back to Home</a></div>')
    else:
        print('<form method="POST" action="">')
        print('<div class="filegrid">')
        for fn in files:
            esc = html.escape(fn)
            print(f'<div class="file"><label><input type="checkbox" name="files" value="{esc}"> {esc}</label></div>')
        print('</div>')
        # Centered action row with only two controls
        print('<div class="row">')
        print('<button class="btn danger" type="submit">Delete selected</button>')
        print('<a class="btn" href="/index2.html">Back to Home</a>')
        print('</div>')
        print('</form>')
    print(footer_html(), end="")

def handle_get():
    render_list_page()

def handle_post():
    form = parse_post_body()
    selected = form.get("files", [])
    if not selected:
        render_list_page(message_err="No files selected.")
        return
    deleted, failed = delete_selected(selected)
    msg_ok = ""
    msg_err = ""
    if deleted:
        msg_ok = "Deleted: " + ", ".join(deleted[:8]) + (" ..." if len(deleted) > 8 else "")
    if failed:
        msg_err = "Failed: " + ", ".join(failed[:8]) + (" ..." if len(failed) > 8 else "")
    render_list_page(message_ok=msg_ok or None, message_err=msg_err or None)

def main():
    method = env("REQUEST_METHOD", "GET").upper()
    if method == "POST":
        handle_post()
    else:
        handle_get()

if __name__ == "__main__":
    main()
