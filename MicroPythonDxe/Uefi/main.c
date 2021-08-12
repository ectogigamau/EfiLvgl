/** @file
  Driver entries for MicroPython Engine on EDK2/UEFI.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>

#include <Protocol/ScriptEngineProtocol.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "upy.h"

int             errno = 0;

EFI_STATUS
EFIAPI
MicroPythonDriverUnload (
  IN EFI_HANDLE         ImageHandle
)
{
  EFI_STATUS      Status;

  DEBUG((DEBUG_INFO, "Unloading UpyDxe..."));

  Status = UseDeinit();

  DEBUG((DEBUG_INFO, "done\r\n"));

  return Status;
}

EFI_STATUS
EFIAPI
MicroPythonDriverInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
)
{
  EFI_STATUS                  Status;
  EFI_SCRIPT_ENGINE_PROTOCOL  *ScriptProtocol;

  Status = FindScriptProtocolByType(EFI_SCRIPT_ENGINE_TYPE_MICROPYTHON, &ScriptProtocol);
  if (Status == EFI_SUCCESS && ScriptProtocol != NULL) {
    return EFI_ALREADY_STARTED;
  }

  Status = UseInit(ImageHandle);

  return Status;
}

