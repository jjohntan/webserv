#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import cgi, cgitb, os, json, html, re, sys
cgitb.enable()

# ========= CONFIG (adjust if needed) =========
# Root is typically your project dir (CGI working dir). Keep these relative.
DATA_DIR = os.path.abspath(os.path.join(os.getcwd(), "pages", "data", "profiles"))
# Files should look like: profile_<id>.json
FILENAME_RE = re.compile(r"^profile_([A-Za-z0-9_\-]+)\.json$")

# ========= UTILITIES =========
def ensure_data_dir():
    try:
        os.makedirs(DATA_DIR, exist_ok=True)
    except Exception:
        pass

def list_profiles():
    ensure_data_dir()
    out = []
    for fname in sorted(os.listdir(DATA_DIR)):
        m = FILENAME_RE.match(fname)
        if not m:
            continue
        pid = m.group(1)
        fpath = os.path.join(DATA_DIR, fname)
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                data = json.load(f)
            name   = data.get("name", "")
            gender = data.get("gender", "")
            hobby  = data.get("hobby", "")
            out.append({
                "id": pid,
                "name": str(name),
                "gender": str(gender),
                "hobby": str(hobby),
                "file": fname
            })
        except Exception:
            # Skip malformed files silently
            continue
    return out

def delete_profile(pid):
    # Only allow safe filenames via our regex to prevent path traversal.
    safe = f"profile_{pid}.json"
    if not FILENAME_RE.match(safe):
        return False
    fpath = os.path.join(DATA_DIR, safe)
    if os.path.isfile(fpath):
        try:
            os.remove(fpath)
            return True
        except Exception:
            return False
    return False

def h(s):
    return html.escape(str(s), quote=True)

# ========= HTML RENDER =========
def render_page(profiles, toast=""):
    # Inline CSS matches your aesthetic
    css = """
    :root{
      --bg:#f7fafc; --card:#ffffff; --txt:#1c2540; --muted:#64748b; --border:#e6ecf2;
      --accent:#4f8cff; --danger:#ef4444; --success:#22c55e;
      --radius:18px; --shadow:0 14px 40px rgba(20,32,60,.08); --shadow-sm:0 6px 20px rgba(20,32,60,.06);
      --focus:0 0 0 3px rgba(79,140,255,.25);
    }
    *{box-sizing:border-box}
    body{margin:0;font-family:ui-sans-serif,system-ui,-apple-system,Segoe UI,Roboto,"Helvetica Neue",Arial;background:var(--bg);color:var(--txt);line-height:1.65;padding:28px 18px}
    .wrap{max-width:1060px;margin:0 auto}
    header{margin-bottom:18px}
    .title{font-size:clamp(26px,4.2vw,40px);font-weight:800}
    .subtitle{color:var(--muted);font-size:15px}
    .card{background:var(--card);border:1px solid var(--border);border-radius:var(--radius);box-shadow:var(--shadow);padding:22px}
    .grid{display:grid;gap:16px;grid-template-columns:1fr}
    @media(min-width:680px){.grid{grid-template-columns:1fr 1fr}}
    @media(min-width:980px){.grid{grid-template-columns:1fr 1fr 1fr}}
    .item{display:flex;flex-direction:column;gap:10px;padding:18px;border:1px solid var(--border);border-radius:16px;background:linear-gradient(180deg,#fff,#fbfdff);box-shadow:var(--shadow-sm);position:relative}
    .name{font-weight:800;font-size:18px}
    .meta{font-size:14px;color:var(--muted)}
    .hobby{font-size:14px}
    form{margin:0}
    .row{display:flex;gap:10px;flex-wrap:wrap}
    .btn{
      appearance:none;border:1px solid var(--border);background:#ffffff;color:var(--txt);
      padding:10px 14px;border-radius:12px;cursor:pointer;text-decoration:none;font-weight:700;box-shadow:var(--shadow-sm);
      transition:transform .08s ease,border-color .15s ease,background .15s ease;font-size:14px
    }
    .btn:hover{transform:translateY(-1px);border-color:#d8e4f2}
    .btn.del{border-color:#ffd6d6;background:#fff2f2;color:#b01616}
    .toast{
      display:%(toast_disp)s; margin-bottom:12px; padding:10px 14px; border-radius:12px;
      background:#effaf3; color:#065f46; border:1px solid #c9f0dd; font-size:14px; box-shadow:var(--shadow-sm)
    }
    .empty{padding:16px;border:1px dashed var(--border);border-radius:14px;text-align:center;color:var(--muted)}
    .toprow{display:flex;justify-content:space-between;align-items:flex-end;gap:12px;margin-bottom:10px}
    a.link{color:#304c89;text-decoration:none;font-weight:700}
    """ % {"toast_disp": ("block" if toast else "none")}

    cards_html = []
    if profiles:
        for p in profiles:
            pid    = h(p["id"])
            pname  = h(p["name"] or "(No name)")
            gender = h(p["gender"] or "—")
            hobby  = h(p["hobby"] or "—")
            card = f"""
            <div class="item">
              <div class="name">{pname}</div>
              <div class="meta">ID: {pid} • {gender}</div>
              <div class="hobby">{hobby}</div>
              <div class="row">
                <form method="POST" action="/cgi_bin/delete_cards.py">
                  <input type="hidden" name="_mode" value="delete" />
                  <input type="hidden" name="id" value="{pid}" />
                  <button class="btn del" type="submit" title="Delete this card">× Delete</button>
                </form>
              </div>
            </div>
            """
            cards_html.append(card)
    else:
        cards_html.append('<div class="empty">No saved cards found.</div>')

    html_out = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Delete Cards • WebServ</title>
  <style>{css}</style>
</head>
<body>
  <div class="wrap">
    <header>
      <div class="title">Delete Cards</div>
      <div class="subtitle">Select a card to remove it permanently.</div>
    </header>

    <section class="card">
      <div class="toprow">
        <div class="toast">{h(toast)}</div>
        <div><a class="link" href="/viewcard.html">View Cards</a> • <a class="link" href="/form.html">Register Card</a> • <a class="link" href="/">Home</a></div>
      </div>
      <div class="grid">
        {''.join(cards_html)}
      </div>
    </section>
  </div>
</body>
</html>"""
    return html_out

# ========= MAIN =========
def main():
    form = cgi.FieldStorage()
    method = os.environ.get("REQUEST_METHOD", "GET").upper()

    toast = ""
    if method == "POST":
        mode = form.getfirst("_mode", "")
        if mode == "delete":
            pid = form.getfirst("id", "")
            if pid and re.match(r"^[A-Za-z0-9_\-]+$", pid):
                ok = delete_profile(pid)
                toast = "Card deleted." if ok else "Delete failed (not found or permission denied)."
            else:
                toast = "Invalid card id."

    profiles = list_profiles()

    # Output
    sys.stdout.write("Content-Type: text/html\r\n\r\n")
    sys.stdout.write(render_page(profiles, toast=toast))

if __name__ == "__main__":
    main()
