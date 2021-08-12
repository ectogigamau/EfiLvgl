/** @file
  Edk2 test intefaces for MicroPythhon.

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

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

#include <Protocol/Cpu.h>
#include <Protocol/VirtualConsole.h>

#include "objuefi.h"

STATIC EDKII_VIRTUAL_CONSOLE_PROTOCOL *mVirtualConsole = NULL;
STATIC EFI_CPU_ARCH_PROTOCOL          *mCpu = NULL;

extern mp_obj_t UpySuspend(mp_obj_t ms);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_ets_suspend_obj, UpySuspend);

typedef struct _mp_obj_register_t {
    mp_obj_base_t     base;
} mp_obj_register_t;

// mp_obj_t ch, mp_obj_t scan, mp_obj_t shift, mp_obj_t toggle
static mp_obj_t mod_ets_press_key(size_t n_args, const mp_obj_t *args)
{
  EFI_KEY_DATA                    KeyData;
  EFI_STATUS                      Status;
  const char                      *Input;
  size_t                          Length;

  if (mVirtualConsole == NULL) {
    Status = gBS->LocateProtocol(&gEdkiiVirtualConsoleProtocolGuid, NULL, (VOID **)&mVirtualConsole);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  SetMem(&KeyData, sizeof(KeyData), 0);

  if (MP_OBJ_IS_STR_OR_BYTES(args[0])) {
    Input = mp_obj_str_get_data(args[0], &Length);
    if (Length > 1) {
      KeyData.Key.UnicodeChar = ((CHAR16 *)Input)[0];
    } else {
      KeyData.Key.UnicodeChar = Input[0];
    }
  } else if (MP_OBJ_IS_INT(args[0])) {
    KeyData.Key.UnicodeChar = mp_obj_get_int(args[0]);
  } else {
    KeyData.Key.UnicodeChar = CHAR_NULL;
  }


  if (n_args > 1) {
    KeyData.Key.ScanCode = mp_obj_get_int(args[1]);
  }

  if (n_args > 2) {
    KeyData.KeyState.KeyShiftState = mp_obj_get_int(args[2]);
    if (KeyData.KeyState.KeyShiftState) {
      KeyData.KeyState.KeyShiftState |= EFI_SHIFT_STATE_VALID;
    }
  }

  if (n_args > 3) {
    KeyData.KeyState.KeyToggleState = mp_obj_get_int(args[3]);
    if (KeyData.KeyState.KeyToggleState) {
      KeyData.KeyState.KeyToggleState |= EFI_TOGGLE_STATE_VALID;
    }
  }

  mVirtualConsole->InputKey(mVirtualConsole, &KeyData);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_ets_press_key_obj, 1, 4, mod_ets_press_key);

//typedef
//EFI_STATUS
//(EFIAPI *EDKII_VIRTUAL_CONSOLE_GET_SCREEN)(
//  IN     EDKII_VIRTUAL_CONSOLE_PROTOCOL        *This,
//  IN OUT VIRTUAL_CONSOLE_CHAR                  *ScreenBuffer,
//  IN OUT UINTN                                 *BufferSize,
//  IN     BOOLEAN                               IncludingHistory
//  );
static mp_obj_t mod_ets_get_screen(size_t n_args, const mp_obj_t *args)
{
  EFI_STATUS                      Status;
  VIRTUAL_CONSOLE_CHAR            *Buffer;
  VIRTUAL_CONSOLE_CHAR            *CharPtr;
  UINTN                           CharNum;
  BOOLEAN                         IncludeHistory;
  UINTN                           Index;
  mp_obj_tuple_t                  *ScreenCharAttr;
  mp_obj_tuple_t                  *ScreenSnapshot;
  CHAR8                           *ScreenChar;

  if (mVirtualConsole == NULL) {
    Status = gBS->LocateProtocol(&gEdkiiVirtualConsoleProtocolGuid, NULL, (VOID **)&mVirtualConsole);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  ScreenSnapshot = NULL;
  Buffer = NULL;
  CharNum = 0;
  IncludeHistory = (n_args == 1) ? (args[0] == mp_const_true) : FALSE;
  Status = mVirtualConsole->GetScreen(mVirtualConsole, Buffer, &CharNum, IncludeHistory);
  if (Status == EFI_BUFFER_TOO_SMALL && CharNum > 0) {
    Buffer = AllocatePool(CharNum * sizeof(VIRTUAL_CONSOLE_CHAR));
    ASSERT(Buffer != NULL);

    ScreenChar = AllocatePool((CharNum + 1) * sizeof(CHAR8));
    ASSERT(ScreenChar != NULL);

    Status = mVirtualConsole->GetScreen(mVirtualConsole, Buffer, &CharNum, IncludeHistory);
    if (!EFI_ERROR(Status)) {
      ScreenSnapshot = MP_OBJ_TO_PTR(mp_obj_new_tuple(2, NULL));
      ScreenCharAttr = MP_OBJ_TO_PTR(mp_obj_new_tuple(CharNum, NULL));

      CharPtr = Buffer;
      for (Index = 0; Index < CharNum; ++Index, ++CharPtr) {
        ScreenChar[Index] = (CHAR8)CharPtr->Char;
        // use space to replace null-char or non-ascii-char
        if (ScreenChar[Index] == 0 || ScreenChar[Index] < 0) {
          ScreenChar[Index] = ' ';
        }

        ScreenCharAttr->items[Index] = mp_obj_new_int_from_uint(CharPtr->Attribute);
      }

      ScreenChar[Index] = '\0';
      ScreenSnapshot->items[0] = mp_obj_new_str_of_type(&mp_type_str, (const byte *)ScreenChar, CharNum);
      ScreenSnapshot->items[1] = MP_OBJ_FROM_PTR(ScreenCharAttr);
    }

    FreePool(Buffer);
    FreePool(ScreenChar);
  }

  return MP_OBJ_FROM_PTR(ScreenSnapshot);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_ets_snapshot_obj, 0, 1, mod_ets_get_screen);

static mp_obj_t mod_ets_clear_history(void)
{
  EFI_STATUS                      Status;

  if (mVirtualConsole == NULL) {
    Status = gBS->LocateProtocol(&gEdkiiVirtualConsoleProtocolGuid, NULL, (VOID **)&mVirtualConsole);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  mVirtualConsole->ClearHistory(mVirtualConsole);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_ets_clear_history_obj, mod_ets_clear_history);

static mp_obj_t mod_ets_debug(size_t n_args, const mp_obj_t *args)
{
  UINTN       Level;
  size_t      Length;
  CHAR8       *MsgType;

  if (n_args == 1) {
    Level = DEBUG_INFO;
  } else {
    Level = mp_obj_get_int(args[1]);
  }

  switch (Level) {
  case DEBUG_ERROR:
    MsgType = "ERROR";
    break;

  case DEBUG_WARN:
    MsgType = "WARN";
    break;

  case DEBUG_VERBOSE:
    MsgType = "VERBOSE";
    break;

  case DEBUG_INFO:
  default:
    MsgType = "INFO";
    break;
  }

  DEBUG((Level, "[ETS.%a] %a", MsgType, mp_obj_str_get_data(args[0], &Length)));

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_ets_debug_obj, 1, 2, mod_ets_debug);

STATIC
VOID
EFIAPI
MsrErrorHandler (
  IN CONST  EFI_EXCEPTION_TYPE  InterruptType,
  IN CONST  EFI_SYSTEM_CONTEXT  SystemContext
  )
{
  if (mCpu != NULL) {
    mCpu->RegisterInterruptHandler (mCpu, EXCEPT_IA32_GP_FAULT, NULL);
  }

  nlr_raise(mp_obj_new_exception_msg_varg(
              &mp_type_Exception,
              "Reserved or unimplemented MSR: 0x%X",
              (UINT32)SystemContext.SystemContextX64->Rcx
              ));
}

static mp_obj_t mod_ets_rdmsr(mp_obj_t msr)
{
  EFI_STATUS  Status;
  UINT64      Value;

  if (mCpu == NULL) {
    Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&mCpu);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  if (mCpu != NULL) {
    Status = mCpu->RegisterInterruptHandler (mCpu, EXCEPT_IA32_GP_FAULT, MsrErrorHandler);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  Value = AsmReadMsr64((UINT32)mp_obj_get_int (msr));
  if (mCpu != NULL) {
    Status = mCpu->RegisterInterruptHandler (mCpu, EXCEPT_IA32_GP_FAULT, NULL);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  return mp_obj_new_int_from_ull(Value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_ets_rdmsr_obj, mod_ets_rdmsr);

static mp_obj_t mod_ets_wrmsr(mp_obj_t msr, mp_obj_t value)
{
  EFI_STATUS  Status;

  if (mCpu == NULL) {
    Status = gBS->LocateProtocol (&gEfiCpuArchProtocolGuid, NULL, (VOID **)&mCpu);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  if (mCpu != NULL) {
    Status = mCpu->RegisterInterruptHandler (mCpu, EXCEPT_IA32_GP_FAULT, MsrErrorHandler);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  AsmWriteMsr64 ((UINT32)mp_obj_get_int (msr), (UINT64)mp_obj_get_int (value));
  if (mCpu != NULL) {
    Status = mCpu->RegisterInterruptHandler (mCpu, EXCEPT_IA32_GP_FAULT, NULL);
    RAISE_UEFI_EXCEPTION_ON_ERROR (Status);
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_ets_wrmsr_obj, mod_ets_wrmsr);

static mp_obj_t mod_ets_get_reg(qstr reg)
{
  UINT64            Value;
  UINTN             Limit;
  IA32_DESCRIPTOR   Desc;
  mp_obj_tuple_t    *TupleValue;

  Value = 0;
  Limit = 0;
  switch (reg) {
  case MP_QSTR_eflags:
    Value = AsmReadEflags();
    break;

  case MP_QSTR_cr0:
    Value = AsmReadCr0();
    break;

  case MP_QSTR_cr2:
    Value = AsmReadCr2();
    break;

  case MP_QSTR_cr3:
    Value = AsmReadCr3();
    break;

  case MP_QSTR_cr4:
    Value = AsmReadCr4();
    break;

  case MP_QSTR_dr0:
    Value = AsmReadDr0();
    break;

  case MP_QSTR_dr1:
    Value = AsmReadDr1();
    break;

  case MP_QSTR_dr2:
    Value = AsmReadDr2();
    break;

  case MP_QSTR_dr3:
    Value = AsmReadDr3();
    break;

  case MP_QSTR_dr4:
    Value = AsmReadDr4();
    break;

  case MP_QSTR_dr5:
    Value = AsmReadDr5();
    break;

  case MP_QSTR_dr6:
    Value = AsmReadDr6();
    break;

  case MP_QSTR_dr7:
    Value = AsmReadDr7();
    break;

  case MP_QSTR_cs:
    Value = AsmReadCs();
    break;

  case MP_QSTR_ds:
    Value = AsmReadDs();
    break;

  case MP_QSTR_es:
    Value = AsmReadEs();
    break;

  case MP_QSTR_fs:
    Value = AsmReadFs();
    break;

  case MP_QSTR_gs:
    Value = AsmReadGs();
    break;

  case MP_QSTR_ss:
    Value = AsmReadSs();
    break;

  case MP_QSTR_tr:
    Value = AsmReadTr();
    break;

  case MP_QSTR_gdtr:
    AsmReadGdtr(&Desc);
    Value = Desc.Base;
    Limit = Desc.Limit;
    break;

  case MP_QSTR_idtr:
    AsmReadIdtr(&Desc);
    Value = Desc.Base;
    Limit = Desc.Limit;
    break;

  case MP_QSTR_ldtr:
    Value = AsmReadLdtr();
    break;

  case MP_QSTR_mm0:
    Value = AsmReadMm0();
    break;

  case MP_QSTR_mm1:
    Value = AsmReadMm1();
    break;

  case MP_QSTR_mm2:
    Value = AsmReadMm2();
    break;

  case MP_QSTR_mm3:
    Value = AsmReadMm3();
    break;

  case MP_QSTR_mm4:
    Value = AsmReadMm4();
    break;

  case MP_QSTR_mm5:
    Value = AsmReadMm5();
    break;

  case MP_QSTR_mm6:
    Value = AsmReadMm6();
    break;

  case MP_QSTR_mm7:
    Value = AsmReadMm7();
    break;

  case MP_QSTR_tsc:
    Value = AsmReadTsc();
    break;

  case MP_QSTR_pmc:
    Value = AsmReadPmc(0);
    break;

  default:
    nlr_raise(mp_obj_new_exception_msg_varg(
                &mp_type_Exception,
                "Invalid or unimplemented register: %s",
                qstr_str (reg)
                ));
    break;
  }

  if (Limit > 0) {
    TupleValue = MP_OBJ_TO_PTR(mp_obj_new_tuple(2, NULL));
    TupleValue->items[0] = mp_obj_new_int_from_ull(Limit);
    TupleValue->items[1] = mp_obj_new_int_from_uint(Value);
    return TupleValue;
  }

  return mp_obj_new_int_from_ull(Value);
}

static void mod_ets_set_reg(qstr reg, mp_obj_t value)
{
  UINT64            Value;
  IA32_DESCRIPTOR   Desc;
  mp_obj_tuple_t    *TupleValue;
  mp_obj_list_t     *ListValue;

  Value       = (UINT64)-1;
  Desc.Limit  = (UINT16)-1;
  Desc.Base   = (UINTN)-1;

  if (MP_OBJ_IS_INT(value)) {
    Value = mp_obj_get_int(value);
  } else if (MP_OBJ_IS_TYPE(value, &mp_type_tuple)) {
    TupleValue = MP_OBJ_TO_PTR(value);
    ASSERT (TupleValue->len >= 2);
    Desc.Limit = mp_obj_get_int(TupleValue->items[0]);
    Desc.Base = mp_obj_get_int(TupleValue->items[1]);
  } else if (MP_OBJ_IS_TYPE(value, &mp_type_list)) {
    ListValue = MP_OBJ_TO_PTR(value);
    ASSERT (ListValue->len >= 2);
    Desc.Limit = mp_obj_get_int(ListValue->items[0]);
    Desc.Base = mp_obj_get_int(ListValue->items[1]);
  } else {
    nlr_raise(mp_obj_new_exception_msg(
                &mp_type_Exception,
                "Unsupported value type"
                ));
  }

  switch (reg) {
  case MP_QSTR_cr0:
    AsmWriteCr0((UINTN)Value);
    break;

  case MP_QSTR_cr2:
    AsmWriteCr2((UINTN)Value);
    break;

  case MP_QSTR_cr3:
    AsmWriteCr3((UINTN)Value);
    break;

  case MP_QSTR_cr4:
    AsmWriteCr4((UINTN)Value);
    break;

  case MP_QSTR_dr0:
    AsmWriteDr0((UINTN)Value);
    break;

  case MP_QSTR_dr1:
    AsmWriteDr1((UINTN)Value);
    break;

  case MP_QSTR_dr2:
    AsmWriteDr2((UINTN)Value);
    break;

  case MP_QSTR_dr3:
    AsmWriteDr3((UINTN)Value);
    break;

  case MP_QSTR_dr4:
    AsmWriteDr4((UINTN)Value);
    break;

  case MP_QSTR_dr5:
    AsmWriteDr5((UINTN)Value);
    break;

  case MP_QSTR_dr6:
    AsmWriteDr6((UINTN)Value);
    break;

  case MP_QSTR_dr7:
    AsmWriteDr7((UINTN)Value);
    break;

  case MP_QSTR_tr:
    AsmWriteTr((UINT16)Value);
    break;

  case MP_QSTR_gdtr:
    AsmWriteGdtr(&Desc);
    break;

  case MP_QSTR_idtr:
    AsmWriteIdtr(&Desc);
    break;

  case MP_QSTR_ldtr:
    AsmWriteLdtr((UINT16)Value);
    break;

  case MP_QSTR_mm0:
    AsmWriteMm0(Value);
    break;

  case MP_QSTR_mm1:
    AsmWriteMm1(Value);
    break;

  case MP_QSTR_mm2:
    AsmWriteMm2(Value);
    break;

  case MP_QSTR_mm3:
    AsmWriteMm3(Value);
    break;

  case MP_QSTR_mm4:
    AsmWriteMm4(Value);
    break;

  case MP_QSTR_mm5:
    AsmWriteMm5(Value);
    break;

  case MP_QSTR_mm6:
    AsmWriteMm6(Value);
    break;

  case MP_QSTR_mm7:
    AsmWriteMm7(Value);
    break;

  default:
    nlr_raise(mp_obj_new_exception_msg_varg(
                &mp_type_Exception,
                "Invalid or unimplemented register: %s",
                qstr_str (reg)
                ));
    break;
  }
}

static mp_obj_t mod_ets_cpuid(size_t n_args, const mp_obj_t *args)
{
  UINT32          Reg1;
  UINT32          Reg2;
  UINT32          Eax;
  UINT32          Ebx;
  UINT32          Ecx;
  UINT32          Edx;
  mp_obj_tuple_t  *Results;

  Reg1 = 0;
  Reg2 = 0;

  if (n_args > 0) {
    Reg1 = mp_obj_get_int(args[0]);
  }

  if (n_args > 1) {
    Reg2 = mp_obj_get_int(args[1]);
  }

  Eax = Reg1;
  Ebx = 0;
  Ecx = Reg2;
  Edx = 0;
  AsmCpuid(Eax, &Eax, &Ebx, &Ecx, &Edx);

  if ((Eax | Ebx | Ecx | Edx) == 0) {
    nlr_raise(mp_obj_new_exception_msg_varg(
                &mp_type_Exception,
                "Invalid cpuid: EAX=%02XH (ECX=%d)",
                Reg1, Reg2
                ));
  }

  Results = MP_OBJ_TO_PTR (mp_obj_new_tuple (4, NULL));
  Results->items[0] = mp_obj_new_int_from_uint (Eax);
  Results->items[1] = mp_obj_new_int_from_uint (Ebx);
  Results->items[2] = mp_obj_new_int_from_uint (Ecx);
  Results->items[3] = mp_obj_new_int_from_uint (Edx);

  return MP_OBJ_FROM_PTR (Results);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (mod_ets_cpuid_obj, 0, 2, mod_ets_cpuid);

void mod_ets_register_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
  if (dest[0] == MP_OBJ_NULL) {
    dest[0] = mod_ets_get_reg(attr);
  } else {
    mod_ets_set_reg(attr, dest[1]);
    dest[0] = MP_OBJ_NULL;  // means sucess to store
  }
}

const mp_obj_type_t mp_type_reg = {
  { &mp_type_type },
  .name = MP_QSTR_REGISTER,
  .attr = mod_ets_register_attr,
};

const mp_obj_type_t mp_const_register_obj = {{ &mp_type_reg }};

////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
STATIC const mp_rom_map_elem_t _ets_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__ets) },
  { MP_ROM_QSTR(MP_QSTR_suspend), MP_ROM_PTR(&mod_ets_suspend_obj) },
  { MP_ROM_QSTR(MP_QSTR_press), MP_ROM_PTR(&mod_ets_press_key_obj) },
  { MP_ROM_QSTR(MP_QSTR_snapshot), MP_ROM_PTR(&mod_ets_snapshot_obj) },
  { MP_ROM_QSTR(MP_QSTR_clear_history), MP_ROM_PTR(&mod_ets_clear_history_obj) },
  { MP_ROM_QSTR(MP_QSTR_debug), MP_ROM_PTR(&mod_ets_debug_obj) },
  { MP_ROM_QSTR(MP_QSTR_rdmsr), MP_ROM_PTR(&mod_ets_rdmsr_obj) },
  { MP_ROM_QSTR(MP_QSTR_wrmsr), MP_ROM_PTR(&mod_ets_wrmsr_obj) },
  { MP_ROM_QSTR(MP_QSTR_regs), MP_ROM_PTR(&mp_const_register_obj) },
  { MP_ROM_QSTR(MP_QSTR_cpuid), MP_ROM_PTR(&mod_ets_cpuid_obj) },
};
STATIC MP_DEFINE_CONST_DICT(_ets_module_globals, _ets_module_globals_table);

const mp_obj_module_t mp_module__ets = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t *)&_ets_module_globals,
};

