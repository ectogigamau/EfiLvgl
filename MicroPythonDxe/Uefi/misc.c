/** @file
  Helper funtions for MicroPython on UEFI.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Protocol/ScriptEngineProtocol.h>
#include <Protocol/PciIo.h>
#include <Protocol/Shell.h>
#include <Guid/FileSystemVolumeLabelInfo.h>

#include <py/mpconfig.h>
#include <py/misc.h>

#include "upy.h"

CONST CHAR8 gGuidFormat[37] = "01234567-89ab-cdef-0123-456789abcdef";

/**
  Count the number of UNIX EOL in the given string.

  @param  AsciiString       Ascii string might contain UNIX EOL.
  @param  ScriptProtocol    Pointer to script engine protocol.

  @retval Number of UNIX EOL.

**/
UINTN
CountUnixEol (
  IN  CONST CHAR8   *AsciiString,
  IN  UINTN         Length
  )
{
  UINTN     Index;
  UINTN     NumberOfUnixEol;

  NumberOfUnixEol = 0;
  for (Index = 0; Index < Length && AsciiString[Index] != '\0'; ++Index) {
    if (AsciiString[Index] == '\n' && (Index == 0 || AsciiString[Index - 1] != '\r')) {
      ++NumberOfUnixEol;
    }
  }

  return NumberOfUnixEol;
}

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
)
{
  EFI_STATUS                      Status;
  UINTN                           HandleCount;
  EFI_HANDLE                      *HandleBuffer;
  UINTN                           Index;

  if (Type == 0) {
    return EFI_INVALID_PARAMETER;
  }

  // Check script engine protocols
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiScriptEngineProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; ++Index) {
    Status = gBS->HandleProtocol(HandleBuffer[Index],
                                 &gEfiScriptEngineProtocolGuid,
                                 (VOID **)ScriptProtocol);
    if (!EFI_ERROR (Status) && (*ScriptProtocol)->GetType(*ScriptProtocol) == Type) {
      FreePool(HandleBuffer);
      return EFI_SUCCESS;
    }
  }

  FreePool(HandleBuffer);
  return EFI_NOT_FOUND;
}

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
  IN  CONST CHAR8         *Src,
  IN        CHAR16        *Dst,
  IN        UINTN         *Length,
  IN        BOOLEAN       ConvertUnixEol
)
{
  CONST UINT8     *SrcBuf;
  CHAR16          *DstBuf;
  UINTN           Index;
  UINTN           UnixEolNum;

  if (ConvertUnixEol) {
    UnixEolNum = CountUnixEol (Src, *Length);
  } else {
    UnixEolNum = 0;
  }

  *Length = utf8_charlen((const byte *)Src, *Length);
  *Length = *Length + UnixEolNum;

  if (Dst == NULL) {
    DstBuf = AllocatePool((*Length + 1) * sizeof(CHAR16));
  } else {
    DstBuf = Dst;
  }
  ASSERT(DstBuf != NULL);

  SrcBuf = (CONST UINT8 *)Src;
  for (Index = 0; Index < *Length; ++Index) {
    if ((*SrcBuf & 0xE0) == 0xE0) {
      DstBuf[Index] = ((UINT16)(SrcBuf[0] & 0x0F) << 12) | ((UINT16)(SrcBuf[1] & 0x3F) << 6) | (SrcBuf[2] & 0x3F);
      SrcBuf += 3;
    } else if ((*SrcBuf & 0xC0) == 0xC0) {
      DstBuf[Index] = ((UINT16)(SrcBuf[0] & 0x1F) << 6) | (SrcBuf[1] & 0x3F);
      SrcBuf += 2;
    } else {
      if (ConvertUnixEol && (*SrcBuf == '\n') && (Index == 0 || *(SrcBuf - 1) != '\r')) {
        DstBuf[Index++] = '\r';
      }
      DstBuf[Index] = *SrcBuf++;
    }
  }
  DstBuf[Index] = '\0';

  return DstBuf;
}

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
  IN        UINTN     *Length
)
{
  CONST CHAR16    *SrcBuf;
  CHAR8           *DstBuf;
  UINTN           Index;
  UINTN           Size;

  SrcBuf = Src;
  for (Size = 0, Index = 0; Index < *Length; ++Index) {
    if (SrcBuf[Index] < (UINT16)0x80) {
      Size += 1;
    } else if (SrcBuf[Index] < (UINT16)0x800) {
      Size += 2;
    } else {
      Size += 3;
    }
  }

  if (Dst == NULL) {
    DstBuf = AllocatePool((Size + 1) * sizeof(CHAR8));
  } else {
    DstBuf = Dst;
  }
  ASSERT(DstBuf != NULL);

  for (Size = 0, Index = 0; Index < *Length; ++Index) {
    if (SrcBuf[Index] < (UINT16)0x80) {
      DstBuf[Size++] = Src[Index];
    } else if (SrcBuf[Index] < (UINT16)0x800) {
      DstBuf[Size++] = 0xC0 | (SrcBuf[Index] >> 6);
      DstBuf[Size++] = 0x80 | (SrcBuf[Index] & 0x3F);
    } else {
      DstBuf[Size++] = 0xE0 | ((SrcBuf[Index] >> 12) & 0x000F);
      DstBuf[Size++] = 0x80 | ((SrcBuf[Index] >> 6) & 0x003F);
      DstBuf[Size++] = 0x80 |  (SrcBuf[Index] & 0x003F);
    }
  }

  DstBuf[Size] = '\0';
  *Length = Size;

  return DstBuf;
}

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
)
{
  return (AsciiStrToGuid (GuidString, Guid) == EFI_SUCCESS);
}

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
)
{
  return AsciiSPrint (
           GuidString,
           sizeof(gGuidFormat),
           "%g",
           Guid
           );
}

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
)
{
  return UnicodeToUtf8 (SrcString, Dsttring, Length);
}

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
)
{
  return Utf8ToUnicode (SrcString, DstString, Length, TRUE);
}

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
)
{
  CONST CHAR16  *StringTmp;
  CONST CHAR16  *SearchStringTmp;
  UINTN         Length;
  UINTN         StringLength;
  UINTN         SearchStringLength;

  //
  // ASSERT both strings are less long than PcdMaximumUnicodeStringLength.
  // Length tests are performed inside StrLen().
  //
  ASSERT (StrSize (String) != 0);
  ASSERT (StrSize (SearchString) != 0);

  if (*SearchString == L'\0') {
    return (CHAR16 *) String;
  }

  if (*String == L'\0') {
    return NULL;
  }

  StringLength = StrLen (String);
  SearchStringLength = StrLen (SearchString);
  while (StringLength >= SearchStringLength) {
    StringTmp = String + StringLength - 1;
    SearchStringTmp = SearchString + SearchStringLength - 1;

    Length = SearchStringLength;
    while ((*StringTmp == *SearchStringTmp) && (Length > 0)) {
      StringTmp--;
      SearchStringTmp--;
      Length--;
    }

    if (Length == 0) {
      return (CHAR16 *) (StringTmp + 1);
    }

    if (*String == L'\0') {
      return NULL;
    }

    --StringLength;
  }

  return NULL;
}

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
  )
{
  CHAR16      *Dest;

  if (Src == NULL) {
    return NULL;
  }

  if (MaxLen == 0) {
    MaxLen = StrLen (Src);
  }

  if (MaxLen == 0) {
    return NULL;
  }

  Dest = AllocatePool((MaxLen + 1) * sizeof(CHAR16));
  if (Dest != NULL) {
    Dest[0] = '\0';
    StrnCatS (Dest, MaxLen + 1, Src, MaxLen);
  }

  return Dest;
}

