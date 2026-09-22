#!/usr/bin/env python3
"""Generate MkDocs compatibility tables from the existing YAML data."""

from pathlib import Path
from typing import Any

import yaml


ROOT = Path(__file__).resolve().parent
DATA = ROOT / "doc"
OUT = ROOT / "compatibility"


def cell(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, list):
        value = ", ".join(str(item) for item in value)
    return str(value).replace("|", "\\|").replace("\n", " ")


def link(label: Any, url: Any) -> str:
    text = cell(label)
    if isinstance(url, str) and url.startswith(("http://", "https://")):
        return f"[{text}]({url})"
    return text


def write_table(path: Path, title: str, headers: list[str], rows: list[list[str]]) -> None:
    lines = [f"# {title}", "", "| " + " | ".join(headers) + " |", "| " + " | ".join("---" for _ in headers) + " |"]
    lines.extend("| " + " | ".join(row) + " |" for row in rows)
    lines.extend(["", "### Status values", "", "- **AS**: Active Serial flash mode", "- **EF**: External Flash", "- **IF**: Internal Flash", "- **NA**: Not Available", "- **NT**: Not Tested", ""])
    path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    boards = yaml.safe_load((DATA / "boards.yml").read_text(encoding="utf-8"))
    board_rows = []
    for item in boards:
        constraints = item.get("Constraints")
        if isinstance(constraints, list):
            constraints = ", ".join(str(value) for value in constraints)
        board_rows.append([
            f"`{cell(item.get('ID'))}`",
            link(item.get("Description"), item.get("URL")),
            cell(item.get("FPGA")),
            cell(item.get("Memory")),
            cell(item.get("Flash")),
            cell(constraints),
        ])
    write_table(OUT / "board.md", "Boards", ["Board name", "Description", "FPGA", "Memory", "Flash", "Constraints"], board_rows)

    fpgas = yaml.safe_load((DATA / "FPGAs.yml").read_text(encoding="utf-8"))
    fpga_rows = []
    for vendor, items in fpgas.items():
        for item in items:
            fpga_rows.append([
                cell(vendor),
                link(item.get("Description"), item.get("URL")),
                cell(item.get("Model")),
                cell(item.get("Memory")),
                cell(item.get("Flash")),
            ])
    write_table(OUT / "fpga.md", "FPGAs", ["Vendor", "Description", "Model", "Memory", "Flash"], fpga_rows)

    cables = yaml.safe_load((DATA / "cable.yml").read_text(encoding="utf-8"))
    cable_rows = []
    for keyword, items in cables.items():
        for item in items:
            cable_rows.append([
                f"`{cell(keyword)}`",
                link(item.get("Name"), item.get("URL")),
                cell(item.get("Description")),
                cell(item.get("Note")),
            ])
    write_table(OUT / "cable.md", "Cables", ["Keyword", "Name", "Description", "Notes"], cable_rows)


if __name__ == "__main__":
    main()
