#!/usr/bin/env python3
import importlib.util
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('generator',root/'Tools/Provision/generate_list.py')
generator=importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
header='uid,student_id,student_name\n'
good=(header+'00112233,S1,NGUYEN VAN A\n04112233445566,S2,TRAN THI B\n00112233445566778899,S3,LE VAN C\n').encode()
canonical,count=generator.validate(good)
assert count==3 and b'\r\n' in canonical
for payload in [header.encode(), b'\xef\xbb\xbf'+good, (header+'aabbccdd,S1,A\n').encode(),
                (header+'BAD,S1,A\n').encode(),
                (header+'00112233,S1,A\n00112233,S2,B\n').encode(),
                (header+'00112233,S1,   \n').encode(),
                (header+'00112233,S1,"NAME"\n').encode(),
                (header+'00112233,1234567890123456,A\n').encode(),
                (header+'00112233,S1,'+'A'*49+'\n').encode(),
                (header+'00112233,S1,TÊN\n').encode(),
                (header+''.join(f'{i:08X},S{i},NAME\n' for i in range(51))).encode()]:
    try: generator.validate(payload)
    except ValueError: pass
    else: raise AssertionError('Invalid list accepted')
print('PASS generator: real UID sizes/leading zero, 11 invalid list groups, no placeholder records')
out=root/'build-host';out.mkdir(exist_ok=True)
cc=shlex.split(os.environ.get('HOST_CC','gcc'))
flags=['-std=c11','-Wall','-Wextra','-g','-O1','-fsanitize=address,undefined',
       '-fno-omit-frame-pointer','-include','Tests/host/fatfs_types.h',
       '-ITests/host','-IApp/Inc','-IModules/Storage',
       '-IMiddlewares/Third_Party/FatFs/src']
sources=['Tests/host/test_list_update.c','Modules/Storage/storage.c',
         'App/Src/storage_manager.c','App/Src/attendance.c','App/Src/feedback.c',
         'App/Src/app.c','Middlewares/Third_Party/FatFs/src/ff.c']
binary=out/'test_list_update'
subprocess.run(cc+flags+sources+['-o',str(binary)],cwd=root,check=True)
env=os.environ.copy();env.setdefault('ASAN_OPTIONS','detect_leaks=0')
subprocess.run([str(binary)],cwd=root,env=env,check=True)
with tempfile.TemporaryDirectory(prefix='rfid-list-') as tmp:
    data=Path(tmp)/'DanhSach.csv';data.write_bytes(good)
    generated=root/'build-host/generated/embedded_list_data.h'
    subprocess.run(['python3','Tools/Provision/generate_list.py',str(data),str(generated)],cwd=root,check=True)
    auto_sources=[s for s in sources if s != 'Tests/host/test_list_update.c']
    auto_sources += ['Tests/host/test_embedded_list.c','App/Src/embedded_list.c','Modules/Storage/student_list_update.c']
    auto_binary=out/'test_embedded_list'
    subprocess.run(cc+flags+['-Ibuild-host/generated']+auto_sources+['-o',str(auto_binary)],cwd=root,check=True)
    subprocess.run([str(auto_binary)],cwd=root,env=env,check=True)
