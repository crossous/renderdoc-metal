#!/usr/bin/env python3
"""Install the macOS editor capture button into a UE project without touching Engine."""

import argparse
import json
from pathlib import Path
import shutil
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path, help="Path to the .uproject file")
    args = parser.parse_args()

    project = args.project.expanduser().resolve()
    if not project.is_file() or project.suffix != ".uproject":
        parser.error(f"not a .uproject file: {project}")

    repo = Path(__file__).resolve().parents[2]
    template = Path(__file__).resolve().parent / "RenderDocMetalCapture"
    destination = project.parent / "Plugins" / template.name
    header = repo / "renderdoc" / "api" / "app" / "renderdoc_app.h"
    if destination.exists():
        parser.error(f"plugin directory already exists; review it before updating: {destination}")

    descriptor = json.loads(project.read_text(encoding="utf-8"))
    plugins = descriptor.setdefault("Plugins", [])
    if any(p.get("Name") == template.name for p in plugins):
        parser.error(f"{template.name} is already listed in {project}")

    shutil.copytree(template, destination)
    vendor = destination / "Source" / template.name / "ThirdParty" / "RenderDoc"
    vendor.mkdir(parents=True, exist_ok=True)
    shutil.copy2(header, vendor / header.name)

    plugins.append({"Name": template.name, "Enabled": True})
    project.write_text(json.dumps(descriptor, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Installed {destination}")
    print(f"Enabled {template.name} in {project}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
