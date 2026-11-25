#!/usr/bin/env python3
"""Check the single STM32F103/GCC target without an ARM assembler/linker."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
cc = shlex.split(os.environ.get('HOST_CC', 'gcc'))
fixture_dir = tempfile.TemporaryDirectory(prefix='rfid-syntax-')
fixture = Path(fixture_dir.name)
(fixture/'DanhSach.csv').write_text('uid,student_id,student_name\n00112233,S1,TEST STUDENT\n')
subprocess.run(['python3','Tools/Provision/generate_list.py',str(fixture/'DanhSach.csv'),
                str(fixture/'embedded_list_data.h')],cwd=root,check=True)
def header_flags(cmd):
    return [('-I'+str(fixture)) if x=='-Ibuild-rtos/generated' else x
            for x in cmd if x.startswith(('-I','-D'))]

dry = subprocess.check_output(['make', '--no-print-directory', '-B', '-n', 'all'], cwd=root, text=True)
commands = [shlex.split(line) for line in dry.splitlines()
            if ' -c ' in line and line.startswith(('arm-none-eabi-gcc ', '/'))]
c_commands = [cmd for cmd in commands if any(x.endswith('.c') for x in cmd)]
sources = [next(x for x in cmd if x.endswith('.c')) for cmd in c_commands]
assert len(sources) == len(set(sources)) == 42
assert sum(Path(s).name == 'main.c' for s in sources) == 1
assert not any(s.startswith('Tests/') for s in sources)
assert 'build-rtos/RFIDAttendance.bin' in dry
for folder in ('App','Core','Modules','Drivers','Middlewares'):
    for p in (root/folder).rglob('*.c'):
        assert p.relative_to(root).as_posix() in sources, 'Unused C file: ' + str(p)
for cmd in c_commands:
    source = next(x for x in cmd if x.endswith('.c'))
    flags = header_flags(cmd)
    subprocess.run(cc + ['-fsyntax-only', '-std=gnu11', '-Wall', '-Wextra',
        '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast', *flags, source],
        cwd=root, check=True, capture_output=True, text=True)
    subprocess.run(cc + ['-MM', '-D__arm__', '-D__thumb__', '-D__ARM_ARCH_7M__', *flags, source],
        cwd=root, check=True, capture_output=True, text=True)
flags = header_flags(c_commands[0])
def preprocess(source):
    return subprocess.check_output(cc + flags + ['-E', '-P', source], cwd=root, text=True)
it = preprocess('Core/Src/stm32f1xx_it.c')
port = preprocess('Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c')
for name in ('SVC_Handler','PendSV_Handler'):
    pattern = r'void\s+' + name + r'\s*\(\s*void\s*\)\s*\{'
    assert not re.search(pattern, it), 'Generated handler conflicts with FreeRTOS: ' + name
    assert len(re.findall(pattern, port)) == 1, 'Missing port handler: ' + name
assert len(re.findall(r'void\s+SysTick_Handler\s*\(\s*void\s*\)\s*\{', it)) == 1
assert 'HAL_IncTick();' in it and 'xPortSysTickHandler();' in it
defines = subprocess.check_output(cc + flags + ['-E','-dM','-x','c','-include',
    'FreeRTOSConfig.h','/dev/null'], cwd=root, text=True)
assert '#define configTOTAL_HEAP_SIZE (9U * 1024U)' in defines
for option in ('DEMO=1','RFID_RTOS_TEST=1','RTOS=0','PROVISION=1','REPLACE_LIST=1'):
    result = subprocess.run(['make', '-n', option], cwd=root, capture_output=True, text=True)
    assert result.returncode != 0, 'Old profile unexpectedly accepted: ' + option
print('PASS: 42 firmware C files, no unused C source, include dependencies complete')
print('PASS: C syntax, single main, RTOS configuration and HAL/RTOS interrupt ownership')
print('PASS: one firmware only, old two-flash options rejected')
fixture_dir.cleanup()
