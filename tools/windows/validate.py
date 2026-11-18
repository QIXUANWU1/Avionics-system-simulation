"""Offline checks; starts no applications or network endpoints."""
from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
from unittest.mock import patch
import collections
import hashlib
import json
import struct
import sys
import unittest

import layout
import run

ROOT=Path(__file__).resolve().parents[2]

class LabChecks(unittest.TestCase):
    def setUp(self):
        self.cfg,self.catalog=run.make_plan()

    def test_inventory_and_routes(self):
        profiles,catalog=layout.load_profiles()
        for profile,cfg in profiles.items():
            with self.subTest(profile=profile):
                inventory=collections.Counter(s for d in cfg['domains'] for s in d['services'])
                self.assertEqual(inventory,dict.fromkeys(range(10),1))
                self.assertEqual(sum(len(s['inputs']) for s in catalog),13)
                expected=[{d['index'] for d in cfg['domains'] if any(s in catalog[i]['inputs'] for i in d['services'])}
                          -{cfg['service_owners'][s]} for s in range(10)]
                self.assertEqual([set(x) for x in cfg['core_fanout']],expected)
                if cfg['advisory']:
                    a=cfg['advisory']
                    self.assertEqual(a['inputs'],[4,3])
                    self.assertEqual(cfg['domains'][a['domain_index']]['id'],'analytics')
                    for s in a['inputs']:expected[s].add(a['domain_index'])
                self.assertEqual([set(x) for x in cfg['fanout']],expected)

    def test_profiles(self):
        self.assertEqual(self.cfg['counts'],{'linux-oci':5,'linux-process':2})
        self.assertEqual(self.cfg['domains'][0]['runtime'],'linux-process')
        self.assertEqual(run.make_plan('baseline')[0]['counts'],{'linux-oci':3,'linux-process':2})
        self.assertEqual(run.make_plan('expanded','oci')[0]['counts'],{'linux-oci':7})
        self.assertEqual(run.make_plan('expanded','process')[0]['counts'],{'linux-process':7})

    def test_observer_does_not_change_routes(self):
        active,_=run.make_plan(observe=True)
        self.assertEqual(active['fanout'],self.cfg['fanout'])
        self.assertEqual(active['service_owners'],self.cfg['service_owners'])

    def test_oci_shape(self):
        node=next(d for d in self.cfg['domains'] if d['id']=='weather_display')
        spec=node['oci_template']
        self.assertEqual(spec['process']['args'],['/ima-worker',*node['worker_args_template']])
        self.assertEqual(spec['process']['args'][1:3],['1','3'])
        self.assertTrue(spec['root']['readonly'])
        self.assertEqual({n['type'] for n in spec['linux']['namespaces']},{'pid','mount','ipc','uts'})
        self.assertEqual(spec['linux']['resources']['memory']['limit'],128*1024*1024)

    def test_static_plan_opens_no_socket_or_subprocess(self):
        buf=StringIO()
        with patch.object(sys,'argv',['run.py','--observe']),patch('socket.socket',side_effect=AssertionError('socket')),\
             patch('subprocess.Popen',side_effect=AssertionError('process')),redirect_stdout(buf):
            self.assertEqual(run.main(),0)
        self.assertEqual(json.loads(buf.getvalue())['counts'],self.cfg['counts'])

    def test_model_is_output_only_to_mirror(self):
        advisory=self.cfg['advisory']
        self.assertEqual(advisory['service_id'],10)
        self.assertIsNone(run.make_plan('baseline')[0]['advisory'])
        self.assertNotIn(10,{p for node in self.catalog for p in node['inputs']})
        self.assertEqual(len(self.cfg['fanout']),10)

    def test_binary_rejects_unusable_wire(self):
        msg=run.WIRE.pack(b'IMAR',2,1,0,11,12,20,1,2000,1,2,3)
        self.assertEqual(run.unpack(msg)['v'],[1,2,3])
        for bad in (msg[:-1],msg[:4]+b'\x01'+msg[5:],msg[:32]+struct.pack('<d',float('nan'))+msg[40:]):
            with self.assertRaises(ValueError):run.unpack(bad)

    def test_manifests(self):
        for manifest in ('config/layout-inputs.sha256','third_party/genann/source-sha256.txt'):
            for line in (ROOT/manifest).read_text(encoding='ascii').splitlines():
                expected,name=line.split(maxsplit=1)
                self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),expected)

if __name__=='__main__':
    unittest.main(verbosity=2)
