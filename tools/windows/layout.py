"""Compile the approved model-to-endpoint allocation; no runtime service discovery."""
from pathlib import Path
import argparse, copy, csv, hashlib, json

HERE = Path(__file__).resolve().parents[2]
PROFILES = ('baseline', 'expanded')
INPUTS = ('config/deployment.json', 'config/expanded_domains.json', 'config/advisory.json', 'core/services.def')

def load_catalog():
    records=[]
    for line in (HERE / INPUTS[3]).read_text(encoding='utf-8').splitlines():
        if not line.startswith('S('): continue
        require(line.endswith(')'), 'invalid service line')
        key,name,units,group,period,age,count,*inputs=next(csv.reader([line[2:-1]],strict=True))
        n=int(count)
        require(0<=n<=6 and len(inputs)==max(n,1), 'service arity')
        records.append(dict(id=len(records),key=key,name=name,units=units,group=int(group),
                            period_ms=int(period),max_age_ms=int(age),inputs=[int(i) for i in inputs[:n]]))
    return records

def input_digest():
    h = hashlib.sha256()
    for name in INPUTS:
        h.update(name.encode() + b'\0')
        h.update(hashlib.sha256((HERE / name).read_bytes()).digest())
    return h.hexdigest()

def require(condition, message):
    if not condition:
        raise ValueError(message)

def load_profiles():
    base, splits, advisory = [json.loads((HERE / p).read_text(encoding='utf-8')) for p in INPUTS[:3]]
    catalog=load_catalog()
    service_count=len(catalog)
    require(0<service_count<=64 and len({s['key'] for s in catalog})==service_count, 'catalog size / unique keys')
    require(base['wire_version']==4, 'unsupported wire version')
    for s in catalog:
        require(s['period_ms'] > 0 and s['period_ms'] % 10 == 0, 'period')
        require(s['max_age_ms']>=s['period_ms'], 'valid age / period')
        require(len(set(s['inputs'])) == len(s['inputs']) and all(0 <= i < service_count and i != s['id'] for i in s['inputs']), 'input IDs')
    boards={b['id'] for b in base['boards']}
    require(len(boards)==len(base['boards'])==6 and {d['board'] for d in base['domains']}==boards,
            'six board roles with allocated domains')
    profiles = {'baseline': base, 'expanded': copy.deepcopy(base)}
    profiles['expanded']['domains'] = []
    base_names = {d['id'] for d in base['domains']}
    require(set(splits) <= base_names, 'unknown split source')
    for domain in base['domains']:
        split = splits.get(domain['id'], [])
        if split:
            assigned = sorted(s for d in split for s in d['services'])
            require(assigned == sorted(domain['services']), 'split must preserve source service inventory')
        for part in split or [domain]:
            d = {**domain, **part}
            d['index'] = len(profiles['expanded']['domains'])
            profiles['expanded']['domains'].append(d)
    for p, cfg in profiles.items():
        domains = cfg['domains']
        require(2 <= len(domains) <= 32 and domains[0]['id']=='sensor_io', 'input surrogate / domain count')
        require(len({d['id'] for d in domains}) == len(domains), 'domain IDs')
        require([d['index'] for d in domains] == list(range(len(domains))), 'contiguous domains')
        require(all(d['services'] for d in domains), 'empty domains')
        owner = [None] * service_count
        for d in domains:
            for sid in d['services']:
                require(0 <= sid < service_count and owner[sid] is None, 'duplicate / unknown owner')
                owner[sid] = d['index']
        require(None not in owner, 'missing owner')
        fanout = [set() for _ in owner]
        total = local = 0
        for s in catalog:
            for sid in s['inputs']:
                total += 1
                if owner[sid] == owner[s['id']]:
                    local += 1
                else:
                    fanout[sid].add(owner[s['id']])
        cfg.update(profile=p, service_owners=owner, logical_edges=total, local_edges=local, cross_edges=total-local,
                   core_fanout=[sorted(x) for x in fanout], advisory=None)
        if p == advisory['profile']:
            matches=[d['index'] for d in domains if d['id']==advisory['domain']]
            require(len(matches)==1, 'advisory destination')
            require(advisory['service_id']==service_count and advisory['period_ms']>0 and advisory['period_ms']%10==0,
                    'advisory ID/period')
            src=[e['id'] for e in advisory['inputs']]
            require(len(src)==2 and len(set(src))==2 and all(0<=e['id']<service_count and catalog[e['id']]['key']==e['key']
                    for e in advisory['inputs']), 'advisory input binding')
            require(all(owner[i]!=matches[0] for i in src),'advisory sources must be in a different domain')
            cfg['advisory']={**advisory,'domain_index':matches[0],'inputs':src,'logical_edges':len(src)}
            for i in src: fanout[i].add(matches[0])
        cfg['fanout']=[sorted(x) for x in fanout]
    require(sum(cfg['advisory'] is not None for cfg in profiles.values())==1,'unknown advisory profile')
    return profiles, catalog

def emit(out):
    profiles, catalog = load_profiles()
    n = len(PROFILES)
    a = next(c['advisory'] for c in profiles.values() if c['advisory'])
    def rows(values):
        return ',\n'.join('  {' + ','.join(map(str, row)) + '}' for row in values)
    out.mkdir(parents=True, exist_ok=True)
    source = f'''/* Generated by layout.py. Do not edit. */
#ifndef IMA_GENERATED_LAYOUT_H
#define IMA_GENERATED_LAYOUT_H
#include <stdint.h>
#define IMA_PROFILE_COUNT {n}u
#define IMA_WIRE_VERSION {profiles[PROFILES[0]]['wire_version']}u
#define IMA_CATALOG_COUNT {len(catalog)}u
#define IMA_ADVISORY_ID {a['service_id']}u
#define IMA_ADVISORY_PERIOD {a['period_ms']}u
#define IMA_ADVISORY_SOURCE_0 {a['inputs'][0]}u
#define IMA_ADVISORY_SOURCE_1 {a['inputs'][1]}u
static const int ima_advisory_domain[IMA_PROFILE_COUNT] = {{{','.join(str(profiles[p]['advisory']['domain_index']) if profiles[p]['advisory'] else '-1' for p in PROFILES)}}};
static const unsigned ima_domain_count[] = {{{','.join(str(len(profiles[p]['domains'])) for p in PROFILES)}}};
static const uint8_t ima_owner[IMA_PROFILE_COUNT][IMA_CATALOG_COUNT] = {{
{rows(profiles[p]['service_owners'] for p in PROFILES)}
}};
static const uint32_t ima_fanout[IMA_PROFILE_COUNT][IMA_CATALOG_COUNT] = {{
{rows([str(sum(1 << d for d in destinations))+'u' for destinations in profiles[p]['fanout']] for p in PROFILES)}
}};
static const uint32_t ima_period_ms[IMA_CATALOG_COUNT] = {{{','.join(str(s['period_ms'])+'u' for s in catalog)}}};
static const uint32_t ima_max_age_ms[IMA_CATALOG_COUNT] = {{{','.join(str(s['max_age_ms'])+'u' for s in catalog)}}};
static const uint64_t ima_inputs[IMA_CATALOG_COUNT] = {{{','.join(str(sum(1 << i for i in s['inputs']))+'ULL' for s in catalog)}}};
#endif
'''
    (out / 'generated_layout.h').write_bytes(source.encode('ascii'))
    files=(*INPUTS,'config/generated_layout.h')
    if out.resolve()==(HERE/'config').resolve():
        (out/'layout-inputs.sha256').write_bytes(('\n'.join(f"{hashlib.sha256((HERE / name).read_bytes()).hexdigest()}  {name}" for name in files)+'\n').encode('ascii'))
    return input_digest()

if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('out', type=Path)
    print(emit(p.parse_args().out))
