/** @file
  UEFI/EDK-II HAL layer prototypes.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <unistd.h>
#include <py/misc.h>

#ifndef   UEFI_MPHAL_H
#define   UEFI_MPHAL_H

#define MICROPY_HAL_HAS_VT100 (0)

void mp_hal_move_cursor_back(uint pos);
void mp_hal_move_cursor(uint pos);
void mp_hal_erase_line_from_cursor(uint n_chars_to_erase);
void mp_hal_set_interrupt_char(char c);
void mp_hal_stdio_mode_raw(void);
void mp_hal_stdio_mode_orig(void);

static inline void mp_hal_delay_ms(mp_uint_t ms) { usleep((ms) * 1000); }

#define RAISE_ERRNO(err_flag, error_val) \
    { if (err_flag) \
        { nlr_raise(mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(error_val))); } }

#define RAISE_EXCEPTION(excpt) nlr_raise(mp_obj_new_exception(&excpt))

#endif
