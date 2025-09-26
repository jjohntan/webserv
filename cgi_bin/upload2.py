#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os, sys, html, cgi, cgitb, time
cgitb.enable()

# ---------- Config ----------
def env(key, default=""):
    return os.environ.get(key, default)

DEFAULT_UPLOAD_DIR = os.path.abspath(os.path.join(os.getcwd(), "pages", "upload"))
UPLOAD_DIR = os.path.abspath(env("UPLOAD_DIR", DEFAULT_UPLOAD_DIR))

# Comma-separated, case-insensitive list. Example to override:
#   ALLOWED_EXTS="jpg,jpeg,png,gif,webp,pdf,txt,zip,mp3,mp4"
ALLOWED_EXTS = {e.strip().lower() for e in env(
    "ALLOWED_EXTS",
    "jpg,jpeg,png,gif,webp,svg,pdf,txt,md,zip,tar,gz,7z,mp3,wav,mp4,mov,avi,webm"
).split(",") if e.strip()}

MAX_BYTES = int(env("MAX_UPLOAD_BYTES", "52428800"))  # 50 MB default
# ---------------------------

def ensure_upload_dir():
    try:
        if not os.path.isdir(UPLOAD_DIR):
            os.makedirs(UPLOAD_DIR, 0o755)
        return True, ""
    except Exception as e:
        return False, str(e)

def sanitize_filename(name: str) -> str:
    base = os.path.basename((name or "").strip())
    if not base or ".." in base or "/" in base or "\\" in base:
        return ""
    return base

def ext_allowed(fname: str) -> bool:
    _, ext = os.path.splitext(fname)
    return (ext.lower().lstrip(".") in ALLOWED_EXTS) if ext else False

def safe_destination(fname: str) -> str:
    """
    If UPLOAD_DIR/fname exists, return 'name (1).ext', 'name (2).ext', etc.
    """
    root, ext = os.path.splitext(fname)
    candidate = os.path.join(UPLOAD_DIR, fname)
    if not os.path.exists(candidate):
        return candidate
    i = 1
    while True:
        new_name = f"{root} ({i}){ext}"
        candidate = os.path.join(UPLOAD_DIR, new_name)
        if not os.path.exists(candidate):
            return candidate
        i += 1

def human_size(n):
    units = ["B","KB","MB","GB","TB"]
    i = 0
    f = float(n)
    while f >= 1024 and i < len(units)-1:
        f /= 1024.0
        i += 1
    return f"{f:.2f} {units[i]}"

def page_head(title="Upload Files"):
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>{html.escape(title)}</title>
  <style>
    :root{{
      --bg:#f7fafc; --card:#ffffff; --txt:#1c2540; --muted:#64748b; --border:#e6ecf2;
      --accent:#4f8cff; --accent-2:#2dd4bf; --accent-3:#f59e0b; --danger:#ef4444;
      --radius:18px; --shadow:0 14px 40px rgba(20,32,60,.08); --shadow-sm:0 6px 20px rgba(20,32,60,.06);
      --focus:0 0 0 3px rgba(79,140,255,.25);
    }}
    *{{box-sizing:border-box}}
    html,body{{height:100%}}
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

    .grid{{display:grid; grid-template-columns:1fr; gap:12px; margin-top:10px}}
    label.small{{font-size:13px; color:#64748b}}
    input[type="file"]{{display:block; width:100%; padding:10px; border:1px dashed var(--border); border-radius:12px; background:#fff}}

    .row{{display:flex; gap:10px; flex-wrap:wrap; align-items:center; margin-top:14px; justify-content:center}}
    .btn{{
      appearance:none; border:1px solid var(--border); background:#ffffff; color:#1c2540;
      padding:10px 14px; border-radius:12px; cursor:pointer; text-decoration:none; font-weight:700; box-shadow:var(--shadow-sm);
      transition:transform .08s ease, box-shadow .15s ease, border-color .15s ease, background .15s ease; font-size:14px;
    }}
    .btn:hover{{transform:translateY(-1px); border-color:#d8e4f2}}
    .btn.primary{{background:#eff5ff; border-color:#dbe8ff; color:#2253c8}}

    .msg{{margin:8px 0; padding:10px 12px; border-radius:12px; font-size:14px}}
    .msg.ok{{background:#effdf8; border:1px solid #d6f4ec; color:#0f8c76}}
    .msg.err{{background:#fff2f2; border:1px solid #ffd6d6; color:#b01616}}

    ul.files{{margin:6px 0 0 18px; padding:0}}
    footer{{margin-top:22px; text-align:center; font-size:13px; color:#64748b}}
  </style>
</head>
<body>
  <div class="wrap">
    <section class="card">
      <h1>Directory Management • Upload Files</h1>
      <div class="muted">Upload path: <code>{html.escape(UPLOAD_DIR)}</code></div>
"""

def page_foot():
    return """    </section>
    <footer>WebServ • CGI • Upload</footer>
  </div>
</body>
</html>"""

def print_headers():
    sys.stdout.write("Content-Type: text/html\r\n\r\n")

def render_form(message_ok=None, message_err=None, details_html=""):
    sys.stdout.write(page_head())
    if message_ok:
        sys.stdout.write(f'<div class="msg ok">{html.escape(message_ok)}</div>')
    if message_err:
        sys.stdout.write(f'<div class="msg err">{html.escape(message_err)}</div>')

    sys.stdout.write('<form method="POST" enctype="multipart/form-data" action="">')
    sys.stdout.write('  <div class="grid">')
    sys.stdout.write('    <div>')
    sys.stdout.write('      <label class="small">Choose file(s) to upload</label>')
    sys.stdout.write('      <input type="file" name="file" multiple />')
    sys.stdout.write('      <div class="muted">Allowed: ' + html.escape(", ".join(sorted(ALLOWED_EXTS))) + f' • Max per file: {human_size(MAX_BYTES)}</div>')
    sys.stdout.write('    </div>')
    sys.stdout.write('  </div>')
    sys.stdout.write('  <div class="row">')
    sys.stdout.write('    <button class="btn primary" type="submit">Upload</button>')
    sys.stdout.write('    <a class="btn" href="/index.html">Back to Home</a>')
    sys.stdout.write('    <a class="btn" href="/upload/">Open /upload/</a>')
    sys.stdout.write('  </div>')
    sys.stdout.write('</form>')

    if details_html:
        sys.stdout.write(details_html)

    sys.stdout.write(page_foot())

def handle_get():
    render_form()

def handle_post():
    ok, err = ensure_upload_dir()
    if not ok:
        render_form(message_err=f"Upload directory not available: {err}")
        return

    form = cgi.FieldStorage()
    if "file" not in form:
        render_form(message_err="No file field in the request.")
        return

    items = form["file"]
    file_items = items if isinstance(items, list) else [items]

    saved, failed = [], []
    for fileitem in file_items:
        if not getattr(fileitem, "filename", ""):
            failed.append(("—", "No filename provided"))
            continue

        original = sanitize_filename(fileitem.filename)
        if not original:
            failed.append((fileitem.filename, "Invalid filename"))
            continue

        if not ext_allowed(original):
            failed.append((original, "Extension not allowed"))
            continue

        dest_path = safe_destination(original)
        if not dest_path.startswith(UPLOAD_DIR + os.sep):
            failed.append((original, "Path escaped upload dir"))
            continue

        total = 0
        try:
            with open(dest_path, "wb") as out:
                CHUNK = 1024 * 256
                while True:
                    chunk = fileitem.file.read(CHUNK)
                    if not chunk:
                        break
                    total += len(chunk)
                    if total > MAX_BYTES:
                        raise ValueError("File exceeds MAX_UPLOAD_BYTES limit")
                    out.write(chunk)
            os.chmod(dest_path, 0o644)

            # Collect saved file info
            st = os.stat(dest_path)
            saved.append((
                os.path.basename(dest_path),
                total,
                st.st_mtime
            ))
        except Exception as e:
            try:
                if os.path.exists(dest_path):
                    os.remove(dest_path)
            except Exception:
                pass
            failed.append((original, str(e)))

    # Compose messages and details list
    msg_ok = ""
    if saved:
        names = ", ".join([n for n, _, _ in saved[:5]]) + (" ..." if len(saved) > 5 else "")
        msg_ok = f"Uploaded: {names}"

    msg_err = ""
    if failed:
        names = ", ".join([n for n, _ in failed[:5]])
        msg_err = f"Failed: {names}"

    details_lines = ["<div class='muted'><strong>Result details</strong></div><ul class='files'>"]
    for n, sz, mtime in saved:
        details_lines.append(
            f"<li>✅ {html.escape(n)} — {human_size(sz)} • Last modified: "
            f"{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(mtime))}</li>"
        )
    for n, e in failed:
        details_lines.append(f"<li>❌ {html.escape(n)} — {html.escape(str(e))}</li>")
    details_lines.append("</ul>")

    render_form(message_ok=msg_ok or None, message_err=msg_err or None, details_html="".join(details_lines))

def main():
    print_headers()
    method = env("REQUEST_METHOD", "GET").upper()
    try:
        if method == "POST":
            handle_post()
        else:
            handle_get()
    except Exception as e:
        sys.stdout.write(page_head("Upload Error"))
        sys.stdout.write(f'<div class="msg err">Unexpected error: {html.escape(str(e))}</div>')
        sys.stdout.write('<div class="row"><a class="btn" href="/index.html">Back to Home</a></div>')
        sys.stdout.write(page_foot())

if __name__ == "__main__":
    main()
