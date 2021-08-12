/** @file
  Object definitions specific to EDK2/UEFI.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#ifndef __UEFI_PYTHON_OBJUEFI_H__
#define __UEFI_PYTHON_OBJUEFI_H__

#include <py/obj.h>

#include <Base.h>
#include <Pi/PiMultiPhase.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>

#define MP_OBJ_IS_MEM(arg)  (MP_OBJ_IS_TYPE(arg, &mp_type_mem)  ||\
                             MP_OBJ_IS_TYPE(arg, &mp_type_guid))

typedef struct _mp_obj_mem_t {
  mp_obj_base_t     base;

  void              *addr;
  union {
    mp_obj_dict_t   *fields;
    mp_obj_tuple_t  *args;
  };

  union {
    struct {
      mp_uint_t   bitoff    : 8;
      mp_uint_t   bitnum    : 8;
      mp_uint_t   readonly  : 1;
      mp_uint_t   hasvarg   : 1;
      mp_uint_t   overrw    : 1; // allow over-read/write
      mp_uint_t   page      : 1; // 0: pool, 1: page
    };
    mp_uint_t     typeattr;
  };

  const char      *typespec;
  mp_uint_t       typesize;
  mp_uint_t       size;
} mp_obj_mem_t;

typedef mp_obj_mem_t      mp_obj_handle_t;

typedef struct _mp_obj_mem_it_t {
    mp_obj_base_t     base;
    mp_obj_mem_t      *memobj;
    mp_uint_t         offset;
    mp_uint_t         cur;
} mp_obj_mem_it_t;

mp_obj_mem_t* efi_mem_new(void *buf, const char *typespec, mp_uint_t readonly, mp_uint_t pmem);
mp_obj_t efi_guid_new(EFI_GUID *guid);
mp_obj_t efi_handle_new(EFI_HANDLE handle);
mp_obj_t efi_buffer_new(void *buf, mp_uint_t len, mp_uint_t readonly);

extern const char *efi_status_msg[35];
extern const mp_obj_type_t mp_type_fptr;
extern const mp_obj_type_t mp_type_guid;
extern const mp_obj_type_t mp_type_handle;
extern const mp_obj_type_t mp_type_mem;
extern const mp_obj_type_t mp_type_efistatus;

#define RAISE_UEFI_EXCEPTION_ON_ERROR(Status)                 \
  if (EFI_ERROR((Status))) {                                  \
    if (((Status) & 0xFFFF) < ARRAY_SIZE(efi_status_msg)) {   \
      nlr_raise(mp_obj_new_exception_msg_varg(                \
                &mp_type_efistatus, "%s (%d)",                \
                efi_status_msg[(Status) & 0xFFFF],            \
                (Status) & 0xFFFF));                          \
    } else {                                                  \
      nlr_raise(mp_obj_new_exception_msg_varg(                \
                &mp_type_efistatus, "errno (%d)",             \
                (Status) & 0xFFFF));                          \
    }                                                         \
  } else if ((Status) != EFI_SUCCESS) {                       \
    mp_raise_OSError ((int)(Status));                         \
  }

typedef UINTN (EFIAPI *FUN_NOARG)(VOID);
typedef UINTN (EFIAPI *FUN_ARGS)(UINTN Arg1, ...);

#endif // __UEFI_PYTHON_OBJUEFI_H__
