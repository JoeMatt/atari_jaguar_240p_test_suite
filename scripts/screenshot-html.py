#!/usr/bin/env python3
"""Render a self-contained HTML viewer for the screenshot gallery.

Reads ``screenshots/manifest.json`` (produced by
``scripts/screenshot-tour.py``) and writes ``screenshots/index.html``
that you can open directly in any browser -- no web server, no JS
framework, no external assets. Just a single file referencing the PNGs
that already live next to it.

Companion script to ``scripts/screenshot-readme.py`` (which embeds the
same manifest into ``README.md``). Either can be re-run independently;
they share the manifest as their only input.

Usage:
    scripts/screenshot-html.py
    scripts/screenshot-html.py --check        # exit 1 if file would change
    scripts/screenshot-html.py --manifest path --out path

Layout:
    * Sticky group nav along the top
    * One section per tour group, with a card grid of captures
    * Click any thumbnail to open the full-size PNG in a new tab
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from html import escape
from pathlib import Path

## Inline stylesheet -- embedded so the file works offline / from
## file:// URLs / dropped into a zip. Kept terse and dependency-free.
_CSS = """
:root {
  --bg: #0c0a14;
  --panel: #161224;
  --border: #2a2342;
  --text: #e8e4f5;
  --muted: #9089b3;
  --accent: #c43a3a;
  --accent-soft: #e57878;
}
* { box-sizing: border-box; }
html, body { margin: 0; padding: 0; background: var(--bg); color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif; }
header {
  position: sticky; top: 0; z-index: 10;
  padding: 1rem 1.5rem;
  background: rgba(12, 10, 20, 0.92);
  backdrop-filter: blur(8px);
  border-bottom: 1px solid var(--border);
}
header h1 { margin: 0 0 0.5rem; font-size: 1.15rem; font-weight: 600;
  color: var(--accent-soft); letter-spacing: 0.02em; }
header .meta { color: var(--muted); font-size: 0.8rem; margin-bottom: 0.75rem; }
nav { display: flex; flex-wrap: wrap; gap: 0.4rem; }
nav a {
  display: inline-block;
  padding: 0.3rem 0.7rem;
  border: 1px solid var(--border);
  border-radius: 999px;
  font-size: 0.8rem;
  color: var(--text);
  text-decoration: none;
  transition: border-color 120ms, color 120ms;
}
nav a:hover, nav a:focus { border-color: var(--accent); color: var(--accent-soft); }
main { max-width: 1280px; margin: 0 auto; padding: 1.5rem; }
section { margin-bottom: 2.5rem; scroll-margin-top: 8.5rem; }
section h2 { font-size: 1rem; text-transform: uppercase; letter-spacing: 0.1em;
  color: var(--accent-soft); margin: 0 0 0.25rem; }
section .count { color: var(--muted); font-size: 0.8rem; margin-bottom: 1rem; }
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 1rem;
}
figure {
  margin: 0;
  background: var(--panel);
  border: 1px solid var(--border);
  border-radius: 6px;
  overflow: hidden;
  display: flex;
  flex-direction: column;
}
figure a { display: block; line-height: 0; }
figure img {
  display: block;
  width: 100%;
  height: auto;
  image-rendering: pixelated;
  background: #000;
  transition: opacity 120ms;
}
figure a:hover img { opacity: 0.85; }
figcaption { padding: 0.6rem 0.75rem; font-size: 0.85rem; }
figcaption .label { color: var(--text); font-weight: 500; }
figcaption .path { color: var(--muted); font-size: 0.7rem; margin-top: 0.2rem;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
footer { padding: 1.5rem; text-align: center; color: var(--muted); font-size: 0.75rem; }
footer code { color: var(--accent-soft); }
"""


def _render(manifest: dict) -> str:
    """Build the full HTML document from a manifest dict."""
    shots = manifest.get("shots", [])
    by_group: dict[str, list[dict]] = defaultdict(list)
    titles: dict[str, str] = {}
    for shot in shots:
        by_group[shot["group"]].append(shot)
        titles.setdefault(shot["group"], shot.get("title", shot["group"]))

    ## Preserve manifest order for sections + nav so the viewer follows
    ## the tour's narrative (matches README gallery + tour script).
    ordered: list[str] = []
    for shot in shots:
        if shot["group"] not in ordered:
            ordered.append(shot["group"])

    core = escape(str(manifest.get("core", "?")))
    content = escape(str(manifest.get("content", "?")))
    total = len(shots)
    n_groups = len(ordered)

    nav_html = "\n      ".join(
        f'<a href="#{escape(g)}">{escape(titles[g])}</a>' for g in ordered
    )

    sections: list[str] = []
    for group in ordered:
        group_shots = by_group[group]
        cards: list[str] = []
        for shot in group_shots:
            note = escape(shot.get("note") or shot["label"].replace("-", " ").title())
            path = escape(shot["path"])
            label = escape(shot["label"])
            cards.append(
                f'<figure>\n'
                f'  <a href="{path}" target="_blank" rel="noopener">'
                f'<img src="{path}" alt="{note}" loading="lazy"></a>\n'
                f'  <figcaption>\n'
                f'    <div class="label">{note}</div>\n'
                f'    <div class="path">{label} &middot; {path}</div>\n'
                f'  </figcaption>\n'
                f'</figure>'
            )
        plural = "s" if len(group_shots) != 1 else ""
        sections.append(
            f'<section id="{escape(group)}">\n'
            f'  <h2>{escape(titles[group])}</h2>\n'
            f'  <div class="count">{len(group_shots)} capture{plural}</div>\n'
            f'  <div class="grid">\n    '
            + "\n    ".join(cards) + "\n  </div>\n"
            f'</section>'
        )

    body = "\n\n".join(sections) if sections else (
        '<section><div class="count">No screenshots yet. '
        'Run <code>make screenshots</code> to generate the gallery.</div></section>'
    )

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Atari Jaguar 240p Test Suite — Screenshot gallery</title>
<style>{_CSS}</style>
</head>
<body>
<header>
  <h1>Atari Jaguar 240p Test Suite — screenshot gallery</h1>
  <div class="meta">
    {total} capture{'s' if total != 1 else ''} across {n_groups} screen{'s' if n_groups != 1 else ''}
    &middot; core <code>{core}</code>
    &middot; content <code>{content}</code>
  </div>
  <nav>
      {nav_html}
  </nav>
</header>
<main>
{body}
</main>
<footer>
  Generated by <code>scripts/screenshot-html.py</code>.
  Re-run with <code>make screenshots-html</code> after a fresh tour.
</footer>
</body>
</html>
"""


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    p.add_argument("--manifest", type=Path,
                   default=Path("screenshots/manifest.json"))
    p.add_argument("--out", type=Path,
                   default=Path("screenshots/index.html"))
    p.add_argument("--check", action="store_true",
                   help="exit 1 if the index would change (CI-friendly)")
    args = p.parse_args(argv[1:])

    if not args.manifest.is_file():
        ## Mirror screenshot-readme.py's CI-friendly behaviour: in a
        ## fresh checkout we may not have the manifest, so don't fail
        ## the wider pipeline -- just no-op.
        print(f">> manifest missing ({args.manifest}); index unchanged.")
        return 0

    manifest = json.loads(args.manifest.read_text())
    new_html = _render(manifest)

    current = args.out.read_text() if args.out.is_file() else ""
    if new_html == current:
        print(">> index.html already up to date.")
        return 0

    if args.check:
        print("!! index.html would change -- run scripts/screenshot-html.py "
              "to regenerate.", file=sys.stderr)
        return 1

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(new_html)
    print(f">> wrote {args.out} ({len(manifest.get('shots', []))} shots)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
