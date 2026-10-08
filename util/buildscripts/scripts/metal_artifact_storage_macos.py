#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Prepare an external test artifact directory while retaining its build-tree path."""
import argparse
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('build-macos-debug'))
    parser.add_argument('--archive-root', type=Path,
                        default=Path('/Volumes/CauseUseMac/RenderDocMetalArchives'))
    parser.add_argument('--name', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]{1,120}', args.name):
        parser.error('Use a single artifact directory name')
    build = args.build_dir.resolve()
    archive = args.archive_root.resolve()
    if not build.is_dir() or not archive.is_dir():
        parser.error('Build directory and mounted external archive must already exist')
    if archive == build or archive.is_relative_to(build):
        parser.error('Archive must be outside the build tree')
    link = build / args.name
    if link.is_symlink():
        target = link.resolve(strict=True)
        if not target.is_relative_to(archive):
            parser.error('Existing link belongs to another archive; preserve it explicitly')
    elif link.exists():
        parser.error('Existing local artifacts must be verified and migrated first')
    else:
        target = archive / 'active-batches' / build.parent.name / args.name
        target.mkdir(parents=True, exist_ok=False)
        link.symlink_to(target, target_is_directory=True)
    print(target)


if __name__ == '__main__':
    main()
