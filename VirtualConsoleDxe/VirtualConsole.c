/** @file
  Produces Simple Text Input Protocol, Simple Text Input Extended Protocol and
  Simple Text Output Protocol upon Serial IO Protocol.

Copyright (c) 2006 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


#include "VirtualConsole.h"

VIRTUAL_CONSOLE_DEV  mVirtualConsoleDev = {
  VIRTUAL_CONSOLE_DEV_SIGNATURE,
  NULL,
  {   // VirtualConsole
    InputKey,
    GetScreen
  },
  {   // DevicePath
    { { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } }},
    { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { END_DEVICE_PATH_LENGTH , 0 } }
  },
  {   // SimpleTextInput
    ConInReset,
    ReadKeyStroke,
    NULL
  },
  {   // SimpleTextInputEx
    ResetEx,
    ReadKeyStrokeEx,
    NULL,
    SetState,
    RegisterKeyNotify,
    UnregisterKeyNotify,
  },
  {   // SimpleTextOutput
    ConOutReset,
    OutputString,
    TestString,
    QueryMode,
    SetMode,
    SetAttribute,
    ClearScreen,
    SetCursorPosition,
    EnableCursor,
    NULL
  },
  {   // SimpleTextOutputMode
    1,                                           // MaxMode
    -1,                                          // Mode
    EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLACK),    // Attribute
    0,                                           // CursorColumn
    0,                                           // CursorRow
    TRUE                                         // CursorVisible
  }
};

VIRTUAL_CONSOLE_MODE_DATA mTerminalConsoleModeData[] = {
  { 80, 25 },
  { 80, 50 },
  { 100, 31 }
};

VIRTUAL_CONSOLE_DEVICE_PATH mVirtualConsoleDevicePathTemplate[] = {
  { .Vendor.Header={HARDWARE_DEVICE_PATH, HW_VENDOR_DP, {sizeof (VENDOR_DEVICE_PATH), 0}}},
  { .Vendor.Header={END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { END_DEVICE_PATH_LENGTH , 0}}}
};

/**
  Free notify functions list.

  @param  ListHead               The list head

  @retval EFI_SUCCESS            Free the notify list successfully.
  @retval EFI_INVALID_PARAMETER  ListHead is NULL.

**/
EFI_STATUS
VirtualConsoleFreeNotifyList (
  IN OUT LIST_ENTRY           *ListHead
  )
{
  VIRTUAL_CONSOLE_KEY_NOTIFY  *KeyNotify;
  LIST_ENTRY                  *Link;

  ASSERT (ListHead != NULL);
  while (!IsListEmpty (ListHead)) {
    Link = GetFirstNode (ListHead);
    KeyNotify = VIRTUAL_CONSOLE_KEY_NOTIFY_FROM_LINK (Link);
    RemoveEntryList (Link);
    FreePool (KeyNotify);
  }

  return EFI_SUCCESS;
}

/**
  Initialize all the text modes which the terminal console supports.

  It returns information for available text modes that the terminal can support.

  @param[out] TextModeCount      The total number of text modes that terminal console supports.

  @return   The buffer to the text modes column and row information.
            Caller is responsible to free it when it's non-NULL.

**/
VIRTUAL_CONSOLE_MODE_DATA *
InitializeVirtualConsoleTextMode (
  OUT INT32                 *TextModeCount
)
{
  EFI_STATUS                Status;
  VIRTUAL_CONSOLE_MODE_DATA *TextModeData;
  UINTN                     Index;
  UINTN                     Mode;

  ASSERT (TextModeCount != NULL);
  TextModeData = AllocatePool (sizeof (mTerminalConsoleModeData) + gST->ConOut->Mode->MaxMode * sizeof (VIRTUAL_CONSOLE_MODE_DATA));
  ASSERT (TextModeData != NULL);
  *TextModeCount = 0;

  //
  // Fill modes queried from gST->ConOut
  //
  for (Mode = 0; Mode < (UINTN) gST->ConOut->Mode->MaxMode; Mode++) {
    Status = gST->ConOut->QueryMode (gST->ConOut, Mode, &TextModeData[*TextModeCount].Columns, &TextModeData[*TextModeCount].Rows);
    if (EFI_ERROR (Status)) {
      continue;
    }
    (*TextModeCount)++;
  }

  //
  // Add default modes when this driver starts before other console drivers.
  //
  Mode = *TextModeCount;
  while (Mode-- != 0) {
    for (Index = 0; Index < ARRAY_SIZE (mTerminalConsoleModeData); Index++) {
      if (TextModeData[Mode].Columns == mTerminalConsoleModeData[Index].Columns && TextModeData[Mode].Rows == mTerminalConsoleModeData[Index].Rows) {
        break;
      }
    }
    if (Index == ARRAY_SIZE (mTerminalConsoleModeData)) {
      CopyMem (&TextModeData[*TextModeCount], &mTerminalConsoleModeData[Index], sizeof (VIRTUAL_CONSOLE_MODE_DATA));
      (*TextModeCount)++;
    }
  }


  DEBUG_CODE (
    for (Mode = 0; Mode < (UINTN) *TextModeCount; Mode++) {
      DEBUG ((DEBUG_INFO, "VirtualConsole - Mode %d, Column = %d, Row = %d\n",
              Mode, TextModeData[Mode].Columns, TextModeData[Mode].Rows));
    }
  );
  return TextModeData;
}

/**

  Unload function for this image.

  @param ImageHandle - Handle for the image of this driver.

  @retval EFI_SUCCESS - Driver unloaded successfully.
  @return other - Driver can not unloaded.

**/
EFI_STATUS
EFIAPI
VirtualConsoleUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                          Status;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL     *TxtOut;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL      *TxtIn;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL   *TxtInEx;
  EFI_KEY_DATA                        KeyData;
  UINTN                               Rows;
  UINTN                               Columns;

  TxtIn = &mVirtualConsoleDev.SimpleInput;
  TxtInEx = &mVirtualConsoleDev.SimpleInputEx;
  TxtOut = &mVirtualConsoleDev.SimpleTextOutput;

  if (mVirtualConsoleDev.Handle != NULL) {
    Status = gBS->UninstallMultipleProtocolInterfaces (
      mVirtualConsoleDev.Handle,
      &gEdkiiVirtualConsoleProtocolGuid, &mVirtualConsoleDev.VirtualConsole,
      &gEfiConsoleInDeviceGuid, NULL,
      &gEfiConsoleOutDeviceGuid, NULL,
      &gEfiDevicePathProtocolGuid, &mVirtualConsoleDev.DevicePath,
      &gEfiSimpleTextInProtocolGuid, TxtIn,
      &gEfiSimpleTextInputExProtocolGuid, TxtInEx,
      &gEfiSimpleTextOutProtocolGuid, TxtOut,
      NULL
    );
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }


  //
  // Clean ConIn
  //
  if (TxtIn->WaitForKey != NULL) {
    gBS->CloseEvent (TxtIn->WaitForKey);
  }
  if (TxtInEx->WaitForKeyEx != NULL) {
    gBS->CloseEvent (TxtInEx->WaitForKeyEx);
  }

  do {
    Status = DequeKeyData (&mVirtualConsoleDev, &KeyData);
  } while (!EFI_ERROR (Status));

  VirtualConsoleFreeNotifyList (&mVirtualConsoleDev.KeyNotifyList);

  //
  // Clean ConOut
  //
  Status = TxtOut->QueryMode (TxtOut, TxtOut->Mode->Mode, &Columns, &Rows);
  if (!EFI_ERROR (Status)) {
    ASSERT (mVirtualConsoleDev.ScreenBuffer != NULL);
    while (Rows-- != 0) {
      FreePool (mVirtualConsoleDev.ScreenBuffer[Rows]);
    }
    FreePool (mVirtualConsoleDev.ScreenBuffer);
  }
  ClearHistory (&mVirtualConsoleDev.VirtualConsole);

  if (mVirtualConsoleDev.SimpleTextOutputModeData != NULL) {
    FreePool (mVirtualConsoleDev.SimpleTextOutputModeData);
  }
  return EFI_SUCCESS;
}

/**
  The user Entry Point for module VirtualConsole. The user code starts with this function.

  @param  ImageHandle    The firmware allocated handle for the EFI image.
  @param  SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeVirtualConsole (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
)
{
  EFI_STATUS                          Status;
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL     *SimpleTextOutput;
  EFI_SIMPLE_TEXT_INPUT_PROTOCOL      *SimpleTextInput;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL   *SimpleTextInputEx;

  //
  // Initialize the Virtual Console Dev
  //
  EfiInitializeLock (&mVirtualConsoleDev.TxtOutLock, TPL_NOTIFY);
  EfiInitializeLock (&mVirtualConsoleDev.TxtInLock, TPL_CALLBACK);

  InitializeListHead (&mVirtualConsoleDev.KeyQueue);
  InitializeListHead (&mVirtualConsoleDev.KeyNotifyList);
  InitializeListHead (&mVirtualConsoleDev.ScreenHistory);

  SimpleTextInput = &mVirtualConsoleDev.SimpleInput;
  SimpleTextInputEx = &mVirtualConsoleDev.SimpleInputEx;
  SimpleTextOutput = &mVirtualConsoleDev.SimpleTextOutput;

  Status = gBS->CreateEvent (
    EVT_NOTIFY_WAIT,
    TPL_CALLBACK,
    WaitForKeyCallback,
    &mVirtualConsoleDev,
    &SimpleTextInput->WaitForKey
  );
  ASSERT_EFI_ERROR (Status);

  Status = gBS->CreateEvent (
    EVT_NOTIFY_WAIT,
    TPL_CALLBACK,
    WaitForKeyCallback,
    &mVirtualConsoleDev,
    &SimpleTextInputEx->WaitForKeyEx
  );
  ASSERT_EFI_ERROR (Status);


  //
  // Initialize SimpleTextOut instance
  //
  SimpleTextOutput->Mode = &mVirtualConsoleDev.SimpleTextOutputMode;
  mVirtualConsoleDev.SimpleTextOutputModeData = InitializeVirtualConsoleTextMode (
                                                  &SimpleTextOutput->Mode->MaxMode
                                                  );
  if (mVirtualConsoleDev.SimpleTextOutputModeData == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FreeResources;
  }

  CopyGuid (&mVirtualConsoleDev.DevicePath.Vendor.Guid, &gEfiCallerIdGuid);

  Status = gBS->InstallMultipleProtocolInterfaces (
    &mVirtualConsoleDev.Handle,
    &gEdkiiVirtualConsoleProtocolGuid, &mVirtualConsoleDev.VirtualConsole,
    &gEfiDevicePathProtocolGuid, &mVirtualConsoleDev.DevicePath,
    &gEfiConsoleInDeviceGuid, NULL,  // Activate ConIn
    &gEfiConsoleOutDeviceGuid, NULL, // Activate ConOut
    &gEfiSimpleTextInProtocolGuid, &mVirtualConsoleDev.SimpleInput,
    &gEfiSimpleTextInputExProtocolGuid, &mVirtualConsoleDev.SimpleInputEx,
    &gEfiSimpleTextOutProtocolGuid, &mVirtualConsoleDev.SimpleTextOutput,
    NULL
  );
  if (EFI_ERROR (Status)) {
    goto FreeResources;
  }
  return EFI_SUCCESS;

FreeResources:
  VirtualConsoleUnload (ImageHandle);

  return Status;
}
