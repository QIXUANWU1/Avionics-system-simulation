#!/usr/bin/env python3
"""Stream a full binary trace to JSONL; no third-party dependencies."""
import argparse
import json
from run import TRACE,unpack,EVENT
NAMES=EVENT
def records(path):
    with open(path,'rb') as f:
        if f.read(8)!=b'IMATRCE1':raise ValueError('trace header')
        while True:
            b=f.read(TRACE.size)
            if not b:return
            if len(b)!=TRACE.size:raise ValueError('truncated record')
            code,ns,wire=TRACE.unpack(b)
            if code not in NAMES:raise ValueError('event code')
            decoded=unpack(wire, legacy_trace=True)
            if decoded['kind']!=code:raise ValueError('event code mismatch')
            yield dict(event=NAMES[code],host_ns=ns,**decoded)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('path');p.add_argument('--count-only',action='store_true');a=p.parse_args();n=0
    for e in records(a.path):
        n+=1
        if not a.count_only:print(json.dumps(e,separators=(',',':')))
    if a.count_only:print(n)
