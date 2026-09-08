import json, sys

data = json.load(sys.stdin).get("data", {})
state = data.get("state", {})

print("=== /api/state 顶层 ===")
print("顶层键:", sorted(data.keys()))
print()

print("=== state 段 ===")
print("state 顶层键:", sorted(state.keys()))
print()

for section in ["capture", "detection", "fan_control", "system", "loopout", 
                "mouse_output", "license", "last_error", "updated_at",
                "crosshair", "core", "calibration", "aim"]:
    if section in state:
        print(f"=== {section} ===")
        print(json.dumps(state[section], indent=2, ensure_ascii=False)[:500])
        print()

print("=== 其他未列出的 state 键 ===")
for k in sorted(state.keys()):
    if k not in ["capture", "detection", "fan_control", "system", "loopout",
                 "mouse_output", "license", "last_error", "updated_at",
                 "crosshair", "core", "calibration", "aim"]:
        print(f"{k}: {json.dumps(state[k], ensure_ascii=False)[:300]}")
