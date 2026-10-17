import csv,json,pathlib,statistics,sys
root=pathlib.Path(sys.argv[1]);result=[]
for path in sorted(root.glob('*.csv')):
    with path.open(encoding='utf-8-sig',newline='') as f:
        rows=csv.reader(f);header=next(rows);index=header.index('FrameTime');data=[]
        for row in rows:
            try:
                if float(row[index])>0:data.append(row)
            except (ValueError,IndexError):pass
    if len(data)<500:raise RuntimeError('Not enough frames: '+str(path))
    sample=data[120:600]
    def mean(name):
        if name not in header:return None
        i=header.index(name)
        return round(statistics.mean(float(row[i]) for row in sample),4)
    frames=sorted(float(row[index]) for row in sample)
    exclusive={name:mean(name) for name in header if name.startswith('Exclusive/RenderThread/')}
    gpu={name:mean(name) for name in header if name.startswith('GPU/') and name!='GPU/Total'}
    result.append(dict(view=path.stem,resolution='1280x720',sample_frames=len(sample),warmup_frames=120,
        average_fps=round(1000/statistics.mean(frames),3),p95_ms=frames[int(len(frames)*.95)-1],max_ms=max(frames),
        counters={name:mean(name) for name in ['FrameTime','GameThreadTime','RenderThreadTime','RHIThreadTime','GPUTime','RHI/DrawCalls','DrawCall/ShadowDepths','LightCount/All','ShadowCacheUsageMB','GPUMem/UsedMB']},
        render_costs=dict(sorted(exclusive.items(),key=lambda x:x[1],reverse=True)[:8]),
        gpu_costs=dict(sorted(gpu.items(),key=lambda x:x[1],reverse=True)[:8])))
(root/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
for row in result:print(json.dumps(row))
