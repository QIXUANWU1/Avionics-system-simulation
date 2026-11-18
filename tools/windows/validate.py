"""Offline checks. Opens no application endpoints."""
from contextlib import redirect_stdout
from io import StringIO
import collections
import hashlib
import json
import struct
import sys
import unittest
from unittest.mock import patch

import layout
import run

class LabChecks(unittest.TestCase):
    def setUp(self):
        self.cfg,self.catalog=run.make_plan()

    def test_inventory_and_routes(self):
        profiles,catalog=layout.load_profiles()
        self.assertEqual(len(catalog),63)
        for profile,cfg in profiles.items():
            with self.subTest(profile=profile):
                n=len(catalog)
                inventory=collections.Counter(s for d in cfg['domains'] for s in d['services'])
                self.assertEqual(inventory,dict.fromkeys(range(n),1))
                expected=[{d['index'] for d in cfg['domains'] if any(s in catalog[i]['inputs'] for i in d['services'])}
                          -{cfg['service_owners'][s]} for s in range(n)]
                self.assertEqual([set(x) for x in cfg['core_fanout']],expected)
                self.assertEqual(cfg['logical_edges'],sum(len(s['inputs']) for s in catalog))
                if cfg['advisory']:
                    a=cfg['advisory']
                    self.assertEqual(a['inputs'],[4,3])
                    self.assertEqual(cfg['domains'][a['domain_index']]['id'],'analytics')
                    for s in a['inputs']:expected[s].add(a['domain_index'])
                self.assertEqual([set(x) for x in cfg['fanout']],expected)

    def test_target_board_placement(self):
        target={s['key']:s for s in self.cfg['target_services']}
        self.assertEqual({s['board'] for s in target.values()},set('ABCDE'))
        self.assertEqual({key for key,s in target.items() if s['runtime']=='tianmai3-native'},
                         {'imu_sample','airdata_sample','imu_backup'})
        self.assertEqual(target['control_effort']['runtime'],'tianmai3-container')
        self.assertEqual(target['route_indication']['runtime'],'linux-oci')
        self.assertEqual(target['plant_motion']['runtime'],'bench-fixture')

    def test_one_runtime_per_host(self):
        for p,count in [('baseline',9),('expanded',15)]:
            for mode,runtime in [('oci','linux-oci'),('process','linux-process')]:
                cfg,_=run.make_plan(p,mode,observe=True)
                self.assertEqual(cfg['counts'],{runtime:count})
                self.assertEqual(cfg['recorder']['runtime'],runtime)
                self.assertEqual('oci_template' in cfg['recorder'],mode=='oci')
        with self.assertRaises(KeyError):
            run.make_plan(mode='mixed')

    def test_observer_does_not_change_routes(self):
        active,_=run.make_plan(observe=True)
        self.assertEqual(active['fanout'],self.cfg['fanout'])
        self.assertEqual(active['service_owners'],self.cfg['service_owners'])
        self.assertEqual(active['recorder']['oci_template']['process']['args'][0],'/ima-record-mirror')

    def test_oci_shape(self):
        node=next(d for d in self.cfg['domains'] if d['id']=='weather_display')
        spec=node['oci_template']
        self.assertEqual(spec['process']['args'],['/ima-worker',*node['worker_args_template']])
        self.assertTrue(spec['root']['readonly'])
        self.assertEqual({n['type'] for n in spec['linux']['namespaces']},{'pid','mount','ipc','uts'})

    def test_static_plan_opens_no_socket_or_subprocess(self):
        buf=StringIO()
        with patch.object(sys,'argv',['run.py','--observe']),patch('socket.socket',side_effect=AssertionError('socket')),\
             patch('subprocess.Popen',side_effect=AssertionError('process')),redirect_stdout(buf):
            self.assertEqual(run.main(),0)
        self.assertEqual(json.loads(buf.getvalue())['counts'],self.cfg['counts'])

    def test_advisory_has_no_feedback(self):
        n=len(self.catalog)
        self.assertEqual(self.cfg['advisory']['service_id'],n)
        self.assertIsNone(run.make_plan('baseline')[0]['advisory'])
        self.assertNotIn(n,{p for node in self.catalog for p in node['inputs']})
        self.assertEqual(len(self.cfg['fanout']),n)

    def test_binary_rejects_old_or_invalid_wire(self):
        msg=run.WIRE.pack(b'IMAR',6,1,0,11,12,20,1,2000,1,2,3)
        self.assertEqual(run.unpack(msg)['v'],[1,2,3])
        for bad in (msg[:-1],msg[:4]+b'\x02'+msg[5:],msg[:32]+struct.pack('<d',float('nan'))+msg[40:]):
            with self.assertRaises(ValueError):run.unpack(bad)

    def test_manifests(self):
        for manifest in ('config/layout-inputs.sha256','third_party/genann/source-sha256.txt'):
            for line in (layout.HERE/manifest).read_text(encoding='ascii').splitlines():
                expected,name=line.split(maxsplit=1)
                self.assertEqual(hashlib.sha256((layout.HERE/name).read_bytes()).hexdigest(),expected)

if __name__=='__main__':
    unittest.main(verbosity=2)
