#!/usr/bin/env python3

import argparse
import json
from pathlib import Path

from dataclasses import dataclass

parser = argparse.ArgumentParser()
parser.add_argument('inputs', nargs='+')
parser.add_argument('-d', dest='dir', required=True)
parser.add_argument('-n', dest='name', required=True)

args = parser.parse_args()

class DuplicateMessage(Exception):
    pass

@dataclass
class Message:
    name: str
    type: int
    subtype: int = None
    fields: list['Field'] = None

@dataclass
class Field:
    name: str
    format: str
    c_type: str = None
    presentation: str = 'DEFAULT'

type_map = {
    'U8': (1, 'uint8_t'),
    'U16': (2, 'uint16_t'),
    'U32': (4, 'uint32_t'),
    'PID': (4, 'uint32_t'),
}

class MetaInfo:
    messages: dict[str, Message]
    def __init__(self) -> None:
        self.messages = {}

    def read(self, f):
        data = json.load(f)
        for msg in data:
            name = msg['name']
            if name in self.messages:
                raise DuplicateMessage(name)
            type_parts = msg['type'].split(':')
            type = int(type_parts[0])
            msg = Message(
                name=name,
                type=type,
                fields=self.parse_fields(msg)
            )
            if len(type_parts) >= 2:
                msg.subtype=int(type_parts[1])
            
            self.messages[msg.name] = msg
    def parse_fields(self, msg):
        acc = []
        for fdesc in msg['fields']:
            if isinstance(fdesc, list):
                f = Field(fdesc[0], fdesc[1])
            else:
                raise Exception(f'Unknwon field type {type(fdesc)}')
            acc.append(f)
            _, f.c_type = type_map[f.format]
        return acc
    
    def write_hdr(self, o):
        o.write('#pragma once\n')
        o.write('#include <msg/meta.h>\n')

        o.write(f'namespace QnxMsg::{args.name} {{\n');

        for m in self.messages.values():
            o.write(f'struct {m.name} {{\n')
            o.write(f'   uint16_t type;\n');
            if (m.subtype is not None):
                o.write(f'   uint16_t subtype;\n');
            for f in m.fields:
                o.write(f'   {f.c_type} {f.name};\n')
            o.write('} qine_attribute_packed;\n')
    
        o.write('extern const QnxMessageList list;\n')

        o.write('}\n')
    
    def write(self, o):
        def c(v: str):
            return '"' + v.replace('"', '\\"') + '"'
        
        def c_init(args: list[str]):
            return '{' + ', '.join(str(v) for v in args) + '}'
        
        def c_def(modifiers, type_name, init_list):
            init_list_str= c_init(init_list)
            o.write(f'{modifiers} const {type_name} = {init_list_str};\n')

        o.write(f'#include "{args.name}.h"\n')

        o.write(f'namespace QnxMsg::{args.name} {{\n');
        o.write('using F = QnxMessageField::Format;\n')
        o.write('using P = QnxMessageField::Presentation;\n');

        for m in self.messages.values():
            for f in m.fields:
                c_def('static', f'QnxMessageField msgf_{m.name}_{f.name}',
                      [c(f.name), f'offsetof({m.name}, {f.name})', f'sizeof({f.c_type})', f'F::{f.format}', f'P::{f.presentation}'])

            c_def('static', f'QnxMessageField msgf_{m.name}[]', [f'msgf_{m.name}_{f.name}' for f in m.fields])

            subtype = str(m.subtype) if m.subtype else '0xFFFF'
            c_def('static', f'QnxMessageType msg_{m.name}', [
                str(m.type), subtype, c(m.name), len(m.fields), f'msgf_{m.name}'
            ])

        c_def('static', 'QnxMessageType all_msg[]', [
            f'msg_{m.name}' for m in self.messages.values()
        ])

        c_def('', f'QnxMessageList list', [
            str(len(self.messages)), 'all_msg'
        ])

        o.write('}\n')
meta = MetaInfo()

for input in args.inputs:
    with open(input, 'rt') as f_in:
        meta.read(f_in)

out_dir = Path(args.dir)
out_dir.mkdir(exist_ok=True)
with open(out_dir / (args.name + '.cpp'), 'wt') as f_out:
    meta.write(f_out)

with open(out_dir / (args.name + '.h'), 'wt') as f_out:
    meta.write_hdr(f_out)