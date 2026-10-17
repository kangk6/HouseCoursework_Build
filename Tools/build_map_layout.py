"""Create legible map walls from measured wall cross-sections, preserving door gaps."""
import json,math,pathlib,collections
root=pathlib.Path(__file__).resolve().parents[1]
sections=json.loads((root/'Tools/Data/map_sections.json').read_text(encoding='utf-8-sig'))
groups=collections.defaultdict(list)
for f,x1,y1,x2,y2 in sections:
    if abs(x1-x2)<.5 and abs(y1-y2)>=45: axis='x'; c=(x1+x2)/2; a,b=sorted((y1,y2))
    elif abs(y1-y2)<.5 and abs(x1-x2)>=45: axis='y'; c=(y1+y2)/2; a,b=sorted((x1,x2))
    else: continue
    groups[int(f),axis].append((c,a,b))
lines=[]
for (f,axis),items in sorted(groups.items()):
    clusters=[]
    for item in sorted(items):
        if not clusters or item[0]-clusters[-1][0][0]>22: clusters.append([item])
        else: clusters[-1].append(item)
    for cluster in clusters:
        # Equal weight per coordinate, rather than per triangulation edge.
        coords=sorted(set(round(c,1) for c,a,b in cluster)); c=(coords[0]+coords[-1])/2
        ranges=sorted((a,b) for _,a,b in cluster); a,b=ranges[0]
        merged=[]
        for lo,hi in ranges[1:]:
            if lo<=b+20: b=max(b,hi)
            else: merged.append((a,b)); a,b=lo,hi
        merged.append((a,b))
        for a,b in merged:
            if b-a<65:continue
            lines.append((f,c,a,c,b) if axis=='x' else (f,a,c,b,c))
def outer(line):
    _,x1,y1,x2,y2=line
    return (abs(x1-x2)<.5 and (abs(x1+2470)<25 or abs(x1+1630)<25)) or (abs(y1-y2)<.5 and (abs(y1-9030)<25 or abs(y1-10730)<25))
# Windows are boundaries on a navigation map. The two ground-floor door
# openings are calibrated from the saved movable door components.
lines=[line for line in lines if not outer(line)]
for f in (0,1):
    lines += [(f,-2470,9030,-2470,10730),(f,-1630,9030,-1630,10730)]
    if f==0:
        lines += [(f,-2470,9030,-2260,9030),(f,-2150,9030,-1630,9030),
                  (f,-2470,10730,-2095,10730),(f,-1995,10730,-1630,10730)]
    else: lines += [(f,-2470,9030,-1630,9030),(f,-2470,10730,-1630,10730)]
text='// Simplified from measured walls; windows closed, actual exterior door gaps retained.\nstatic const float MapWalls[][5] = {\n'
for line in lines:text+='    {'+','.join(f'{v:.2f}f' for v in line)+'},\n'
text+='};\n'
(root/'Source/HouseCoursework/MapWalls.inl').write_text(text,encoding='utf8')
(root/'Tools/Data/map_walls.json').write_text(json.dumps(lines,indent=2),encoding='utf8')
print(f'{len(sections)} measured cross-sections -> {len(lines)} simplified wall runs')
