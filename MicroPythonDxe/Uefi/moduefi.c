/** @file
  _uefi module for MicroPython.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <py/mpconfig.h>
#include <py/nlr.h>
#include <py/runtime.h>
#include <py/objtuple.h>
#include <py/objstr.h>
#include <extmod/misc.h>
#include <py/obj.h>
#include <py/objarray.h>
#include <py/objexcept.h>
#include <py/objint.h>
#include <py/objfun.h>

#include <Uefi/UefiSpec.h>
#include <Pi/PiDxeCis.h>

#include <Protocol/ScriptFileProtocol.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

#include "objuefi.h"
#include "uefi_mphal.h"
#include "upy.h"

STATIC mp_obj_t moduefi_listfs(void) {
  EFI_STATUS                  status;
  EFI_SCRIPT_FILE_PROTOCOL    *sfp;
  CHAR16                      **fst;
  CHAR8                       *fs;
  UINTN                       len;
  UINTN                       fsn;
  UINTN                       index;
  mp_obj_tuple_t              *fslist;

  fslist = (mp_obj_tuple_t *)&mp_const_none_obj;
  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (!EFI_ERROR(status)) {
    status = sfp->GetFileSystemTable (sfp, &fst, &fsn);
    if (!EFI_ERROR(status) && fst != NULL && fsn > 0) {
      fslist = MP_OBJ_TO_PTR(mp_obj_new_tuple(fsn, NULL));
      for (index = 0; index < fsn; ++index) {
        len = StrLen(fst[index]);
        fs = ToUpyString (fst[index], NULL, &len);
        fslist->items[index] = mp_obj_new_str(fs, len);
        FREE_NON_NULL(fs);
      }
    }
  }

  return MP_OBJ_FROM_PTR(fslist);
}
MP_DEFINE_CONST_FUN_OBJ_0(moduefi_listfs_obj, moduefi_listfs);

mp_obj_mem_t mp_gBS_obj = {
  .base = {&mp_type_mem},
  .addr = NULL,
  .fields = NULL,
  .typeattr = 0,
  .typespec = NULL,
  .typesize = sizeof(EFI_BOOT_SERVICES),
  .size = sizeof(EFI_BOOT_SERVICES),
};

mp_obj_mem_t mp_gRT_obj = {
  .base = {&mp_type_mem},
  .addr = NULL,
  .fields = NULL,
  .typeattr = 0,
  .typespec = NULL,
  .typesize = sizeof(EFI_RUNTIME_SERVICES),
  .size = sizeof(EFI_RUNTIME_SERVICES),
};

mp_obj_mem_t mp_gST_obj = {
  .base = {&mp_type_mem},
  .addr = NULL,
  .fields = NULL,
  .typeattr = 0,
  .typespec = NULL,
  .typesize = sizeof(EFI_SYSTEM_TABLE),
  .size = sizeof(EFI_SYSTEM_TABLE),
};

mp_obj_mem_t mp_gDS_obj = {
  .base = {&mp_type_mem},
  .addr = NULL,
  .fields = NULL,
  .typeattr = 0,
  .typespec = NULL,
  .typesize = sizeof(EFI_DXE_SERVICES),
  .size = sizeof(EFI_DXE_SERVICES),
};

mp_obj_mem_t mp_gThis_obj = {
  .base = {&mp_type_mem},
  .addr = NULL,
  .fields = NULL,
  .typeattr = 0,
  .typespec = "T",
  .typesize = sizeof(EFI_HANDLE),
  .size = sizeof(EFI_HANDLE),
};

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
extern const struct _mp_obj_mem_t  mp_nullptr_obj;
STATIC const mp_rom_map_elem_t uefi_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__uefi) },
  { MP_ROM_QSTR(MP_QSTR_mem), MP_ROM_PTR(&mp_type_mem) },
  { MP_ROM_QSTR(MP_QSTR_guid), MP_ROM_PTR(&mp_type_guid) },
  { MP_ROM_QSTR(MP_QSTR_null), MP_ROM_PTR(&mp_nullptr_obj)},
  { MP_ROM_QSTR(MP_QSTR_efistatus), MP_ROM_PTR(&mp_type_efistatus) },

  { MP_ROM_QSTR(MP_QSTR_listfs), MP_ROM_PTR(&moduefi_listfs_obj) },

#if defined(MDE_CPU_IA32)
  { MP_ROM_QSTR(MP_QSTR_arch), MP_ROM_QSTR(MP_QSTR_IA32) },
#elif defined(MDE_CPU_X64)
  { MP_ROM_QSTR(MP_QSTR_arch), MP_ROM_QSTR(MP_QSTR_X64) },
#endif

  { MP_ROM_QSTR(MP_QSTR_gBS), MP_ROM_PTR(&mp_gBS_obj) },
  { MP_ROM_QSTR(MP_QSTR_gRT), MP_ROM_PTR(&mp_gRT_obj) },
  { MP_ROM_QSTR(MP_QSTR_gST), MP_ROM_PTR(&mp_gST_obj) },
  { MP_ROM_QSTR(MP_QSTR_gDS), MP_ROM_PTR(&mp_gDS_obj) },
  { MP_ROM_QSTR(MP_QSTR_gThis), MP_ROM_PTR(&mp_gThis_obj) },

  // enum constants
  { MP_ROM_QSTR(MP_QSTR_AllHandles), MP_ROM_INT(AllHandles) },
  { MP_ROM_QSTR(MP_QSTR_ByRegisterNotify), MP_ROM_INT(ByRegisterNotify) },
  { MP_ROM_QSTR(MP_QSTR_ByProtocol), MP_ROM_INT(ByProtocol) },
  { MP_ROM_QSTR(MP_QSTR_AllocateAnyPages), MP_ROM_INT(AllocateAnyPages) },
  { MP_ROM_QSTR(MP_QSTR_AllocateMaxAddress), MP_ROM_INT(AllocateMaxAddress) },
  { MP_ROM_QSTR(MP_QSTR_AllocateAddress), MP_ROM_INT(AllocateAddress) },
  { MP_ROM_QSTR(MP_QSTR_TimerCancel), MP_ROM_INT(TimerCancel) },
  { MP_ROM_QSTR(MP_QSTR_TimerPeriodic), MP_ROM_INT(TimerPeriodic) },
  { MP_ROM_QSTR(MP_QSTR_TimerRelative), MP_ROM_INT(TimerRelative) },
  { MP_ROM_QSTR(MP_QSTR_EFI_NATIVE_INTERFACE), MP_ROM_INT(EFI_NATIVE_INTERFACE) },
  { MP_ROM_QSTR(MP_QSTR_EfiReservedMemoryType), MP_ROM_INT(EfiReservedMemoryType) },
  { MP_ROM_QSTR(MP_QSTR_EfiLoaderCode), MP_ROM_INT(EfiLoaderCode) },
  { MP_ROM_QSTR(MP_QSTR_EfiLoaderData), MP_ROM_INT(EfiLoaderData) },
  { MP_ROM_QSTR(MP_QSTR_EfiBootServicesCode), MP_ROM_INT(EfiBootServicesCode) },
  { MP_ROM_QSTR(MP_QSTR_EfiBootServicesData), MP_ROM_INT(EfiBootServicesData) },
  { MP_ROM_QSTR(MP_QSTR_EfiRuntimeServicesCode), MP_ROM_INT(EfiRuntimeServicesCode) },
  { MP_ROM_QSTR(MP_QSTR_EfiRuntimeServicesData), MP_ROM_INT(EfiRuntimeServicesData) },
  { MP_ROM_QSTR(MP_QSTR_EfiConventionalMemory), MP_ROM_INT(EfiConventionalMemory) },
  { MP_ROM_QSTR(MP_QSTR_EfiUnusableMemory), MP_ROM_INT(EfiUnusableMemory) },
  { MP_ROM_QSTR(MP_QSTR_EfiACPIReclaimMemory), MP_ROM_INT(EfiACPIReclaimMemory) },
  { MP_ROM_QSTR(MP_QSTR_EfiACPIMemoryNVS), MP_ROM_INT(EfiACPIMemoryNVS) },
  { MP_ROM_QSTR(MP_QSTR_EfiMemoryMappedIO), MP_ROM_INT(EfiMemoryMappedIO) },
  { MP_ROM_QSTR(MP_QSTR_EfiMemoryMappedIOPortSpace), MP_ROM_INT(EfiMemoryMappedIOPortSpace) },
  { MP_ROM_QSTR(MP_QSTR_EfiPalCode), MP_ROM_INT(EfiPalCode) },
  { MP_ROM_QSTR(MP_QSTR_EfiPersistentMemory), MP_ROM_INT(EfiPersistentMemory) },
  { MP_ROM_QSTR(MP_QSTR_EfiResetCold), MP_ROM_INT(EfiResetCold) },
  { MP_ROM_QSTR(MP_QSTR_EfiResetWarm), MP_ROM_INT(EfiResetWarm) },
  { MP_ROM_QSTR(MP_QSTR_EfiResetShutdown), MP_ROM_INT(EfiResetShutdown) },
  { MP_ROM_QSTR(MP_QSTR_EfiResetPlatformSpecific), MP_ROM_INT(EfiResetPlatformSpecific) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMemoryTypeNonExistent), MP_ROM_INT(EfiGcdMemoryTypeNonExistent) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMemoryTypeReserved), MP_ROM_INT(EfiGcdMemoryTypeReserved) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMemoryTypeSystemMemory), MP_ROM_INT(EfiGcdMemoryTypeSystemMemory) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMemoryTypeMemoryMappedIo), MP_ROM_INT(EfiGcdMemoryTypeMemoryMappedIo) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMemoryTypePersistentMemory), MP_ROM_INT(EfiGcdMemoryTypePersistentMemory) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMemoryTypeMoreReliable), MP_ROM_INT(EfiGcdMemoryTypeMoreReliable) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMemoryTypeMaximum), MP_ROM_INT(EfiGcdMemoryTypeMaximum) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdIoTypeNonExistent), MP_ROM_INT(EfiGcdIoTypeNonExistent) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdIoTypeReserved), MP_ROM_INT(EfiGcdIoTypeReserved) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdIoTypeIo), MP_ROM_INT(EfiGcdIoTypeIo) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdIoTypeMaximum), MP_ROM_INT(EfiGcdIoTypeMaximum) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdAllocateAnySearchBottomUp), MP_ROM_INT(EfiGcdAllocateAnySearchBottomUp) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdAllocateMaxAddressSearchBottomUp), MP_ROM_INT(EfiGcdAllocateMaxAddressSearchBottomUp) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdAllocateAddress), MP_ROM_INT(EfiGcdAllocateAddress) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdAllocateAnySearchTopDown), MP_ROM_INT(EfiGcdAllocateAnySearchTopDown) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdAllocateMaxAddressSearchTopDown), MP_ROM_INT(EfiGcdAllocateMaxAddressSearchTopDown) },
  { MP_ROM_QSTR(MP_QSTR_EfiGcdMaxAllocateType), MP_ROM_INT(EfiGcdMaxAllocateType) },
};
STATIC MP_DEFINE_CONST_DICT(uefi_module_globals, uefi_module_globals_table);

const mp_obj_module_t mp_module__uefi = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t *)&uefi_module_globals,
};

void moduefi_init(EFI_HANDLE ImageHandle)
{
  UINTN       Index;

  for (Index = 0; Index < gST->NumberOfTableEntries; Index++) {
    if (CompareGuid(&(gEfiDxeServicesTableGuid), &(gST->ConfigurationTable[Index].VendorGuid))) {
      mp_gDS_obj.addr = gST->ConfigurationTable[Index].VendorTable;
    }
  }

  mp_gST_obj.addr = gST;
  mp_gBS_obj.addr = gST->BootServices;
  mp_gRT_obj.addr = gST->RuntimeServices;
  mp_gThis_obj.addr = &gImageHandle;
  DEBUG((DEBUG_INFO, "moduefi_init(): ImageHandle=%x\r\n", ImageHandle));
}

void moduefi_deinit(void)
{
  DEBUG((DEBUG_INFO, "moduefi_deinit()\r\n"));
}

