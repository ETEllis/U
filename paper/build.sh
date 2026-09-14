#!/bin/sh
set -eu
paper_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$paper_dir"
tectonic --keep-logs --keep-intermediates main.tex
if command -v zip >/dev/null 2>&1; then
  zip -q -X arxiv-source.zip main.tex main.bbl references.bib build.sh README.md SOURCE_NOTES.md
fi
