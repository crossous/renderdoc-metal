#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""CPU-only verification of all bound compute AIR; no replay/output acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET
import zipfile

from audit_ue_metal_ray_capture import analyse
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'shader_tools'))
from metal_air_processor import disassemble


def query_calls(module):
    # Require the native allocation/reset/advance lifecycle, not a type, declaration,
    # debug label or arbitrary substring. Inspect only the selected entry's module.
    calls=re.findall(r'\bcall\b[^\n]*@(air\.[A-Za-z0-9_.$]+)\(',module)
    required=('allocate_intersection_query.','reset_intersection_query.','next_intersection_query.')
    if not all(any(c.startswith('air.'+prefix) for c in calls) for prefix in required):return []
    return [c for c in calls if '_intersection_query' in c]


def verify(xml,output,inventory=None):
    xml=Path(xml);output=Path(output);output.mkdir(parents=True,exist_ok=True)
    inventory=inventory or analyse(xml)
    libraries={}
    for _event,node in ET.iterparse(xml,events=('end',)):
        if node.tag!='chunk':continue
        if node.get('name')=='MTLDevice::newLibraryWithData':
            fields={x.get('name'):x for x in node}
            libraries[int(fields['Library'].text)]=f"{int(fields['data'].text):06}"
        node.clear()
    selected={}
    for dispatch in inventory['compute_dispatches']:
        selected.setdefault((dispatch['library'],dispatch['kernel']),[]).append(dispatch)
    result=dict(status='CPU AIR INSPECTION ONLY',GPU_commands_submitted=0,replay_validated=False,
        ray_outputs_validated=False,xml=str(xml.resolve()),entries=[],bound_ray_query_dispatches=[],
        limitations=['AIR query calls with bound PSO and nonzero captured groups prove a ray-query workload, not that every invocation takes the query branch.',
            'Scope is all captured compute dispatches regardless of labels; other shader stages are not classified.',
            'Native completion, AS header/root resource relocation and output comparisons require separate verification.'])
    with zipfile.ZipFile(xml.with_suffix('')) as archive:
        for (library,kernel),dispatches in selected.items():
            item=dict(library=library,kernel=kernel,dispatches=dispatches)
            try:
                data=archive.read(libraries[library]);sha=hashlib.sha256(data).hexdigest()
                # Resource ID and hash prevent two libraries with the same exported
                # entry name from replacing one another's module evidence.
                stem=f'{library}-{sha[:12]}'
                binary=output/(stem+'.metallib');binary.write_bytes(data)
                module=disassemble(binary,kernel);air=output/(stem+'.ll');air.write_text(module)
                calls=query_calls(module)
                item.update(status='DISASSEMBLED',library_sha256=sha,AIR=str(air.resolve()),ray_query_calls=calls)
                if calls:
                    for dispatch in dispatches:
                        indirect=dispatch['captured_indirect'];groups=indirect['groups'] if indirect else dispatch['direct_groups']
                        if groups and len(groups)==3 and all(n>0 for n in groups):
                            result['bound_ray_query_dispatches'].append(dict(dispatch=dispatch,
                                library_sha256=sha,AIR=str(air.resolve()),ray_query_calls=calls,
                                groups_source='captured per-use indirect evidence' if indirect else 'direct dispatch'))
            except (KeyError,ValueError,OSError,RuntimeError) as error:item.update(status='UNVALIDATED',error=str(error))
            # A failed disassembly is evidence failure; never classify by a label.
            except Exception as error:item.update(status='UNVALIDATED',error=str(error))
            result['entries'].append(item)
            (output/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xml',type=Path);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();result=verify(args.xml,args.output)
    print('CPU AIR entries',len(result['entries']),'nonzero bound ray-query dispatches',len(result['bound_ray_query_dispatches']))
    return int(any(x['status']=='UNVALIDATED' for x in result['entries']))


if __name__=='__main__':sys.exit(main())
