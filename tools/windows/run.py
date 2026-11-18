#!/usr/bin/env python3
"""Windows-side offline planner and trace-codec support; no Linux runtime code."""
from pathlib import Path
import argparse, collections, json, math, struct, sys
from layout import PROFILES, load_profiles

HERE=Path(__file__).resolve().parents[2]
WIRE = struct.Struct('<4sBBHIIIIQ3d')
TRACE = struct.Struct('<BQ56s')
EVENT = {1:'publish',2:'progress',3:'consume',4:'accepted_receive',5:'advisory'}

def unpack(data):
    if len(data)!=WIRE.size: raise ValueError('length')
    magic,ver,kind,sid,epoch,seq,ms,flags,ns,a,b,c=WIRE.unpack(data)
    if magic!=b'IMAR' or ver!=2 or kind not in EVENT or not all(map(math.isfinite,(a,b,c))):
        raise ValueError('wire')
    return dict(kind=kind,sid=sid,epoch=epoch,seq=seq,model_ms=ms,flags=flags,source_ns=ns,v=[a,b,c])

def make_plan(profile='expanded',mode='mixed',base_port=31000,observe=False):
    profiles, catalog=load_profiles()
    cfg=profiles[profile]
    cfg.update(base_port=base_port,collector_port=base_port+200,observer_enabled=observe,
               run_mode=mode,transport='static-direct-loopback-sampling/2')
    for d in cfg['domains']:
        d['port']=base_port+100+d['index']
        d['runtime']=('linux-process' if mode=='process' or mode=='mixed' and d['id'] in {'sensor_io','input_status'}
                      else 'linux-oci')
        d['actual_host']='shared Linux host'
        d['worker_args_template']=[str(PROFILES.index(profile)), str(d['index']), str(base_port),
                                   '{SESSION_EPOCH}', '{LINUX_START_NS}', '{DURATION_MS}', '1' if observe else '0']
        if d['runtime']=='linux-oci':
            d['oci_template']=oci_config(d['id'],d['worker_args_template'],'{SESSION_EPOCH}',True)
        d['planned_isolation']='PID/mount/IPC/UTS; shared host network' if d['runtime']=='linux-oci' else 'ordinary Linux process'
    cfg['counts']=dict(collections.Counter(d['runtime'] for d in cfg['domains']))
    return cfg,catalog

def oci_config(domain,args,unique,limits):
    return dict(ociVersion='1.0.2',root=dict(path='rootfs',readonly=True),hostname='ima-'+domain.replace('_','-'),
        process=dict(terminal=False,user=dict(uid=1000,gid=1000),args=['/ima-worker',*args],env=['PATH=/'],cwd='/',
                     noNewPrivileges=True,capabilities={x:[] for x in ['bounding','effective','inheritable','permitted','ambient']},
                     rlimits=[dict(type='RLIMIT_NOFILE',hard=128,soft=128)]),
        linux=dict(namespaces=[dict(type=x) for x in ['pid','mount','ipc','uts']],
                   **(dict(cgroupsPath='civil-weather-lab/'+unique+'/'+domain,
                           resources=dict(memory=dict(limit=134217728),pids=dict(limit=64))) if limits else {})),
        mounts=[dict(destination='/proc',type='proc',source='proc',options=['nosuid','noexec','nodev'])])

def main():
    p=argparse.ArgumentParser(description='Default: print the laboratory plan only. No sockets, workers or containers are started.')
    p.add_argument('--profile',choices=PROFILES,default='expanded')
    p.add_argument('--mode',choices=['mixed','oci','process'],default='mixed')
    p.add_argument('--seconds',type=float,default=12)
    p.add_argument('--base-port',type=int,default=31000)
    p.add_argument('--observe',action='store_true')
    args=p.parse_args()
    if not 1<=args.seconds<=3600 or not 1024<=args.base_port<=65000:p.error('duration/port range')
    cfg,cat=make_plan(args.profile,args.mode,args.base_port,args.observe)
    cfg['planned_seconds']=args.seconds
    print(json.dumps(cfg,ensure_ascii=False,indent=2))
    return 0

if __name__=='__main__':
    sys.exit(main())
