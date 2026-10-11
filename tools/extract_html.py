
#!/usr/bin/env python3
"""Extract PAGE_HTML from a C++ source file into index.html."""

import re
import sys
from pathlib import Path


def extract_html(input_file: str, output_file: str) -> None:
    source_path = Path(input_file)
    output_path = Path(output_file)

    if not source_path.is_file():
        raise SystemExit(f"ERROR: Source file not found: {source_path}")

    content = source_path.read_text(encoding="utf-8")

    # Locate the PAGE_HTML declaration and its raw C++ string.
    declaration = re.search(
        r"\bPAGE_HTML\b[^=]*=\s*R\"HTML\(",
        content,
    )

    if declaration is None:
        raise SystemExit(
            f"ERROR: PAGE_HTML raw string not found in {source_path}"
        )

    html_start = declaration.end()
    html_end = content.find(")HTML\"", html_start)

    if html_end == -1:
        raise SystemExit(
            f"ERROR: Closing )HTML\" delimiter not found in {source_path}"
        )

    html = content[html_start:html_end]

    if not html.strip():
        raise SystemExit("ERROR: Extracted HTML is empty")

    if not re.search(r"<!DOCTYPE\s+html|<html\b", html, re.IGNORECASE):
        raise SystemExit("ERROR: Extracted content does not look like HTML")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(html, encoding="utf-8", newline="\n")

    print(f"OK: Extracted HTML from {source_path}")
    print(f"Output: {output_path}")
    print(f"Characters: {len(html)}")
    print(f"Lines: {html.count(chr(10)) + 1}")


if __name__ == "__main__":
    source = (
        sys.argv[1]
        if len(sys.argv) > 1
        else "lego_technic_esp32/web_page.cpp"
    )
    destination = sys.argv[2] if len(sys.argv) > 2 else "index.html"

    extract_html(source, destination)
