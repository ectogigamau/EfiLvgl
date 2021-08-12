/** @file
  Header file containing all common definitions.

  Copyright (c) 2018, Intel Corporation. All rights reserved. <BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef   UEFI_UPY_H
#define   UEFI_UPY_H

#include <Base.h>
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Protocol/ScriptEngineProtocol.h>
#include <Protocol/ScriptFileProtocol.h>

#define FREE_NON_NULL(Pointer)        \
  do {                                \
    if ((Pointer) != NULL) {          \
      FreePool((VOID *)(Pointer));    \
      (Pointer) = NULL;               \
    }                                 \
  } while(FALSE)

#define CLOSE_FILE(Fh)                \
  do {                                \
    if ((Fh) != NULL) {               \
      Fh->Close((Fh));                \
      (Fh) = NULL;                    \
    }                                 \
  } while(FALSE)

//
// Function Prototypes
//

/**
  Initialize the script engine of MicroPython.

  @param  ImageHandle   Image handle of driver calling this method.

  @retval EFI_SUCCESS           The script engine is initialized successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support environment
                                variable.

**/
EFI_STATUS
EFIAPI
UseInit (
  EFI_HANDLE        ImageHandle
);

/**
  Destroy the script engine and release all resources allocated before.

  @retval EFI_SUCCESS     The script engine is destroied successfully.
  @retval EFI_NOT_FOUND   The script engine has been already destroied.

**/
EFI_STATUS
EFIAPI
UseDeinit (
  VOID
);

/**
  Initialize and install the script file protocol.

  This method won't install another script file protocol if there's already
  one located.

  @param  ImageHandle   Image handle of driver calling this method.
  @param  Protocol      Pointer to file script protocol installed or located.

  @retval EFI_SUCCESS           The script file protocol is initialized successfully.
  @retval EFI_ALREADY_STARTED   There's a script file protocol already installed.
  @retval EFI_NOT_STARTED       Failed to install script file protocol.

**/
EFI_STATUS
EFIAPI
UsfInit (
  IN  EFI_HANDLE                 ImageHandle,
  OUT EFI_SCRIPT_FILE_PROTOCOL   **Protocol
);

/**
  Destroy and uninstall the script file protocol.

  @retval EFI_SUCCESS     The script file protocol is uninstalled successfully.
  @retval EFI_NOT_FOUND   The script file protocol has been already uninstalled.

**/
EFI_STATUS
EFIAPI
UsfDeinit (
  EFI_HANDLE        ImageHandle
);

/**
  Get the consistent mapping name for a given device path.

  @param  DevicePath   Device path of a file system.
  @param  Table        Table of device path of all file systems currently available.

  @retval Name string           If given device path is found in given table.
  @retval NULL                  There's no matching device found in table.

**/
CHAR16 *
EFIAPI
GetConsistMappingName (
  IN CONST EFI_DEVICE_PATH_PROTOCOL     *DevicePath,
  IN       EFI_DEVICE_PATH_PROTOCOL     **Table
  );

/**
  Initialize the device path table of file systems currently available.

  @param  Table   Pointer containing the device path list.

  @retval EFI_SUCCESS     Complete the table initialization.

**/
EFI_STATUS
EFIAPI
ConsistMappingTableInit (
  OUT EFI_DEVICE_PATH_PROTOCOL    ***Table
  );

/**
  Release the resource allocated in the table.

  @param  Table   Pointer containing the device path list.

  @retval EFI_SUCCESS     Complete the table de-initialization.

**/
EFI_STATUS
EFIAPI
ConsistMappingTableDeinit (
  IN EFI_DEVICE_PATH_PROTOCOL     **Table
  );

/**
  Initialize the uefi module.

  @param  ImageHandle     Handle of driver calling this method.

**/
VOID
moduefi_init (
  EFI_HANDLE ImageHandle
  );

/**
  De-initialize the uefi module.

  @param  ImageHandle     Handle of driver calling this method.

**/
VOID moduefi_deinit (
  VOID
  );

/**
  Locate the script engine protocol supporting given type.

  @param  Type              Type of a script engine.
  @param  ScriptProtocol    Pointer to script engine protocol.

  @retval EFI_SUCCESS       Found a script engine protocol of given type.
  @retval EFI_NOT_FOUND     No script engine protocol supporting given type.

**/
EFI_STATUS
EFIAPI
FindScriptProtocolByType (
  IN      UINT32                          Type,
      OUT EFI_SCRIPT_ENGINE_PROTOCOL      **ScriptProtocol
);

/**
  Convert a string encoded in UTF8 (supported by MicroPython) to UTF16
  (supported by UEFI).

  @param  Src             Pointer to source string (UTF8).
  @param  Dst             Pointer to destination string (UTF16). If NULL is given,
                          this method will allocate enough memory to store the
                          converted string.
  @param  Length          On input, string length of source string.
                          On output, string length of destination string.
  @param  ConvertUnixEol  Do EOL convertion from UNIX format to DOS format.

  @retval String pointer to the converted UTF16 string.

**/
CHAR16*
EFIAPI
Utf8ToUnicode (
  IN        CONST CHAR8   *Src,
  IN        CHAR16        *Dst,
  IN  OUT   UINTN         *Length,
  IN        BOOLEAN       ConvertUnixEol
);

/**
  Convert a string encoded in UTF16 (supported by UEFI) to UTF8
  (supported by MicroPython).

  @param  Src             Pointer to source string (UTF16).
  @param  Dst             Pointer to destination string (UTF8). If NULL is given,
                          this method will allocate enough memory to store the
                          converted string.
  @param  Length          On input, string length of source string.
                          On output, string length of destination string.

  @retval String pointer to the converted UTF8 string.

**/
CHAR8*
EFIAPI
UnicodeToUtf8 (
  IN  CONST CHAR16    *Src,
  IN        CHAR8     *Dst,
  IN  OUT   UINTN     *Length
);

/**
  Convert a GUID in registry format to structure EFI_GUID.

  @param  GuidString               Pointer to a Null-terminated ASCII string.
  @param  Guid                     Pointer to the converted GUID.

  @retval RETURN_SUCCESS           Guid is translated from String.
  @retval RETURN_INVALID_PARAMETER If String is NULL.
                                   If Data is NULL.
  @retval RETURN_UNSUPPORTED       If String is not as the above format.

**/
BOOLEAN
EFIAPI
StringToGuid (
  IN CONST CHAR8      *GuidString,
  IN EFI_GUID         *Guid
);

/**
  Convert a GUID in structure EFI_GUID to registry format.

  @param  Guid                    Pointer to the converted GUID.
  @param  GuidString              Pointer to a Null-terminated ASCII string.

  @retval Length of string converted.

**/
UINTN
EFIAPI
GuidToString (
  IN EFI_GUID   *Guid,
  IN CHAR8      *GuidString
);

/**
  Convert a UEFI string to string supported by MicroPython.

  @param  SrcString       Pointer to source string (UTF16).
  @param  DstString       Pointer to destination string (UTF8). If NULL is given,
                          this method will allocate enough memory to store the
                          converted string.
  @param  Length          On input, string length of source string.
                          On output, string length of destination string.

  @retval String pointer to the converted UTF8 string.

**/
CHAR8*
EFIAPI
ToUpyString (
  IN  CONST CHAR16  *SrcString,
  IN  CHAR8         *Dsttring,
  IN  UINTN         *Length
);

/**
  Convert a string to UEFI supported format.

  @param  SrcString       Pointer to source string (UTF8).
  @param  DstString       Pointer to destination string (UTF16). If NULL is given,
                          this method will allocate enough memory to store the
                          converted string.
  @param  Length          On input, string length of source string.
                          On output, string length of destination string.

  @retval String pointer to the converted UTF16 string.

**/
CHAR16*
EFIAPI
ToUefiString (
  IN  CONST CHAR8   *SrcString,
  IN  CHAR16        *DstString,
  IN  UINTN         *Length
);

/**
  Returns the first occurrence of a Null-terminated Unicode sub-string
  in a Null-terminated Unicode string.

  This function is similar to StrStr() but scans the string from right (end) to
  the left (start).

  @param  String          The pointer to a Null-terminated Unicode string.
  @param  SearchString    The pointer to a Null-terminated Unicode string to search for.

  @retval NULL            If the SearchString does not appear in String.
  @return others          If there is a match.

**/
CHAR16 *
EFIAPI
StrStrR (
  IN      CONST CHAR16              *String,
  IN      CONST CHAR16              *SearchString
  );

/**
  Duplicate a string with maximum length.

  @param  Str         The pointer to a Null-terminated Unicode string.
  @param  MaxLen      Maximum characters to duplicate. 0 means the exact copy
                      of Src string.

  @retval NULL            If out of resource or source string empty.
  @return others          If the string is duplicated successfully.

**/
CHAR16 *
EFIAPI
StrDup (
  IN CONST CHAR16   *Src,
  IN       UINTN    MaxLen
  );

/**
  Connect to the network layer

  This routine is the constructor for the EfiSocketLib when the
  library is linked directly to an application.  This routine
  walks the ::cEslSocketBinding table to create ::ESL_SERVICE
  structures, associated with the network adapters, which this
  routine links to the ::ESL_LAYER structure.

  This routine is called from ::EslConstructor as a result of the
  constructor redirection in ::mpfnEslConstructor at the end of this
  file.

  @retval EFI_SUCCESS   Successfully connected to the network layer

**/
EFI_STATUS
EslServiceNetworkConnect (
  VOID
  );

/**
  Disconnect from the network layer

  Destructor for the EfiSocketLib when the library is linked
  directly to an application.  This routine walks the
  ::cEslSocketBinding table to remove the ::ESL_SERVICE
  structures (network connections) from the ::ESL_LAYER structure.

  This routine is called from ::EslDestructor as a result of the
  destructor redirection in ::mpfnEslDestructor at the end of this
  file.

  @retval EFI_SUCCESS   Successfully disconnected from the network layer

**/
EFI_STATUS
EslServiceNetworkDisconnect (
  VOID
  );

extern CONST CHAR8 gGuidFormat[37];

#endif

