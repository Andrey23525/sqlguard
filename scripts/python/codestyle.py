#!/usr/bin/env python3
"""Download the project CodeStyle Google Doc and print it as Markdown.

Usage:
    python scripts/python/codestyle.py
    python scripts/python/codestyle.py --output scripts/python/codestyle.md

The document is exported as plain text through the public Google Docs export
endpoint. No credentials are required for a document shared by link.
"""

from __future__ import annotations

import argparse
import html
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

DOCUMENT_ID = "1M56lPmmvdWBVK5md5DnycTVfYxkcyOSfa5AhJ1CHdT8"
EXPORT_URL = (
    f"https://docs.google.com/document/d/{DOCUMENT_ID}/export?format=txt"
)
USER_AGENT = "sqlguard-codestyle-fetcher/1.0"


def fetch_document(url: str, timeout: float = 30.0) -> str:
    """Fetch the exported document as UTF-8 text."""
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            raw = response.read()
            charset = response.headers.get_content_charset() or "utf-8"
        return raw.decode(charset, errors="replace")
    except (urllib.error.URLError, TimeoutError, OSError) as error:
        return fetch_with_curl(url, timeout, error)


def fetch_with_curl(
    url: str,
    timeout: float,
    original_error: BaseException,
) -> str:
    """Fallback downloader for hosts whose TLS chain Python rejects."""
    curl = shutil.which("curl")
    if curl is None:
        raise original_error
    command = [
        curl,
        "--location",
        "--silent",
        "--show-error",
        "--max-time",
        str(int(timeout)),
        "--user-agent",
        USER_AGENT,
        url,
    ]
    if sys.platform == "win32":
        command.insert(1, "--ssl-no-revoke")
    result = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "curl failed")
    return result.stdout


def to_markdown(document: str) -> str:
    """Normalise the exported text into readable Markdown."""
    text = document.lstrip("\ufeff")
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    text = html.unescape(text)
    text = re.sub(r"[ \t]+\n", "\n", text)
    text = re.sub(r"\n{4,}", "\n\n\n", text)
    text = text.strip()
    if not text:
        raise ValueError("Google Docs returned an empty document")
    return "# CodeStyle\n\n" + text + "\n"


def main(argv: list[str]) -> int:
    """Run the downloader and print or save the Markdown."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="also save the Markdown to this path",
    )
    parser.add_argument(
        "--url",
        default=EXPORT_URL,
        help="override the Google Docs export URL",
    )
    args = parser.parse_args(argv)

    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")

    try:
        document = fetch_document(args.url)
        markdown = to_markdown(document)
    except (urllib.error.URLError, TimeoutError, OSError, RuntimeError, ValueError) as error:
        print(f"codestyle: cannot load the document: {error}", file=sys.stderr)
        return 1

    print(markdown)

    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(markdown, encoding="utf-8")
        print(f"\nSaved to {args.output}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
