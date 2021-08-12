/** @file
  Implementation for EFI_SIMPLE_TEXT_INPUT_PROTOCOL protocol.

(C) Copyright 2014 Hewlett-Packard Development Company, L.P.<BR>
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

/**
  Check if the key queue is empty or not.

  @param  ConsoleDev           Terminal driver private structure

  @retval TRUE    The queue is empty.
  @retval FALSE   The queue is not empty.

**/
BOOLEAN
IsKeyQueueEmpty (
  IN  VIRTUAL_CONSOLE_DEV *ConsoleDev
  )
{
  BOOLEAN                 Empty;

  EfiAcquireLock (&ConsoleDev->TxtInLock);
  Empty = IsListEmpty (&ConsoleDev->KeyQueue);
  EfiReleaseLock (&ConsoleDev->TxtInLock);

  return Empty;
}

/**
  Reads the next keystroke from the input device. The WaitForKey Event can
  be used to test for existence of a keystroke via WaitForEvent () call.

  @param  ConsoleDev           Terminal driver private structure
  @param  KeyData                  A pointer to a buffer that is filled in with the
                                   keystroke state data for the key that was
                                   pressed.

  @retval EFI_SUCCESS              The keystroke information was returned.
  @retval EFI_OUT_OF_RESOURCES     There is insufficient resource to enqueue the key.

**/
EFI_STATUS
EnqueKeyData (
  IN  VIRTUAL_CONSOLE_DEV *ConsoleDev,
  IN  EFI_KEY_DATA *KeyData
  )
{
  VIRTUAL_CONSOLE_KEY_DATA   *Key;
  ASSERT (KeyData != NULL);

  Key = AllocatePool (sizeof (*Key));
  if (Key == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  CopyMem (&Key->KeyData, KeyData, sizeof (EFI_KEY_DATA));

  EfiAcquireLock (&ConsoleDev->TxtInLock);
  InsertTailList (&ConsoleDev->KeyQueue, &Key->Link);
  EfiReleaseLock (&ConsoleDev->TxtInLock);

  return EFI_SUCCESS;
}

/**
  Reads the next keystroke from the input device. The WaitForKey Event can
  be used to test for existence of a keystroke via WaitForEvent () call.

  @param  ConsoleDev           Terminal driver private structure
  @param  KeyData                  A pointer to a buffer that is filled in with the
                                   keystroke state data for the key that was
                                   pressed.

  @retval EFI_SUCCESS              The keystroke information was returned.
  @retval EFI_NOT_READY            There was no keystroke data available.

**/
EFI_STATUS
DequeKeyData (
  IN  VIRTUAL_CONSOLE_DEV *ConsoleDev,
  OUT EFI_KEY_DATA *KeyData
  )
{
  LIST_ENTRY                 *Link;
  VIRTUAL_CONSOLE_KEY_DATA   *Key;
  ASSERT (KeyData != NULL);

  if (IsKeyQueueEmpty (ConsoleDev)) {
    return EFI_NOT_READY;
  }

  EfiAcquireLock (&ConsoleDev->TxtInLock);
  Link = GetFirstNode (&ConsoleDev->KeyQueue);
  RemoveEntryList (Link);
  EfiReleaseLock (&ConsoleDev->TxtInLock);

  Key = BASE_CR (Link, VIRTUAL_CONSOLE_KEY_DATA, Link);
  CopyMem (KeyData, &Key->KeyData, sizeof (EFI_KEY_DATA));
  FreePool (Key);

  return EFI_SUCCESS;
}

/**
  Implements EFI_SIMPLE_TEXT_INPUT_PROTOCOL.Reset().
  This driver only perform dependent serial device reset regardless of
  the value of ExtendeVerification

  @param  This                     Indicates the calling context.
  @param  ExtendedVerification     Skip by this driver.

  @retval EFI_SUCCESS              The reset operation succeeds.
  @retval EFI_DEVICE_ERROR         The dependent serial port reset fails.

**/
EFI_STATUS
EFIAPI
ConInReset (
  IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  IN  BOOLEAN                         ExtendedVerification
  )
{
  EFI_STATUS    Status;
  VIRTUAL_CONSOLE_DEV  *ConsoleDev;
  EFI_KEY_DATA         KeyData;

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_CON_IN (This);

  //
  // Report progress code here
  //
  REPORT_STATUS_CODE_WITH_DEVICE_PATH (
    EFI_PROGRESS_CODE,
    (EFI_PERIPHERAL_REMOTE_CONSOLE | EFI_P_PC_RESET),
    (EFI_DEVICE_PATH_PROTOCOL *) &ConsoleDev->DevicePath
    );

  //
  // Clean up the internal key queue
  //
  do {
    Status = DequeKeyData (ConsoleDev, &KeyData);
  } while (!EFI_ERROR (Status));

  if (EFI_ERROR (Status)) {
    REPORT_STATUS_CODE_WITH_DEVICE_PATH (
      EFI_ERROR_CODE | EFI_ERROR_MINOR,
      (EFI_PERIPHERAL_REMOTE_CONSOLE | EFI_P_EC_CONTROLLER_ERROR),
      (EFI_DEVICE_PATH_PROTOCOL *) &ConsoleDev->DevicePath
      );
  }

  return Status;
}

/**
  Implements EFI_SIMPLE_TEXT_INPUT_PROTOCOL.ReadKeyStroke().

  @param  This                Indicates the calling context.
  @param  Key                 A pointer to a buffer that is filled in with the
                              keystroke information for the key that was sent
                              from terminal.

  @retval EFI_SUCCESS         The keystroke information is returned successfully.
  @retval EFI_NOT_READY       There is no keystroke data available.
  @retval EFI_DEVICE_ERROR    The dependent serial device encounters error.

**/
EFI_STATUS
EFIAPI
ReadKeyStroke (
  IN  EFI_SIMPLE_TEXT_INPUT_PROTOCOL  *This,
  OUT EFI_INPUT_KEY                   *Key
  )
{
  VIRTUAL_CONSOLE_DEV  *ConsoleDev;
  EFI_STATUS    Status;
  EFI_KEY_DATA  KeyData;

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_CON_IN (This);

  Status = DequeKeyData (ConsoleDev, &KeyData);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CopyMem (Key, &KeyData.Key, sizeof (EFI_INPUT_KEY));

  return EFI_SUCCESS;

}

//
// Simple Text Input Ex protocol functions
//

/**
  Reset the input device and optionally run diagnostics

  @param  This                     Protocol instance pointer.
  @param  ExtendedVerification     Driver may perform diagnostics on reset.

  @retval EFI_SUCCESS              The device was reset.
  @retval EFI_DEVICE_ERROR         The device is not functioning properly and could
                                   not be reset.

**/
EFI_STATUS
EFIAPI
ResetEx (
  IN EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN BOOLEAN                            ExtendedVerification
  )
{
  VIRTUAL_CONSOLE_DEV  *ConsoleDev;

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_CON_IN_EX (This);

  return ConInReset (&ConsoleDev->SimpleInput, ExtendedVerification);
}


/**
  Reads the next keystroke from the input device. The WaitForKey Event can
  be used to test for existence of a keystroke via WaitForEvent () call.

  @param  This                     Protocol instance pointer.
  @param  KeyData                  A pointer to a buffer that is filled in with the
                                   keystroke state data for the key that was
                                   pressed.

  @retval EFI_SUCCESS              The keystroke information was returned.
  @retval EFI_NOT_READY            There was no keystroke data available.
  @retval EFI_DEVICE_ERROR         The keystroke information was not returned due
                                   to hardware errors.
  @retval EFI_INVALID_PARAMETER    KeyData is NULL.

**/
EFI_STATUS
EFIAPI
ReadKeyStrokeEx (
  IN  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,
  OUT EFI_KEY_DATA                      *KeyData
  )
{
  VIRTUAL_CONSOLE_DEV  *ConsoleDev;

  if (KeyData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_CON_IN_EX (This);
  return DequeKeyData (ConsoleDev, KeyData);

}


/**
  Set certain state for the input device.

  @param  This                     Protocol instance pointer.
  @param  KeyToggleState           A pointer to the EFI_KEY_TOGGLE_STATE to set the
                                   state for the input device.

  @retval EFI_SUCCESS              The device state was set successfully.
  @retval EFI_DEVICE_ERROR         The device is not functioning correctly and
                                   could not have the setting adjusted.
  @retval EFI_UNSUPPORTED          The device does not have the ability to set its
                                   state.
  @retval EFI_INVALID_PARAMETER    KeyToggleState is NULL.

**/
EFI_STATUS
EFIAPI
SetState (
  IN EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN EFI_KEY_TOGGLE_STATE               *KeyToggleState
  )
{
  VIRTUAL_CONSOLE_DEV  *ConsoleDev;
  if (KeyToggleState == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_CON_IN_EX (This);
  ConsoleDev->KeyToggleState = *KeyToggleState;

  return EFI_SUCCESS;
}



/**
  Test if the key has been registered on input device.

  @param  RegsiteredData           A pointer to a buffer that is filled in with the
                                   keystroke state data for the key that was
                                   registered.
  @param  InputData                A pointer to a buffer that is filled in with the
                                   keystroke state data for the key that was
                                   pressed.

  @retval TRUE                     Key be pressed matches a registered key.
  @retval FLASE                    Match failed.

**/
BOOLEAN
IsKeyRegistered (
  IN EFI_KEY_DATA  *RegsiteredData,
  IN EFI_KEY_DATA  *InputData
  )
{
  ASSERT (RegsiteredData != NULL && InputData != NULL);

  if ((RegsiteredData->Key.ScanCode    != InputData->Key.ScanCode) ||
      (RegsiteredData->Key.UnicodeChar != InputData->Key.UnicodeChar)) {
    return FALSE;
  }

  //
  // Assume KeyShiftState/KeyToggleState = 0 in Registered key data means these state could be ignored.
  //
  if (RegsiteredData->KeyState.KeyShiftState != 0 &&
      RegsiteredData->KeyState.KeyShiftState != InputData->KeyState.KeyShiftState) {
    return FALSE;
  }
  if (RegsiteredData->KeyState.KeyToggleState != 0 &&
      RegsiteredData->KeyState.KeyToggleState != InputData->KeyState.KeyToggleState) {
    return FALSE;
  }

  return TRUE;

}

/**
  Register a notification function for a particular keystroke for the input device.

  @param  This                     Protocol instance pointer.
  @param  KeyData                  A pointer to a buffer that is filled in with the
                                   keystroke information data for the key that was
                                   pressed.
  @param  Callback                 Points to the function to be called when the key
                                   sequence is typed specified by KeyData.
  @param  NotifyHandle             Points to the unique handle assigned to the
                                   registered notification.

  @retval EFI_SUCCESS              The notification function was registered
                                   successfully.
  @retval EFI_OUT_OF_RESOURCES     Unable to allocate resources for necessary data
                                   structures.
  @retval EFI_INVALID_PARAMETER    KeyData or NotifyHandle is NULL.

**/
EFI_STATUS
EFIAPI
RegisterKeyNotify (
  IN EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN EFI_KEY_DATA                       *KeyData,
  IN EFI_KEY_NOTIFY_FUNCTION            Callback,
  OUT VOID                              **NotifyHandle
  )
{
  VIRTUAL_CONSOLE_DEV                   *ConsoleDev;
  LIST_ENTRY                            *Link;
  VIRTUAL_CONSOLE_KEY_NOTIFY            *KeyNotify;


  if (KeyData == NULL || NotifyHandle == NULL || Callback == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_CON_IN_EX (This);

  //
  // Return EFI_SUCCESS if the (KeyData, NotificationFunction) is already registered.
  //
  for (Link = GetFirstNode (&ConsoleDev->KeyNotifyList); !IsNull (&ConsoleDev->KeyNotifyList, Link); Link = GetNextNode (&ConsoleDev->KeyNotifyList, Link)) {
    KeyNotify = VIRTUAL_CONSOLE_KEY_NOTIFY_FROM_LINK (Link);
    if (IsKeyRegistered (&KeyNotify->KeyData, KeyData)) {
      if (KeyNotify->Callback == Callback) {
        *NotifyHandle = KeyNotify;
        return EFI_SUCCESS;
      }
    }
  }

  //
  // Allocate resource to save the notification function
  //
  KeyNotify = AllocatePool (sizeof (VIRTUAL_CONSOLE_KEY_NOTIFY));
  if (KeyNotify == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  KeyNotify->Signature = VIRTUAL_CONSOLE_KEY_NOTIFY_SIGNATURE;
  KeyNotify->Callback = Callback;
  CopyMem (&KeyNotify->KeyData, KeyData, sizeof (EFI_KEY_DATA));

  InsertTailList (&ConsoleDev->KeyNotifyList, &KeyNotify->Link);

  *NotifyHandle = KeyNotify;

  return EFI_SUCCESS;
}


/**
  Remove a registered notification function from a particular keystroke.

  @param  This                     Protocol instance pointer.
  @param  NotifyHandle             The handle of the notification function being
                                   unregistered.

  @retval EFI_SUCCESS              The notification function was unregistered
                                   successfully.
  @retval EFI_INVALID_PARAMETER    The NotificationHandle is invalid.

**/
EFI_STATUS
EFIAPI
UnregisterKeyNotify (
  IN EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *This,
  IN VOID                               *NotifyHandle
  )
{
  VIRTUAL_CONSOLE_DEV                   *ConsoleDev;
  VIRTUAL_CONSOLE_KEY_NOTIFY            *KeyNotify;
  LIST_ENTRY                            *Link;

  if (NotifyHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_CON_IN_EX (This);

  for (Link = GetFirstNode (&ConsoleDev->KeyNotifyList); !IsNull (&ConsoleDev->KeyNotifyList, Link); Link = GetNextNode (&ConsoleDev->KeyNotifyList, Link)) {
    KeyNotify = VIRTUAL_CONSOLE_KEY_NOTIFY_FROM_LINK (Link);
    if (KeyNotify == NotifyHandle) {
      RemoveEntryList (&KeyNotify->Link);
      gBS->FreePool (KeyNotify);
      return EFI_SUCCESS;
    }
  }

  //
  // NotificationHandle is not found in database
  //
  return EFI_INVALID_PARAMETER;
}

/**
  Event notification function for EFI_SIMPLE_TEXT_INPUT_PROTOCOL.WaitForKey event
  Signal the event if there is key available

  @param  Event                    Indicates the event that invoke this function.
  @param  Context                  Indicates the calling context.

**/
VOID
EFIAPI
WaitForKeyCallback (
  IN  EFI_EVENT       Event,
  IN  VOID            *Context
  )
{
  //
  // Someone is waiting on the keystroke event, if there's
  // a key pending, signal the event
  //
  if (!IsKeyQueueEmpty ((VIRTUAL_CONSOLE_DEV *) Context)) {
    gBS->SignalEvent (Event);
  }
}

/**
  Simple interface to input single key.

  @param This       Indicates the calling context.
  @param KeyData    Pointer to a buffer that is filled in with the
                    eystroke state data for the key that was
                    ressed.

  @retval EFI_SUCCESS              The keystroke was input successfully.
  @retval EFI_OUT_OF_RESOURCES     There is insufficient resource to enqueue the key.

**/
EFI_STATUS
EFIAPI
InputKey (
  IN EDKII_VIRTUAL_CONSOLE_PROTOCOL *This,
  IN EFI_KEY_DATA * KeyData
  )
{
  VIRTUAL_CONSOLE_DEV               *ConsoleDev;

  ConsoleDev = VIRTUAL_CONSOLE_DEV_FROM_VIRTUAL_CONSOLE (This);
  return EnqueKeyData (ConsoleDev, KeyData);
}
