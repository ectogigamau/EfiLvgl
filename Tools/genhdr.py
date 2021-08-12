## @file
# Generate version and qstr headers
#
# Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
# This program and the accompanying materials
# are licensed and made available under the terms and conditions of the BSD License
# which accompanies this distribution.  The full text of the license may be found at
# http://opensource.org/licenses/bsd-license.php
#
# THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
# WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#

from __future__ import print_function

import os
import sys
import re
import argparse
import subprocess
import shlex
import shutil

FORCED_QSTR = [
    "Q(IA32)\n",
    "Q(X64)\n"
]

PP_CMD = "cl.exe /nologo /P"

def preprocess(in_file, out_file, incs, option):
    inc_option = " /I".join(incs)
    cmd = "%s %s /I%s /Fi%s %s" % (PP_CMD, option, inc_option, out_file, in_file)
    subprocess.check_call(shlex.split(cmd, posix=False))

def collect_qstr(src, mpy_dir, qstr_dir):
    python = "python"
    script = os.path.join(mpy_dir, "py", "makeqstrdefs.py")
    cmd = r"%s %s split %s %s dummy" % (python, script, src, qstr_dir)
    subprocess.check_call(shlex.split(cmd, posix=False))

def cat_qstr(dst, mpy_dir, qstr_dir):
    python = "python"
    script = os.path.join(mpy_dir, "py", "makeqstrdefs.py")
    cmd = r"%s %s cat dummy %s %s" % (python, script, qstr_dir, dst + ".tmp")
    subprocess.check_call(shlex.split(cmd, posix=False))

    with open(dst, "w+") as out_file:
        for f in [os.path.join(mpy_dir, "py", "qstrdefs.h"), dst + ".tmp"]:
            with open(f, "r") as in_file:
                shutil.copyfileobj(in_file, out_file, 10*1024*1024)
        out_file.writelines(["\r\n"])
        out_file.writelines(FORCED_QSTR)
    os.remove(dst + ".tmp")
    os.remove(dst + ".tmp.hash")

def gen_qstr(qstr_file, collected_qstr, mpy_dir, qstr_dir, incs, option):
    qstr_pattern = re.compile(r"(Q\([^()]+\))", re.DOTALL | re.MULTILINE)
    with open(collected_qstr, "r+") as fd:
        content = fd.read()
        content = qstr_pattern.sub(r'"\1"', content)
        fd.seek(0)
        fd.truncate(0)
        fd.write(content)

    pp_file = collected_qstr + '.i'
    preprocess(collected_qstr, pp_file, incs, option)

    qstr_pattern = re.compile(r'"(Q\([^()]+\))"', re.DOTALL | re.MULTILINE)
    with open(pp_file, "r+") as fd:
        content = fd.read()
        content = qstr_pattern.sub(r'\1', content)
        fd.seek(0)
        fd.truncate(0)
        fd.write(content)

    python = "python"
    script = os.path.join(mpy_dir, "py", "makeqstrdata.py")
    cmd = r"%s %s %s" % (python, script, pp_file)
    with open(qstr_file, "w+") as output:
        subprocess.check_call(shlex.split(cmd, posix=False), stdout=output)

    os.remove(collected_qstr)
    os.remove(pp_file)

def get_pkg_inc(pkg, arch):
    in_inc_section = False
    inc_paths = []
    pkg_folder = os.path.dirname(pkg)
    with open(pkg, 'r') as fd:
        for line in fd:
            line = line.split('#')[0].strip()

            if not line:
                continue

            if line[0] == '[':
                in_inc_section = line.startswith("[Includes")
                in_inc_section = in_inc_section and ('.' not in line or arch in line)
                continue

            if in_inc_section:
                inc_paths.append(os.path.normpath(os.path.join(pkg_folder, line)))
    return inc_paths

def gen_version(mpy_dir, hdr_file):
    python = "python"
    script = os.path.join(mpy_dir, "py", "makeversionhdr.py")

    cwd = os.getcwd()
    os.chdir(mpy_dir)
    subprocess.check_call([python, script, hdr_file])
    os.chdir(cwd)

def clear_file(f):
    with open(f, "w") as fd:
        pass

def clear_tree(folder):
    cwd = os.getcwd()
    os.chdir(folder)
    try:
        shutil.rmtree(".")
    except:
        pass
    os.chdir(cwd)

def replace_macro(s, m):
    for name in m:
        s = s.replace("$(%s)" % name, m[name])
    return s

def main(args):
    if "PACKAGES_PATH" in os.environ:
        pkg_paths = os.environ["PACKAGES_PATH"]
    elif "WORKSPACE" in os.environ:
        pkg_paths = os.environ['WORKSPACE']
    else:
        raise(EnvironmentError("EDK2 build environment was not setup correctly!"))

    args.inf_file = os.path.abspath(args.inf_file)
    pkg_paths = pkg_paths.split(os.path.pathsep)
    inf_path = os.path.dirname(args.inf_file)

    macros = {}
    pkg_files = []
    src_files = []
    inc_paths = [inf_path]
    cc_options = ''

    with open(args.inf_file, 'r') as fd:
        in_def_section = False
        in_bld_section = False
        in_src_section = False
        in_pkg_section = False

        for line in fd:
            line = line.split('#')[0].strip()

            if not line:
                continue

            if line[0] == '[':
                in_def_section = line.startswith("[Defines")
                in_src_section = line.startswith("[Sources")
                in_bld_section = line.startswith("[BuildOptions")
                in_pkg_section = line.startswith("[Packages")
                continue

            if in_def_section:
                if line.startswith("DEFINE "):
                    name, value = list(map(str.strip, line[6:].split("=", 1)))
                    macros[name] = value

            if in_src_section:
                line = line.split('|')[0].strip()
                line = replace_macro(line, macros)
                src = os.path.normpath(os.path.join(inf_path, line))
                folder = os.path.dirname(src)
                if line[-2:].lower() == '.c':
                    src_files.append(src)

                if folder not in inc_paths:
                    inc_paths.append(folder)

            if in_bld_section:
                tokens = line.split(':', 1)
                if len(tokens) == 2 and tokens[0].strip() == 'MSFT':
                    if '==' in tokens[1]:
                        tool,option = list(map(str.strip, tokens[1].split('==', 1)))
                    else:
                        tool,option = list(map(str.strip, tokens[1].split('=', 1)))
                    _,_,arch,tool,_ = tool.split('_')
                    if arch in [args.arch, '*'] and tool == 'CC':
                        cc_options += ' ' + option

            if in_pkg_section:
                for pkg_path in pkg_paths:
                    pkg = os.path.normpath(os.path.join(pkg_path, line))
                    if os.path.exists(pkg) and pkg not in pkg_files:
                        pkg_files.append(pkg)

    pkg_incs = []
    for pkg in pkg_files:
        pkg_incs += get_pkg_inc(pkg, args.arch)
    inc_paths = pkg_incs + inc_paths

    mpy_dir = os.path.join(inf_path, "..", "MicroPython")
    qstr_dir = os.path.join(inf_path, "Uefi", "genhdr", "qstr")
    collected_qstr_file = os.path.join(inf_path, "Uefi", "genhdr", "qstrdefs.collected.h")
    qstr_file = os.path.join(inf_path, "Uefi", "genhdr", "qstrdefs.generated.h")

    # prepare files or dirs
    gen_version(mpy_dir, os.path.join(inf_path, "Uefi", "genhdr", "mpversion.h"))
    clear_file(qstr_file)
    if not os.path.exists(qstr_dir):
        os.makedirs(qstr_dir)
    else:
        clear_tree(qstr_dir)

    for src in src_files:
        # collect qstr macros
        pp_file = src + ".i"
        preprocess (src, pp_file, inc_paths, cc_options)
        collect_qstr (pp_file, mpy_dir, qstr_dir)
        os.remove(pp_file)

    cat_qstr(collected_qstr_file, mpy_dir, qstr_dir)
    gen_qstr(qstr_file, collected_qstr_file, mpy_dir, qstr_dir, inc_paths, cc_options)

if __name__ == "__main__":
    external_tools_ready = True
    try:
        output = subprocess.check_output(["python", "--version"], stderr=subprocess.STDOUT)
        print("Found:", output.decode("utf-8").split('\n')[0])
    except:
        print("ERROR: 'python' is not in PATH env.")
        print("       Please add the path of 'python' in PATH env before running this tool.")
        external_tools_ready = False

    try:
        output = subprocess.check_output(shlex.split(PP_CMD, posix=False)[0], stderr=subprocess.STDOUT)
        print("Found:", output.decode("utf-8").split('\n')[0])
    except:
        print("ERROR: '%s' cannot run." % PP_CMD)
        print("       Please initialize build environment before running this tool.")
        external_tools_ready = False

    if not external_tools_ready:
        sys.exit(1)

    print("")
    parser = argparse.ArgumentParser(description='Generate qstr header files.')
    parser.add_argument('inf_file', help='Driver inf file path')
    parser.add_argument('-a', dest="arch", default='X64', choices=['IA32', 'X64'], help='ARCH: IA32/X64')
    main(parser.parse_args())

