import json,pathlib
root=pathlib.Path(__file__).resolve().parents[1]/'Docs/BaseAcceptance'
def read(n):return json.loads((root/n).read_text(encoding='utf-8-sig'))
full=read('FurnitureAcceptance_full_scan.json'); retest=read('FurnitureAcceptance_table_drawer_retest.json')
old=read('full_scan_build.json')['sha256']; new=read('table_retest_build.json')['sha256']
assert len(full)==131 and sum(r['passed'] for r in full)==129
assert len(retest)==2 and all(r['passed'] and r['cycles_completed']==2 for r in retest)
fixes={(r['actor'],r['component']):r for r in retest}
merged=[]
for row in full:
    key=(row['actor'],row['component']); r=dict(fixes.get(key,row))
    r['build_sha256']=new if key in fixes else old
    r['evidence_run']='table_drawer_retest' if key in fixes else 'full_scan'
    assert r['passed'] and r['cycles_completed']==2
    merged.append(r)
(root/'FurnitureAcceptance.json').write_text(json.dumps(merged,indent=2),encoding='utf8')
print('131 aggregated results; original full scan retained')
