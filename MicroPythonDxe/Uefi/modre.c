/** @file
  Oniguruma Regular Expression module for MicroPythhon.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Paul Sokolovsky
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

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <py/runtime.h>
#include <py/binary.h>
#include <py/objstr.h>
#include <py/stackctrl.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include "upy.h"
#include "onigurumaglue.h"

#if MICROPY_PY_URE

typedef struct _mp_obj_re_t {
  mp_obj_base_t base;
  regex_t       *re;
} mp_obj_re_t;

typedef struct _mp_obj_match_t {
  mp_obj_base_t       base;
  const mp_obj_type_t *str_type;
  const char          *start;
  OnigRegion          *caps;
} mp_obj_match_t;

typedef struct {
  const mp_obj_type_t *str_type;
  const char          *start;
  const char          *end;
  int                 max_split;
  mp_obj_t            result;
} split_data_t;

int
string_scan_callback (
  int           num,
  int           pos,
  OnigRegion    *region,
  void          *data
  )
{
  UINTN         index;
  CONST CHAR8   *match;
  split_data_t  *splitdata;

  splitdata = data;
  if (splitdata->max_split > 0 && num >= splitdata->max_split) {
    return 1;
  }

  index = (region->num_regs > 1) ? 1 : 0;
  for (; index < region->num_regs; ++index) {
    match = splitdata->start + region->beg[index];
    mp_obj_list_append(
      splitdata->result,
      mp_obj_new_str_of_type(
        splitdata->str_type,
        (const byte *)splitdata->end,
        match - splitdata->end
        )
      );

    splitdata->end = splitdata->start + region->end[index];
  }

  return 0;
}

STATIC void match_print (const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
  (void)kind;
  mp_obj_match_t *self = MP_OBJ_TO_PTR(self_in);
  mp_printf(print, "<match num=%d>", self->caps->num_regs);
}

STATIC mp_obj_t match_group (mp_obj_t self_in, mp_obj_t no_in)
{
  mp_obj_match_t  *self = MP_OBJ_TO_PTR(self_in);
  mp_int_t        no = mp_obj_get_int(no_in);

  if (no < 0 || no >= self->caps->num_regs) {
    nlr_raise(mp_obj_new_exception_arg1(&mp_type_IndexError, no_in));
  }

  return mp_obj_new_str_of_type(self->str_type,
                                (const byte *)self->start + self->caps->beg[no],
                                self->caps->end[no] - self->caps->beg[no]);
}
MP_DEFINE_CONST_FUN_OBJ_2 (uefi_match_group_obj, match_group);

STATIC mp_obj_t match_del (mp_obj_t self_in)
{
  mp_obj_match_t  *self = MP_OBJ_TO_PTR(self_in);

  DEBUG((DEBUG_INFO, "match_del()\r\n"));
  OnigurumaRegionFree(self->caps);

  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(uefi_match_del_obj, match_del);

STATIC const mp_rom_map_elem_t match_locals_dict_table[] = {
  { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&uefi_match_del_obj)},
  { MP_ROM_QSTR(MP_QSTR_group), MP_ROM_PTR(&uefi_match_group_obj)},
};
STATIC MP_DEFINE_CONST_DICT (match_locals_dict, match_locals_dict_table);

STATIC const mp_obj_type_t match_type = {
  {&mp_type_type },
  .name = MP_QSTR_match,
  .print = match_print,
  .locals_dict = (void *)&match_locals_dict,
};


STATIC void re_print (const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind)
{
  (void)kind;
  mp_obj_re_t *self = MP_OBJ_TO_PTR(self_in);
  mp_printf(print, "<re %p>", self);
}


STATIC mp_obj_t re_match (size_t n_args, const mp_obj_t *args)
{
  mp_obj_re_t     *self = MP_OBJ_TO_PTR(args[0]);
  const char      *string = mp_obj_str_get_str(args[1]);
  OnigRegion      *mregion;
  mp_obj_match_t  *mobj;

  mobj = (mp_obj_match_t *)&mp_const_none_obj;
  EFI_STATUS status = OnigurumaMatch (self->re, string, &mregion);
  if (!EFI_ERROR(status) && mregion->num_regs > 0) {
    mobj = m_new_obj_with_finaliser(mp_obj_match_t);
    if (mobj != NULL) {
      mobj->base.type = &match_type;
      mobj->start     = string;
      mobj->str_type  = mp_obj_get_type(args[1]);
      mobj->caps      = mregion;
    }
  }

  return MP_OBJ_FROM_PTR(mobj);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (uefi_re_match_obj, 2, 4, re_match);

STATIC mp_obj_t re_search (size_t n_args, const mp_obj_t *args)
{
  return re_match(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (uefi_re_search_obj, 2, 4, re_search);

STATIC mp_obj_t re_split (size_t n_args, const mp_obj_t *args)
{
  mp_obj_re_t         *self = MP_OBJ_TO_PTR(args[0]);
  size_t              len;
  split_data_t        splitdata;

  splitdata.str_type = mp_obj_get_type(args[1]);
  mp_obj_str_get_data(args[1], &len);
  const char *string = mp_obj_str_get_str(args[1]);

  int maxsplit = 0;
  if (n_args > 2) {
    maxsplit = mp_obj_get_int(args[2]);
  }

  splitdata.result = mp_obj_new_list(0, NULL);
  splitdata.start  = string;
  splitdata.end = splitdata.start;
  splitdata.max_split = maxsplit;

  OnigurumaSplit(
    self->re,
    string,
    string_scan_callback,
    &splitdata
    );

  mp_obj_list_append(
    splitdata.result,
    mp_obj_new_str_of_type(
      splitdata.str_type,
      (const byte *)splitdata.end,
      len - (splitdata.end - splitdata.start)
      )
    );

  return splitdata.result;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (uefi_re_split_obj, 2, 3, re_split);

STATIC mp_obj_t re_del (mp_obj_t self_in)
{
  mp_obj_re_t  *self = MP_OBJ_TO_PTR(self_in);

  DEBUG((DEBUG_INFO, "re_del()\r\n"));
  OnigurumaFree(self->re);

  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(uefi_re_del_obj, re_del);

STATIC const mp_rom_map_elem_t re_locals_dict_table[] = {
  { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&uefi_re_del_obj)},
  { MP_ROM_QSTR(MP_QSTR_match), MP_ROM_PTR(&uefi_re_match_obj)},
  { MP_ROM_QSTR(MP_QSTR_search), MP_ROM_PTR(&uefi_re_search_obj)},
  { MP_ROM_QSTR(MP_QSTR_split), MP_ROM_PTR(&uefi_re_split_obj)},
};

STATIC MP_DEFINE_CONST_DICT (re_locals_dict, re_locals_dict_table);

STATIC const mp_obj_type_t re_type = {
  {&mp_type_type },
  .name = MP_QSTR__re,
  .print = re_print,
  .locals_dict = (void *)&re_locals_dict,
};


STATIC mp_obj_t uefi_mod_re_compile (size_t n_args, const mp_obj_t *args)
{
  const char *re_str = mp_obj_str_get_str(args[0]);
  mp_obj_re_t *o = m_new_obj_with_finaliser(mp_obj_re_t);

  if (o != NULL) {
    o->base.type = &re_type;

    int flags = 0;
    if (n_args > 1) {
      flags = mp_obj_get_int(args[1]);
    }

    EFI_STATUS status = OnigurumaCompile(re_str, flags, &o->re);
    if (EFI_ERROR(status)) {
      mp_raise_msg(&mp_type_Exception, "Invalid regular expression");
    }
  }

  return MP_OBJ_FROM_PTR(o);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (uefi_mod_re_compile_obj, 1, 2, uefi_mod_re_compile);


STATIC mp_obj_t uefi_mod_re_match (size_t n_args, const mp_obj_t *args)
{
  regex_t               *regex;
  OnigRegion            *mregion;
  mp_obj_match_t        *mobj;

  const char *pattern = mp_obj_str_get_str(args[0]);
  const char *string = mp_obj_str_get_str(args[1]);

  mobj = (mp_obj_match_t *)&mp_const_none_obj;
  EFI_STATUS status = OnigurumaCompile (pattern, ONIG_OPTION_DEFAULT, &regex);
  if (!EFI_ERROR(status)) {
    OnigurumaMatch (regex, string, &mregion);
    if (mregion != NULL && mregion->num_regs > 0) {
      mobj = m_new_obj_with_finaliser(mp_obj_match_t);
      if (mobj != NULL) {
        mobj->base.type = &match_type;
        mobj->start     = string;
        mobj->str_type  = mp_obj_get_type(args[1]);
        mobj->caps      = mregion;
      }
    }

    OnigurumaFree(regex);
  }

  return MP_OBJ_FROM_PTR(mobj);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (uefi_mod_re_match_obj, 2, 4, uefi_mod_re_match);

STATIC mp_obj_t uefi_mod_re_search (size_t n_args, const mp_obj_t *args)
{
  return uefi_mod_re_match(n_args, args);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (uefi_mod_re_search_obj, 2, 4, uefi_mod_re_search);


STATIC mp_obj_t uefi_mod_re_split (size_t n_args, const mp_obj_t *args)
{
  size_t          len;
  split_data_t    splitdata;
  regex_t         *regex;
  const char      *pattern = mp_obj_str_get_str(args[0]);
  const char      *string = " ";
  int             maxsplit = 0;

  if (n_args > 1) {
    splitdata.str_type = mp_obj_get_type(args[1]);
    string = mp_obj_str_get_str(args[1]);
  } else {
    splitdata.str_type = (mp_obj_type_t*)&mp_type_str;
  }

  if (n_args > 2) {
    maxsplit = mp_obj_get_int(args[2]);
  }

  splitdata.result = mp_obj_new_list(0, NULL);
  EFI_STATUS status = OnigurumaCompile (pattern, ONIG_OPTION_DEFAULT, &regex);
  if (!EFI_ERROR(status)) {
    splitdata.start  = string;
    splitdata.end = splitdata.start;
    splitdata.max_split = maxsplit;

    OnigurumaSplit(
      regex,
      string,
      string_scan_callback,
      &splitdata
      );

    mp_obj_str_get_data(args[1], &len);
    mp_obj_list_append(
      splitdata.result,
      mp_obj_new_str_of_type(
        splitdata.str_type,
        (const byte *)splitdata.end,
        len - (splitdata.end - splitdata.start)
        )
      );

    OnigurumaFree (regex);
  }

  return splitdata.result;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (uefi_mod_re_split_obj, 2, 4, uefi_mod_re_split);

STATIC const mp_rom_map_elem_t mp_module_re_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR__re)},
  { MP_ROM_QSTR(MP_QSTR_compile),   MP_ROM_PTR(&uefi_mod_re_compile_obj) },
  { MP_ROM_QSTR(MP_QSTR_match),     MP_ROM_PTR(&uefi_mod_re_match_obj)},
  { MP_ROM_QSTR(MP_QSTR_search),    MP_ROM_PTR(&uefi_mod_re_search_obj)},
  { MP_ROM_QSTR(MP_QSTR_split),     MP_ROM_PTR(&uefi_mod_re_split_obj)},

  { MP_ROM_QSTR(MP_QSTR_A),           MP_ROM_INT(ONIG_OPTION_ASCII) },
  { MP_ROM_QSTR(MP_QSTR_ASCII),       MP_ROM_INT(ONIG_OPTION_ASCII) },
  { MP_ROM_QSTR(MP_QSTR_I),           MP_ROM_INT(ONIG_OPTION_IGNORECASE) },
  { MP_ROM_QSTR(MP_QSTR_IGNORECASE),  MP_ROM_INT(ONIG_OPTION_IGNORECASE) },
  { MP_ROM_QSTR(MP_QSTR_M),           MP_ROM_INT(ONIG_OPTION_MULTILINE) },
  { MP_ROM_QSTR(MP_QSTR_MULTILINE),   MP_ROM_INT(ONIG_OPTION_MULTILINE) },
};

STATIC MP_DEFINE_CONST_DICT (mp_module_re_globals, mp_module_re_globals_table);

const mp_obj_module_t mp_module__re = {
  .base = {&mp_type_module },
  .globals = (mp_obj_dict_t *)&mp_module_re_globals,
};

#endif //MICROPY_PY_URE

