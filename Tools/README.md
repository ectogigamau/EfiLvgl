# Tool: genhdr.py
A python script tool used to collect all QSTR references and generate corresponding macro definitions in file qstrdefs.generated.h, as well as to collect version information and generate corresponding macros in file mpversion.h. These two files are needed by MicroPython to build.

## Usage
Note:

- This tool is only needed to run, if developers has changed, added new QSTRs and/or upgraded MicroPython, to reflect the correct QDEF() definitions for fixed strings or version strings.
- Initialize EDK II and Visual Studio build environment in advance, which are needed to do C file preprocessing.
- Add path to python.exe into system PATH environment before running this tool.

<Python2/Python3> genhdr.py -a <arch> <path_to_MicroPythonDxe.inf_file>

