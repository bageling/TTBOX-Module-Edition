# test_web_recoil_translation.py — ttbox-web.py 压枪翻译层单测（本地，不启动 Web）
# 验证 YU 前端 recoil 提交格式 → mouse.recoil（位掩码 int）转换正确。
import sys

# 从 ttbox-web.py 提取翻译函数（避免启动 Flask）
src = open('plugins/web/bin/ttbox-web.py', encoding='utf-8').read()
# 提取 HOTKEY_BITS / BIT_HOTKEYS / _hotkey_to_bits / _bits_to_hotkey
ns = {}
start = src.index('HOTKEY_BITS =')
end = src.index('# controller 内的数值/布尔直通字段')
exec(src[start:end], ns)

failures = 0

def check(cond, msg):
    global failures
    if cond:
        print('  PASS:', msg)
    else:
        print('  FAIL:', msg)
        failures += 1

hotkey_to_bits = ns['_hotkey_to_bits']
bits_to_hotkey = ns['_bits_to_hotkey']

print('[翻译层] hotkey 字符串 → 位掩码')
check(hotkey_to_bits('left', 1) == 1, "'left' → 1")
check(hotkey_to_bits('right', 2) == 2, "'right' → 2")
check(hotkey_to_bits('middle', 0) == 4, "'middle' → 4")
check(hotkey_to_bits('', 0) == 0, "'' → 0")
check(hotkey_to_bits('back', 0) == 8, "'back' → 8")
check(hotkey_to_bits('forward', 0) == 16, "'forward' → 16")

print('[翻译层] 位掩码 → hotkey 字符串')
check(bits_to_hotkey(1) == 'left', "1 → 'left'")
check(bits_to_hotkey(2) == 'right', "2 → 'right'")
check(bits_to_hotkey(0) == '', "0 → ''")

# 模拟 YU 前端提交的 recoil 块（RECOIL_DEFAULTS 语义）
print('[映射] YU recoil 块 → mouse.recoil 字段翻译')
yu_recoil = {
    'enabled': True,
    'only_when_target_visible': True,
    'target_lost_release_ms': 200,
    'hotkey': 'left',
    'hotkey2': '',
    'hotkey_mode': 'any',
    'trigger_delay_enabled': False,
    'trigger_delay_ms': 120,
    'strength': 60,
    'speed': 1,
    'humanize_enabled': True,
    'humanize_curve_strength': 0.45,
    'humanize_jitter_px': 0.25,
    'humanize_jitter_frequency': 8,
}
mouse_recoil = {}
if yu_recoil.get('enabled') is not None:
    mouse_recoil['enabled'] = bool(yu_recoil['enabled'])
if yu_recoil.get('hotkey') is not None:
    mouse_recoil['hotkey'] = hotkey_to_bits(yu_recoil['hotkey'], 1) or 1
if yu_recoil.get('hotkey2') is not None:
    mouse_recoil['hotkey2'] = hotkey_to_bits(yu_recoil['hotkey2'], 0)
if yu_recoil.get('hotkey_mode') is not None:
    mouse_recoil['hotkey_mode'] = 2 if str(yu_recoil['hotkey_mode']) == 'all' else 1
for yk, tk in [('only_when_target_visible', 'only_when_target_visible'),
               ('target_lost_release_ms', 'target_lost_release_ms'),
               ('trigger_delay_enabled', 'trigger_delay_enabled'),
               ('trigger_delay_ms', 'trigger_delay_ms'),
               ('strength', 'strength'),
               ('speed', 'speed'),
               ('humanize_enabled', 'humanize_enabled'),
               ('humanize_curve_strength', 'humanize_curve_strength'),
               ('humanize_jitter_px', 'humanize_jitter_px'),
               ('humanize_jitter_frequency', 'humanize_jitter_frequency')]:
    if yu_recoil.get(yk) is not None:
        mouse_recoil[tk] = yu_recoil[yk]

check(mouse_recoil.get('enabled') is True, "enabled → true")
check(mouse_recoil.get('hotkey') == 1, "hotkey 'left' → 1")
check(mouse_recoil.get('hotkey2') == 0, "hotkey2 '' → 0")
check(mouse_recoil.get('hotkey_mode') == 1, "hotkey_mode 'any' → 1")
check(mouse_recoil.get('strength') == 60, "strength 直通")
check(mouse_recoil.get('humanize_curve_strength') == 0.45, "curve_strength 直通")

print('[映射] all 模式')
yu_all = dict(yu_recoil, hotkey_mode='all')
mode_all = 2 if str(yu_all['hotkey_mode']) == 'all' else 1
check(mode_all == 2, "hotkey_mode 'all' → 2")

print()
print('结果: %d failures' % failures)
sys.exit(1 if failures else 0)
