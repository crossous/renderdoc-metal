#!/usr/bin/env python3
"""RenderDoc external Shader Processor: compiled MetalLib -> editable MetalAIRAsm.

Use as a custom tool with arguments:
  /absolute/path/metal_air_processor.py {input_file} {output_file} {entry_point}
This is an AIR disassembler, not an AIR-to-MSL decompiler.
"""
import re
import subprocess
import sys
from pathlib import Path

def disassemble(path, entry):
    run = subprocess.run(['/usr/bin/xcrun', 'metal-objdump', '--metallib', '--disassemble', str(path)],
                         capture_output=True, text=True, check=True, timeout=20)
    return extract_module(run.stdout, entry)


def extract_module(text, entry):
    starts = list(re.finditer(r'^source_filename =', text, re.M))
    for i, start in enumerate(starts):
        end = starts[i+1].start() if i+1 < len(starts) else len(text)
        module = text[start.start():end]
        module = re.split(r'^0x[0-9a-fA-F]+.*--.*:$', module, maxsplit=1, flags=re.M)[0]
        if re.search(r'@(?:' + re.escape(entry) + '|' + re.escape('"'+entry+'"') + r')\(', module):
            return module
    raise ValueError('The metallib contains no AIR module defining entry '+entry)

if __name__ == '__main__':
    if len(sys.argv) != 4:
        sys.exit('Usage: metal_air_processor.py input.metallib output.ll entry_point')
    try:
        text = disassemble(Path(sys.argv[1]), sys.argv[3])
        Path(sys.argv[2]).write_text(text)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError): print(error.stderr, file=sys.stderr)
        sys.exit(1)
