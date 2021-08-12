/** @file
  UEFI/EDK-II HAL layer.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <py/mpstate.h>
#include <py/mphal.h>
#include <py/runtime.h>
#include <extmod/misc.h>
#include <lib/mp-readline/readline.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>

#include "upy.h"

void mp_hal_set_interrupt_char(char c) {
  // configure terminal settings to (not) let ctrl-C through
}

int mp_hal_stdin_rx_chr(void) {
  unsigned char           c;
  UINTN                   EventIndex;
  EFI_STATUS              Status;
  EFI_INPUT_KEY           Key;

  gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
  Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
  ASSERT_EFI_ERROR(Status);

  switch (Key.ScanCode) {
  case SCAN_NULL:
    c = (CHAR8)(Key.UnicodeChar);
    break;

  case SCAN_UP:
    c = CHAR_CTRL_P;
    break;

  case SCAN_DOWN:
    c = CHAR_CTRL_N;
    break;

  case SCAN_RIGHT:
    c = CHAR_CTRL_F;
    break;

  case SCAN_LEFT:
    c = CHAR_CTRL_B;
    break;

  case SCAN_END:
    c = CHAR_CTRL_E;
    break;

  case SCAN_HOME:
    c = CHAR_CTRL_A;
    break;

  case SCAN_DELETE:
    c = CHAR_CTRL_D;
    break;

  case SCAN_ESC:
    c = CHAR_CTRL_C;
    break;

  default:
    c = (CHAR8)(Key.UnicodeChar);
    break;
  }

  return c;
}

void mp_hal_stdout_tx_strn(const char *str, size_t len) {
  CHAR16  *String;

  String = ToUefiString(str, NULL, (UINTN *)&len);
  gST->ConOut->OutputString (gST->ConOut, String);
  mp_uos_dupterm_tx_strn(str, len);

  FREE_NON_NULL(String);
}

// cooked is same as uncooked because the terminal does some postprocessing
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
  mp_hal_stdout_tx_strn(str, len);
}

void mp_hal_stdout_tx_str(const char *str) {
  mp_hal_stdout_tx_strn(str, strlen(str));
}

mp_uint_t mp_hal_ticks_ms(void) {
  return 0;
}

#include <Library/UefiBootServicesTableLib.h>
void mp_hal_move_cursor_back(uint pos) {
  int X = gST->ConOut->Mode->CursorColumn;

  if (pos > 0) {
    X -= (short)pos;
    if (X < 0) {
      X = 0;
    }
    gST->ConOut->SetCursorPosition(gST->ConOut, (unsigned int)X, (unsigned int)gST->ConOut->Mode->CursorRow);
  }
}

void mp_hal_erase_line_from_cursor(uint n_chars_to_erase) {
  int n_chars = n_chars_to_erase;

  while (n_chars > 0) {
    gST->ConOut->OutputString(gST->ConOut, L" ");
    n_chars -= 1;
  }
  if (n_chars == 0 && n_chars_to_erase != 0) {
    mp_hal_move_cursor_back(n_chars_to_erase);
  }
}

