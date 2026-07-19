"""
search.py - search PDF text for keywords
Usage: python docs/search.py keyword1 keyword2 ...
Output: docs/reference/search_results.txt

Searches both PDFs, prints surrounding context for every hit.
Run with no args to dump all page text (useful for discovery).
"""

import sys
import os
import fitz

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REF_DIR    = os.path.join(SCRIPT_DIR, "..", "docs", "reference")

PDFS = {
    "DATASHEET": os.path.join(REF_DIR, "External-CAN-FD-Controller-with-SPI-Interface-DS20006027B.pdf"),
    "REFMANUAL": os.path.join(REF_DIR, "MCP25XXFD-CAN-FD-Controller-Module-Family-Reference-Manual-DS20005678E.pdf"),
}

OUTFILE = os.path.join(REF_DIR, "search_results.txt")
CONTEXT = 400  # chars either side of a hit


def extract_all():
    pages = {}
    for name, path in PDFS.items():
        doc = fitz.open(path)
        pages[name] = [doc[i].get_text() for i in range(doc.page_count)]
    return pages


def search(pages, keywords):
    results = []
    for name, page_list in pages.items():
        for pnum, text in enumerate(page_list, 1):
            for kw in keywords:
                idx = 0
                while True:
                    idx = text.find(kw, idx)
                    if idx == -1:
                        break
                    start = max(0, idx - CONTEXT)
                    end = min(len(text), idx + len(kw) + CONTEXT)
                    results.append((name, pnum, kw, text[start:end]))
                    idx += len(kw)
    return results


def dump_all(pages):
    results = []
    for name, page_list in pages.items():
        for pnum, text in enumerate(page_list, 1):
            results.append((name, pnum, "", text))
    return results


if __name__ == "__main__":
    keywords = sys.argv[1:]
    pages = extract_all()

    if keywords:
        results = search(pages, keywords)
    else:
        results = dump_all(pages)

    with open(OUTFILE, "w", encoding="utf-8", errors="replace") as f:
        if not results:
            f.write("No results found for: %s\n" % keywords)
        for name, pnum, kw, snippet in results:
            f.write("\n" + "="*60 + "\n")
            f.write("[%s] Page %d  keyword: %r\n" % (name, pnum, kw))
            f.write("="*60 + "\n")
            f.write(snippet)
            f.write("\n")

    print("Results: %d hits -> %s (%d bytes)" % (
        len(results), OUTFILE, os.path.getsize(OUTFILE)))
