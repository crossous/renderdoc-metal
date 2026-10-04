#!/usr/bin/python3
# SPDX-License-Identifier: MIT
import hashlib
import json
import platform
import sys
from pathlib import Path
root = Path(sys.argv[1])
files = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
         for folder in ['bin', 'lib', 'licenses', 'source']
         for p in sorted((root / folder).glob('**/*')) if p.is_file()}
(root / 'manifest.json').write_text(json.dumps({
    'schema': 1, 'architecture': platform.machine(),
    'metal2vulkan': '43c46ac8a24adf1a6e872b8a52c706ec9614fad0',
    'SPIRV-Cross': 'vulkan-sdk-1.4.357.0', 'SPIRV-Tools': 'vulkan-sdk-1.4.357.0',
    'files': files}, indent=2) + '\n')
