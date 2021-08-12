/** @file
  Shell application for using MicroPython script engine.

  Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved. <BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _UPY_H_
#define _UPY_H_

#include <Uefi.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/ScriptEngineProtocol.h>
#include <Protocol/ScriptFileProtocol.h>
#include <Protocol/Shell.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Library/FileHandleLib.h>

#define FREE_NON_NULL(Pointer)        \
  do {                                \
    if ((Pointer) != NULL) {          \
      FreePool((Pointer));            \
      (Pointer) = NULL;               \
    }                                 \
  } while(FALSE)

#define MICROPYTHON_DRIVER_NAME     L"MicroPythonDxe.efi"

#endif

