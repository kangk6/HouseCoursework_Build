"""Compare isolated rendered output, not just light/component state."""
import argparse, hashlib, json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

p=argparse.ArgumentParser()
p.add_argument('--root',default='D:/HouseCoursework/Docs/HallLightingFix/Final')
p.add_argument('--game',default='D:/HouseCoursework_TabFinal/Windows')
p.add_argument('--reviewed',action='store_true',help='Only after separately inspecting the generated images')
a=p.parse_args();root=Path(a.root);game=Path(a.game)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
names=['SM_Lamp_E2','SM_Lamp_E3','SM_Lamp_C1_2','SM_Lamp_E4']
rows=[];sheet=Image.new('RGB',(1536,1280),'#111111');draw=ImageDraw.Draw(sheet)
for row,name in enumerate(names):
    pixels={}; hashes={}
    for col,state in enumerate(['on','glow','off']):
        path=root/('d3d12_Fixture_Isolated_'+name+'_'+state+'.png')
        image=Image.open(path).convert('RGB');pixels[state]=np.asarray(image,dtype=float)@np.array([.2126,.7152,.0722]);hashes[state]=sha(path)
        image.thumbnail((512,288));sheet.paste(image,(col*512,row*320+25));draw.text((col*512+6,row*320+5),name+' / '+state,fill='white')
    h,w=pixels['on'].shape
    # Lower wall/floor band excludes the glowing shade; a glow-only fake must
    # fail this independent direct-illumination comparison.
    band=np.s_[int(h*.70):int(h*.87),int(w*.15):int(w*.85)]
    center=np.s_[int(h*.20):int(h*.70),int(w*.20):int(w*.80)]
    illumination=pixels['on'][band]-pixels['glow'][band]
    emission=pixels['glow'][center]-pixels['off'][center]
    lit_fraction=float((illumination>8).mean());glow_fraction=float((emission>8).mean())
    rows.append({'actor':name,'wall_floor_brightened_fraction':lit_fraction,'shade_bulb_brightened_fraction':glow_fraction,'image_sha256':hashes,'passed':lit_fraction>.02 and glow_fraction>.01})
sheet.save(root/'hall_isolated_review.png')
report={'passed':all(r['passed'] for r in rows),'visual_reviewed':a.reviewed,'renderer':'D3D12','height':1080,
        'method':'Same camera, other controlled fixtures/fills off. ON versus glow-only verifies direct illumination; glow-only versus OFF verifies emission. Static coverage/input checks are separate. Ambient night light remains.',
        'fixtures':rows,'candidate_executable_sha256':sha(game/'HouseCoursework/Binaries/Win64/HouseCoursework.exe'),
        'package_assets':[{'path':str(f.relative_to(game)),'sha256':sha(f)} for f in sorted((game/'HouseCoursework/Content/Paks').glob('*')) if f.is_file()]}
(root/'hall_visual_summary.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(rows,indent=2))
if not report['passed']:raise SystemExit('Rendered fixture illumination/emission failed')
