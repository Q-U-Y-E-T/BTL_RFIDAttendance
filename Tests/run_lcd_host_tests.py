#!/usr/bin/env python3
"""Kiem thu phan LCD (Tran Ngoc Son) tren may tinh: lcd.c + lcd_display.c
voi LCD 1602/PCF8574 gia lap, va ghep voi App cua Quyet (FatFs that, dia RAM)."""
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
         '-ITests/host/lcd', '-ITests/host', '-IApp/Inc', '-IModules/Storage',
         '-IModules/LCD', '-IMiddlewares/Third_Party/FatFs/src']
sources = ['Tests/host/lcd/test_lcd_display.c',
           'Modules/LCD/lcd.c', 'Modules/LCD/lcd_display.c',
           'Modules/Storage/storage.c', 'App/Src/storage_manager.c',
           'App/Src/attendance.c', 'App/Src/feedback.c', 'App/Src/app.c',
           'Middlewares/Third_Party/FatFs/src/ff.c']
binary = out / 'test_lcd_display'
subprocess.run(cc + flags + sources + ['-o', str(binary)], cwd=root, check=True)
env = os.environ.copy()
env.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
subprocess.run([str(binary)], cwd=root, check=True, env=env)
