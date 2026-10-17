"""Check every window's results and rendered evidence, including legacy shot alias."""
import json
from pathlib import Path
import sys


def verify(root, heights=(720, 1080)):
    runs = []
    for height in heights:
        report = json.loads((root / f"ventilation_{height}.json").read_text(encoding="utf-8-sig"))
        checks = report["checks"]
        if any(item.get("passed") is not True for item in checks):
            raise ValueError(f"{height}: failed ventilation check")
        cases = {item["case"] for item in checks}
        images = []
        for index in range(16):
            prefix = f"window_{index:02d}_"
            for suffix in ("closed_collision_and_mesh", "aim_closed_aperture",
                           "e_opens_without_disappearing", "open_aperture_can_be_targeted",
                           "e_closes_and_restores_collision"):
                if prefix + suffix not in cases:
                    raise ValueError(f"{height}: missing {prefix + suffix}")
            for state in ("closed", "open"):
                filename = f"{height}_Ventilation_window_{index:02d}_{state}.png"
                note = None
                if not (root / filename).exists() and index == 15 and state == "open":
                    # v0.16.4 schedules both requests at +0.25s in the SAME
                    # stationary window-15 step. UE keeps the latter name.
                    # Preserve that original name rather than invent a capture.
                    filename = f"{height}_Ventilation_curtain_A.png"
                    note = "Window-15 open pose; same-step screenshot request uses curtain_A name in v0.16.4"
                image = root / filename
                if not image.is_file() or image.stat().st_size < 1000:
                    raise ValueError(f"{height}: rendered evidence missing: {filename}")
                images.append(dict(window=index, state=state, file=filename, note=note))
        runs.append(dict(height=height, checks=len(checks), windows=16, images=images))
    result = dict(passed=True, method="Fresh result and image completeness; visual inspection is separate", runs=runs)
    (root / "window_evidence_index.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


if __name__ == "__main__":
    result = verify(Path(sys.argv[1]), [int(value) for value in sys.argv[2:]] or (720, 1080))
    print("Verified 16 window open/closed evidence pairs at each resolution; aliases recorded explicitly.")
