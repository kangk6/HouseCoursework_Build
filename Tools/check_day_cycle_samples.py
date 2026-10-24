"""Summarize actual frame readbacks from the continuous-day acceptance run."""
import argparse
import json
from pathlib import Path


def summarize(report):
    samples = report['samples']
    probe = report.get('probe_only', False)
    assert len(samples) > (100 if probe else 1000), 'Insufficient continuous-day samples'
    assert all(s['luma'] >= 0 and s['window_luma'] >= 0 for s in samples)
    changes = []
    for a, b in zip(samples, samples[1:]):
        changes.append({
            'hour': b['hour'], 'day': b['day'],
            'real_seconds': round(b['elapsed'] - a['elapsed'], 5),
            'mean_luma_step': round(abs(b['luma'] - a['luma']), 5),
            'upper_view_luma_step': round(abs(b['window_luma'] - a['window_luma']), 5),
        })
    boundaries = {}
    for name, hour in [('sunrise', 6), ('sunset', 18), ('midnight', 0)]:
        nearby = [c for c in changes if abs((c['hour'] - hour + 12) % 24 - 12) < .2]
        if probe and len(nearby) < 10:
            continue
        assert len(nearby) >= 10, name + ': insufficient boundary samples'
        boundaries[name] = {
            'samples': len(nearby),
            'max_mean_luma_step': max(c['mean_luma_step'] for c in nearby),
            'max_upper_view_luma_step': max(c['upper_view_luma_step'] for c in nearby),
        }
    # Fixed camera and no panel changes. This flags gross flashes, not a claim
    # that a single luminance statistic proves the entire scene looks natural.
    passed = bool(boundaries) and all(b['max_mean_luma_step'] < 3 and b['max_upper_view_luma_step'] < 5
                 for b in boundaries.values())
    return {
        'samples': len(samples), 'boundaries': boundaries,
        'largest_steps': sorted(changes, key=lambda c: c['mean_luma_step'], reverse=True)[:8],
        'boundary_flash_check_passed': passed,
        'scope': 'Fixed study viewpoint, 0-255 luminance readback, accelerated natural day; inspect saved frames as well. Not an all-room or performance certification.',
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('report', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = summarize(json.loads(args.report.read_text(encoding='utf-8-sig')))
    text = json.dumps(result, ensure_ascii=False, indent=2) + '\n'
    if args.output:
        args.output.write_text(text, encoding='utf-8')
    print(text)
    raise SystemExit(0 if result['boundary_flash_check_passed'] else 1)
