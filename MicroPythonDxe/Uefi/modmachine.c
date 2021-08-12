/** @file
  Edk2 version machine module for MicroPythhon.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <stdio.h>
#include <stdint.h>

#include <py/runtime.h>
#include <py/obj.h>

#include <Library/IoLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/Smbios.h>
#include <Library/UefiBootServicesTableLib.h>

#include "objuefi.h"

#if MICROPY_PY_MACHINE

typedef struct _mp_obj_port_t {
  mp_obj_base_t   base;
  mp_uint_t       width;
} mp_obj_port_t;

STATIC void port_attr(mp_obj_t self_in, qstr attr_in, mp_obj_t *dest)
{
  mp_obj_port_t *self = MP_OBJ_TO_PTR(self_in);

  switch (attr_in) {
  case MP_QSTR_WIDTH:
    if (dest[0] == MP_OBJ_NULL) {
      // Load
      dest[0] = mp_obj_new_int_from_uint(self->width);
    } else if (dest[0] == MP_OBJ_SENTINEL) {
      if (dest[1] == MP_OBJ_NULL) {
        // Delete
      } else {
        // Store
      }
    }
    break;

  default:
    nlr_raise(mp_obj_new_exception_msg_varg(
                &mp_type_AttributeError,
                "Non-existing attribute: %s",
                qstr_str(attr_in)
                ));
  }
}

STATIC mp_obj_t port_subscr(mp_obj_t self_in, mp_obj_t index, mp_obj_t value)
{
  mp_obj_port_t  *self = MP_OBJ_TO_PTR(self_in);
  mp_uint_t      val;
  mp_int_t       index_val;

  if (MP_OBJ_IS_SMALL_INT(index)) {
    index_val = MP_OBJ_SMALL_INT_VALUE(index);
  } else if (!mp_obj_get_int_maybe(index, &index_val)) {
    nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_TypeError,
                                            "%q number must be integers, not %s",
                                            self->base.type->name, mp_obj_get_type_str(index)));
  }

  if (value == MP_OBJ_NULL) {
    //
    // delete
    //
    return mp_const_none;
  } else if (value == MP_OBJ_SENTINEL) {
    //
    // load
    //
    switch (self->width) {
    case 8:
      val = (mp_uint_t)IoRead8(index_val);
      break;

    case 16:
      val = (mp_uint_t)IoRead16(index_val);
      break;

    case 32:
      val = (mp_uint_t)IoRead32(index_val);
      break;

    default:
      return mp_const_none;
    }
    return mp_obj_new_int_from_uint(val);

  } else {
    //
    // store
    //
    switch (self->width) {
    case 8:
      val = (mp_uint_t)IoWrite8(index_val, (UINT8)mp_obj_get_int(value));
      break;

    case 16:
      val = (mp_uint_t)IoWrite16(index_val, (UINT16)mp_obj_get_int(value));
      break;

    case 32:
      val = (mp_uint_t)IoWrite32(index_val, (UINT32)mp_obj_get_int(value));
      break;

    }
    return mp_const_none;
  }
}

const mp_obj_type_t mp_type_port;

mp_obj_t mp_obj_new_port(mp_uint_t n)
{
  mp_obj_port_t *o = m_new_obj(mp_obj_port_t);

  o->base.type = &mp_type_port;
  if (n <= 8) {
    n = 8;
  } else if (n <= 16) {
    n = 16;
  } else {
    n = 32;
  }
  o->width = n;
  return MP_OBJ_FROM_PTR(o);
}

STATIC mp_obj_t port_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args)
{
  (void)type_in;
  mp_arg_check_num(n_args, n_kw, 0, 1, false);

  switch (n_args) {
  case 0:
    // return a new 8-bit port object
    return mp_obj_new_port(8);
  case 1:
  default:
    {
      return mp_obj_new_port(mp_obj_get_int(args[0]));
    }
  }
}

STATIC void port_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind)
{
  mp_obj_port_t *o = MP_OBJ_TO_PTR(o_in);

  if (!(MICROPY_PY_UJSON && kind == PRINT_JSON)) {
    kind = PRINT_REPR;
  }
  mp_printf(print, "%d-bit port", o->width);
}

const mp_obj_type_t mp_type_port = {
  { &mp_type_type },
  .name = MP_QSTR_port,
  .print = port_print,
  .make_new = port_make_new,
  .subscr = port_subscr,
  .attr = port_attr,
};

const mp_obj_port_t mp_const_port8_obj = { { &mp_type_port }, 8 };
const mp_obj_port_t mp_const_port16_obj = { { &mp_type_port }, 16 };
const mp_obj_port_t mp_const_port32_obj = { { &mp_type_port }, 32 };

const mp_obj_mem_t machine_mem8_obj = {
  .base = {&mp_type_mem},
  .addr = 0,
  .fields = NULL,
  .typeattr = 0,
  .typespec = "B",
  .typesize = 1,
  .size = 0,
};

const mp_obj_mem_t machine_mem16_obj = {
  .base = {&mp_type_mem},
  .addr = 0,
  .fields = NULL,
  .typeattr = 0,
  .typespec = "H",
  .typesize = 2,
  .size = 0,
};

const mp_obj_mem_t machine_mem32_obj = {
  .base = {&mp_type_mem},
  .addr = 0,
  .fields = NULL,
  .typeattr = 0,
  .typespec = "I",
  .typesize = 4,
  .size = 0,
};

const mp_obj_mem_t machine_mem64_obj = {
  .base = {&mp_type_mem},
  .addr = 0,
  .fields = NULL,
  .typeattr = 0,
  .typespec = "Q",
  .typesize = 8,
  .size = 0,
};


STATIC mp_obj_t mod_machine_reset(size_t n_args, const mp_obj_t *args) {
  gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_machine_reset_obj, 0, 1, mod_machine_reset);


STATIC mp_obj_t mod_machine_freq(size_t n_args, const mp_obj_t *args) {
  EFI_STATUS                Status;
  EFI_SMBIOS_PROTOCOL       *Smbios;
  EFI_SMBIOS_HANDLE         SmbiosHandle;
  EFI_SMBIOS_TYPE           RecordType;
  SMBIOS_TABLE_TYPE4        *Type4Record;

  //
  // Update Front Page banner strings base on SmBios Table.
  //
  Status = gBS->LocateProtocol(&gEfiSmbiosProtocolGuid, NULL,(VOID **)&Smbios);
  if (EFI_ERROR(Status)) {
    return mp_const_none;
  }

  SmbiosHandle  = SMBIOS_HANDLE_PI_RESERVED;
  RecordType    = EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION;
  Status = Smbios->GetNext(Smbios, &SmbiosHandle, &RecordType,
                           (EFI_SMBIOS_TABLE_HEADER **)&Type4Record, NULL);
  if (EFI_ERROR(Status)) {
    return mp_const_none;
  }

  return MP_OBJ_NEW_SMALL_INT(Type4Record->CurrentSpeed);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_machine_freq_obj, 0, 1, mod_machine_freq);


STATIC const mp_rom_map_elem_t machine_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_umachine) },

  { MP_ROM_QSTR(MP_QSTR_mem8), MP_ROM_PTR(&machine_mem8_obj) },
  { MP_ROM_QSTR(MP_QSTR_mem16), MP_ROM_PTR(&machine_mem16_obj) },
  { MP_ROM_QSTR(MP_QSTR_mem32), MP_ROM_PTR(&machine_mem32_obj) },
  { MP_ROM_QSTR(MP_QSTR_mem64), MP_ROM_PTR(&machine_mem64_obj) },

  { MP_ROM_QSTR(MP_QSTR_port), MP_ROM_PTR(&mp_type_port) },
  { MP_ROM_QSTR(MP_QSTR_port8), MP_ROM_PTR(&mp_const_port8_obj) },
  { MP_ROM_QSTR(MP_QSTR_port16), MP_ROM_PTR(&mp_const_port16_obj) },
  { MP_ROM_QSTR(MP_QSTR_port32), MP_ROM_PTR(&mp_const_port32_obj) },

  { MP_ROM_QSTR(MP_QSTR_reset),  MP_ROM_PTR(&mod_machine_reset_obj) },
  { MP_ROM_QSTR(MP_QSTR_freq),  MP_ROM_PTR(&mod_machine_freq_obj) },
};

STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t mp_module_machine = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t*)&machine_module_globals,
};

#endif // MICROPY_PY_MACHINE
