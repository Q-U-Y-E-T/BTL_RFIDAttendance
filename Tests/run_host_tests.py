#!/usr/bin/env python3
"""Build production C against actual FatFs and a RAM disk, then run assertions."""
import os
import pathlib
import shlex
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
out = root / 'build-host'
out.mkdir(exist_ok=True)
cc = shlex.split(os.environ.get('HOST_CC', 'gcc'))
flags = ['-std=c11', '-Wall', '-Wextra', '-g', '-O1',
         '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
         '-DAPP_ENABLE_DEMO=0', '-include', 'Tests/host/fatfs_types.h',
         '-ITests/host', '-IApp/Inc', '-IModules/Storage',
         '-IMiddlewares/Third_Party/FatFs/src']
sources = ['Tests/host/test_suite.c', 'Modules/Storage/storage.c',
           'App/Src/storage_manager.c', 'App/Src/attendance.c',
           'App/Src/feedback.c', 'App/Src/app.c',
           'Middlewares/Third_Party/FatFs/src/ff.c']
binary = out / 'test_suite'
subprocess.run(cc + flags + sources + ['-o', str(binary)], cwd=root, check=True)
# This suite allocates no heap memory. Disable leak tracing for sandbox/ptrace
# environments; address and undefined-behavior checks remain enabled.
env = os.environ.copy()
env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
subprocess.run([str(binary)], cwd=root, check=True, env=env)
