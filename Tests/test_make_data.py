#!/usr/bin/env python3
from pathlib import Path
import json
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='rfid-make-') as tmp:
    sandbox=Path(tmp)/'project'
    shutil.copytree(root,sandbox,ignore=shutil.ignore_patterns('build-*','__pycache__','.git'))
    fake=Path(tmp)/'fake-tool'
    fake.write_text('''#!/usr/bin/env python3
from pathlib import Path
import json,sys
args=sys.argv[1:]
with Path('calls.jsonl').open('a') as log:log.write(json.dumps(args)+'\\n')
output=args[args.index('-o')+1] if '-o' in args else args[-1]
Path(output).parent.mkdir(parents=True,exist_ok=True)
Path(output).write_bytes(b'BUILD-DEPENDENCY-TEST-ONLY')
''')
    fake.chmod(0o755)
    data=sandbox/'Tools/Provision/DanhSach.csv'
    data.write_text('uid,student_id,student_name\n00112233,S1,FIRST NAME\n')
    command=['make','--no-print-directory','-j4',f'CC={fake}',f'OBJCOPY={fake}','SIZE=true']
    def run():
        subprocess.run(command,cwd=sandbox,check=True,capture_output=True,text=True)
        return (sandbox/'calls.jsonl').read_text().splitlines()
    first=run()
    generated=sandbox/'build-rtos/generated/embedded_list_data.h'
    old=generated.read_bytes()
    second=run()
    assert len(second)==len(first), 'Unchanged CSV unexpectedly recompiles firmware'
    data.write_text('uid,student_id,student_name\n00112233,S1,NEW NAME\n')
    third=run()
    assert len(third)>len(second) and generated.read_bytes()!=old
    calls=[json.loads(line) for line in third[len(second):]]
    assert any('App/Src/embedded_list.c' in args for args in calls)
    data.write_text('uid,student_id,student_name\n')
    fail=subprocess.run(command,cwd=sandbox,capture_output=True,text=True)
    assert fail.returncode != 0 and 'chua co sinh vien' in fail.stderr
print('PASS Make data flow: unchanged data avoids rebuild; changed CSV recompiles; empty input rejected')
