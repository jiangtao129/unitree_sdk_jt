#!/usr/bin/env python3
"""
Convert doc/keyDemo3_operation_manual.md to an A4 PDF via
markdown -> HTML (with CJK-friendly CSS) -> Chrome headless.

Usage:
    python3 doc/md_to_pdf.py               # default in/out under doc/
    python3 doc/md_to_pdf.py input.md out.pdf
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import markdown


CSS = r"""
@page {
    size: A4;
    margin: 16mm 14mm 16mm 14mm;
}

html, body {
    font-family: "Noto Sans CJK SC", "Noto Sans CJK",
                 "WenQuanYi Zen Hei", "SimSun", sans-serif;
    font-size: 10.5pt;
    line-height: 1.55;
    color: #1f2328;
    background: #ffffff;
    margin: 0;
    padding: 0;
}

h1, h2, h3, h4 {
    font-weight: 700;
    line-height: 1.25;
    margin-top: 1.2em;
    margin-bottom: 0.55em;
    page-break-after: avoid;
}
h1 { font-size: 20pt; border-bottom: 2px solid #d0d7de; padding-bottom: 0.25em; }
h2 { font-size: 15pt; border-bottom: 1px solid #d0d7de; padding-bottom: 0.2em; }
h3 { font-size: 12.5pt; }
h4 { font-size: 11pt; }

p { margin: 0.4em 0 0.6em; }

ul, ol { padding-left: 1.4em; margin: 0.4em 0 0.6em; }
li { margin: 0.15em 0; }

blockquote {
    margin: 0.6em 0;
    padding: 0.4em 0.9em;
    border-left: 4px solid #d0d7de;
    color: #57606a;
    background: #f6f8fa;
}

code {
    font-family: "JetBrains Mono", "Source Code Pro", "Consolas",
                 "DejaVu Sans Mono", monospace;
    font-size: 9.5pt;
    background: #eff1f4;
    padding: 0.08em 0.35em;
    border-radius: 3px;
}

pre {
    font-family: "JetBrains Mono", "Source Code Pro", "Consolas",
                 "DejaVu Sans Mono", monospace;
    font-size: 9pt;
    background: #1f2328;
    color: #e5e7eb;
    padding: 10px 12px;
    border-radius: 6px;
    overflow-x: auto;
    page-break-inside: avoid;
    margin: 0.6em 0;
}
pre code {
    background: transparent;
    color: inherit;
    padding: 0;
    border-radius: 0;
    font-size: inherit;
}

table {
    border-collapse: collapse;
    margin: 0.6em 0;
    width: 100%;
    page-break-inside: avoid;
    font-size: 10pt;
}
th, td {
    border: 1px solid #d0d7de;
    padding: 5px 9px;
    text-align: left;
    vertical-align: top;
}
th {
    background: #f6f8fa;
    font-weight: 700;
}
tr:nth-child(even) td {
    background: #fbfcfd;
}

hr {
    border: 0;
    border-top: 1px solid #d0d7de;
    margin: 1.2em 0;
}

em { color: #0969da; font-style: italic; }
strong { color: #24292f; }

/* avoid breaking list/table bullets across pages if possible */
li, tr { page-break-inside: avoid; }
"""


def build_html(md_text: str) -> str:
    body = markdown.markdown(
        md_text,
        extensions=["fenced_code", "tables", "sane_lists", "attr_list"],
    )
    return f"""<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>keyDemo3 Operation Manual</title>
<style>{CSS}</style>
</head>
<body>
{body}
</body>
</html>
"""


def md_to_pdf(md_path: Path, pdf_path: Path) -> None:
    md_text = md_path.read_text(encoding="utf-8")
    html = build_html(md_text)
    html_path = pdf_path.with_suffix(".html")
    html_path.write_text(html, encoding="utf-8")

    # Chrome headless print-to-pdf. A unique user-data-dir avoids clashing
    # with any running Chrome session on the same machine.
    user_data = f"/tmp/chrome-md2pdf-{os.getpid()}"
    os.makedirs(user_data, exist_ok=True)
    cmd = [
        "google-chrome",
        "--headless=new",
        "--disable-gpu",
        "--no-sandbox",
        "--hide-scrollbars",
        "--no-pdf-header-footer",
        f"--user-data-dir={user_data}",
        f"--print-to-pdf={pdf_path}",
        f"file://{html_path.resolve()}",
    ]
    subprocess.run(cmd, check=True)
    print(f"wrote {pdf_path}")


def main() -> int:
    here = Path(__file__).resolve().parent
    md = here / "keyDemo3_operation_manual.md"
    pdf = here / "keyDemo3_operation_manual.pdf"
    if len(sys.argv) >= 3:
        md = Path(sys.argv[1])
        pdf = Path(sys.argv[2])
    md_to_pdf(md, pdf)
    return 0


if __name__ == "__main__":
    sys.exit(main())
