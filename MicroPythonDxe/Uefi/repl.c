/** @file
  MicroPython REPL for UEFI/EDK-II.

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
#include <py/parse.h>
#include <py/repl.h>
#include <extmod/misc.h>
#include <genhdr/mpversion.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>

#if MICROPY_USE_READLINE == 0

#include "repl.h"

#ifndef CHAR_CTRL_D
#define CHAR_CTRL_D (4)
#endif

#ifndef CHAR_CTRL_E
#define CHAR_CTRL_E (5)
#endif

typedef enum {
  REPL_MODE_NORMAL = 0,
  REPL_MODE_CONTINUE,
  REPL_MODE_PASTE,
  REPL_MODES
} REPL_MODE;

STATIC struct _REPL_DATA {
  vstr_t      *Line;
  UINTN       OrigLineLength;
  UINTN       CursorPos;
  CHAR8       *Prompt[REPL_MODES];
  CHAR8       *History[MICROPY_REPL_HISTORY];
  UINTN       HistoryIndex;
} mReplData;

#define REPL_HIST_SIZE (MP_ARRAY_SIZE (mReplData.History))

STATIC
CHAR8*
ReplStringDup (
  CONST CHAR8 *OldString
  )
{
  UINTN       Length;
  CHAR8       *NewString;

  Length = AsciiStrLen(OldString);
  NewString = m_new_maybe(CHAR8, Length + 1);
  if (NewString == NULL) {
    return NULL;
  }

  CopyMem(NewString, OldString, Length + 1);
  return NewString;
}

VOID
ReplLineStrip (
  vstr_t  *Line
  )
{
  CHAR8   Char;

  while (Line->len > 0) {
    Char = Line->buf[Line->len - 1];
    if (Char == '\n' || Char == ' ' || Char == '\r' || Char == '\t') {
      Line->len--;
    } else {
      break;
    }
  }
}

VOID
ReplPrint (
  CONST CHAR8   *String,
  UINTN         Length
  )
{
  UINTN         Index;
  CONST CHAR8   *Buf;
  UINTN         PrintLength;

  Index       = 0;
  Buf         = String;
  PrintLength = 0;
  while (Index < Length) {
    if (String[Index] == '\n') {
      mp_hal_stdout_tx_strn(Buf, PrintLength);
      mp_hal_stdout_tx_str("\r\n");
      mp_hal_stdout_tx_str(mReplData.Prompt[REPL_MODE_CONTINUE]);

      Buf = String + Index + 1;
      PrintLength = 0;
    } else {
      ++PrintLength;
    }
    ++Index;
  }

  if (PrintLength > 0) {
    mp_hal_stdout_tx_strn(Buf, PrintLength);
  }
}

VOID
ReplClear (
  VOID
  )
{
  UINTN     Lines;
  UINTN     Index;
  UINTN     PromptLength;

  Lines = 0;
  Index = 0;
  while (Index < mReplData.Line->len) {
    if (mReplData.Line->buf[Index++] == '\n') {
      ++Lines;
    }
  }

  PromptLength = AsciiStrLen(mReplData.Prompt[REPL_MODE_NORMAL]);
  gST->ConOut->SetCursorPosition(
    gST->ConOut,
    PromptLength,
    (UINTN)gST->ConOut->Mode->CursorRow - Lines
    );

  Index = 0;
  while (Index < mReplData.Line->len) {
    gST->ConOut->OutputString(gST->ConOut, L" ");
    if (mReplData.Line->buf[Index++] == '\n') {
      gST->ConOut->OutputString(gST->ConOut, L"\r\n");
      for (UINTN Index2 = 0; Index2 < PromptLength; ++Index2) {
        gST->ConOut->OutputString(gST->ConOut, L" ");
      }
    }
  }

  gST->ConOut->SetCursorPosition(
    gST->ConOut,
    PromptLength,
    (UINTN)gST->ConOut->Mode->CursorRow - Lines
    );

  mReplData.CursorPos = 0;
  mReplData.OrigLineLength = 0;
  vstr_reset(mReplData.Line);
}

VOID
ReplPushHistory (
  CONST CHAR8 *Line
  )
{
  CHAR8   *History;
  UINTN   Last;

  Last = (REPL_HIST_SIZE + mReplData.HistoryIndex - 1) % REPL_HIST_SIZE;
  if (Line != NULL &&
      Line[0] != '\0' &&
      (mReplData.History[Last] == NULL ||
       AsciiStrCmp (mReplData.History[Last], Line) != 0)) {

    History = ReplStringDup (Line);
    if (History != NULL) {
      mReplData.History[mReplData.HistoryIndex] = History;
    }

    //
    // Make sure there's a NULL history after last one so that we can know
    // where to stop when traversing the history list with up/down key.
    //
    mReplData.HistoryIndex = (mReplData.HistoryIndex + 1) % REPL_HIST_SIZE;
    mReplData.History[mReplData.HistoryIndex] = NULL;
  }
}

VOID ReplReset (
  vstr_t      *Line
  )
{
  if (Line != NULL) {
    mReplData.Line = Line;
  }
  mReplData.OrigLineLength = mReplData.Line->len;
  mReplData.CursorPos = mReplData.OrigLineLength;
  mReplData.Prompt[REPL_MODE_NORMAL] = ">>> ";
  mReplData.Prompt[REPL_MODE_CONTINUE] = "... ";
  mReplData.Prompt[REPL_MODE_PASTE] = "=== ";

  mp_hal_stdout_tx_str(mReplData.Prompt[REPL_MODE_NORMAL]);
}

/**
  Initialize REPL.

  @param  Line    Pointer to a vstr_t holding current input text.

**/
VOID
ReplInit (
  vstr_t      *Line
  )
{
  mp_hal_stdout_tx_str(
    "MicroPython " MICROPY_GIT_TAG " on " MICROPY_BUILD_DATE "; "
    MICROPY_PY_SYS_PLATFORM
    " version\r\nUse Ctrl-D to exit, Ctrl-E for paste mode\r\n"
    );

  SetMem(mReplData.History, REPL_HIST_SIZE * sizeof(CONST CHAR8 *), 0);
  mReplData.HistoryIndex = 0;
  mReplData.Line = Line;
}

/**
  Initialize REPL.

  @retval  MP_PARSE_SINGLE_INPUT  If one line code input in REPL.
  @retval  MP_PARSE_FILE_INPUT    If multiple lines of code input in REPL.
  @retval  -1                     If REPL exits.

**/
INTN
ReplLoop (
  VOID
  )
{
  UINTN                   EventIndex;
  EFI_STATUS              Status;
  EFI_INPUT_KEY           Key;
  REPL_MODE               ReplMode;
  INTN                    RedrawStepBack;
  BOOLEAN                 RedrawFromCursor;
  INTN                    RedrawStepForward;
  UINTN                   OldLineLength;
  UINTN                   HistoryCursor;
  CONST CHAR8             *CompleteString;
  UINTN                   CompleteLength;
  UINTN                   Index;

  ReplReset(NULL);

  ReplMode = REPL_MODE_NORMAL;
  HistoryCursor = mReplData.HistoryIndex;

  for (;;) {
    OldLineLength = mReplData.Line->len;
    RedrawStepBack = 0;
    RedrawFromCursor = FALSE;
    RedrawStepForward = 0;

    gBS->WaitForEvent(1, &gST->ConIn->WaitForKey, &EventIndex);
    Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    ASSERT_EFI_ERROR(Status);

    switch (Key.ScanCode) {
    case SCAN_NULL:
      switch (Key.UnicodeChar) {
      case CHAR_CTRL_E:
        mp_hal_stdout_tx_str("\r\nPaste mode; Ctrl-C to cancel, Ctrl-D to finish\r\n");
        mp_hal_stdout_tx_str(mReplData.Prompt[REPL_MODE_PASTE]);
        vstr_reset(mReplData.Line);
        ReplMode = REPL_MODE_PASTE;
        break;

      case CHAR_CTRL_D:
        if (ReplMode == REPL_MODE_NORMAL) {
          return -1;
        } else if (ReplMode == REPL_MODE_CONTINUE) {
          mp_hal_stdout_tx_str("\r\n");
          ReplLineStrip(mReplData.Line);
          ReplPushHistory(vstr_null_terminated_str(mReplData.Line) + mReplData.OrigLineLength);
          return MP_PARSE_SINGLE_INPUT;
        } else if (ReplMode == REPL_MODE_PASTE) {
          mp_hal_stdout_tx_str("\r\n");
          return MP_PARSE_FILE_INPUT;
        }
        break;

      case '\r':
      case '\n':
        mp_hal_stdout_tx_str("\r\n");
        if (ReplMode == REPL_MODE_NORMAL) {
          if (mp_repl_continue_with_input(vstr_null_terminated_str(mReplData.Line))) {
            ReplMode = REPL_MODE_CONTINUE;
          } else {
            ReplPushHistory(vstr_null_terminated_str(mReplData.Line) + mReplData.OrigLineLength);
            return MP_PARSE_SINGLE_INPUT;
          }
        }

        if (ReplMode == REPL_MODE_CONTINUE) {
          //
          // End the input if there're two continuous blank lines.
          //
          if (mReplData.Line->len >= 2 &&
              mReplData.Line->buf[mReplData.Line->len - 1] == '\n' &&
              mReplData.Line->buf[mReplData.Line->len - 2] == '\n') {
            ReplLineStrip(mReplData.Line);
            ReplPushHistory(vstr_null_terminated_str(mReplData.Line) + mReplData.OrigLineLength);
            return MP_PARSE_SINGLE_INPUT;
          }

          vstr_ins_char (mReplData.Line, mReplData.CursorPos++, '\n');
          mp_hal_stdout_tx_str (mReplData.Prompt[REPL_MODE_CONTINUE]);
          mReplData.OrigLineLength = mReplData.Line->len;

        } else if (ReplMode == REPL_MODE_PASTE) {

          vstr_ins_char(mReplData.Line, mReplData.CursorPos++, '\n');
          mp_hal_stdout_tx_str (mReplData.Prompt[REPL_MODE_PASTE]);
          mReplData.OrigLineLength = mReplData.Line->len;

        }
        break;

      case CHAR_BACKSPACE:
        if (mReplData.CursorPos > mReplData.OrigLineLength &&
            mReplData.Line->buf[mReplData.CursorPos - 1] != '\n') {
          vstr_cut_out_bytes(mReplData.Line, mReplData.CursorPos - 1, 1);
          // set redraw parameters
          RedrawStepBack = 1;
          RedrawFromCursor = TRUE;
        }
        break;

      case CHAR_TAB:
        if (ReplMode != REPL_MODE_PASTE) {
          CompleteLength = mp_repl_autocomplete (
                             mReplData.Line->buf + mReplData.OrigLineLength,
                             mReplData.CursorPos - mReplData.OrigLineLength,
                             &mp_plat_print,
                             &CompleteString
                             );

          if (CompleteLength == 0) {
            // no match
          } else if (CompleteLength == (UINTN)(-1)) {
            // many matches
            mp_hal_stdout_tx_str (mReplData.Prompt[REPL_MODE_NORMAL]);
            ReplPrint (mReplData.Line->buf + mReplData.OrigLineLength,
                       mReplData.CursorPos - mReplData.OrigLineLength);
            RedrawFromCursor = TRUE;
          } else {
            // one match
            for (Index = 0; Index < CompleteLength; ++Index) {
              vstr_ins_byte (mReplData.Line, mReplData.CursorPos + Index, *CompleteString++);
            }
            // set redraw parameters
            RedrawFromCursor = TRUE;
            RedrawStepForward = CompleteLength;
          }
        }
        break;

      default:
        // printable character
        vstr_ins_char(mReplData.Line, mReplData.CursorPos, (CHAR8)(Key.UnicodeChar));
        // set redraw parameters
        RedrawFromCursor = TRUE;
        RedrawStepForward = 1;
        break;
      }
      break;

    case SCAN_UP:
      if (ReplMode == REPL_MODE_NORMAL &&
          mReplData.History[(REPL_HIST_SIZE + HistoryCursor - 1) % REPL_HIST_SIZE] != NULL) {

        if (mReplData.Line->len > 0) {
          ReplClear();
        }

        // set line to history
        HistoryCursor = (REPL_HIST_SIZE + HistoryCursor - 1) % REPL_HIST_SIZE;
        mReplData.Line->len = mReplData.OrigLineLength;
        vstr_add_str(mReplData.Line, mReplData.History[HistoryCursor]);

        // set redraw parameters
        RedrawStepBack = mReplData.CursorPos - mReplData.OrigLineLength;
        RedrawFromCursor = TRUE;
        RedrawStepForward = mReplData.Line->len - mReplData.OrigLineLength;

      }
      break;

    case SCAN_DOWN:
      if (ReplMode == REPL_MODE_NORMAL &&
          mReplData.History[(HistoryCursor + 1) % REPL_HIST_SIZE] != NULL) {

        if (mReplData.Line->len > 0) {
          ReplClear();
        }

        // set line to history
        HistoryCursor = (HistoryCursor + 1) % REPL_HIST_SIZE;
        vstr_cut_tail_bytes(mReplData.Line, mReplData.Line->len - mReplData.OrigLineLength);
        vstr_add_str(mReplData.Line, mReplData.History[HistoryCursor]);

        // set redraw parameters
        RedrawStepBack = mReplData.CursorPos - mReplData.OrigLineLength;
        RedrawFromCursor = TRUE;
        RedrawStepForward = mReplData.Line->len - mReplData.OrigLineLength;

      }
      break;

    case SCAN_RIGHT:
      if (mReplData.CursorPos < mReplData.Line->len) {
        RedrawStepForward = 1;
      }
      break;

    case SCAN_LEFT:
      if (mReplData.CursorPos > mReplData.OrigLineLength) {
        RedrawStepBack = 1;
      }
      break;

    case SCAN_END:
      RedrawStepForward = mReplData.Line->len - mReplData.CursorPos;
      break;

    case SCAN_HOME:
      RedrawStepBack = mReplData.CursorPos - mReplData.OrigLineLength;
      break;

    case SCAN_DELETE:
      if (mReplData.CursorPos < mReplData.Line->len) {
        vstr_cut_out_bytes(mReplData.Line, mReplData.CursorPos, 1);
        RedrawFromCursor = TRUE;
      }
      break;

    case SCAN_ESC:
      if (ReplMode == REPL_MODE_PASTE) {
        vstr_reset(mReplData.Line);
        mp_hal_stdout_tx_str("\r\n");
        return MP_PARSE_SINGLE_INPUT;
      } else {
        ReplClear();
      }
      ReplMode = REPL_MODE_NORMAL;
      break;

    default:
      break;
    }

    if (RedrawStepBack > 0) {
      mp_hal_move_cursor_back(RedrawStepBack);
      mReplData.CursorPos -= RedrawStepBack;
    }

    if (RedrawFromCursor) {
      if (mReplData.Line->len < OldLineLength) {
        // erase old chars
        mp_hal_erase_line_from_cursor(OldLineLength - mReplData.CursorPos);
      }
      // draw new chars
      //mp_hal_stdout_tx_strn(mReplData.Line->buf + mReplData.CursorPos, mReplData.Line->len - mReplData.CursorPos);
      ReplPrint(mReplData.Line->buf + mReplData.CursorPos, mReplData.Line->len - mReplData.CursorPos);
      // move cursor forward if needed (already moved forward by length of line, so move it back)
      mp_hal_move_cursor_back(mReplData.Line->len - (mReplData.CursorPos + RedrawStepForward));
      mReplData.CursorPos += RedrawStepForward;
    } else if (RedrawStepForward > 0) {
      // draw over old chars to move cursor forwards
      mp_hal_stdout_tx_strn(mReplData.Line->buf + mReplData.CursorPos, RedrawStepForward);
      mReplData.CursorPos += RedrawStepForward;
    }
  }
}

#endif

