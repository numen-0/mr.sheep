import os
import subprocess
import argparse
from enum import Enum
import difflib
import filecmp
import hashlib

###############################################################################

test_ext = ".expected"
mr_sheep_bin = "./mr.sheep"
mr_sheep_ext = ".baa"
wool_ext = ".wool"
wool_bin = "./wool"
wool_dir = "./test/wool/"

OK_C = "\033[32;1m"
CMD_C = "\033[35;3m"
WARN_C = "\033[33;1m"
ERROR_C = "\033[31;1m"
RESET_C = "\033[0m"

###############################################################################

def t_replace_extension(file_name, new_extension):
    base_name, _ = os.path.splitext(file_name)
    new_file_name = base_name + new_extension
    return new_file_name

def t_path_join(dir_path, file_name):
    return os.path.join(dir_path, file_name)

def t_get_dir(dir, ext):
    files = []
    for f in [t_path_join(dir, f) for f in os.listdir(dir)]:
        if os.path.isfile(f):
            if f.endswith(ext):
                files.append(f)
        elif os.path.isdir(f):
            files.extend(t_get_dir(f, ext))  # Use extend instead of expand
    return files

def t_exec(cmd):
    try:
        return subprocess.run(cmd, shell=True, check=True, text=True, capture_output=True)
    except Exception as e:
        return None

def t_dump(file_path, text):
    print(f"test: dumping stdout to '{file_path}'")
    with open(file_path, 'w') as file:
        file.write(text)


###############################################################################

def t_compare_binaries(actual_file, expected_file):
    try:
        return filecmp.cmp(actual_file, expected_file, shallow=False)
    except FileNotFoundError:
        print(f"test: {ERROR_C}expected binary file '{expected_file}' not found.{RESET_C}")
    except Exception as e:
        print(f"test: error comparing binaries: {e}")
    return False

def t_compare_output(actual, expected_file):
    try:
        with open(expected_file, 'r') as file:
            expected = file.read()
        
        if actual == expected:
            return True
        else:
            print(f"test: {ERROR_C}output does not match the expected result{RESET_C}")
            print("--- Actual vs Expected ---")
            diff = difflib.unified_diff(expected.splitlines(), actual.splitlines(), fromfile=expected_file, tofile="actual_output", lineterm='')
            for line in diff:
                print(line)
    except FileNotFoundError:
        print(f"test: {ERROR_C}expected output file '{expected_file}' not found.{RESET_C}")
    except Exception as e:
        print(f"test: error comparing output: {e}")
    return False

###############################################################################

def test_print_sheep(path):
    with open(path, 'r') as file:
        print(file.read())

def test_milk(dump=False):
    print(f"[{OK_C}Milk Test{RESET_C}]")
    pass

def test_wool(dump=False):
    counter, total = 0, 0
    print(f"[{OK_C}Wool Test{RESET_C}]")
    for f in t_get_dir(wool_dir, wool_ext):
        total += 1
        print(f"test: testing '{f}'")
        mr_sheep_bc = t_replace_extension(f, mr_sheep_ext)
        expected_bc = mr_sheep_bc + test_ext

        if dump: # dump new binaryes
            mr_sheep_bc = expected_bc

        # compile to bytecode
        cmd = f"{wool_bin} {f} {mr_sheep_bc}"
        print(f"\t[level:00]: {CMD_C}{cmd:64}{RESET_C} > ", end="")
        result = t_exec(cmd)

        if not result or result.returncode != 0:
            print(f"{ERROR_C}FAIL{RESET_C}")
            continue
        print(f"{OK_C}SUCCESS{RESET_C}")

        # compare baa binary
        if not dump:
            print(f"\t[level:01]: {"baa binary check":64} > ", end="")
            if not t_compare_binaries(mr_sheep_bc, expected_bc):
                print(f"{WARN_C}BINARY MISMATCH{RESET_C}")
            else:
                print(f"{OK_C}SUCCESS{RESET_C}")

        # run bytecode
        cmd = f"{mr_sheep_bin} {mr_sheep_bc}"
        print(f"\t[level:10]: {CMD_C}{cmd:64}{RESET_C} > ", end="")
        result = t_exec(cmd)

        if not result or result.returncode != 0:
            print(f"{ERROR_C}FAIL{RESET_C}")
            continue
        print(f"{OK_C}SUCCESS{RESET_C}")

        # dump/compare stdout
        expected_out_file = f + test_ext
        if dump: # dump stdout
            t_dump(expected_out_file, result.stdout)
        else: # compare stdout
            print(f"\t[level:11]: {"output check":64} > ", end="")
            result = t_compare_output(result.stdout, expected_out_file)
            if not result:
                print(f"{ERROR_C}FAIL{RESET_C}")
            else:
                print(f"{OK_C}SUCCESS{RESET_C}")
                counter += 1
    if not dump:
        print(f"\nresult: passed test: {counter}/{total}")

###############################################################################

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Mr.Sheep proyect")
    
    parser.add_argument('-c', '--no-color', action='store_true', help='deactivate colored output')
    parser.add_argument('-d', '--dump', action='store_true', help='activate dump mode')
    parser.add_argument('-q', '--quiet', action='store_true', help='suppress sheep')
    
    args = parser.parse_args()
    
    if args.no_color:
        OK_C = ""
        CMD_C = ""
        WARN_C = ""
        ERROR_C = ""
        RESET_C = ""

    if not args.quiet:
        test_print_sheep("./mr.sheep.txt")
    test_wool(dump=args.dump)
    test_milk(dump=args.dump)

