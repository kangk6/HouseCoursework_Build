from PIL import Image,ImageDraw
from pathlib import Path
import sys
root=Path(sys.argv[1]);height=sys.argv[2] if len(sys.argv)>2 else '720'
for start in [0,10]:
    canvas=Image.new('RGB',(1920,1100),'#17191c');d=ImageDraw.Draw(canvas)
    for j,idx in enumerate(range(start,start+10)):
        for col,state in enumerate(['off','dim']):
            p=root/(height+'_Fixture_Inspection_%02d_%s.png'%(idx,state))
            if not p.exists():continue
            x=(j%2)*960+col*480;y=(j//2)*220
            src=Image.open(p).convert('RGB');w,h=src.size
            crop=src.crop((int(w*.25),int(h*.18),int(w*.75),int(h*.70)));crop.thumbnail((470,190))
            canvas.paste(crop,(x,y+25));d.text((x+8,y+5),'Fixture %02d / %s'%(idx,state),fill='white')
    canvas.save(root/('review_'+height+'_%02d.png'%start))
