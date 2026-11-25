#!/usr/bin/env python3
import hashlib
from pathlib import Path
import re
import sys


def validate(raw):
    try:
        text = raw.decode('ascii')
    except UnicodeDecodeError:
        raise ValueError('Dung ASCII khong dau, khong BOM.') from None
    lines = text.splitlines()
    if not lines or lines[0] != 'uid,student_id,student_name':
        raise ValueError('Tieu de phai la uid,student_id,student_name')
    if len(lines) < 2:
        raise ValueError('Danh sach chua co sinh vien. Dien UID THAT, ma va ten vao Tools/Provision/DanhSach.csv.')
    if len(lines) > 51:
        raise ValueError('Toi da 50 sinh vien.')
    seen = set()
    for number, line in enumerate(lines[1:], 2):
        fields = line.split(',')
        if len(fields) != 3:
            raise ValueError(f'Dong {number}: can dung 3 truong, khong dong trong.')
        uid, student_id, name = fields
        if not re.fullmatch(r'(?:[0-9A-F]{8}|[0-9A-F]{14}|[0-9A-F]{20})', uid):
            raise ValueError(f'Dong {number}: UID phai co 8/14/20 ky tu HEX HOA, giu so 0 dau.')
        if uid in seen:
            raise ValueError(f'Dong {number}: UID trung.')
        seen.add(uid)
        if not 1 <= len(student_id) <= 15 or any(not 33 <= ord(c) <= 126 or c == '"' for c in student_id):
            raise ValueError(f'Dong {number}: ma sinh vien 1..15 ky tu, khong cach/ngoac kep.')
        if not 1 <= len(name) <= 48 or not name.strip() or any(not 32 <= ord(c) <= 126 or c == '"' for c in name):
            raise ValueError(f'Dong {number}: ten 1..48 ky tu ASCII, khong ngoac kep/toan dau cach.')
    return ('\r\n'.join(lines) + '\r\n').encode('ascii'), len(lines)-1


def main():
    if len(sys.argv) != 3:
        raise SystemExit('Usage: generate_list.py INPUT.csv OUTPUT.h')
    try:
        data, count = validate(Path(sys.argv[1]).read_bytes())
    except (OSError, ValueError) as exc:
        raise SystemExit(str(exc)) from None
    out = Path(sys.argv[2])
    values = ',\n'.join('    ' + ','.join(f'0x{b:02X}' for b in data[i:i+16]) for i in range(0,len(data),16))
    body = '#ifndef EMBEDDED_LIST_DATA_H\n#define EMBEDDED_LIST_DATA_H\n#include <stdint.h>\n'
    body += f'#define EMBEDDED_STUDENT_COUNT {count}U\n'
    body += f'#define EMBEDDED_LIST_VERSION "{hashlib.sha256(data).hexdigest()}"\n'
    body += 'static const uint8_t embedded_csv[] = {\n' + values + '\n};\n#endif\n'
    out.parent.mkdir(parents=True, exist_ok=True)
    if not out.exists() or out.read_text() != body:
        temporary = out.with_suffix('.tmp')
        temporary.write_text(body)
        temporary.replace(out)
    print(f'Danh sach hop le: {count} sinh vien, {len(data)} byte; SHA256 {hashlib.sha256(data).hexdigest()[:12]}')


if __name__ == '__main__':
    main()
