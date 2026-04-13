import json
import os
import subprocess

config_path = 'ConfigureFile/mcconfig/controller_config.json'
memspec_path = 'ConfigureFile/memspec/16Gb_DDR5_6400_B_x8_3DS_2H.json'

def update_config(updates):
    # Always start clean from initial_config so previous tests don't leak over
    data = json.loads(json.dumps(initial_config))
    for k, v in updates.items():
        data['RefreshConfig'][k] = v
    with open(config_path, 'w') as f:
        json.dump(data, f, indent=4)

def run_test(log_name, mode, updates):
    print(f'Running {log_name} with mode {mode}...')
    update_config(updates)
    
    env = os.environ.copy()
    env['TEST_MODE'] = mode
    os.makedirs('DMU/build', exist_ok=True)
    with open(f'logs/{log_name}', 'w') as log_file:
        subprocess.run(['../../build/DMU/dmu_refresh_test'], cwd='DMU/build', env=env, stdout=log_file, stderr=subprocess.STDOUT)

# === 动态缩小物理参数以加速仿真 ===
with open(memspec_path, 'r') as f:
    initial_memspec = json.load(f)

test_memspec = json.loads(json.dumps(initial_memspec))
test_memspec['RefreshAcTiming']['tREFI1'] = 40.0
test_memspec['RefreshAcTiming']['tREFI2'] = 20.0
with open(memspec_path, 'w') as f:
    json.dump(test_memspec, f, indent=4)

# Only compile ONCE here at the beginning!
print('Compiling dmu_refresh_test executable...')
os.makedirs('logs', exist_ok=True)
os.makedirs('build', exist_ok=True)
subprocess.run(['cmake', '..'], cwd='build', stdout=subprocess.DEVNULL)
subprocess.run(['make', 'dmu_refresh_test', '-j4'], cwd='build', stdout=subprocess.DEVNULL)
print('Compilation complete. Running tests...')

with open(config_path, 'r') as f:
    initial_config = json.load(f)

run_test('stagger_log.txt', 'STAGGER', {'REFAB_ENABLE': True, 'REF_STAGGER_ENABLE': True, 'REFRESH_POSTPONE_ENABLE': False})
run_test('critical_log.txt', 'CRITICAL', {'REFAB_ENABLE': True, 'REFRESH_POSTPONE_ENABLE': True, 'REF_STAGGER_ENABLE': True})
run_test('burst_log.txt', 'BURST', {'REFAB_ENABLE': True})
run_test('refsb_log.txt', 'REFSB', {'REFAB_ENABLE': False})
run_test('rfm_log.txt', 'RFM', {'RAA_THRESHOLD': 1})
run_test('rfm_16_log.txt', 'RFM', {'RAA_THRESHOLD': 16, 'REFRESH_ENABLE': False})
run_test('rfm_32_log.txt', 'RFM', {'RAA_THRESHOLD': 32, 'REFRESH_ENABLE': False})

# === 基础刷新回归验证 ===
# BASELINE: 零流量，验证纯刷新时序（tREFI 周期、Stagger 初始化、Pending 计数、REFab 定期发出）
run_test('baseline_log.txt', 'BASELINE', {'REFAB_ENABLE': True, 'REF_STAGGER_ENABLE': True, 'REFRESH_POSTPONE_ENABLE': True})
# INTEGRATION: 全特性开启 + 混合读写流量，验证所有刷新功能在业务负载下的协同工作
run_test('integration_log.txt', 'INTEGRATION', {'REFAB_ENABLE': True, 'REF_STAGGER_ENABLE': True, 'REFRESH_POSTPONE_ENABLE': True, 'RAA_THRESHOLD': 1})
# RFMsb: REFsb 模式下的 RFM 应为 RFMsb 而非 RFMab
run_test('rfmsb_log.txt', 'RFM', {'REFAB_ENABLE': False, 'RAA_THRESHOLD': 1})
# FGR 2x: memspec 已配置 RefreshMode=1 (FGR)，验证 tREFI 减半后刷新频率翻倍
run_test('fgr_log.txt', 'STAGGER', {'REFAB_ENABLE': True, 'REF_STAGGER_ENABLE': True, 'REFRESH_POSTPONE_ENABLE': False})

# === 测试结束，恢复初始配置 ===
with open(config_path, 'w') as f:
    json.dump(initial_config, f, indent=4)
# 恢复底层物理 Specs
with open(memspec_path, 'w') as f:
    json.dump(initial_memspec, f, indent=4)

print('All extreme refresh logs generated successfully using dmu_refresh_test!')
