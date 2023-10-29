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

@dataclass
class Include:
    module: str
    structs: list[str]

@dataclass
class TypeRef:
    name: str
    definition: 'Type' = None
    array_size: int = 0

    @property
    def array_suffix(self):
        return f'[{self.array_size}]' if self.array_size > 0 else ''

@dataclass
class Msg:
    name: str
    type: int
    subtype: int = None

    request: TypeRef = None
    reply: TypeRef = None

class Type:
    name: str
    c_type: str

    def make_ref(self):
        return TypeRef(self.name, self)

@dataclass
class Primitive(Type):
    name: str
    c_type: str
    size: int

@dataclass
class Struct(Type):
    name: str = None
    fields: list['Field'] = field(default_factory=list)

    # used to identify if definition was inline of reply or request
    inline: bool = False

    @property
    def c_def_struct(self):
        return f'struct_def_{self.name}'

    @property
    def c_def_fields(self):
        return f'struct_fields_{self.name}'
    
    @property
    def c_type(self):
        return self.name
    
@dataclass
class ExternalStruct(Type):
    name: str = None
    module: str = None

    @property
    def c_def_struct(self):
        return f'{self.module}::struct_def_{self.name}'
    
    @property
    def c_type(self):
        return f'{self.module}::{self.name}'

@dataclass
class Field:
    name: str
    type: TypeRef
    presentation: str = 'DEFAULT'

    # Containing definition, gets linked in after parsing
    in_def: Struct = None

    @property
    def c_name(self):
        return f'msg_field_{self.in_def.name}_{self.name}'
    

primitive_types = [
    Primitive('char', 'char', 1),
    Primitive('u8', 'uint8_t', 1),
    Primitive('u16', 'uint16_t', 2),
    Primitive('u32', 'uint32_t', 4),
    Primitive('i8', 'int8_t', 1),
    Primitive('i16', 'int16_t', 2),
    Primitive('i32', 'int32_t', 4),
    Primitive('fd', 'int16_t', 2),
    Primitive('pid', 'int16_t', 2),
    Primitive('nid', 'int32_t', 4),
    Primitive('path', 'Qnx::PathBuf', 256),
    Primitive('termios', 'Qnx::Termios', None),
    Primitive('time', 'Qnx::Time', None),
    Primitive('stdfds', 'Qnx::Stdfds', None),
]

primitive_type_map = {t.name: t for t in primitive_types}

class DuplicateDef(Exception):
    pass

def check_duplicate(name_list):
    seen = set()
    for d in name_list:
        if d in seen:
            raise DuplicateDef(d)
        seen.add(d)

def filter_class(items, *classes):
    bins = tuple([list() for _ in classes])
    for item in items:
        bin_index = None
        for ci, c in enumerate(classes):
            if isinstance(item, c):
                bin_index = ci
        assert bin_index is not None
        bins[bin_index].append(item)
    return bins

class MetaInfo:
    includes: list[Include]
    structs: list[Struct]
    ext_structs: list[ExternalStruct]
    messages: list[Msg]
    sruct_map: dict[str, Struct]

    def __init__(self, structs, messages, includes) -> None:
        self.structs = structs
        self.messages = messages
        self.includes = includes
        self.ext_structs = []
        check_duplicate(d.name for d in self.messages)

    def resolve(self, tr: TypeRef):
        if tr.definition:
            return
        try:
            tr.definition = self.struct_map[tr.name]
        except KeyError:
            tr.definition = primitive_type_map[tr.name]

    def prepare(self):
        # first construct inline defs
        def add_inline(msg: Msg, sub: TypeRef, suffix):
            if sub.definition and sub.definition.inline:
                sub.definition.name = msg.name + suffix
                self.structs.append(sub.definition)

        for msg in self.messages:
            add_inline(msg, msg.request, '_request')
            add_inline(msg, msg.reply, '_reply')

        # add included defs
        for i in self.includes:
            for s in i.structs:
                self.ext_structs.append(ExternalStruct(s, i.module))

        all_structs = self.structs + self.ext_structs
        check_duplicate(d.name for d in all_structs)
        self.struct_map = {d.name: d for d in all_structs}

        # Resolve everything
        for msg in self.messages:
            self.resolve(msg.request)
            self.resolve(msg.reply)
        
        for s in self.structs:
            if isinstance(s, Struct):
                for f in s.fields:
                    self.resolve(f.type)

        # Now handle type and subtype
        for msg in self.messages:
            d = msg.request.definition
            assert isinstance(d, (Struct, ExternalStruct))
            fields = ['type']
            if msg.subtype is not None:
                fields.append('subtype')

            if isinstance(d, Struct) and d.inline:
                # Add them for inline requests
                u16 = primitive_type_map['u16'].make_ref()
                d.fields = [Field(f, u16, in_def=d, presentation='HEX') for f in fields] + d.fields
            else:
                if isinstance(d, Struct):
                    # Check existence, unless external
                    missing = set(fields) - set(f.name for f in d.fields)
                    if len(missing) > 0:
                        raise Exception(f'Missing fields {missing} on {d.name}  ')

        # And check the struct fields and resolve padds
        for d in self.structs:
            c = 0
            for f in d.fields:
                if f.name == 'padd':
                    c += 1
                    f.name = f'padd_{c}'
            check_duplicate(f.name for f in d.fields)
            for f in d.fields:
                f.in_def = d

    
    def write_hdr(self, name, o):
        o.write('#pragma once\n')
        o.write('#include <msg/meta.h>\n')

        for i in self.includes:
            o.write(f'#include <gen_msg/{i.module}.h>\n')

        o.write(f'namespace QnxMsg::{name} {{\n');

        for s in self.structs:
            if isinstance(s, ExternalStruct):
                continue
            o.write(f'struct {s.name} {{\n')
            for f in s.fields:
                o.write(f'   {f.type.definition.c_type} m_{f.name}{f.type.array_suffix};\n')
            o.write('} qine_attribute_packed;\n')

            o.write(f'extern const Meta::Struct {s.c_def_struct};\n')

        for m in self.messages:
            o.write(f'namespace msg_{m.name} {{\n')
            if m.type is not None:
                o.write(f'   static constexpr int TYPE = {m.type};\n')
            if m.subtype is not None:
                o.write(f'   static constexpr int SUBTYPE = {m.subtype};\n')
            o.write('}\n')
    
        o.write('extern const Meta::MessageList list;\n')

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
        o.write('using F = Meta::Field::Format;\n')
        o.write('using P = Meta::Field::Presentation;\n');


        for s in self.structs:
            for f in s.fields:
                fd = f.type.definition
                head = [c(f.name), f'offsetof({s.name}, m_{f.name})', f'sizeof({s.name}{f.type.array_suffix})']
                if isinstance(fd, Primitive):
                    c_def('static', f'Meta::Field {f.c_name}',
                    head + ['F::' + fd.name.upper(), 'P::' + f.presentation, 'nullptr', f.type.array_size])
                else:
                    c_def('static', f'Meta::Field {f.c_name}',
                    head + ['F::SUB', 'P::DEFAULT', '&' + fd.c_def_struct, f.type.array_size])
                
            c_def('static', f'Meta::Field {s.c_def_fields}[]', [f.c_name for f in s.fields])

            c_def('', f'Meta::Struct {s.c_def_struct}', [
                len(s.fields), s.c_def_fields, f'sizeof({s.name})'
            ])
            o.write('\n')

        # Messages
        for m in self.messages:
            c_def('static', f'Meta::Message {m.name}', [
                c(m.name), m.type, str(m.subtype is not None).lower(), m.subtype or '0',
                '&' + m.request.definition.c_def_struct, '&' + m.reply.definition.c_def_struct
            ])

        c_def('static', 'Meta::Message *all_msg[]', ['&' + m.name for m in self.messages])

        c_def('', f'Meta::MessageList list', [
            str(len(self.messages)), 'all_msg'
        ])

        o.write('}\n')

@dataclass
class Match:
    k: str
    v: int

@dataclass
class Reply:
    d: Struct

class MetaTransformer(lark.Transformer):
    def NUMBER(self, v):
        return int(v, base=0)

    def start(self, defs: list[Struct]):
        return MetaInfo(*filter_class(defs, Struct, Msg, Include))
    
    @v_args(inline=True)
    def def_msg(self, id, type, subtype, request, reply):
        return Msg(id, type, subtype, request, reply)
    
    @v_args(inline=True)
    def embedded_struct_body(self, struct):
        struct.inline = True
        return TypeRef(None, struct)
    
    @v_args(inline=True)
    def type_ref(self, ref):
        return TypeRef(str(ref))
    
    @v_args(inline=True)
    def type_ref_array(self, ref, len):
        return TypeRef(str(ref), array_size=len)
    
    @v_args(inline=True)
    def def_struct(self, name, struct):
        struct.name = str(name)
        return struct

    def struct_body(self, fields):
        return Struct(fields=fields)
    
    @v_args(inline=True)
    def field(self, name, type, presentation):
        return Field(str(name), type, presentation=presentation or 'DEFAULT')
    
    def p_hex(self, _):
        return 'HEX'
    
    def p_oct(self, _):
        return 'OCT'
    
    def includes(self, values):
        return [str(v) for v in values]
    
    @v_args(inline=True)
    def include(self, module, structs):
        return Include(module, structs)

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
