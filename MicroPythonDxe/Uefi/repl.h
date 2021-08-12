/** @file
  Public definitions of UEFI version of REPL.

  Copyright (c) 2018, Intel Corporation. All rights reserved. <BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef   UEFI_REPL_H
#define   UEFI_REPL_H

#include <py/misc.h>

/**
  Initialize REPL.

  @param  Line    Pointer to a vstr_t holding current input text.

**/
VOID
ReplInit (
  vstr_t      *Line
  );

/**
  Initialize REPL.

  @retval  MP_PARSE_SINGLE_INPUT  If one line code input in REPL.
  @retval  MP_PARSE_FILE_INPUT    If multiple lines of code input in REPL.
  @retval  -1                     If REPL exits.

**/
INTN
ReplLoop (
  VOID
  );

#endif
