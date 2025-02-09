"""
transform milk code to wool code to mr.sheep vm bytecode compiler
[syntax]:
: main # function
    1 2 + . # print 3
    dup * . # print 9
    exit    # return 9
;
"""

import sys
import re
from enum import Enum

## tools ######################################################################
def counter():
    _c = 0
    def c(reset=False):
        nonlocal _c
        if reset:
            _c = 0
        else:
            _c += 1
        return _c
    return c
c = counter()

## milk headers ###############################################################

# structure (Ints.TYPE, arg0, arg1, ...)
class Intruction(Enum):
    ADD = c()
    PUSH = c()
    POP = c()
    DUP = c()
    DUMP = c()
    _COUNT = c()

    def __str__(self):
        return str("%s(%d)" % self, self.value)

inst_to_dump_map = {
    Intruction.ADD: lambda inst: """
    ; [ADD]
    mov &0, &255
    dec &0
    add ^0, ^255
""",
    Intruction.PUSH: lambda inst: """
    ; [PUSH]
    inc &0
    mov ^0, %s
""" % inst[1],
    Intruction.POP: lambda inst: """
    ; [POP]
    dec &0
""",
    Intruction.DUP: lambda inst: """
    ; [DUP]
    mov &255, ^0
    inc &0
    mov ^0, &255
""",
    Intruction.DUMP: lambda inst: """
    ; [DUMP]
    print_c ^0
""",
}
assert len(inst_to_dump_map) + 1 == Intruction._COUNT.value, "milk:error: not all inst maps are defined"

def milk_inst_dump(inst):
    return inst_to_dump_map[inst[0]](inst)
    
inst_to_argc_map = {
    Intruction.ADD: 0,
    Intruction.PUSH: 1,
    Intruction.POP: 0,
    Intruction.DUP: 0,
    Intruction.DUMP: 0,
}
assert len(inst_to_argc_map) + 1 == Intruction._COUNT.value, "milk:error: not all inst maps are defined"
rep_to_inst_map = {
    '+': Intruction.ADD,
    'push': Intruction.PUSH,
    'pop': Intruction.POP,
    'dup': Intruction.DUP,
    '.': Intruction.DUMP,
}
assert len(rep_to_inst_map) + 1 >= Intruction._COUNT.value, "milk:error: not all inst maps are defined"

word_pattern = r"[a-zA-Z_][a-zA-Z_0-9]*"

## milk #######################################################################

def milk_get_ref(back_ref, i):
    prev = back_ref[0]
    for t in back_ref:
        if t[1] < i:
            prev = t
        else:
            break
    return prev

def milk_get_ref_str(msg, back_ref, i):
    ref = milk_get_ref(back_ref, i)
    return "%s:%d:%s\n%s" % input_file, ref[0], ref[2]

def milk_find(lst, item):
    return -1 if item not in lst else lst.index(item)

def milk_process_toknes(tokens, back_ref):
    functions, f_body, f_start = {}, [], False
    n, indx_s, indx_e = len(tokens), -1, -1
    errors = False
    err = lambda str: oprint(milk_get_ref_str(str, back_ref, n - len(tokens)))

    while tokens:
        if not f_start:
            if tokens[0] == ':':
                f_start = True
                tokens = tokens[1:]
                indx_s = milk_find(tokens, ':')
                indx_e = milk_find(tokens, ';')
                if indx_s < indx_e or indx_e == -1:
                    errors = True
                    err(f"error: you didn't close the function '{tokens[0]}'")
                    # indx_e == -1 -> there are no more closers, so we consume
                    # all the tokens (ts[-1:] = [])
                    # else, we skip to the next func
                    tokens = tokens[indx_e:]
            else:
                errors = True
                err(f"error: token outside function def. '{tokens[0]}'")
                tokens = tokens[1:]
            continue
        # else: # f_start
        if not re.fullmatch(word_pattern, word):
            errors = True
            err(f"error: invalid function name. '{tokens[0]}'")
            tokens = tokens[indx_e:]
            continue
        # TODO: grab func name check no collititons bettween std and user
        #       get the body of the func

        pass
        break

    if errors:
        exit(1)


def milk_process_file(input_file):
    tokens = []
    # list((line_num, token_num, line))
    back_ref = []
    try:
        with open(input_file, 'r') as f:
            for i, line in enumerate(f):
                # remove comments and save back_ref for debuging
                index = line.find('#')
                if index != -1:
                    line = line[:index]
                ts = line.split()
                back_ref.append((i, len(tokens), line))
                tokens.extend(ts)
    except FileNotFoundError:
        print(f"milk:error: the file '{input_file}' was not found.")
        exit(1)
    print(back_ref)
    print(tokens)
    milk_process_toknes(tokens, back_ref)

def milk_dump_to_file(output_file, content):
    with open(output_file, 'w') as outfile:
        outfile.write(content)

def milk_make_the_magic_happen(input_file, output_file):
    content = milk_process_file(input_file)
    # milk_dump_to_file(output_file, content)
    # print(f"milk: successfully processed {input_file} -> {output_file}")

input_file = ""
output_file = ""
## main #######################################################################
if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <input_file> <output_file>")
        exit(1)
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    milk_make_the_magic_happen(input_file, output_file)

