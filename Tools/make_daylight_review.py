from pathlib import Path
import json
from PIL import Image,ImageDraw,ImageFont,ImageStat,ImageChops
root=Path(__file__).resolve().parents[1]
evidence=root/'Docs/Daylight/Final'
font=ImageFont.truetype(str(root/'Content/Slate/Fonts/LXGWWenKai-Light.ttf'),20)
canvas=Image.new('RGB',(1280,800),(13,17,21));d=ImageDraw.Draw(canvas)
labels=[('morning','上午 08:00'),('noon','正午 12:00'),('evening','傍晚 17:00'),('night','夜间 21:00')]
values={}
for i,(name,label) in enumerate(labels):
 im=Image.open(evidence/f'720_Daylight_{name}.png').convert('RGB')
 x=(i%2)*640;y=(i//2)*400
 canvas.paste(im.resize((640,360)),(x,y));d.text((x+18,y+368),label,font=font,fill=(235,226,204))
 values[name]={'floor_crop_mean_rgb':ImageStat.Stat(im.crop((380,340,820,560))).mean}
canvas.save(evidence/'daylight_comparison.png')
(evidence/'raster_observations.json').write_text(json.dumps({'scope':'Rendered screenshot diagnostics only; not illuminance measurement or geographic daylight validation','regions':values},ensure_ascii=False,indent=2),encoding='utf-8')
