/** @file
  File FD definitions.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <py/obj.h>

#include <Base.h>
#include "Protocol/SimpleFileSystem.h"
#include <Protocol/ScriptEngineProtocol.h>
#include <Protocol/ScriptFileProtocol.h>

#ifndef __MICROPY_INCLUDED_UEFI_FILE_H__
#define __MICROPY_INCLUDED_UEFI_FILE_H__

typedef struct _mp_obj_fdfile_t {
  mp_obj_base_t             base;
  int                       fd;
  EFI_FILE_HANDLE           fh;
  EFI_SCRIPT_FILE_PROTOCOL  *sfp;
} mp_obj_fdfile_t;

extern const mp_obj_type_t mp_type_fileio;
extern const mp_obj_type_t mp_type_textio;

#endif // __MICROPY_INCLUDED_UEFI_FILE_H__
