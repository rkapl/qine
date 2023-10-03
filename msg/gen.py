#!/usr/bin/env python3.11

import argparse
import json
import lark
from lark import v_args
import enum
from pathlib import Path
from dataclasses import dataclass, field

self_path = Path(__file__).parent

parser = argparse.ArgumentParser()
parser.add_argument('inputs', nargs='+')
parser.add_argument('-d', dest='dir', required=True)

args = parser.parse_args()

grammar = lark.Lark((self_path / 'qmsg.lark').read_text())

class DefinitionKind(enum.Enum):
    MSG = 0
    MSG_TYPE = 1
    MSG_SUBTYPE = 2
    REPLY = 3
    STRUCT = 4

@dataclass
class TypeRef:
    name: str
    c_type: str
    size: int
    definition: 'Definition' = None

@dataclass
class Definition:
    kind: DefinitionKind
    name: str
    type: int = None
    subtype: int = None
    reply: 'Definition' = None
    reply_name: str = None
    fields: list['Field'] = field(default_factory=list)

    @property
    def c_name(self):
        return f'msg_{self.name}'

    @property
    def c_ffields(self):
        return f'msgf_{self.name}'

@dataclass
class Field:
    name: str
    type: str
    in_def: Definition = None
    type_ref: TypeRef = None
    presentation: str = 'DEFAULT'

    @property
    def c_name(self):
        return f'msgf_{self.in_def.name}_{self.name}'
    

primitive_types = [
    TypeRef('u8', 'uint8_t', 1),
    TypeRef('u16', 'uint16_t', 2),
    TypeRef('u32', 'uint32_t', 4),
    TypeRef('pid', 'uint32_t', 4),
]

primitive_type_map = {t.name: t for t in primitive_types}

class DuplicateDef(Exception):
    pass

class DuplicateField(Exception):
    pass

def check_duplicate(name_list, exc_factory):
    seen = set()
    for d in name_list:
        if d in seen:
            raise exc_factory(d)
        seen.add(d)

class MetaInfo:
    defs: list[Definition]
    def __init__(self, defs) -> None:
        self.defs = defs
        check_duplicate((d.name for d in defs), DuplicateDef)

    def prepare(self):
        self.def_map = {d.name: d for d in self.defs if d.kind == DefinitionKind.STRUCT}
        flat_defs = []
        for d in self.defs:
            if d.reply:
                flat_defs.append(d.reply)
            flat_defs.append(d)
        self.defs = flat_defs

        for d in self.defs:
            # Add fields for type and subtype
            match_fields = []
            if d.type is not None:
                match_fields.append(Field('type', 'u16'))
                d.kind = DefinitionKind.MSG_TYPE
                if d.subtype is not None:
                    d.kind = DefinitionKind.MSG_SUBTYPE
                    match_fields.append(Field('subtype', 'u16'))
            d.fields = match_fields + d.fields

            for f in d.fields:
                f.in_def = d
                f.type_ref = primitive_type_map.get(f.type) or self.def_map.get(f.type)
                if f.type_ref is None:
                    raise KeyError(f.type)
    
    def write_hdr(self, name, o):
        o.write('#pragma once\n')
        o.write('#include <msg/meta.h>\n')

        o.write(f'namespace QnxMsg::{name} {{\n');

        for d in self.defs:
            o.write(f'struct {d.name} {{\n')
            if d.type is not None:
                o.write(f'   static constexpr uint16_t type = {d.type};\n')
            if d.subtype is not None:
                o.write(f'   static constexpr uint16_t subtype = {d.subtype};\n')

            for f in d.fields:
                o.write(f'   {f.type_ref.c_type} m_{f.name};\n')
            o.write('} qine_attribute_packed;\n')
    
        o.write('extern const QnxMessageList list;\n')

        o.write('}\n')
    
    def write(self, name, o):
        def c(v: str):
            return '"' + v.replace('"', '\\"') + '"'
        
        def c_init(args: list[str]):
            return '{' + ', '.join(str(v) for v in args) + '}'
        
        def c_def(modifiers, type_name, init_list):
            init_list_str= c_init(init_list)
            o.write(f'{modifiers} const {type_name} = {init_list_str};\n')

        o.write(f'#include "{name}.h"\n')

        o.write(f'namespace QnxMsg::{name} {{\n');
        o.write('using F = QnxMessageField::Format;\n')
        o.write('using P = QnxMessageField::Presentation;\n');

        for d in self.defs:
            for f in d.fields:
                format = 'SUB' if f.type_ref.definition else f.type_ref.name.upper()
                c_def('static', f'QnxMessageField {f.c_name}',
                      [c(f.name), f'offsetof({d.name}, m_{f.name})', f'sizeof({f.type_ref.c_type})', 'F::' + format, 'P::' + f.presentation])

            c_def('static', f'QnxMessageField msgf_{d.name}[]', [f'msgf_{d.name}_{f.name}' for f in d.fields])

            resolved_reply = 'NULL'
            if d.reply is not None:
                resolved_reply = '&' + d.reply.c_name
            match = f'QnxMessageType::Kind::{d.kind.name}'
            c_def('static', f'QnxMessageType msg_{d.name}', [
                match, str(d.type or 0), str(d.subtype or 0), c(d.name), resolved_reply, len(d.fields), f'msgf_{d.name}'
            ])

        c_def('static', 'QnxMessageType all_msg[]', [
            d.c_name for d in self.defs
        ])

        c_def('', f'QnxMessageList list', [
            str(len(self.defs)), 'all_msg'
        ])

        o.write('}\n')

@dataclass
class Match:
    k: str
    v: int

@dataclass
class Reply:
    d: Definition

class MetaTransformer(lark.Transformer):
    def start(self, defs: list[Definition]):
        meta = MetaInfo(defs)
        return meta

    def mk_def(self, kind, name: str, children):
        d = Definition(kind, name)
        d.fields = []
        for c in children:
            if isinstance(c, Match):
                setattr(d, c.k, c.v)
            elif isinstance(c, Reply):
                d.reply = c.d
                d.reply.name = d.name + '_reply'
            elif isinstance(c, Field):
                d.fields.append(c)
        check_duplicate((f.name for f in d.fields), DuplicateField)
        return d

    @v_args(inline=True)
    def def_(self, kind, name: str, children):
        return self.mk_def(kind, name, children)
    
    @v_args(inline=True)
    def reply(self, children):
        return Reply(self.mk_def(DefinitionKind.REPLY, None, children))
    
    @v_args(inline=True)   
    def kind_struct(self, items):
        print(items)
        return DefinitionKind.STRUCT
    
    @v_args(inline=True)
    def kind_msg(self):
        return DefinitionKind.MSG
    
    def fields(self, fields):
        return fields
    
    @v_args(inline=True)
    def match_type(self, number):
        return Match('type', number)
    
    @v_args(inline=True)
    def match_subtype(self, number):
        return Match('subtype', number)
    
    @v_args(inline=True)
    def field(self, name, type, presentation):
        return Field(str(name), str(type), presentation=presentation or 'DEFAULT')
    
    def p_hex(self, _):
        return 'HEX'

for input in args.inputs:
    ast = grammar.parse(Path(input).read_text())
    meta: MetaInfo = MetaTransformer().transform(ast)
    meta.prepare()

    name = Path(input).stem
    out_dir = Path(args.dir)
    out_dir.mkdir(exist_ok=True)
    with open(out_dir / (name + '.cpp'), 'wt') as f_out:
        meta.write(name, f_out)

    with open(out_dir / (name + '.h'), 'wt') as f_out:
        meta.write_hdr(name, f_out)
