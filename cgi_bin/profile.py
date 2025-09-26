#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import cgitb, json, os, sys, urllib.parse, shutil, re, uuid, html
cgitb.enable()

# -------- Paths (repo layout) --------
HERE = os.path.dirname(os.path.realpath(__file__))      # .../cgi_bin
ROOT = os.path.normpath(os.path.join(HERE, ".."))       # repo root

DATA_DIR = os.path.join(ROOT, "pages", "data")          # old combined location
DATA_FILE = os.path.join(DATA_DIR, "profiles.json")     # old combined file

PROFILES_DIR = os.path.join(DATA_DIR, "profiles")       # NEW: per-file dir

LEGACY_DIR = os.path.join(HERE, "data")                 # very old
LEGACY_FILE = os.path.join(LEGACY_DIR, "profiles.json") # very old combined file

FILENAME_RE = re.compile(r"^profile_([A-Za-z0-9_\-]+)\.json$")  # safe file pattern
SAFE_ID_RE  = re.compile(r"[A-Za-z0-9_\-]+")

# -------- Utilities --------
def slugify(s: str) -> str:
    s = (s or "").strip().lower()
    s = re.sub(r"[^a-z0-9_\-]+", "-", s).strip("-")
    return s or "user"

def new_id_from_name(name: str) -> str:
    base = slugify(name)
    cand = base
    i = 1
    # avoid collisions
    while os.path.exists(os.path.join(PROFILES_DIR, f"profile_{cand}.json")):
        i += 1
        cand = f"{base}-{i}"
    return cand

def safe_id(pid: str, name: str) -> str:
    # prefer provided id if safe; else derive from name; else random short
    if pid and SAFE_ID_RE.fullmatch(pid):
        return pid
    if name:
        return new_id_from_name(name)
    return uuid.uuid4().hex[:8]

def ensure_dirs():
    os.makedirs(PROFILES_DIR, exist_ok=True)

def load_per_file_profiles():
    """Return list of profiles from per-file dir."""
    ensure_dirs()
    out = []
    for fname in sorted(os.listdir(PROFILES_DIR)):
        m = FILENAME_RE.match(fname)
        if not m:
            continue
        fpath = os.path.join(PROFILES_DIR, fname)
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                data = json.load(f)
            # normalize minimal fields
            out.append({
                "id": data.get("id") or m.group(1),
                "name": data.get("name", ""),
                "gender": data.get("gender", ""),
                "hobby": data.get("hobby", "")
            })
        except Exception:
            # skip malformed files
            continue
    return out

def write_profile_file(profile):
    """Write a single profile to its file."""
    ensure_dirs()
    pid = profile["id"]
    fname = f"profile_{pid}.json"
    if not FILENAME_RE.match(fname):
        raise ValueError("unsafe id/filename")
    fpath = os.path.join(PROFILES_DIR, fname)
    tmp = fpath + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(profile, f, ensure_ascii=False)
    os.replace(tmp, fpath)

def migrate_if_needed():
    """If we still have a combined profiles.json (or legacy), split to per-file."""
    ensure_dirs()

    # Move legacy combined file to modern combined path first (if per-file empty)
    if (not os.path.exists(DATA_FILE)) and os.path.exists(LEGACY_FILE):
        try:
            os.makedirs(DATA_DIR, exist_ok=True)
            shutil.move(LEGACY_FILE, DATA_FILE)
        except Exception:
            try:
                shutil.copy2(LEGACY_FILE, DATA_FILE)
            except Exception:
                pass

    # If per-file dir already has profiles, nothing to do
    if any(FILENAME_RE.match(x) for x in os.listdir(PROFILES_DIR) if os.path.isfile(os.path.join(PROFILES_DIR, x))):
        return

    # Otherwise, split the combined file if it exists
    if os.path.exists(DATA_FILE):
        try:
            with open(DATA_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            profiles = (data or {}).get("profiles", [])
            for p in profiles:
                pid = safe_id(str(p.get("id", "")).strip(), str(p.get("name", "")).strip())
                record = {
                    "id": pid,
                    "name": str(p.get("name", "")),
                    "gender": str(p.get("gender", "")),
                    "hobby": str(p.get("hobby", ""))
                }
                write_profile_file(record)
        except Exception:
            # ignore migration failures silently
            pass

def parsed_query():
    qs = os.environ.get("QUERY_STRING", "")
    if qs:
        return urllib.parse.parse_qs(qs, keep_blank_values=True)
    uri = os.environ.get("REQUEST_URI", "")
    if uri and "?" in uri:
        qs = uri.split("?", 1)[1]
        return urllib.parse.parse_qs(qs, keep_blank_values=True)
    return {}

def read_body():
    try:
        length = int(os.environ.get("CONTENT_LENGTH") or "0")
    except:
        length = 0
    body = sys.stdin.read(length) if length > 0 else ""
    return urllib.parse.parse_qs(body, keep_blank_values=True)

def respond_json(obj, status="200 OK", extra_headers=None):
    # Helpful headers for debugging in your server
    profile_count = 0
    try:
        profile_count = len(load_per_file_profiles())
    except Exception:
        pass
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: application/json; charset=utf-8\r\n")
    sys.stdout.write(f"X-Profiles-Dir: {PROFILES_DIR}\r\n")
    sys.stdout.write(f"X-Profile-Count: {profile_count}\r\n")
    sys.stdout.write("X-Debug-Version: 5\r\n")
    if extra_headers:
        for k, v in extra_headers.items():
            sys.stdout.write(f"{k}: {v}\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write(json.dumps(obj, ensure_ascii=False))

def respond_html(title, body_html, status="200 OK", extra_headers=None):
    sys.stdout.write(f"Status: {status}\r\n")
    sys.stdout.write("Content-Type: text/html; charset=utf-8\r\n")
    if extra_headers:
        for k, v in extra_headers.items():
            sys.stdout.write(f"{k}: {v}\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write(f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>{title}</title>
  <style>
    :root{{
      --bg:#f7fafc; --card:#ffffff; --txt:#1c2540; --muted:#64748b; --border:#e6ecf2;
      --accent:#4f8cff; --accent-2:#22c55e; --accent-3:#f59e0b; --danger:#ef4444;
      --radius:18px; --shadow:0 14px 40px rgba(20,32,60,.08);
      --shadow-sm:0 6px 20px rgba(20,32,60,.06); --focus:0 0 0 3px rgba(79,140,255,.25);
    }}
    *{{box-sizing:border-box}}
    body{{
      margin:0; font-family:ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, "Helvetica Neue", Arial;
      color:var(--txt); background:
        radial-gradient(1200px 800px at -10% -10%, #e9f2ff 0%, transparent 70%) no-repeat,
        radial-gradient(1200px 800px at 110% 10%, #eafff9 0%, transparent 70%) no-repeat,
        var(--bg);
      line-height:1.65; padding:28px 18px;
    }}
    .wrap{{max-width:820px;margin:0 auto}}
    .card{{background:var(--card); border:1px solid var(--border); border-radius:var(--radius); box-shadow:var(--shadow); padding:22px}}
    .title{{font-size:clamp(26px,4.2vw,40px); font-weight:800; display:flex; align-items:center; gap:10px}}
    .subtitle{{color:var(--muted); font-size:15px}}
    .p-card{{border:1px solid var(--border); border-radius:16px; padding:18px; box-shadow:var(--shadow-sm);
            background:linear-gradient(180deg,#ffffff,#fbfdff); display:flex; flex-direction:column; gap:8px; min-height:160px}}
    .p-title{{font-weight:900; font-size:22px; display:flex; align-items:center; gap:10px}}
    .p-meta{{color:var(--muted); font-size:14px; display:flex; align-items:center; gap:8px}}
    .chip{{display:inline-flex; align-items:center; gap:8px; font-size:12px; padding:6px 10px; border-radius:999px; border:1px solid var(--border); background:#fff}}
    .row{{display:flex; gap:10px; flex-wrap:wrap; margin-top:16px}}
    .btn{{
      appearance:none; border:1px solid var(--border); background:#ffffff; color:var(--txt);
      padding:12px 16px; border-radius:12px; cursor:pointer; text-decoration:none; font-weight:800; font-size:16px;
      box-shadow:var(--shadow-sm); transition:transform .08s ease, border-color .15s ease, background .15s ease;
      display:inline-flex; align-items:center; gap:10px;
    }}
    .btn:hover{{transform:translateY(-1.5px); border-color:#d8e4f2}}
    .btn.primary{{background:#eef4ff; border-color:#d9e7ff}}
    .ico{{width:18px;height:18px; display:inline-block;}}
    .brand{{width:42px;height:42px;border-radius:12px;display:grid;place-items:center;
            background:linear-gradient(135deg,#e8f0ff,#eafff7); border:1px solid var(--border); box-shadow:var(--shadow);}}
  </style>
</head>
<body>
  <!-- SVG sprite -->
  <svg width="0" height="0" style="position:absolute;left:-9999px;visibility:hidden" aria-hidden="true">
    <symbol id="ico-check" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <polyline points="20 6 9 17 4 12"/>
    </symbol>
    <symbol id="ico-user" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <path d="M20 21a8 8 0 10-16 0"/><circle cx="12" cy="7" r="4"/>
    </symbol>
    <symbol id="ico-card" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <rect x="3" y="5" width="18" height="14" rx="2"/><line x1="3" y1="10" x2="21" y2="10"/>
    </symbol>
    <symbol id="ico-plus" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>
    </symbol>
    <symbol id="ico-home" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <path d="M3 12l9-9 9 9"/><path d="M9 21V9h6v12"/>
    </symbol>
    <symbol id="ico-spark" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <path d="M12 2v6M12 16v6M4.93 4.93l4.24 4.24M14.83 14.83l4.24 4.24M2 12h6M16 12h6M4.93 19.07l4.24-4.24M14.83 9.17l4.24-4.24"/>
    </symbol>
    <symbol id="ico-id" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <rect x="3" y="5" width="18" height="14" rx="2"/>
      <line x1="7" y1="9" x2="11" y2="9"/><line x1="7" y1="13" x2="11" y2="13"/>
      <circle cx="16.5" cy="11.5" r="2.5"/>
    </symbol>
  </svg>

  <div class="wrap">
    <header style="margin-bottom:18px; display:flex; align-items:flex-start; gap:12px">
      <div class="brand" aria-hidden="true">
        <svg width="22" height="22" style="color:var(--accent)"><use href="#ico-check"/></svg>
      </div>
      <div>
        <div class="title">
          <svg class="ico" width="24" height="24" style="color:var(--accent)"><use href="#ico-check"/></svg>
          {title}
        </div>
        <div class="subtitle">Action completed successfully.</div>
      </div>
    </header>
    <section class="card">
      {body_html}
    </section>
  </div>
</body>
</html>""")

# -------- Handlers --------
def normalize_profile(fields):
    name = fields.get("name", [""])[0].strip()
    gender = fields.get("gender", [""])[0].strip()
    hobby = fields.get("hobby", [""])[0].strip()
    raw_id = fields.get("id", [""])[0].strip()
    pid = safe_id(raw_id, name)
    return {"id": pid, "name": name, "gender": gender, "hobby": hobby}

def do_list(query):
    migrate_if_needed()
    try:
        profiles = load_per_file_profiles()
    except Exception as e:
        return respond_json({"profiles": [], "error": f"load_failed: {e}"}, "200 OK")

    q = (query.get("q", [""])[0] or "").lower()
    if q:
        profiles = [p for p in profiles if q in (p.get("name", "").lower())]

    debug = query.get("debug", ["0"])[0] == "1"
    payload = {"profiles": profiles[:9]}
    if debug:
        payload["debug"] = {
            "cwd": os.getcwd(),
            "script": __file__,
            "profiles_dir": PROFILES_DIR,
            "count": len(profiles),
            "env_query_string": os.environ.get("QUERY_STRING", ""),
            "env_request_uri": os.environ.get("REQUEST_URI", "")
        }
    respond_json(payload)

def do_save_or_update(fields):
    prof = normalize_profile(fields)
    if not prof["name"]:
        return respond_json({"ok": False, "error": "Name is required"}, "400 Bad Request")

    migrate_if_needed()

    # Save/overwrite per-file (upsert)
    try:
        write_profile_file(prof)
    except Exception as e:
        return respond_json({"ok": False, "error": f"save_failed: {e}"}, "500 Internal Server Error")

    # Response type
    force_view = (fields.get("_view", [""])[0] or "").lower() == "html"
    accept = os.environ.get("HTTP_ACCEPT", "").lower()
    wants_json = ("application/json" in accept) and ("text/html" not in accept) and not force_view

    if wants_json:
        return respond_json({"ok": True, "profile": prof})

    # HTML success card
    hobby_chip = ""
    if prof.get("hobby"):
        hobby_chip = f'<span class="chip"><svg class="ico" aria-hidden="true" style="color:#7a93b7"><use href="#ico-spark"/></svg>{html.escape(prof["hobby"])}</span>'

    body = f"""
      <div class="p-card">
        <div class="p-title">
          <svg class="ico" aria-hidden="true" style="color:var(--accent)"><use href="#ico-user"/></svg>
          {html.escape(prof['name'] or '(Unnamed)')}
        </div>
        <div class="p-meta">
          <svg class="ico" aria-hidden="true" style="color:#7a93b7"><use href="#ico-id"/></svg>
          {html.escape(prof.get('gender','—'))} • ID: {html.escape(prof['id'])}
        </div>
        <div class="row">
          {hobby_chip}
        </div>
        <div class="row" style="margin-top:18px">
          <a class="btn primary" href="/viewcard.html">
            <svg class="ico" aria-hidden="true"><use href="#ico-card"/></svg>
            View Cards
          </a>
          <a class="btn" href="/form.html">
            <svg class="ico" aria-hidden="true"><use href="#ico-plus"/></svg>
            Add Another
          </a>
          <a class="btn" href="/">
            <svg class="ico" aria-hidden="true"><use href="#ico-home"/></svg>
            Home
          </a>
        </div>
      </div>
    """
    return respond_html("Profile Saved", body)

# -------- Main --------
def main():
    method = os.environ.get("REQUEST_METHOD", "GET").upper()
    qs = parsed_query()

    if method == "GET":
        return do_list(qs)

    if method == "POST":
        fields = read_body()
        mode = (fields.get("_mode", ["save"])[0] or "save").lower()
        if mode in ("save", "update"):
            return do_save_or_update(fields)
        return respond_json({"ok": False, "error": "Unsupported mode"}, "400 Bad Request")

    respond_json({"ok": False, "error": "Method not allowed"}, "405 Method Not Allowed")

if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        respond_json({"ok": False, "error": str(e)}, "500 Internal Server Error")
