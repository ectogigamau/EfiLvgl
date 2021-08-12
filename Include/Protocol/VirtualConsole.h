/** @file
  The Virtual Console Protocol defines the interface to intercept the user
  input and BIOS output on ConIn and ConOut, for test purpose.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under
the terms and conditions of the BSD License that accompanies this distribution.
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __VIRTUAL_CONSOLE_H_
#define __VIRTUAL_CONSOLE_H_

#include <Protocol/SimpleTextInEx.h>

//
// GUID for EDKII Platform Logo Protocol
//
#define EDKII_VIRTUAL_CONSOLE_PROTOCOL_GUID \
  { 0x372f6584, 0xf4c3, 0x47d4, { 0x92, 0x99, 0xac, 0x6c, 0x1b, 0xbe, 0x4e, 0xc } };


typedef struct _EDKII_VIRTUAL_CONSOLE_PROTOCOL EDKII_VIRTUAL_CONSOLE_PROTOCOL;

typedef struct {
  CHAR16                              Char;
  UINTN                               Attribute;
} VIRTUAL_CONSOLE_CHAR;

/**
  Simple interface to input single key.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_VIRTUAL_CONSOLE_INPUT_KEY)(
  IN     EDKII_VIRTUAL_CONSOLE_PROTOCOL        *This,
  IN     EFI_KEY_DATA                          *KeyData
  );

/**
  Caller needs to query the text console mode (column x row) through
  standard UEFI interfaces to determine the size of ScreenBuffer.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_VIRTUAL_CONSOLE_GET_SCREEN)(
  IN     EDKII_VIRTUAL_CONSOLE_PROTOCOL        *This,
  IN OUT VIRTUAL_CONSOLE_CHAR                  *ScreenBuffer,
  IN OUT UINTN                                 *BufferSize,
  IN     BOOLEAN                               IncludingHistory
  );

/**
  Clear the saved screen history content.
**/
typedef
EFI_STATUS
(EFIAPI *EDKII_VIRTUAL_CONSOLE_CLEAR_HISTORY)(
  IN     EDKII_VIRTUAL_CONSOLE_PROTOCOL        *This
  );

struct _EDKII_VIRTUAL_CONSOLE_PROTOCOL {
  EDKII_VIRTUAL_CONSOLE_INPUT_KEY            InputKey;
  EDKII_VIRTUAL_CONSOLE_GET_SCREEN           GetScreen;
  EDKII_VIRTUAL_CONSOLE_CLEAR_HISTORY        ClearHistory;
};


extern EFI_GUID gEdkiiVirtualConsoleProtocolGuid;

#endif
