"""Render the parity audit and its implementation plans as one HTML document."""

from html import escape
from pathlib import Path
from urllib.parse import unquote
from datetime import datetime, timezone
import argparse
import re


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
PLANS = sorted((DOCS / "implementation").glob("[0-9][0-9]-*.md"))
AUDIT = DOCS / "feature-parity.md"
INDEX = DOCS / "implementation" / "README.md"
OUTPUT = DOCS / "feature-parity.html"
GUIDES = [(DOCS / "status-effects.md", "status-effects", "Status Effects usage"),
          (DOCS / "chat-filters.md", "chat-filters", "Native chat filters"),
          (DOCS / "settings-organization.md", "settings-organization", "Settings organization"),
          (DOCS / "settings-audit.md", "settings-audit", "Settings inventory"),
          (DOCS / "native-mob-data-findings.md", "mob-data", "HeadsUp and native data"),
          (DOCS / "world-target-bar.md", "world-target", "World Target Bar"),
          (DOCS / "target-rings.md", "target-rings", "Target Rings"),
          (DOCS / "heal-prediction.md", "heal-prediction", "Heal bar prediction"),
          (DOCS / "presets.md", "presets", "Party presets"),
          (DOCS / "chat-input.md", "chat-input", "Chat input limit and counter"),
          (DOCS / "chat-windows.md", "chat-windows", "Reactive chat and shared stacking"),
          (DOCS / "config-window.md", "config-window", "Resizable Configuration")]
GUIDES.append((DOCS / "satchel.md", "satchel", "Satchel inventories and storage slips"))
GUIDES.append((DOCS / "actionbars.md", "actionbars", "Hotbars, macros and controller crossbar"))
GUIDES.append((DOCS / "foundations.md", "foundations", "Shared foundations"))
GUIDES.append((DOCS / "reference/module-foundations.md", "module-foundations", "Module foundations"))
EMBEDDED = {
    AUDIT.resolve(): "audit",
    INDEX.resolve(): "contract",
    **{path.resolve(): "p" + path.name[:2] for path in PLANS},
    **{path.resolve(): anchor for path, anchor, _ in GUIDES},
}


def slug(value):
    return re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")


def link_target(target, source):
    if re.match(r"^[a-z]+:", target, re.I):
        return target
    path, _, fragment = target.partition("#")
    resolved = (source.parent / unquote(path)).resolve() if path else source.resolve()
    if resolved in EMBEDDED:
        prefix = EMBEDDED[resolved]
        if prefix == "contract" and fragment in {"shared-implementation-contract", "dependency-notes"}:
            return "#" + prefix + "-" + fragment
        return "#" + prefix
    return resolved.as_uri() + ("#" + fragment if fragment else "")


def inline(value, source):
    tokens = []

    def token(html):
        tokens.append(html)
        return "\x00" + str(len(tokens) - 1) + "\x00"

    def code(match):
        return token("<code>" + escape(match.group(1)) + "</code>")

    value = re.sub(chr(96) + r"([^" + chr(96) + r"]+)" + chr(96), code, value)

    def link(match):
        target = link_target(match.group(2), source)
        extra = ' target="_blank" rel="noopener"' if not target.startswith("#") else ""
        return token('<a href="' + escape(target, quote=True) + '"' + extra + ">"
                     + escape(match.group(1)) + "</a>")

    value = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", link, value)
    value = escape(value)
    value = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", value)
    value = re.sub(
        r"\bP(\d{2})\b",
        lambda m: '<a class="plan-ref" href="#p' + m.group(1) + '">' + m.group(0) + "</a>",
        value,
    )
    return re.sub(r"\x00(\d+)\x00", lambda m: tokens[int(m.group(1))], value)


def markdown(text, source, prefix):
    """Render the headings, paragraphs and flat lists used by these documents."""
    output, paragraph, items = [], [], []
    list_tag = None

    def flush_paragraph():
        if paragraph:
            output.append("<p>" + inline(" ".join(paragraph), source) + "</p>")
            paragraph.clear()

    def flush_list():
        nonlocal list_tag
        if items:
            output.append("<" + list_tag + ">" + "".join(items) + "</" + list_tag + ">")
            items.clear()
        list_tag = None

    for line in text.splitlines():
        line = line.strip()
        if not line:
            flush_paragraph()
            flush_list()
            continue
        heading = re.match(r"^(#{1,6}) (.+)", line)
        if heading:
            flush_paragraph()
            flush_list()
            level, title = len(heading.group(1)), heading.group(2)
            if level > 1:
                tag = "h" + str(min(level + 1, 6))
                output.append("<" + tag + ' id="' + prefix + "-" + slug(title)
                              + '">' + inline(title, source) + "</" + tag + ">")
            continue
        item = re.match(r"^(- |\d+\. )(.+)", line)
        if item:
            flush_paragraph()
            next_tag = "ul" if item.group(1) == "- " else "ol"
            if list_tag and list_tag != next_tag:
                flush_list()
            list_tag = next_tag
            content = item.group(2)
            pending = content.startswith("[ ] ")
            complete = content.startswith("[x] ")
            if pending or complete:
                content = content[4:]
            items.append('<li' + (' class="criterion"' if pending or complete else "") + ">"
                         + ('<span class="check done" aria-label="Complete">✓</span>' if complete else
                            '<span class="check" aria-label="Pending"></span>' if pending else "")
                         + inline(content, source) + "</li>")
            continue
        flush_list()
        paragraph.append(line)
    flush_paragraph()
    flush_list()
    return "\n".join(output)


CSS = """
:root {
  color-scheme:dark;
  --bg:#090909;--surface:#121212;--surface2:#202020;--border:#383838;
  --text:#f5f5f5;--text-dim:#b3b3b3;--accent:#fff;--accent-dim:#252525;
  --completed:#75e59a;--completed-dim:#12251a;--pending:#7bbcff;--pending-dim:#142235;
  --font:'IBM Plex Sans','Segoe UI',sans-serif;
  --mono:'IBM Plex Mono',Consolas,monospace;
  --serif:'Source Serif 4',Palatino,'Book Antiqua',serif;
}
*{box-sizing:border-box}
html{scroll-behavior:smooth;scroll-padding-top:26px}
body{margin:0;background:var(--bg);color:var(--text);font-family:var(--font);font-size:15px;line-height:1.75;
  background-image:radial-gradient(var(--border) .65px,transparent .65px);background-size:18px 18px}
a{color:var(--accent);text-underline-offset:3px}
a:hover{text-decoration-thickness:2px}
button,input{font:inherit}
button{cursor:pointer}
:focus-visible{outline:2px solid var(--accent);outline-offset:4px}
.layout{display:grid;grid-template-columns:272px minmax(0,1fr);max-width:1570px;margin:auto;min-height:100vh}
.sidebar{position:sticky;top:0;height:100dvh;overflow-y:auto;padding:33px 22px 24px 28px;border-right:1px solid var(--border);
  background:var(--bg);scrollbar-width:thin}
.brand{font:600 21px var(--serif);letter-spacing:-.4px;margin:0 0 2px}
.edition{font:10px var(--mono);color:var(--text-dim);letter-spacing:1.5px;text-transform:uppercase}
.nav-main{margin:30px 0 26px;display:grid;gap:5px}
.sidebar a{text-decoration:none}
.nav-main a{font-size:13px;padding:7px 10px;border-radius:3px}
.nav-main a:hover,.sidebar a.active{background:var(--accent-dim);color:var(--accent)}
.nav-label{font:10px var(--mono);text-transform:uppercase;letter-spacing:1.5px;color:var(--text-dim);margin:22px 0 10px}
.plan-nav{display:grid;gap:2px}
.plan-nav a{display:grid;grid-template-columns:27px minmax(0,1fr);gap:6px;padding:6px 7px;line-height:1.4;font-size:12px;border-radius:3px;min-width:0}
.plan-nav a span{font:10px var(--mono);padding-top:2px;color:var(--text-dim)}
.plan-nav a.completed span,.completed{color:var(--completed)}
.plan-nav a.pending span,.pending{color:var(--pending)}
.plan-nav a:hover{background:var(--surface2)}
.nav-foot{border-top:1px solid var(--border);margin-top:26px;padding-top:16px;color:var(--text-dim);font-size:11px}
main{min-width:0;padding:57px clamp(24px,5vw,78px) 80px}
.eyebrow{font:11px var(--mono);letter-spacing:1.6px;text-transform:uppercase;color:var(--accent);margin:0 0 14px}
.hero h1{font:500 clamp(38px,4.8vw,66px)/1.09 var(--serif);letter-spacing:-2px;max-width:900px;margin:0 0 24px}
.hero h1 em{font-weight:400;color:var(--accent)}
.lead{max-width:740px;font-size:17px;color:var(--text-dim);margin:0 0 24px}
.hero-meta{font:11px/1.8 var(--mono);color:var(--text-dim);padding-top:18px;border-top:1px solid var(--border)}
.stats{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));border-block:1px solid var(--border);margin:33px 0 28px;background:var(--surface)}
.stat{padding:23px 24px 22px}
.stat+.stat{border-left:1px solid var(--border)}
.stat strong{font:500 43px/1 var(--serif);display:block;color:var(--accent);margin-bottom:9px}
.stat span{display:block;font-size:12px;line-height:1.5}
.stat small{display:block;font-size:11px;color:var(--text-dim);margin-top:4px}
.jump{display:inline-flex;gap:12px;align-items:center;text-decoration:none;font-size:13px;font-weight:600;border-bottom:1px solid var(--accent);padding-bottom:3px}
.status-legend{display:flex;gap:22px;flex-wrap:wrap;margin:20px 0;font-size:12px}
.status-legend span:before{content:'';display:inline-block;width:8px;height:8px;background:currentColor;border-radius:50%;margin-right:8px}
.stat.pending strong{color:var(--pending)}
.chapter{margin-top:70px;scroll-margin-top:26px}
.chapter-top{display:flex;align-items:baseline;gap:16px;border-bottom:2px solid var(--text);padding-bottom:13px;margin-bottom:22px}
.chapter-num{font:11px var(--mono);color:var(--accent)}
.chapter h2{font:500 31px/1.2 var(--serif);letter-spacing:-.6px;margin:0}
.chapter-note{color:var(--text-dim);font-size:13px;max-width:790px;margin:0 0 22px}
.prose{max-width:950px}
.prose h3{font:500 22px/1.3 var(--serif);margin:36px 0 13px;letter-spacing:-.15px}
.prose p{margin:12px 0}
.prose ul,.prose ol{padding-left:23px;margin:12px 0 20px}
.prose li{padding-left:4px;margin:9px 0;overflow-wrap:anywhere}
.prose li::marker{color:var(--text-dim)}
code{font:11px/1.65 var(--mono);background:var(--surface2);border-radius:3px;padding:2px 4px;overflow-wrap:anywhere}
.prose a{overflow-wrap:anywhere}
.prose>p:first-child{color:var(--text-dim)}
.audit-details{border:1px solid var(--border);border-radius:4px;padding:0 22px;background:var(--surface)}
.audit-details>summary{font-weight:500;padding:17px 0;cursor:pointer}
.audit-details .prose{padding-bottom:16px}
.tools{padding:18px;background:var(--surface);border:1px solid var(--border);border-radius:4px;margin:24px 0}
.search-label{font:10px var(--mono);letter-spacing:1px;text-transform:uppercase;display:block;color:var(--text-dim);margin-bottom:8px}
.search-row{display:flex;gap:10px}
.search-row input{min-width:0;flex:1;width:100%;padding:10px 13px;border:1px solid var(--border);border-radius:3px;background:var(--bg);color:var(--text)}
.search-row button,.tool-actions button{border:1px solid var(--border);padding:8px 12px;border-radius:3px;background:var(--surface);color:var(--text);font-size:12px}
.search-row button:hover,.tool-actions button:hover{border-color:var(--accent);color:var(--accent)}
.filters-row{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:12px;margin-top:14px}
.filters,.tool-actions{display:flex;flex-wrap:wrap;gap:7px}
.filter{border:0;background:var(--surface2);padding:6px 10px;border-radius:3px;color:var(--text-dim);font-size:12px}
.filter[aria-pressed=true]{background:var(--accent-dim);color:var(--accent);font-weight:600}
.result-count{font:11px var(--mono);color:var(--text-dim);margin:18px 0}
.plan-group-label{display:flex;gap:14px;align-items:baseline;margin:32px 0 12px}
.plan-group-label h3{font:500 23px var(--serif);margin:0}
.plan-group-label span{font:10px var(--mono);color:var(--text-dim)}
.plan{border-top:1px solid var(--border);background:var(--surface);scroll-margin-top:26px}
.plan:last-of-type{border-bottom:1px solid var(--border)}
.plan>summary{display:grid;grid-template-columns:42px minmax(0,1fr) auto 18px;gap:14px;padding:21px 19px;align-items:start;cursor:pointer;list-style:none}
.plan[data-state=completed]>summary{border-left:3px solid var(--completed)}
.plan[data-state=pending]>summary{border-left:3px solid var(--pending)}
.plan>summary::-webkit-details-marker{display:none}
.plan>summary:after{content:'+';font:20px/1.2 var(--mono);color:var(--text-dim)}
.plan[open]>summary:after{content:'−'}
.plan>summary:hover{background:var(--accent-dim)}
.plan[open]>summary{background:var(--accent-dim);border-bottom:1px solid var(--border)}
.plan-id{font:11px/1.8 var(--mono);color:var(--accent)}
.plan-title{font-weight:500;font-size:16px;line-height:1.45}
.plan-caption{display:block;font-size:11px;color:var(--text-dim);font-weight:400;margin-top:5px}
.badge{font:9px/1.5 var(--mono);text-transform:uppercase;letter-spacing:.4px;padding:4px 7px;background:var(--pending-dim);color:var(--pending);border-radius:2px;white-space:nowrap}
.badge.implemented{background:var(--completed-dim);color:var(--completed)}
.plan-body{padding:17px 29px 30px 75px}
.plan-body>p:first-child{font-size:12px}
.plan-source{font:11px var(--mono);margin-top:25px;padding-top:18px;border-top:1px solid var(--border)}
.criterion{list-style:none;position:relative}
.check{position:absolute;left:-20px;top:.5em;width:11px;height:11px;border:1px solid var(--pending)}
.check.done{border-color:var(--completed);color:var(--completed);font-size:10px;line-height:9px}
.criterion:has(.done){color:var(--completed)}
.criterion:not(:has(.done)){color:var(--pending)}
.plan-ref{font-family:var(--mono);font-size:.85em}
.empty{padding:30px;border:1px dashed var(--border);color:var(--text-dim)}
footer{margin-top:65px;padding-top:20px;border-top:1px solid var(--border);font-size:11px;color:var(--text-dim)}
[hidden]{display:none!important}
@keyframes enter{from{opacity:0;transform:translateY(7px)}to{opacity:1;transform:none}}
.hero{animation:enter .4s ease both}
@media(prefers-reduced-motion:reduce){html{scroll-behavior:auto}*{animation:none!important;transition:none!important}}
@media(min-width:1600px){main{padding-inline:85px}}
@media(max-width:1000px){
  html{scroll-padding-top:75px}.layout{display:block}.sidebar{height:auto;padding:12px 22px;position:sticky;z-index:20;border:0;border-bottom:1px solid var(--border);display:flex;align-items:center;gap:20px;overflow-x:auto}
  .brand{font-size:17px;white-space:nowrap}.edition,.nav-label,.plan-nav,.nav-foot{display:none}
  .nav-main{display:flex;margin:0;gap:2px;white-space:nowrap}.nav-main a{font-size:12px}
  main{padding:34px 24px 60px}.hero h1{max-width:700px}.chapter{scroll-margin-top:80px}
}
@media(max-width:600px){
  body{font-size:14px}.sidebar{padding:10px 15px;gap:12px}.nav-main a{padding:5px 7px;font-size:11px}
  main{padding-inline:18px}.hero h1{letter-spacing:-1px}.lead{font-size:15px}
  .stat{padding:18px 10px}.stat strong{font-size:34px}.stat span{font-size:11px}.stat small{font-size:10px}
  .chapter{margin-top:48px}.chapter h2{font-size:27px}.audit-details{padding-inline:15px}
  .plan>summary{grid-template-columns:30px minmax(0,1fr) 14px;padding:17px 12px;gap:10px}
  .plan>summary .badge{grid-column:2;justify-self:start}.plan>summary:after{grid-column:3;grid-row:1}
  .plan-body{padding:15px 17px 25px}.tools{padding:13px}.tool-actions button{padding:6px 9px}
}
@media print{
  :root{color-scheme:light;--bg:white;--surface:white;--surface2:#f2f2f2;--text:#111;--text-dim:#444;--border:#ccc;--accent:#111;--accent-dim:#eee;--completed:#167534;--completed-dim:#e9f6ed;--pending:#1854a0;--pending-dim:#edf4ff}
  body{background:white;font-size:10pt}.layout{display:block;max-width:none}.sidebar,.tools,.jump,.result-count,.plan-source{display:none}
  main{padding:0}.hero h1{font-size:34pt}.stats{margin-block:18px}.chapter{margin-top:25px}
  .plan{break-inside:auto}.plan>summary{break-after:avoid}.plan-body{padding:0 0 15px}.plan-body h3{break-after:avoid}
  .plan>summary:after{display:none}a{color:inherit;text-decoration:none}.prose{max-width:none}
}
"""

JS = """
(() => {
  const plans = [...document.querySelectorAll('details.plan')];
  const input = document.getElementById('plan-search');
  const filters = [...document.querySelectorAll('.filter')];
  let category = 'all';
  const applyFilter = () => {
    const terms = input.value.toLowerCase().trim().split(/\\s+/).filter(Boolean);
    let count = 0;
    plans.forEach(plan => {
      const match = (category === 'all' || plan.dataset.category === category) &&
        terms.every(term => plan.textContent.toLowerCase().includes(term));
      plan.hidden = !match;
      if (match) count++;
    });
    document.querySelectorAll('.plan-group').forEach(group => {
      group.hidden = ![...group.querySelectorAll('.plan')].some(plan => !plan.hidden);
    });
    document.getElementById('result-count').textContent = count + ' of ' + plans.length + ' plans';
    document.getElementById('empty').hidden = count > 0;
  };
  input.addEventListener('input', applyFilter);
  document.getElementById('clear-search').addEventListener('click', () => {
    input.value = ''; category = 'all';
    filters.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.category === 'all')));
    applyFilter(); input.focus();
  });
  filters.forEach(button => button.addEventListener('click', () => {
    category = button.dataset.category;
    filters.forEach(other => other.setAttribute('aria-pressed', String(button === other)));
    applyFilter();
  }));
  document.getElementById('expand').addEventListener('click', () => plans.forEach(plan => { if (!plan.hidden) plan.open = true; }));
  document.getElementById('collapse').addEventListener('click', () => plans.forEach(plan => plan.open = false));
  document.getElementById('print').addEventListener('click', () => window.print());
  const revealHash = () => {
    if (!location.hash) return;
    const target = document.getElementById(decodeURIComponent(location.hash.slice(1)));
    if (!target) return;
    const parent = target.closest('details');
    if (parent) {
      input.value = ''; category = 'all';
      filters.forEach(button => button.setAttribute('aria-pressed', String(button.dataset.category === 'all')));
      applyFilter(); parent.open = true;
    }
    requestAnimationFrame(() => target.scrollIntoView({block:'start'}));
  };
  document.addEventListener('click', event => {
    const anchor = event.target.closest('a[href^="#"]');
    if (anchor && anchor.hash === location.hash) revealHash();
  });
  window.addEventListener('hashchange', revealHash);
  const navLinks = [...document.querySelectorAll('.sidebar a')];
  const observer = new IntersectionObserver(entries => {
    entries.forEach(entry => {
      if (!entry.isIntersecting) return;
      navLinks.forEach(link => link.classList.toggle('active', link.hash === '#' + entry.target.id));
    });
  }, {rootMargin:'-10% 0px -70% 0px'});
  document.querySelectorAll('section.chapter, details.plan').forEach(section => observer.observe(section));
  let printState;
  window.addEventListener('beforeprint', () => {
    printState = [...document.querySelectorAll('details')].map(detail => [detail, detail.open, detail.hidden]);
    document.querySelectorAll('.plan-group').forEach(group => group.hidden = false);
    printState.forEach(([detail]) => { detail.open = true; detail.hidden = false; });
  });
  window.addEventListener('afterprint', () => {
    if (printState) printState.forEach(([detail, open, hidden]) => { detail.open = open; detail.hidden = hidden; });
    applyFilter();
  });
  applyFilter(); revealHash();
})();
"""


def render(export_diagram=False):
    if not PLANS:
        raise ValueError("No implementation records found.")
    nav, plan_html = [], []
    plan_titles = {}
    completed_count = 0
    completed_records = []
    is_new = lambda path: int(path.name[:2]) <= 12 or path.name[:2] == "28"
    for path in PLANS:
        content = path.read_text(encoding="utf-8")
        number = path.name[:2]
        title = content.splitlines()[0].split(": ", 1)[1]
        plan_titles[number] = title
        status = re.search(r"Status: \*\*([^*]+)\*\*", content).group(1)
        state = "completed" if status.startswith("Implemented") else "pending"
        completed_count += state == "completed"
        if state == "completed":
            completed_records.append("P" + number)
        priority_match = re.search(r"Priority: (.+)", content)
        priority = priority_match.group(1).rstrip(".") if priority_match else "Native data cleanup"
        category = "new" if is_new(path) else "existing"
        nav.append('<a class="' + state + '" href="#p' + number + '"><span>' + number + "</span>" + escape(title) + "</a>")
        plan_html.append(
            '<details class="plan" id="p' + number + '" data-category="' + category + '" data-state="' + state + '">'
            '<summary><span class="plan-id">P' + number + '</span><span class="plan-title">' + escape(title)
            + '<span class="plan-caption">' + escape(priority) + '</span></span><span class="badge '
            + ("missing" if status.startswith("Missing") else "implemented" if status.startswith("Implemented") else "partial") + '">' + escape(status) + "</span></summary>"
            '<div class="plan-body prose">' + markdown(content, path, "p" + number)
            + '<div class="plan-source"><a href="' + path.as_uri() + '" target="_blank" rel="noopener">Source Markdown ↗</a></div>'
            + "</div></details>"
        )

    audit_text = AUDIT.read_text(encoding="utf-8")
    updated_match = re.search(r"^Status updated ([A-Za-z]+ \d{1,2}, \d{4})\.", audit_text, re.M)
    if updated_match is None:
        raise ValueError("The feature audit must record its status update date.")
    updated = updated_match.group(1)
    latest = re.search(r"(?ms)^## Latest updates\n(.*?)(?=^## |\Z)", audit_text)
    if latest is None:
        raise ValueError("The feature audit must include a Latest updates section.")
    latest_html = markdown(latest.group(1).strip(), AUDIT, "updates")
    audit_html = markdown(audit_text[:latest.start()] + audit_text[latest.end():], AUDIT, "audit")
    contract_text = INDEX.read_text(encoding="utf-8").split("## Shared implementation contract", 1)[1]
    contract_html = markdown("## Shared implementation contract" + contract_text, INDEX, "contract")
    guide_html = "".join('<section class="chapter" id="' + anchor + '"><div class="chapter-top">'
                         '<span class="chapter-num">' + str(index + 3).zfill(2) + '</span><h2>' + escape(title)
                         + '</h2></div><article class="prose">' + markdown(path.read_text(encoding="utf-8"), path, anchor)
                         + '</article></section>' for index, (path, anchor, title) in enumerate(GUIDES))
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    guide_nav = "".join('<a href="#' + anchor + '">' + escape(title) + '</a>' for _, anchor, title in GUIDES)
    distribution = ROOT / "dist" / "xiuinative.dll"
    release_html = ('<p class="hero-meta"><span class="completed">Current distribution · ' + escape(version)
                    + '</span> &nbsp; / &nbsp; <a href="' + distribution.as_uri()
                    + '">xiuinative.dll · one standalone DLL</a></p>') if distribution.exists() else ""
    title = "XIUI — Feature tracker & implementation plans"
    output = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="XIUI feature audit, {{TOTAL}} implementation records, native status usage and HeadsUp research.">
<title>""" + title + """</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500&amp;family=IBM+Plex+Sans:wght@400;500;600&amp;family=Source+Serif+4:ital,wght@0,400;0,500;0,600;1,400&amp;display=swap" rel="stylesheet">
<style>""" + CSS + """</style>
</head>
<body>
<div class="layout">
<nav class="sidebar" aria-label="Document navigation">
<div><div class="brand">XIUI</div><div class="edition">Feature parity / 2026</div></div>
<div class="nav-main"><a href="#overview">Overview</a><a href="#updates">Latest updates</a><a href="#audit">Full audit</a><a href="#plans">Implementation records</a>""" + guide_nav + """<a href="#contract">Shared contract</a></div>
<div class="nav-label">01—12, 28 / New features</div><div class="plan-nav">""" + "".join(item for path, item in zip(PLANS, nav) if is_new(path)) + """</div>
<div class="nav-label">13—27 / Existing modules</div><div class="plan-nav">""" + "".join(item for path, item in zip(PLANS, nav) if not is_new(path)) + """</div>
<div class="nav-foot">Updated · {{UPDATED_DATE}}<br><span class="completed">{{COMPLETED}} completed records</span></div>
</nav>
<main>
<header class="hero" id="overview">
<p class="eyebrow">Lua XIUI → XIUI / Feature parity</p>
<h1>The features.<br><em>The gaps. The path forward.</em></h1>
<p class="lead">Latest completed changes, remaining features, implementation records and usage guides in one reference.</p>
<div class="hero-meta">UPDATED {{UPDATED_SHORT}} &nbsp; / &nbsp; LUA 1.8.4 → NATIVE {{VERSION}} + DEVELOPMENT CHANGES<br>{{COMPLETED_RECORDS}} implemented. Other features remain future work.</div>
""" + release_html + """
<div class="stats" aria-label="Audit totals">
<div class="stat pending"><strong>{{ABSENT_FAMILIES}}</strong><span>Absent feature families</span><small>{{ABSENT_NAMES}}</small></div>
<div class="stat pending"><strong>{{EXISTING_PENDING}}</strong><span>Remaining existing-module packages</span><small>Source comparison</small></div>
<div class="stat"><strong>{{TOTAL}}</strong><span>Implementation records</span><small><span class="completed">{{COMPLETED}} completed</span><span class="pending">{{PENDING}} future</span></small></div>
</div>
<div class="status-legend" aria-label="Completion colors"><span class="completed">Completed in the working tree</span><span class="pending">Not yet implemented / deferred</span></div>
<a class="jump" href="#plans">Browse the implementation plans <span aria-hidden="true">↓</span></a>
</header>
<section class="chapter" id="updates">
<div class="chapter-top"><span class="chapter-num completed">LATEST</span><h2>Latest completed changes</h2></div>
<p class="chapter-note">Current feature, Party, text and configuration improvements. The totals above count the {{TOTAL}} retained implementation records; individual fixes are tracked here and in the full audit.</p>
<article class="prose">""" + latest_html + """</article>
</section>
<section class="chapter" id="audit">
<div class="chapter-top"><span class="chapter-num">01</span><h2>The full comparison</h2></div>
<p class="chapter-note">The source audit with current implementation status, remaining gaps, Native coverage and deferred work.</p>
<details class="audit-details" open><summary>Read the feature-parity audit</summary><article class="prose">""" + audit_html + """</article></details>
</section>
<section class="chapter" id="plans">
<div class="chapter-top"><span class="chapter-num">02</span><h2>Implementation plans</h2></div>
<p class="chapter-note">Open a plan for its source evidence, dependencies, implementation steps, input behavior, completion criteria and focused validation. Search matches the complete plan text.</p>
<div class="tools">
<label class="search-label" for="plan-search">Find a plan</label>
<div class="search-row"><input id="plan-search" type="search" placeholder="Try pet, MP cost, controller, profile…" autocomplete="off"><button id="clear-search" type="button">Clear</button></div>
<div class="filters-row"><div class="filters" aria-label="Plan categories">
<button class="filter" data-category="all" aria-pressed="true" type="button">All {{TOTAL}}</button>
<button class="filter" data-category="new" aria-pressed="false" type="button">New features · {{NEW_COUNT}}</button>
<button class="filter" data-category="existing" aria-pressed="false" type="button">Existing modules · {{EXISTING_COUNT}}</button>
</div><div class="tool-actions"><button id="expand" type="button">Expand visible</button><button id="collapse" type="button">Collapse all</button><button id="print" type="button">Print / PDF</button></div></div>
</div>
<p class="result-count" id="result-count" role="status">{{TOTAL}} of {{TOTAL}} plans</p>
<div class="plan-group"><div class="plan-group-label"><h3>New feature systems</h3><span>P01—P12, P28</span></div>""" + "".join(item for path, item in zip(PLANS, plan_html) if is_new(path)) + """</div>
<div class="plan-group"><div class="plan-group-label"><h3>Complete the existing modules</h3><span>P13—P27</span></div>""" + "".join(item for path, item in zip(PLANS, plan_html) if not is_new(path)) + """</div>
<p class="empty" id="empty" hidden>No plans match this search. Clear the search or choose another category.</p>
</section>
""" + guide_html + """
<section class="chapter" id="contract">
<div class="chapter-top"><span class="chapter-num">{{CONTRACT_NUMBER}}</span><h2>Shared implementation contract</h2></div>
<p class="chapter-note">The common foundations and dependency notes for current and future work.</p>
<article class="prose">""" + contract_html + """</article>
</section>
<footer>XIUI · {{UPDATED_DATE}} · Audit, all {{TOTAL}} implementation records, Status Effects, Native Chat Filters and settings guides, the settings inventory, and native-data research.<br>Regenerate from the repository root: <code>python scripts/render_feature_parity.py</code>. All content, styles and interactions are in this file; web fonts are optional.</footer>
</main>
</div>
<script>""" + JS + """</script>
</body>
</html>
"""
    output = output.replace("{{VERSION}}", escape(version)).replace("{{CONTRACT_NUMBER}}", str(len(GUIDES) + 3).zfill(2))
    output = output.replace("{{UPDATED_DATE}}", escape(updated)).replace("{{UPDATED_SHORT}}", escape(updated.upper()))
    output = output.replace("{{COMPLETED}}", str(completed_count)).replace("{{PENDING}}", str(len(PLANS) - completed_count))
    completed_ids = {record[1:] for record in completed_records}
    existing = [path for path in PLANS if not is_new(path)]
    new = [path for path in PLANS if is_new(path)]
    families = [("01",), ("02", "03", "04"), ("05",), ("06",), ("07",), ("08", "09", "10"), ("11",), ("12",), ("28",)]
    absent = sum(any(number not in completed_ids for number in family) for family in families)
    output = output.replace("{{EXISTING_PENDING}}", str(sum(path.name[:2] not in completed_ids for path in existing)))
    output = output.replace("{{TOTAL}}", str(len(PLANS))).replace("{{NEW_COUNT}}", str(len(new))).replace("{{EXISTING_COUNT}}", str(len(existing)))
    output = output.replace("{{ABSENT_FAMILIES}}", str(absent))
    absent_names = "; ".join(plan_titles[path.name[:2]] for path in new if path.name[:2] not in completed_ids)
    output = output.replace("{{ABSENT_NAMES}}", escape(absent_names or "All audited feature families implemented"))
    output = output.replace("{{COMPLETED_RECORDS}}", ", ".join(completed_records))
    generated = datetime.now(timezone.utc).isoformat(timespec="seconds")
    sources = [AUDIT, INDEX, *PLANS, *(path for path, _, _ in GUIDES), ROOT / "VERSION"]
    provenance = ("<!-- Generated by scripts/render_feature_parity.py at " + generated
                  + ". Regenerate: ./scripts/update-docs.ps1\nInputs: "
                  + ", ".join(path.relative_to(ROOT).as_posix() for path in sources) + " -->\n")
    output = output.replace("<!doctype html>\n", "<!doctype html>\n" + provenance, 1)
    OUTPUT.write_text(output, encoding="utf-8")
    print(str(OUTPUT))
    if export_diagram:
        archive = Path.home() / ".agent" / "diagrams" / "xiui-feature-parity.html"
        archive.parent.mkdir(parents=True, exist_ok=True)
        archive.write_text(output, encoding="utf-8")
        print(str(archive))
    print(f"Included {len(PLANS)} implementation records, the audit, {len(GUIDES)} guides and shared contract.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--export-diagram", action="store_true", help="Also write the personal .agent/diagrams copy")
    render(parser.parse_args().export_diagram)
