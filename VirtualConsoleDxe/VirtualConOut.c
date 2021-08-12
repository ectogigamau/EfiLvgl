/** @file
  Implementation for EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL protocol.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
Copyright (C) 2016 Silicon Graphics, Inc. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "VirtualConsole.h"

CHAR16 mCrLfString[] = { CHAR_CARRIAGE_RETURN, CHAR_LINEFEED, CHAR_NULL };


/**
  Empty the screen history.

  @param  History           History list.

**/
VOID
VirtualConsoleClearHistory (
  LIST_ENTRY  *History
  )
{
  LIST_ENTRY                        *Link;
  VIRTUAL_CONSOLE_SCREEN_LINE       *Line;

  for (Link = GetFirstNode (History); !IsNull (History, Link);) {
    Line = BASE_CR (Link, VIRTUAL_CONSOLE_SCREEN_LINE, Link);
    Link = RemoveEntryList (Link);

    FreePool (Line->Line);
    FreePool (Line);
  }
}

/**
  Clear specified columns x raws on the virtual console screen.

  @param  Screen          Pointer to screen buffer.
  @param  Columns         Specify the number of columns to clear.
  @param  Rows            Specify the number of columns to clear.
  @param  Attribute       Screen attribute used for clear.

**/
VOID
VirtualConsoleClearScreen (
  VIRTUAL_CONSOLE_CHAR              **Screen,
  UINTN                             Columns,
  UINTN                             Rows,
  UINTN                             Attribute
  )
{

  UINTN                         Index;
  //
  // Clear the first row.
  //
  for (Index = 0; Index < Columns; Index++) {
    Screen[0][Index].Attribute = Attribute;
    Screen[0][Index].Char = CHAR_NULL;
  }
  //
  // Clone to the other rows.
  //
  for (Index = 1; Index < Rows; Index++) {
    CopyMem (Screen[Index], Screen[0], sizeof (VIRTUAL_CONSOLE_CHAR) * Columns);
  }
}

/**
  ConInReset the text output device hardware and optionally run diagnostics.

  Implements SIMPLE_TEXT_OUTPUT.ConInReset().
  If ExtendeVerification is TRUE, then perform dependent Graphics Console
  device reset, and set display mode to mode 0.
  If ExtendedVerification is FALSE, only set display mode to mode 0.

  @param  This                  Protocol instance pointer.
  @param  ExtendedVerification  Indicates that the driver may perform a more
                                exhaustive verification operation of the device
                                during reset.

  @retval EFI_SUCCESS          The text output device was reset.
  @retval EFI_DEVICE_ERROR     The text output device is not functioning correctly and
                               could not be reset.

**/
EFI_STATUS
EFIAPI
ConOutReset (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  BOOLEAN                          ExtendedVerification
  )
{
  EFI_STATUS    Status;
  Status = This->SetMode (This, 0);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = This->SetAttribute (This, EFI_TEXT_ATTR (This->Mode->Attribute & 0x0F, EFI_BACKGROUND_BLACK));
  return Status;
}


/**
  Write a Unicode string to the output device.

  Implements SIMPLE_TEXT_OUTPUT.OutputString().
  The Unicode string will be converted to Glyphs and will be
  sent to the Graphics Console.

  @param  This                    Protocol instance pointer.
  @param  WString                 The NULL-terminated Unicode string to be displayed
                                  on the output device(s). All output devices must
                                  also support the Unicode drawing defined in this file.

  @retval EFI_SUCCESS             The string was output to the device.
  @retval EFI_DEVICE_ERROR        The device reported an error while attempting to output
                                  the text.
  @retval EFI_UNSUPPORTED         The output device's mode is not currently in a
                                  defined text mode.
  @retval EFI_WARN_UNKNOWN_GLYPH  This warning code indicates that some of the
                                  characters in the Unicode string could not be
                                  rendered and were skipped.

**/
EFI_STATUS
EFIAPI
OutputString (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  CHAR16                           *WString
  )
{
  VIRTUAL_CONSOLE_DEV                  *Private;
  INTN                                  Mode;
  UINTN                                 MaxColumn;
  UINTN                                 MaxRow;
  EFI_STATUS                            Status;
  UINTN                                 Count;
  UINTN                                 Index;
  VIRTUAL_CONSOLE_SCREEN_LINE           *Line;
  INT32                                 OriginAttribute;

  if (This->Mode->Mode == -1) {
    //
    // If current mode is not valid, return error.
    //
    return EFI_UNSUPPORTED;
  }

  Status = EFI_SUCCESS;
  //
  // Current mode
  //
  Mode      = This->Mode->Mode;
  Private   = VIRTUAL_CONSOLE_DEV_FROM_CON_OUT (This);


  OriginAttribute = This->Mode->Attribute;
  EfiAcquireLock (&Private->TxtOutLock);

  MaxColumn = Private->SimpleTextOutputModeData[Mode].Columns;
  MaxRow    = Private->SimpleTextOutputModeData[Mode].Rows;

  while (*WString != L'\0') {

    if (*WString == CHAR_BACKSPACE) {
      //
      // If the cursor is at the left edge of the display, then move the cursor
      // one row up.
      //
      if (This->Mode->CursorColumn == 0 && This->Mode->CursorRow > 0) {
        This->Mode->CursorRow--;
        This->Mode->CursorColumn = (INT32) (MaxColumn - 1);
      } else if (This->Mode->CursorColumn > 0) {
        //
        // If the cursor is not at the left edge of the display, then move the cursor
        // left one column.
        //
        This->Mode->CursorColumn--;
      }
      Private->ScreenBuffer[This->Mode->CursorRow][This->Mode->CursorColumn].Char = CHAR_NULL;

      WString++;

    } else if (*WString == CHAR_LINEFEED) {
      //
      // If the cursor is at the bottom of the display, then scroll the display one
      // row, and do not update the cursor position. Otherwise, move the cursor
      // down one row.
      //
      if (This->Mode->CursorRow == (INT32) (MaxRow - 1)) {
        //
        // Save top row in end of history
        // Scroll Screen Up One Row
        //
        Line = AllocatePool (sizeof (*Line));
        ASSERT (Line != NULL);
        Line->Line = Private->ScreenBuffer[0];
        InsertTailList (&Private->ScreenHistory, &Line->Link);
        for (Index = 0; Index < (UINTN) This->Mode->CursorRow; Index++) {
          CopyMem (Private->ScreenBuffer[Index], Private->ScreenBuffer[Index + 1], sizeof (VIRTUAL_CONSOLE_CHAR) * MaxColumn);
        }
        //
        // Set last line as blank
        //
        ZeroMem (Private->ScreenBuffer[This->Mode->CursorRow], sizeof (VIRTUAL_CONSOLE_CHAR) * MaxColumn);
      } else {
        This->Mode->CursorRow++;
      }

      WString++;

    } else if (*WString == CHAR_CARRIAGE_RETURN) {
      //
      // Move the cursor to the beginning of the current row.
      //
      This->Mode->CursorColumn = 0;
      WString++;

    } else if (*WString == WIDE_CHAR) {

      This->Mode->Attribute |= EFI_WIDE_ATTRIBUTE;
      WString++;

    } else if (*WString == NARROW_CHAR) {

      This->Mode->Attribute &= (~ (UINT32) EFI_WIDE_ATTRIBUTE);
      WString++;

    } else {
      //
      // Print the character at the current cursor position and move the cursor
      // right one column. If this moves the cursor past the right edge of the
      // display, then the line should wrap to the beginning of the next line. This
      // is equivalent to inserting a CR and an LF. Note that if the cursor is at the
      // bottom of the display, and the line wraps, then the display will be scrolled
      // one line.
      // If wide char is going to be displayed, need to display one character at a time
      // Or, need to know the display length of a certain string.
      //
      // Index is used to determine how many character width units (wide = 2, narrow = 1)
      // Count is used to determine how many characters are used regardless of their attributes
      //
      for (Count = 0, Index = 0; (This->Mode->CursorColumn + Index) < MaxColumn; Count++) {
        if (WString[Count] == CHAR_NULL ||
            WString[Count] == CHAR_BACKSPACE ||
            WString[Count] == CHAR_LINEFEED ||
            WString[Count] == CHAR_CARRIAGE_RETURN ||
            WString[Count] == WIDE_CHAR ||
            WString[Count] == NARROW_CHAR) {
          break;
        }

        //
        // Is the wide attribute on?
        //
        if ((This->Mode->Attribute & EFI_WIDE_ATTRIBUTE) != 0) {
          if ((This->Mode->CursorColumn + Index + 2) > MaxColumn) {
            //
            // This is the end-case where if we are at column 79 and about to print a wide character
            // We should prevent this from happening because we will wrap inappropriately.  We should
            // not print this character until the next line.
            //
            Index += 2;
            break;
          } else {
            Private->ScreenBuffer[This->Mode->CursorRow][This->Mode->CursorColumn + Index].Attribute = This->Mode->Attribute;
            Private->ScreenBuffer[This->Mode->CursorRow][This->Mode->CursorColumn + Index].Char = WString[Count];
            Index += 2;
          }
        } else {
          Private->ScreenBuffer[This->Mode->CursorRow][This->Mode->CursorColumn + Index].Attribute = This->Mode->Attribute;
          Private->ScreenBuffer[This->Mode->CursorRow][This->Mode->CursorColumn + Index].Char = WString[Count];
          Index += 1;
        }
      }

      //
      // At the end of line, output carriage return and line feed
      //
      WString += Count;
      This->Mode->CursorColumn += (INT32) Index;

      if (This->Mode->CursorColumn >= (INT32) MaxColumn) {
        EfiReleaseLock (&Private->TxtOutLock);
        This->OutputString (This, mCrLfString);
        EfiAcquireLock (&Private->TxtOutLock);
      }
    }
  }

  This->Mode->Attribute = OriginAttribute;
  EfiReleaseLock (&Private->TxtOutLock);

  return Status;

}

/**
  Verifies that all characters in a Unicode string can be output to the
  target device.

  Implements SIMPLE_TEXT_OUTPUT.TestString().
  If one of the characters in the *WString is neither valid Unicode
  drawing characters, not ASCII code, then this function will return
  EFI_UNSUPPORTED

  @param  This    Protocol instance pointer.
  @param  WString The NULL-terminated Unicode string to be examined for the output
                  device(s).

  @retval EFI_SUCCESS      The device(s) are capable of rendering the output string.
  @retval EFI_UNSUPPORTED  Some of the characters in the Unicode string cannot be
                           rendered by one or more of the output devices mapped
                           by the EFI handle.

**/
EFI_STATUS
EFIAPI
TestString (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  CHAR16                           *WString
  )
{
  return EFI_SUCCESS;
}


/**
  Returns information for an available text mode that the output device(s)
  supports

  Implements SIMPLE_TEXT_OUTPUT.QueryMode().
  It returns information for an available text mode that the Graphics Console supports.
  In this driver,we only support text mode 80x25, which is defined as mode 0.

  @param  This                  Protocol instance pointer.
  @param  ModeNumber            The mode number to return information on.
  @param  Columns               The returned columns of the requested mode.
  @param  Rows                  The returned rows of the requested mode.

  @retval EFI_SUCCESS           The requested mode information is returned.
  @retval EFI_UNSUPPORTED       The mode number is not valid.

**/
EFI_STATUS
EFIAPI
QueryMode (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            ModeNumber,
  OUT UINTN                            *Columns,
  OUT UINTN                            *Rows
  )
{
  VIRTUAL_CONSOLE_DEV  *Private;

  if (ModeNumber >= (UINTN) This->Mode->MaxMode) {
    return EFI_UNSUPPORTED;
  }

  Private   = VIRTUAL_CONSOLE_DEV_FROM_CON_OUT (This);


  if (Private->SimpleTextOutputModeData[ModeNumber].Columns <= 0 || Private->SimpleTextOutputModeData[ModeNumber].Rows <= 0) {
    return EFI_UNSUPPORTED;
  }

  *Columns = Private->SimpleTextOutputModeData[ModeNumber].Columns;
  *Rows = Private->SimpleTextOutputModeData[ModeNumber].Rows;
  return EFI_SUCCESS;
}

/**
  Sets the output device(s) to a specified mode.

  Implements SIMPLE_TEXT_OUTPUT.SetMode().
  Set the Graphics Console to a specified mode. In this driver, we only support mode 0.

  @param  This                  Protocol instance pointer.
  @param  ModeNumber            The text mode to set.

  @retval EFI_SUCCESS           The requested text mode is set.
  @retval EFI_DEVICE_ERROR      The requested text mode cannot be set because of
                                Graphics Console device error.
  @retval EFI_UNSUPPORTED       The text mode number is not valid.

**/
EFI_STATUS
EFIAPI
SetMode (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            ModeNumber
  )
{
  VIRTUAL_CONSOLE_DEV             *Private;
  VIRTUAL_CONSOLE_MODE_DATA       *NewMode;
  VIRTUAL_CONSOLE_MODE_DATA       *OldMode;
  UINTN                           Index;

  Private   = VIRTUAL_CONSOLE_DEV_FROM_CON_OUT (This);

  //
  // Make sure the requested mode number is supported
  //
  if (ModeNumber >= (UINTN) This->Mode->MaxMode) {
    return EFI_UNSUPPORTED;
  }


  NewMode = &Private->SimpleTextOutputModeData[ModeNumber];

  if (NewMode->Columns <= 0 && NewMode->Rows <= 0) {
    return EFI_UNSUPPORTED;
  }

  EfiAcquireLock (&Private->TxtOutLock);

  //
  // If the new mode is the same as the old mode, then just return EFI_SUCCESS
  //
  if ((INT32) ModeNumber != This->Mode->Mode) {
    //
    // Destroy history
    //
    VirtualConsoleClearHistory (&Private->ScreenHistory);

    //
    // Destroy old screen content
    //
    if (This->Mode->Mode != -1) {
      OldMode = &Private->SimpleTextOutputModeData[This->Mode->Mode];
      for (Index = 0; Index < OldMode->Rows; Index++) {
        FreePool (Private->ScreenBuffer[Index]);
      }
      FreePool (Private->ScreenBuffer);
    }

    Private->ScreenBuffer = AllocatePool (sizeof (VIRTUAL_CONSOLE_CHAR *) * NewMode->Rows);
    ASSERT (Private->ScreenBuffer != NULL);
    for (Index = 0; Index < NewMode->Rows; Index++) {
      Private->ScreenBuffer[Index] = AllocatePool (sizeof (VIRTUAL_CONSOLE_CHAR) * NewMode->Columns);
      ASSERT (Private->ScreenBuffer[Index] != NULL);
    }

    //
    // The new mode is valid, so commit the mode change
    //
    This->Mode->Mode = (INT32) ModeNumber;

    //
    // Move the text cursor to the upper left hand corner of the display and flush it
    //
    This->Mode->CursorColumn = 0;
    This->Mode->CursorRow = 0;
  }
  VirtualConsoleClearScreen (Private->ScreenBuffer, NewMode->Columns, NewMode->Rows, This->Mode->Attribute);
  EfiReleaseLock (&Private->TxtOutLock);
  return EFI_SUCCESS;
}


/**
  Sets the background and foreground colors for the OutputString () and
  ClearScreen () functions.

  Implements SIMPLE_TEXT_OUTPUT.SetAttribute().

  @param  This                  Protocol instance pointer.
  @param  Attribute             The attribute to set. Bits 0..3 are the foreground
                                color, and bits 4..6 are the background color.
                                All other bits are undefined and must be zero.

  @retval EFI_SUCCESS           The requested attribute is set.
  @retval EFI_DEVICE_ERROR      The requested attribute cannot be set due to Graphics Console port error.
  @retval EFI_UNSUPPORTED       The attribute requested is not defined.

**/
EFI_STATUS
EFIAPI
SetAttribute (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            Attribute
  )
{
  VIRTUAL_CONSOLE_DEV                  *Private;

  if ((Attribute | 0x7F) != 0x7F) {
    return EFI_UNSUPPORTED;
  }

  Private = VIRTUAL_CONSOLE_DEV_FROM_CON_OUT (This);

  EfiAcquireLock (&Private->TxtOutLock);
  This->Mode->Attribute = (INT32) Attribute;
  EfiReleaseLock (&Private->TxtOutLock);

  return EFI_SUCCESS;
}


/**
  Clears the output device(s) display to the currently selected background
  color.

  Implements SIMPLE_TEXT_OUTPUT.ClearScreen().

  @param  This                  Protocol instance pointer.

  @retval  EFI_SUCCESS      The operation completed successfully.
  @retval  EFI_DEVICE_ERROR The device had an error and could not complete the request.
  @retval  EFI_UNSUPPORTED  The output device is not in a valid text mode.

**/
EFI_STATUS
EFIAPI
ClearScreen (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This
  )
{
  VIRTUAL_CONSOLE_DEV           *Private;
  VIRTUAL_CONSOLE_MODE_DATA     *ModeData;

  if (This->Mode->Mode == -1) {
    //
    // If current mode is not valid, return error.
    //
    return EFI_UNSUPPORTED;
  }

  Private   = VIRTUAL_CONSOLE_DEV_FROM_CON_OUT (This);
  EfiAcquireLock (&Private->TxtOutLock);

  ModeData = &Private->SimpleTextOutputModeData[This->Mode->Mode];
  VirtualConsoleClearScreen (Private->ScreenBuffer, ModeData->Columns, ModeData->Rows, This->Mode->Attribute);

  This->Mode->CursorColumn  = 0;
  This->Mode->CursorRow     = 0;

  EfiReleaseLock (&Private->TxtOutLock);

  return EFI_SUCCESS;
}


/**
  Sets the current coordinates of the cursor position.

  Implements SIMPLE_TEXT_OUTPUT.SetCursorPosition().

  @param  This        Protocol instance pointer.
  @param  Column      The position to set the cursor to. Must be greater than or
                      equal to zero and less than the number of columns and rows
                      by QueryMode ().
  @param  Row         The position to set the cursor to. Must be greater than or
                      equal to zero and less than the number of columns and rows
                      by QueryMode ().

  @retval EFI_SUCCESS      The operation completed successfully.
  @retval EFI_DEVICE_ERROR The device had an error and could not complete the request.
  @retval EFI_UNSUPPORTED  The output device is not in a valid text mode, or the
                           cursor position is invalid for the current mode.

**/
EFI_STATUS
EFIAPI
SetCursorPosition (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  UINTN                            Column,
  IN  UINTN                            Row
  )
{
  VIRTUAL_CONSOLE_DEV         *Private;
  VIRTUAL_CONSOLE_MODE_DATA   *ModeData;
  EFI_STATUS                  Status;

  if (This->Mode->Mode == -1) {
    //
    // If current mode is not valid, return error.
    //
    return EFI_UNSUPPORTED;
  }

  Status = EFI_SUCCESS;

  Private   = VIRTUAL_CONSOLE_DEV_FROM_CON_OUT (This);
  EfiAcquireLock (&Private->TxtOutLock);

  ModeData  = &Private->SimpleTextOutputModeData[This->Mode->Mode];

  if ((Column >= ModeData->Columns) || (Row >= ModeData->Rows)) {
    Status = EFI_UNSUPPORTED;
    goto Done;
  }

  This->Mode->CursorColumn  = (INT32) Column;
  This->Mode->CursorRow     = (INT32) Row;

Done:
  EfiReleaseLock (&Private->TxtOutLock);

  return Status;
}


/**
  Makes the cursor visible or invisible.

  Implements SIMPLE_TEXT_OUTPUT.EnableCursor().

  @param  This                  Protocol instance pointer.
  @param  Visible               If TRUE, the cursor is set to be visible, If FALSE,
                                the cursor is set to be invisible.

  @retval EFI_SUCCESS           The operation completed successfully.
  @retval EFI_UNSUPPORTED       The output device's mode is not currently in a
                                defined text mode.

**/
EFI_STATUS
EFIAPI
EnableCursor (
  IN  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL  *This,
  IN  BOOLEAN                          Visible
  )
{
  VIRTUAL_CONSOLE_DEV         *Private;

  if (This->Mode->Mode == -1) {
    //
    // If current mode is not valid, return error.
    //
    return EFI_UNSUPPORTED;
  }
  Private = VIRTUAL_CONSOLE_DEV_FROM_CON_OUT (This);
  EfiAcquireLock (&Private->TxtOutLock);
  This->Mode->CursorVisible = Visible;
  EfiReleaseLock (&Private->TxtOutLock);

  return EFI_SUCCESS;
}

/**
  Caller needs to query the text console mode (column x row) through
  standard UEFI interfaces to determine the size of ScreenBuffer.

  @param This               Indicates the calling context.
  @param ScreenBuffer       Buffer to pass screen content back.
  @param CharNum            Buffer to pass character numbers back.
  @param IncludingHistory   Flag to include screen history.

  @retval EFI_SUCCESS             Got the screen content.
  @retval EFI_INVALID_PARAMETER   If CharNum or ScreenBuffer are NULL.

**/
EFI_STATUS
EFIAPI
GetScreen (
  IN     EDKII_VIRTUAL_CONSOLE_PROTOCOL        *This,
  IN OUT VIRTUAL_CONSOLE_CHAR                  *ScreenBuffer,
  IN OUT UINTN                                 *CharNum,
  IN     BOOLEAN                               IncludingHistory
  )
{
  EFI_STATUS                        Status;
  VIRTUAL_CONSOLE_DEV               *Private;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL   *TxtOut;
  UINTN                             Rows;
  UINTN                             Columns;
  UINTN                             Index;
  UINTN                             HistoryRows;
  LIST_ENTRY                        *Link;
  VIRTUAL_CONSOLE_SCREEN_LINE       *Line;
  UINTN                             LineIndex;

  if (CharNum == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Private = VIRTUAL_CONSOLE_DEV_FROM_VIRTUAL_CONSOLE (This);
  TxtOut = &Private->SimpleTextOutput;

  Status = TxtOut->QueryMode (TxtOut, TxtOut->Mode->Mode, &Columns, &Rows);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  HistoryRows = 0;
  if (IncludingHistory) {
    for (Link = GetFirstNode (&Private->ScreenHistory); !IsNull (&Private->ScreenHistory, Link); Link = GetNextNode (&Private->ScreenHistory, Link)) {
      HistoryRows++;
    }
  }
  if (*CharNum < Columns * (HistoryRows + Rows)) {
    *CharNum = Columns * (HistoryRows + Rows);
    return EFI_BUFFER_TOO_SMALL;
  }
  if (ScreenBuffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *CharNum = Columns * (HistoryRows + Rows);
  EfiAcquireLock (&Private->TxtOutLock);
  LineIndex = 0;
  if (IncludingHistory) {
    //
    // Copy history
    //
    for (Link = GetFirstNode (&Private->ScreenHistory); !IsNull (&Private->ScreenHistory, Link); Link = GetNextNode (&Private->ScreenHistory, Link)) {
      Line = BASE_CR (Link, VIRTUAL_CONSOLE_SCREEN_LINE, Link);
      CopyMem (&ScreenBuffer[LineIndex * Columns], Line->Line, Columns * sizeof (VIRTUAL_CONSOLE_CHAR));
      LineIndex++;
    }
  }
  //
  // Copy screen
  //
  for (Index = 0; Index < Rows; Index++) {
    CopyMem (&ScreenBuffer[LineIndex * Columns], Private->ScreenBuffer[Index], Columns * sizeof (VIRTUAL_CONSOLE_CHAR));
    LineIndex++;
  }
  EfiReleaseLock (&Private->TxtOutLock);
  return EFI_SUCCESS;
}

/**
  Clear the saved screen history content.

  @param This      Indicates the calling context.

  @retval EFI_SUCCESS     This screen history was cleared.

**/
EFI_STATUS
EFIAPI
ClearHistory (
  IN     EDKII_VIRTUAL_CONSOLE_PROTOCOL        *This
  )
{
  VIRTUAL_CONSOLE_DEV               *Private;

  Private = VIRTUAL_CONSOLE_DEV_FROM_VIRTUAL_CONSOLE (This);
  EfiAcquireLock (&Private->TxtOutLock);
  VirtualConsoleClearHistory (&Private->ScreenHistory);
  EfiReleaseLock (&Private->TxtOutLock);

  return EFI_SUCCESS;
}
