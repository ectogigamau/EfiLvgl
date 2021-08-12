# MicroPython Interpreter for UEFI

This package contains an implementation of [MicroPython](https://github.com/micropython) for UEFI. MicroPython is an implementation of Python 3 optimized to run in constrained environments.

The MicroPython interpreter is a major component of the MicroPython Test Framework for UEFI.

## Overview

This package contains three main components:

  * MicroPython driver
      * The core MicroPython interpreter implemented as a DXE protocol.
      * Source code folder: `MicroPythonDxe`
      * Binary file: `MicroPythonDxe.efi`
  * MicroPython application
      * User interactive interface (wrapper) of the MicroPython interpreter, which can be launched as a UEFI Shell application or  Shell-Independent UEFI application (via BDS Boot Option, similar to launching an OS loader).
      * Source code folder: `MicroPythonApp`
      * Binary file: `micropython.efi`
  * Virtual console driver
      * Implement an emulated console device, used to simulate user interaction in test scripts.

## Features

### MicroPython Features

All official MicroPython features are implemented, with the following exceptions:
  * Floating point (including math)
  * Inline assembly code
  * Compiled Python bytecode
  * Some hardware feature modules (pulse, i2c, ...)
  * Some external libraries (select, ssl, socket, libffi, ...)

Note that MicroPython does not implement 100% of CPython syntax. Please refer to [MicroPython differences from CPython](http://docs.micropython.org/en/latest/pyboard/genrst/index.html#micropython-differences-from-cpython) for more information.

### UEFI Specific Features

* Asynchronous execution mode (run in the background of UEFI firmware)
* Full UEFI services support (via `uefi` module)
* Direct physical memory access (including memory allocation)
* Enhanced regular expression (via `_re` module)

## How to build

The MicroPython Interpreter for UEFI (`MicroPythonPkg`) must be built in an EDK II build environment. Please refer to [Getting Started with EDK II](https://github.com/tianocore/tianocore.github.io/wiki/Getting-Started-with-EDK-II) for information on setting up an EDK II build environment.

    C:\edk2> edksetup.bat
    C:\edk2> build -p MicroPythonPkg\MicroPythonPkg.dsc -a X64 -t VS2017 -b DEBUG

Once the build is completed, three binary files generated under the build output folder: `Build\MicroPythonPkg\DEBUG_VS2017\X64`

- `micropython.efi`: main application used to launch the MicroPython interpreter.
- `MicroPythonDxe.efi`: a driver which is the actual implementation of MicroPython interpreter and will be loaded automatically by `micropython.efi`.
- `VirtualConsoleDxe.efi`: virtual console driver used only for test interaction, must be loaded manually in advance.

### Currently supported EDK II tool chains

* `VS2013/VS2013x86`
* `VS2015/VS2015x86`
* `VS2017`

Additional toolchains will be supported in future releases.

## Usage

Normally `micropython.efi` and `MicroPythonDxe.efi` must be used together and run from the same folder. `micropython.efi` is used as the entry application for launching the MicroPython interpreter in a UEFI environment. It will try locate and load `MicroPythonDxe.efi` automatically. It is not necessary to load `MicroPythonDxe.efi` manually in advance.

### Command Line Options

```
micropython.efi  \[ -a | -c | -i \]  \[<path_to_python_script_file> | "<single_line_python_script>"\]

-a    Launch the MicroPython interpreter and then exit.
      Let it run in the background
-c    Execute the Python script passed on command line and then exit
-i    Launch the REPL (after executing the Python script, if given)
```

If no option is specified on the command line, the application will enter [REPL](https://docs.micropython.org/en/latest/esp8266/reference/repl.html) mode.

## Accessing UEFI Services

UEFI services are provided by module `uefi` (`Lib/Uefi/uefi.py`). The user must import this module before using any UEFI services in a Python script.

This module provides following global types and objects:

- `uefi.mem("c_type_string" \[, <memory_address>\] \[, <alloc_page>\] \[, <read_only>\])``
  - A Python type used to access physical memory, including a ctypes+struct-like mechanism which can be used to describe the memory type and layout (C data type/structure/function).
- `uefi.efistatus`
  - Exception corresponding to error code of `EFI_STATUS`
- `uefi.bs`
  - A `uefi.mem` object used to wrap `gBS (EFI_BOOT_SERVICES)`. Users can use it to access all fields in `gBS`.
- `uefi.rt`
  - A `uefi.mem` object used to wrap `gRT (EFI_RUNTIME_SERVICES)`. Users can use it to access all fields in `gRT`.
- `uefi.ds`
  - A `uefi.mem` object used to wrap `gDS (EFI_DXE_SERVICES)`. Users can use it to access all fields in `gDS`.
- `uefi.st` (System Table Object)
  - A `uefi.mem` object used to wrap `gST (EFI_SYSTEM_TABLE)`. Users can use it to access all fields in `gST`.
- `uefi.VariableStorage`
  - A Pythonic wrapper of all variable services provided in `gRT`.

The following example utilizes UEFI services:

```
python
import Lib.Uefi.uefi as uefi

# Reserve 8-byte memory
count = uefi.mem("Q")
try:
    uefi.bs.GetNextMonotonicCount(count.REF())
    print("Next monotoinc number:", count.VALUE)
except efistatus as status:
    print("ERROR: cannot get next monotonic count")

# Access variable "LangCodes"
vars = uefi.VariableStorage("8BE4DF61-93CA-11D2-AA0D-00E098032B8C")
print(vars["LangCodes"])
```

## Syntax of c_type_string in uefi.mem

Basic data types:

```
'b' : INT8
'B' : UINT8
'h' : INT16
'H' : UINT16
'l' : INT32
'L' : UINT32
'q' : INT64
'Q' : UINT64
'i' : int
'I' : unsigned int
'n' : INTN
'N' : UINTN
'G' : EFI_GUID
'P' : VOID*
'a' : CHAR8[]
'u' : CHAR16[]
'F' : (VOID*)(...)
'E' : EFI_STATUS
'T' : EFI_HANDLE
'O' : <struct>
```

All other complex data types (C structures) must be defined via `ucollections.OrderedDict` and referenced by form of `O#<struct_def_variable_name>`. The following example describes a C structure in Python code:

```
c
typedef struct {
  EFI_HANDLE  AgentHandle;
  EFI_HANDLE  ControllerHandle;
  UINT32      Attributes;
  UINT32      OpenCount;
} EFI_OPEN_PROTOCOL_INFORMATION_ENTRY;
```

This example is a Pythonic equivalent:

```
python
import uefi
from ucollections import OrderedDict

EFI_OPEN_PROTOCOL_INFORMATION_ENTRY = OrderedDict([
    ("AgentHandle",         "T"),
    ("ControllerHandle",    "T"),
    ("Attributes",          "L"),
    ("OpenCount",           "L"),
])

InfoEntry = uefi.mem("O#EFI_OPEN_PROTOCOL_INFORMATION_ENTRY")
InfoEntry.AgentHandle = uefi.null
InfoEntry.ControllerHandle = 0x12345678 # just for example
InfoEntry.Attributes = 0x80000001
InfoEntry.OpenCount = 0
```
