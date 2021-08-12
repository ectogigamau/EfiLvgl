/** @file
  Configuration file for MicroPython on UEFI.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// options to control how Micro Python is built
#define MICROPY_NLR_SETJMP          (0)

#if   defined (_WIN64) || defined (__x86_64__)
#define MICROPY_EMIT_X64            (1)
#else
#define MICROPY_EMIT_X86            (1)
#endif

#define MICROPY_PY_IO_BUFFEREDWRITER              (1)
#define MICROPY_NO_ALLOCA                         (1)
#define MICROPY_ALLOC_PATH_MAX                    (PATH_MAX)
#define MICROPY_COMP_MODULE_CONST                 (1)
#define MICROPY_COMP_TRIPLE_TUPLE_ASSIGN          (1)
#define MICROPY_COMP_RETURN_IF_EXPR               (1)
#define MICROPY_ENABLE_GC                         (1)
#define MICROPY_ENABLE_FINALISER                  (1)
#define MICROPY_GCREGS_SETJMP                     (1)
#define MICROPY_STACK_CHECK                       (1)
#define MICROPY_MALLOC_USES_ALLOCATED_SIZE        (1)
#define MICROPY_MEM_STATS                         (1)
#define MICROPY_DEBUG_PRINTERS                    (0)
// Printing debug to stderr may give tests which
// check stdout a chance to pass, etc.
#define MICROPY_DEBUG_PRINTER_DEST                mp_stderr_print
#define MICROPY_HELPER_REPL                       (1)
#define MICROPY_REPL_HISTORY                      (64)
#define MICROPY_ENABLE_SOURCE_LINE                (1)
#define MICROPY_FLOAT_IMPL                        (MICROPY_FLOAT_IMPL_NONE)
#define MICROPY_LONGINT_IMPL                      (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_STREAMS_NON_BLOCK                 (1)
#define MICROPY_STREAMS_POSIX_API                 (1)
#define MICROPY_OPT_CACHE_MAP_LOOKUP_IN_BYTECODE  (1)
#define MICROPY_OPT_MPZ_BITWISE                   (1)
#define MICROPY_CAN_OVERRIDE_BUILTINS             (1)
#define MICROPY_ENABLE_DOC_STRING                 (1)

#define MICROPY_PY_FUNCTION_ATTRS                 (1)
#define MICROPY_PY_DESCRIPTORS                    (1)
#define MICROPY_PY_BUILTINS_STR_UNICODE           (1)
#define MICROPY_PY_BUILTINS_STR_CENTER            (1)
#define MICROPY_PY_BUILTINS_STR_PARTITION         (1)
#define MICROPY_PY_BUILTINS_STR_SPLITLINES        (1)
#define MICROPY_PY_BUILTINS_MEMORYVIEW            (1)
#define MICROPY_PY_BUILTINS_FROZENSET             (1)
#define MICROPY_PY_BUILTINS_COMPILE               (1)
#define MICROPY_PY_BUILTINS_NOTIMPLEMENTED        (1)
#define MICROPY_PY_BUILTINS_INPUT                 (1)
#define MICROPY_PY_BUILTINS_POW3                  (0)
#define MICROPY_PY_BUILTINS_HELP                  (1)
#define MICROPY_PY_BUILTINS_RANGE_BINOP           (1)
#define MICROPY_PY_BUILTINS_COMPLEX               (0)
#define MICROPY_PY_MICROPYTHON_MEM_INFO           (1)
#define MICROPY_PY_ALL_SPECIAL_METHODS            (1)
#define MICROPY_PY_REVERSE_SPECIAL_METHODS        (1)
#define MICROPY_PY_ARRAY_SLICE_ASSIGN             (1)
#define MICROPY_PY_BUILTINS_SLICE_ATTRS           (1)
#define MICROPY_PY_SYS_EXIT                       (1)
#define MICROPY_PY_SYS_PLATFORM                   "uefi"
#define MICROPY_PY_SYS_MAXSIZE                    (1)
#define MICROPY_PY_SYS_STDFILES                   (1)
#define MICROPY_PY_SYS_EXC_INFO                   (1)
#define MICROPY_PY_COLLECTIONS                    (1)
#define MICROPY_PY_COLLECTIONS_DEQUE              (1)
#define MICROPY_PY_COLLECTIONS_ORDEREDDICT        (1)
#define MICROPY_PY_COLLECTIONS_NAMEDTUPLE__ASDICT (1)
#define MICROPY_PY_MATH                           (0)
#define MICROPY_PY_CMATH                          (0)
#define MICROPY_PY_IO                             (1)
#define MICROPY_PY_IO_FILEIO                      (1)
#define MICROPY_PY_GC_COLLECT_RETVAL              (1)
#define MICROPY_PY_DELATTR_SETATTR                (1)
#define MICROPY_PY_ASYNC_AWAIT                    (0)

#define MICROPY_PY_UERRNO               (1)
#define MICROPY_PY_UZLIB                (1)
#define MICROPY_PY_UJSON                (1)
#define MICROPY_PY_URE                  (1)
#define MICROPY_PY_UHEAPQ               (1)
#define MICROPY_PY_UHASHLIB             (1)
#define MICROPY_PY_UBINASCII            (1)
#define MICROPY_PY_UBINASCII_CRC32      (1)
#define MICROPY_PY_URANDOM              (1)
#define MICROPY_PY_URANDOM_EXTRA_FUNCS  (1)
#define MICROPY_PY_MACHINE              (1)
#define MICROPY_PY_UTIME_MP_HAL         (1)

#define MICROPY_PY_UTIME                (1)

#define MICROPY_ERROR_REPORTING         (MICROPY_ERROR_REPORTING_DETAILED)
#define MICROPY_WARNINGS                (1)
#define MICROPY_ERROR_PRINTER           (&mp_stderr_print)
#define MICROPY_PY_STR_BYTES_CMP_WARN   (1)

extern const struct _mp_print_t mp_stderr_print;

#define MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF    (1)
#define MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE      (256)
#define MICROPY_KBD_EXCEPTION                     (1)


// Just assume Windows is little-endian - mingw32 gcc doesn't
// define standard endianness macros.
#define MP_ENDIANNESS_LITTLE (1)

#if _FILE_OFFSET_BITS == 64 && !defined(__LP64__)
typedef INT64 mp_off_t;
#else
typedef INTN  mp_off_t;
#endif

typedef void *machine_ptr_t; // must be of pointer size
typedef const void *machine_const_ptr_t; // must be of pointer size


#define MICROPY_PORT_BUILTINS \
    { MP_OBJ_NEW_QSTR(MP_QSTR_open), (mp_obj_t)&mp_builtin_open_obj }, \


extern const struct _mp_obj_module_t mp_module_time;
extern const struct _mp_obj_module_t mp_module_os;
extern const struct _mp_obj_module_t mp_module_ure;
extern const struct _mp_obj_module_t mp_module_machine;
extern const struct _mp_obj_module_t mp_module__uefi;
extern const struct _mp_obj_module_t mp_module__ets;
extern const struct _mp_obj_module_t mp_module__re;

#define MICROPY_PORT_BUILTIN_MODULES \
    { MP_OBJ_NEW_QSTR(MP_QSTR_uos), (mp_obj_t)&mp_module_os }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_array), (mp_obj_t)&mp_module_array }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_ure), (mp_obj_t)&mp_module_ure }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_umachine), (mp_obj_t)&mp_module_machine }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_utime), (mp_obj_t)&mp_module_time }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR__uefi), (mp_obj_t)&mp_module__uefi }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR__ets), (mp_obj_t)&mp_module__ets }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR__re), (mp_obj_t)&mp_module__re }, \


#define MICROPY_MPHALPORT_H         "uefi_mphal.h"
#define MP_STATE_PORT               MP_STATE_VM

#define MICROPY_PORT_ROOT_POINTERS \
    const char *readline_hist[50]; \


typedef INTN    mp_int_t;
typedef UINTN   mp_uint_t;
typedef UINTN   u_int;
typedef INTN    ssize_t;
typedef INT64   off_t;

#ifdef __GNUC__
#define MP_NOINLINE __attribute__((noinline))

#ifdef __x86_64__
#define MP_SSIZE_MAX                INT64_MAX
#else
#define MP_SSIZE_MAX                INT32_MAX
#endif

#endif

// MSVC specifics
#ifdef _MSC_VER
#if   defined (_WIN64)
#define __x86_64__
#else
#define __i386__
#endif

// Sanity check

#if ( _MSC_VER < 1800 )
    #error Can only build with Visual Studio 2013 toolset or above
#endif


// CL specific overrides from mpconfig
#undef  NORETURN
#define NORETURN
#define MP_NOINLINE                 __declspec(noinline)
#define MP_LIKELY(x)                (x)
#define MP_UNLIKELY(x)              (x)
#define MICROPY_PORT_CONSTANTS      { "dummy", 0 } //can't have zero-sized array
#ifdef _WIN64
#define MP_SSIZE_MAX                INT64_MAX
#else
#define MP_SSIZE_MAX                INT32_MAX
#endif

// CL specific definitions

#define restrict                    __restrict
#define inline                      __inline
#define alignof(t)                  __alignof(t)


// Put static/global variables in sections with a known name
// This used to be required for GC, not the case anymore but keep it as it makes the map file easier to inspect
// For this to work this header must be included by all sources, which is the case normally
#define MICROPY_PORT_DATASECTION "upydata"
#define MICROPY_PORT_BSSSECTION "upybss"
#pragma data_seg(MICROPY_PORT_DATASECTION)
#pragma bss_seg(MICROPY_PORT_BSSSECTION)


// System headers (needed e.g. for nlr.h)

#include <stddef.h> //for NULL
#include <Library/DebugLib.h> //for ASSERT
#include <Library/BaseLib.h>

#endif
