/** @file
  Implementation of Script File Protocol for MicroPython on UEFI.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <string.h>

#include <py/mpstate.h>
#include <py/nlr.h>
#include <py/compile.h>
#include <py/runtime.h>
#include <py/builtin.h>
#include <py/repl.h>
#include <py/gc.h>
#include <py/stackctrl.h>
#include <py/mphal.h>

#include <lib/mp-readline/readline.h>
#include <extmod/misc.h>
#include <genhdr/mpversion.h>

#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/FileHandleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciIo.h>
#include <Protocol/Shell.h>
#include <Protocol/ScriptEngineProtocol.h>
#include <Protocol/ScriptFileProtocol.h>
#include <Guid/FileSystemVolumeLabelInfo.h>

#include "upy.h"

#define USF_PRIVATE_DATA_SIGNATURE      SIGNATURE_32 ('_', 'S', 'F', 'P')
#define USF_PRIVATE_DATA_FROM_SF(a)     CR (a, USF_PRIVATE_DATA, ScriptFile, USF_PRIVATE_DATA_SIGNATURE)

typedef struct {
  CONST UINT32                Signature;
  EFI_SCRIPT_FILE_PROTOCOL    ScriptFile;
  EFI_FILE_HANDLE             FilePool[64];
  EFI_DEVICE_PATH_PROTOCOL    **FileSystemTable;
  CONST CHAR16                *CurrentDirectory;
} USF_PRIVATE_DATA;

STATIC USF_PRIVATE_DATA mUsfData;

STATIC
EFI_STATUS
EFIAPI
SysStdIoClose(
  IN EFI_FILE_PROTOCOL *This
  )
{
  return (EFI_SUCCESS);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoDelete(
  IN EFI_FILE_PROTOCOL *This
  )
{
  return (EFI_SUCCESS);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoFlush(
  IN EFI_FILE_PROTOCOL *This
  )
{
  return (EFI_SUCCESS);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoOpen(
  IN  EFI_FILE_PROTOCOL *This,
  OUT EFI_FILE_PROTOCOL **NewHandle,
  IN  CHAR16 *FileName,
  IN  UINT64 OpenMode,
  IN  UINT64 Attributes
  )
{
  return (EFI_NOT_FOUND);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoGetPosition(
  IN EFI_FILE_PROTOCOL *This,
  OUT UINT64 *Position
  )
{
  return (EFI_UNSUPPORTED);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoSetPosition(
  IN EFI_FILE_PROTOCOL *This,
  IN UINT64 Position
  )
{
  return (EFI_UNSUPPORTED);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoGetInfo(
  IN EFI_FILE_PROTOCOL *This,
  IN EFI_GUID *InformationType,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  return (EFI_UNSUPPORTED);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoSetInfo(
  IN EFI_FILE_PROTOCOL *This,
  IN EFI_GUID *InformationType,
  IN UINTN BufferSize,
  IN VOID *Buffer
  )
{
  return (EFI_UNSUPPORTED);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoWrite(
  IN      EFI_FILE_PROTOCOL *This,
  IN OUT  UINTN             *BufferSize,
  IN      VOID              *Buffer
  )
{
  return (EFI_UNSUPPORTED);
}

STATIC
EFI_STATUS
EFIAPI
SysStdIoRead(
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  return (EFI_UNSUPPORTED);
}

STATIC
EFI_STATUS
EFIAPI
SysStdOutWrite(
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  IN VOID *Buffer
  )
{
  EFI_STATUS  Status;
  UINTN       NewSize;

  NewSize = *BufferSize;
  Buffer = ToUefiString(Buffer, NULL, &NewSize);
  Status = gST->ConOut->OutputString(gST->ConOut, Buffer);
  FREE_NON_NULL(Buffer);

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
SysStdErrWrite(
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  IN VOID *Buffer
  )
{
  EFI_STATUS  Status;
  UINTN       NewSize;

  NewSize = *BufferSize;
  Buffer = ToUefiString(Buffer, NULL, &NewSize);
  Status = gST->StdErr->OutputString(gST->StdErr, Buffer);
  FreePool(Buffer);

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
SysStdInRead(
  IN EFI_FILE_PROTOCOL *This,
  IN OUT UINTN *BufferSize,
  OUT VOID *Buffer
  )
{
  CHAR8               *CurrentString;
  BOOLEAN             Done;
  UINTN               Column;         // Column of current cursor
  UINTN               Row;            // Row of current cursor
  UINTN               StringCurPos;   // Line index corresponding to the cursor
  UINTN               MaxStr;         // Maximum possible line length
  UINTN               TotalColumn;    // Num of columns in the console
  UINTN               TotalRow;       // Num of rows in the console
  EFI_INPUT_KEY       Key;

  EFI_STATUS          Status;
  UINTN               EventIndex;

  //
  // If buffer is not large enough to hold a CHAR16, return minimum buffer size
  //
  if (*BufferSize < sizeof (CHAR8) * 2) {
    *BufferSize = sizeof (CHAR8) * 2;
    return (EFI_BUFFER_TOO_SMALL);
  }

  Done              = FALSE;
  CurrentString     = Buffer;
  StringCurPos      = 0;
  Status            = EFI_SUCCESS;

  //
  // Get the screen setting and the current cursor location
  //
  Column      = gST->ConOut->Mode->CursorColumn;
  Row         = gST->ConOut->Mode->CursorRow;
  gST->ConOut->QueryMode (gST->ConOut, gST->ConOut->Mode->Mode, &TotalColumn, &TotalRow);

  //
  // Limit the line length to the buffer size or the minimun size of the
  // screen. (The smaller takes effect)
  //
  MaxStr = TotalColumn - Column;
  if (MaxStr > *BufferSize / sizeof (CHAR8)) {
    MaxStr = *BufferSize / sizeof (CHAR8);
  }
  ZeroMem (CurrentString, MaxStr);

  do {
    //
    // Read a key
    //
    gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &EventIndex);
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (EFI_ERROR (Status)) {
      break;
    }

    switch (Key.UnicodeChar) {
    case 0:
      switch (Key.ScanCode) {
      case SCAN_DELETE:
        //
        // Move characters behind current position one character forward
        //
        if (StringCurPos != 0) {
          CurrentString[--StringCurPos] = ' ';
        }
      default:
        break;
      }
      break;

    case CHAR_CARRIAGE_RETURN:
      //
      // All done, print a newline at the end of the string
      //
      gST->ConOut->OutputString(gST->ConOut, L"\n");
      Done = TRUE;
      break;

    case CHAR_BACKSPACE:
      if (StringCurPos != 0) {
        CurrentString[--StringCurPos] = ' ';
      }
      break;

    default:
      CurrentString[StringCurPos++] = (CHAR8)Key.UnicodeChar;
      break;
    }

    if (StringCurPos >= MaxStr) {
      Done = TRUE;
    }

    //
    // Set the cursor position for this key
    //
    gST->ConOut->SetCursorPosition (gST->ConOut, Column + StringCurPos, Row);
  } while (!Done);

  //
  // Return the data to the caller
  //
  *BufferSize = StringCurPos * sizeof (CHAR8);

  return Status;
}

STATIC EFI_FILE_PROTOCOL mStdIn = {
  EFI_FILE_REVISION,
  SysStdIoOpen,
  SysStdIoClose,
  SysStdIoDelete,
  SysStdInRead,
  SysStdIoWrite,
  SysStdIoGetPosition,
  SysStdIoSetPosition,
  SysStdIoGetInfo,
  SysStdIoSetInfo,
  SysStdIoFlush
};

STATIC EFI_FILE_PROTOCOL mStdOut = {
  EFI_FILE_REVISION,
  SysStdIoOpen,
  SysStdIoClose,
  SysStdIoDelete,
  SysStdIoRead,
  SysStdOutWrite,
  SysStdIoGetPosition,
  SysStdIoSetPosition,
  SysStdIoGetInfo,
  SysStdIoSetInfo,
  SysStdIoFlush
};

STATIC EFI_FILE_PROTOCOL mStdErr = {
  EFI_FILE_REVISION,
  SysStdIoOpen,
  SysStdIoClose,
  SysStdIoDelete,
  SysStdIoRead,
  SysStdErrWrite,
  SysStdIoGetPosition,
  SysStdIoSetPosition,
  SysStdIoGetInfo,
  SysStdIoSetInfo,
  SysStdIoFlush
};

/**
  Help method to split unified path string into two parts: drive + file path.
**/
VOID
SplitDrive (
  IN      CONST CHAR16        *FullPath,
     OUT  CHAR16              **Drive,
     OUT  CHAR16              **LeftPath
)
{
  CHAR16      *Path;
  UINTN       DriveLength;
  UINTN       PathLength;

  *Drive = NULL;
  *LeftPath = NULL;

  Path = StrStr (FullPath, EFI_SCRIPT_FILE_DRIVE_STR);
  if (Path != NULL) {
    FullPath = Path + 1;
    Path = StrStr (FullPath, EFI_SCRIPT_FILE_DRIVE_STR);
    if (Path == NULL) {
      return;
    }
    Path += 1;
  }

  DriveLength = Path - FullPath;
  PathLength = StrLen (Path);

  if (DriveLength > 0) {
    *Drive = StrDup (FullPath, DriveLength);
  }

  if (PathLength > 0) {
    *LeftPath = StrDup (Path, PathLength);
  }
}

/**
  Help method to concatenate two path strings.
**/
CHAR16*
PathnCat (
  IN OUT  CHAR16                    *Dst,
  IN      CONST CHAR16              *Src,
  IN      UINTN                     MaxLen
)
{
  EFI_STATUS    Status;
  UINTN         Length;

  if (Dst == NULL || Src == NULL || MaxLen == 0 || Src[0] == '\0') {
    return Dst;
  }

  Length = StrLen (Dst);
  if (Length > 0) {
      if (Dst[Length - 1] != EFI_SCRIPT_FILE_PATH_SEP_CHAR) {
          if (Src[0] != EFI_SCRIPT_FILE_PATH_SEP_CHAR) {
              Status = StrnCatS(Dst, Length + MaxLen + 1, EFI_SCRIPT_FILE_PATH_SEP_STR, 1);
              if (EFI_ERROR(Status)) {
                  return NULL;
              }
              Length += 1;
          }
      }
      else if (Src[0] == EFI_SCRIPT_FILE_PATH_SEP_CHAR) {
          Src += 1;
      }
  }

  Status = StrnCatS (Dst, Length + MaxLen + 1, Src, MaxLen);
  return EFI_ERROR(Status) ? NULL : Dst;
}

/**
  Return the device path of a file system with given label string.
**/
STATIC
EFI_DEVICE_PATH_PROTOCOL*
GetDevicePathByLabel (
  IN CONST CHAR16         *Label
)
{
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Sfs;
  EFI_FILE_HANDLE                   Root;
  UINTN                             HandleCount;
  EFI_HANDLE                        *HandleBuffer;
  UINTN                             Index;
  UINTN                             LabelLength;
  CHAR16                            Buffer[100];
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  EFI_STATUS                        Status;

  DevicePath    = NULL;
  HandleBuffer  = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (!EFI_ERROR(Status)) {
    for (Index = 0; Index < HandleCount; ++Index) {
      Status = gBS->HandleProtocol(HandleBuffer[Index],
                                   &gEfiSimpleFileSystemProtocolGuid,
                                   (VOID **)&Sfs);
      ASSERT_EFI_ERROR(Status);

      Status = Sfs->OpenVolume(Sfs, &Root);
      if (!EFI_ERROR(Status)) {
        LabelLength = ARRAY_SIZE(Buffer) - 1;
        Status = Root->GetInfo(Root, &gEfiFileSystemVolumeLabelInfoIdGuid, &LabelLength, Buffer);
        if (!EFI_ERROR(Status) && StrCmp(Label, Buffer) == 0) {
          DevicePath = DevicePathFromHandle (HandleBuffer[Index]);
          Root->Close (Root);
          break;
        }

        Root->Close (Root);
      }
    }
  }

  FREE_NON_NULL (HandleBuffer);

  return DevicePath;
}

/**
  Return the device handle bound with a file system given in consistent mapping
  name.
**/
STATIC
EFI_HANDLE
EFIAPI
GetHandleFromMapName (
  IN      CONST CHAR16          *MapName
)
{
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  UINTN                             HandleCount;
  EFI_HANDLE                        *HandleBuffer;
  EFI_HANDLE                        Device;
  UINTN                             Index;
  CHAR16                            *Name;
  EFI_STATUS                        Status;

  if (MapName == NULL) {
    return NULL;
  }

  // Check script engine protocols
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR(Status)) {
    return NULL;
  }

  Device = NULL;
  for (Index = 0; Index < HandleCount; ++Index) {
    DevicePath = DevicePathFromHandle (HandleBuffer[Index]);
    Name = GetConsistMappingName (DevicePath, mUsfData.FileSystemTable);

    if (Name != NULL && StrCmp(MapName, Name) == 0) {
      FREE_NON_NULL (Name);
      Device = HandleBuffer[Index];
      break;
    }

    FREE_NON_NULL (Name);
  }

  FreePool(HandleBuffer);

  return Device;
}

/**
  Convert given file path into unified format.
**/
STATIC
EFI_STATUS
EFIAPI
UsfFilePathNorm (
  IN       EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN CONST CHAR16                        *FilePath,
       OUT CHAR16                        **FileNormPath,
  IN       BOOLEAN                       ConvertToDefaultForm
)
{
  CHAR16                            *TempPath;
  CHAR16                            *FilePathCopy;
  CHAR16                            *Path;
  CHAR16                            *Mapping;
  CHAR16                            *Label;
  CONST CHAR16                      *CurDir;
  UINTN                             CurDirLength;
  UINTN                             RootLength;
  UINTN                             PathLength;
  EFI_SHELL_PROTOCOL                *Shell;
  EFI_DEVICE_PATH_PROTOCOL          *DevPath;
  UINTN                             Index;
  USF_PRIVATE_DATA                  *PrivateData;

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);

  *FileNormPath = NULL;
  Path          = NULL;
  Mapping       = NULL;
  DevPath       = NULL;
  CurDir        = NULL;
  Shell         = NULL;

  //
  // Upy will use '/' as path separator. EDK-II uses '\\'.
  //
  PathLength   = StrLen (FilePath);
  FilePathCopy = StrDup (FilePath, PathLength);
  ASSERT (FilePathCopy != NULL);

  for (Index = 0; Index < PathLength; ++Index) {
    if (FilePathCopy[Index] == '/') {
      FilePathCopy[Index] = EFI_SCRIPT_FILE_PATH_SEP_CHAR;
    }
  }

  //
  // Check file path prefix first
  //
  if (StrnCmp(This->Prefix, FilePathCopy, StrLen(This->Prefix)) != 0) {
    //
    // Convert to the internal format.
    //
    gBS->LocateProtocol(&gEfiShellProtocolGuid, NULL, (VOID **)&Shell);
    //
    // Absolute or relative path?
    //
    TempPath = StrStr(FilePathCopy, EFI_SCRIPT_FILE_DRIVE_STR);
    if (TempPath == NULL) {
      //
      // Relative path. Current working directory is needed.
      //
      if (Shell == NULL) {
        if (PrivateData->CurrentDirectory == NULL) {
          FREE_NON_NULL (FilePathCopy);
          return EFI_NO_MAPPING;
        }

        SplitDrive (PrivateData->CurrentDirectory, &Mapping, (CHAR16 **)&CurDir);
      } else {
        CurDir = Shell->GetCurDir(NULL);
        DevPath = Shell->GetDevicePathFromFilePath(CurDir);
        if (DevPath == NULL) {
          FREE_NON_NULL (FilePathCopy);
          return EFI_NO_MAPPING;
        }

        Mapping = GetConsistMappingName (DevPath, mUsfData.FileSystemTable);
        ASSERT (Mapping != NULL);

        CurDir = StrStr (CurDir, EFI_SCRIPT_FILE_DRIVE_STR);
        if (CurDir != NULL) {
          // skip the leading ':' and '\'
          CurDir += (CurDir[1] == EFI_SCRIPT_FILE_PATH_SEP_CHAR) ? 2 : 1;
          CurDir = StrDup (CurDir, 0);
        }
      }

      Path = FilePathCopy;
      if (Path[0] == EFI_SCRIPT_FILE_PATH_SEP_CHAR) {
        FREE_NON_NULL (CurDir);
        Path += 1;  // skip the leading '\'
      }
    } else {
      //
      // Absolute path
      //
      if (Shell == NULL) {
        FREE_NON_NULL (FilePathCopy);
        return EFI_NO_MAPPING;
      }
      DevPath = Shell->GetDevicePathFromFilePath(FilePathCopy);
      Mapping = GetConsistMappingName (DevPath, mUsfData.FileSystemTable);
      ASSERT (Mapping != NULL);

      Path = TempPath + 1;  // skip over the ':'
      CurDir = NULL;
    }

    FREE_NON_NULL (DevPath);

  } else if (ConvertToDefaultForm && StrStr (FilePathCopy, EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL) == NULL) {

    TempPath = StrStr (FilePathCopy, EFI_SCRIPT_FILE_ROOT_TYPE_LABEL);
    if (TempPath != NULL) {
      CurDir =  NULL;
      Label = StrStr (TempPath, EFI_SCRIPT_FILE_DRIVE_STR) + 1;
      Path = StrStr (Label, EFI_SCRIPT_FILE_PATH_SEP_STR);
      if (Path != NULL) {
        Label[Path - Label] = '\0';
        Path += 1;
      } else {
        Path = NULL;
      }

      DevPath = GetDevicePathByLabel (Label);
      if (DevPath == NULL) {
        FREE_NON_NULL (FilePathCopy);
        return EFI_NOT_FOUND;
      }

      Mapping = GetConsistMappingName (DevPath, mUsfData.FileSystemTable);
      ASSERT (Mapping != NULL);
    }
  }

  if (Mapping != NULL) {

    RootLength = StrLen (EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL) + StrLen(Mapping) + 1;
    CurDirLength = (CurDir == NULL || CurDir[0] == '\0') ? 0 : StrLen (CurDir) + 1;
    PathLength = (Path == NULL || Path[0] == '\0') ? 0 : StrLen (Path) + 1;
    TempPath = AllocatePool((RootLength + CurDirLength + PathLength + 1) * 2);
    ASSERT (TempPath != NULL);

    TempPath[0] = '\0';
    StrnCatS (TempPath, RootLength, EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL, StrLen (EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL));
    StrnCatS (TempPath, RootLength, Mapping, StrLen (Mapping));

    if (CurDirLength > 0) {
      PathnCat(TempPath, CurDir, CurDirLength);
    }

    if (PathLength > 0) {
      PathnCat(TempPath, Path, PathLength);
    }

    *FileNormPath = TempPath;
    FREE_NON_NULL (FilePathCopy);
  } else {
    *FileNormPath = FilePathCopy;
  }

  FREE_NON_NULL (Mapping);
  FREE_NON_NULL (CurDir);

  return EFI_SUCCESS;
}

/**
  Open the root of a file system with given consistent mapping name.
**/
STATIC
EFI_STATUS
EFIAPI
UsfOpenRoot (
  IN      CONST CHAR16                  *RootName,
      OUT EFI_FILE_HANDLE               *RootFile
)
{
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Sfs;
  EFI_FILE_HANDLE                   Root;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;
  UINTN                             HandleCount;
  EFI_HANDLE                        *HandleBuffer;
  UINTN                             Index;
  CHAR16                            *Name;
  EFI_STATUS                        Status;

  // Check script engine protocols
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; ++Index) {
    DevicePath = DevicePathFromHandle (HandleBuffer[Index]);
    Name = GetConsistMappingName (DevicePath, mUsfData.FileSystemTable);

    if (Name != NULL && StrCmp(RootName, Name) == 0) {
      Status = gBS->HandleProtocol(HandleBuffer[Index],
                                   &gEfiSimpleFileSystemProtocolGuid,
                                   (VOID **)&Sfs);
      if (!EFI_ERROR(Status)) {
        Status = Sfs->OpenVolume(Sfs, &Root);
      }
    } else {
      Status = EFI_VOLUME_CORRUPTED;
    }

    FREE_NON_NULL (Name);

    if (!EFI_ERROR(Status)) {
      *RootFile = Root;
      break;
    }
  }

  FreePool(HandleBuffer);
  return Status;
}
/**
  Open the root of a file system labeled by given label string.
**/
STATIC
EFI_STATUS
EFIAPI
UsfOpenRootByLabel (
  IN      CONST CHAR16                  *RootLabel,
      OUT EFI_FILE_HANDLE               *RootFile
)
{
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Sfs;
  EFI_FILE_HANDLE                   Root;
  UINTN                             HandleCount;
  EFI_HANDLE                        *HandleBuffer;
  UINTN                             Index;
  UINTN                             LabelLength;
  CHAR16                            Label[100];
  EFI_STATUS                        Status;

  // Check script engine protocols
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; ++Index) {
    Status = gBS->HandleProtocol(HandleBuffer[Index],
                                 &gEfiSimpleFileSystemProtocolGuid,
                                 (VOID **)&Sfs);
    ASSERT_EFI_ERROR(Status);

    Status = Sfs->OpenVolume(Sfs, &Root);
    if (!EFI_ERROR(Status)) {
      LabelLength = 64;
      Status = Root->GetInfo(Root, &gEfiFileSystemVolumeLabelInfoIdGuid, &LabelLength, Label);
      if (!EFI_ERROR(Status) && StrCmp(Label, RootLabel) == 0) {
        *RootFile = Root;
        break;
      }
    }

    Status = EFI_VOLUME_CORRUPTED;
  }

  FreePool(HandleBuffer);
  return Status;
}

/**
  Open a file or directory and return an EFI_FILE_HANDLE for it.

  @param  This        A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FilePath    The Null-terminated string of the name of the file to be
                       opened.
  @param  Mode        The mode to open the file. The only valid combinations
                      that the file may be opened with are: Read, Read/Write,
                      or Create/Read/Write.
  @param  Attributes  Only valid for EFI_FILE_MODE_CREATE, in which case these
                      are the attribute bits for the newly created file.
  @param  FileHandle  Handle of opened file passed back, if succeeded.

  @retval EFI_SUCCESS          The file was opened.
  @retval EFI_NOT_FOUND        The specified file could not be found on the device.
  @retval EFI_NO_MEDIA         The device has no medium.
  @retval EFI_MEDIA_CHANGED    The device has a different medium in it or the medium is no
                               longer supported.
  @retval EFI_DEVICE_ERROR     The device reported an error.
  @retval EFI_VOLUME_CORRUPTED The file system structures are corrupted.
  @retval EFI_WRITE_PROTECTED  An attempt was made to create a file, or open a file for write
                               when the media is write-protected.
  @retval EFI_ACCESS_DENIED    The service denied access to the file.
  @retval EFI_OUT_OF_RESOURCES Not enough resources were available to open the file.
  @retval EFI_VOLUME_FULL      The volume is full.

**/
STATIC
EFI_STATUS
EFIAPI
UsfOpen (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      CHAR16                        *FilePath,
  IN      UINT64                        Mode,
  IN      UINT64                        Attributes,
     OUT  EFI_FILE_HANDLE               *FileHandle
)
{
  EFI_STATUS            Status;
  EFI_FILE_HANDLE       Root;
  CHAR16                *FileNormPath;
  CHAR16                *FileName;
  CHAR16                *RootName;

  if (This == NULL || FilePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Root          = NULL;
  *FileHandle   = NULL;
  FileNormPath  = NULL;

  Status = UsfFilePathNorm (This, FilePath, &FileNormPath, FALSE);
  if (FileNormPath == NULL) {
    DEBUG((DEBUG_INFO, "EFI_SCRIPT_FILE_PROTOCOL.Open(): unrecognized file path [%s]\r\n", FilePath));
    return EFI_UNSUPPORTED;
  }

  DEBUG((DEBUG_INFO, "EFI_SCRIPT_FILE_PROTOCOL.Open(): %s\r\n", FileNormPath));

  RootName = FileNormPath;
  FileName = NULL;
  if (StrStr(RootName, EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL) != NULL) {
    // Default file path, using consistent mapping name as root
    RootName += StrLen (EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL);
    FileName = StrStr (RootName, EFI_SCRIPT_FILE_PATH_SEP_STR);
    if (FileName != NULL) {
      *FileName++ = '\0';
    }
    Status = UsfOpenRoot (RootName, &Root);
  } else if (StrStr(RootName, EFI_SCRIPT_FILE_ROOT_TYPE_LABEL) != NULL) {
    RootName += StrLen (EFI_SCRIPT_FILE_ROOT_TYPE_LABEL);
    FileName = StrStr (RootName, EFI_SCRIPT_FILE_PATH_SEP_STR);
    if (FileName != NULL) {
      *FileName++ = '\0';
    }
    Status = UsfOpenRootByLabel(RootName, &Root);
  } else {
    Status = EFI_NOT_FOUND;
  }

  if (EFI_ERROR(Status)) {
    DEBUG((DEBUG_ERROR, "EFI_SCRIPT_FILE_PROTOCOL.Open(): failed to open root [%s]\r\n", RootName));
    Status = EFI_VOLUME_CORRUPTED;
    goto Exit;
  }

  if (FileName == NULL) {
    *FileHandle = Root;
    goto Exit;
  }

  if (Root != NULL) {
    Status = Root->Open(Root, FileHandle, FileName, Mode, Attributes);
    if (EFI_ERROR(Status)) {
      DEBUG((DEBUG_ERROR, "EFI_SCRIPT_FILE_PROTOCOL.Open(): failed to open file [%s]\r\n", FileName));
    }

    Root->Close(Root);
  }

Exit:
  FREE_NON_NULL (FileNormPath);
  return Status;
}

/**
  Reserve a standard file descriptor (integer) for given file handle.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FileHandle      Handle of an opened file.
  @param  FileDescriptor  An integer mapped to given FileHandle.

  @retval EFI_SUCCESS           A FileDescriptor is mapped to given FileHandle.
  @retval EFI_NOT_FOUND         Too many opened files.
  @retval EFI_INVALID_PARAMETER Invalid FileHandle is given.

**/
STATIC
EFI_STATUS
EFIAPI
UsfMap (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      EFI_FILE_HANDLE               FileHandle,
     OUT  int                           *FileDescriptor
)
{
  EFI_STATUS            Status;
  UINTN                 Index;
  USF_PRIVATE_DATA      *PrivateData;

  if (This == NULL || FileHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);
  Status = EFI_NOT_FOUND;
  *FileDescriptor = -1;
  for (Index = 3; Index < ARRAY_SIZE(PrivateData->FilePool); ++Index) {
    if (PrivateData->FilePool[Index] == NULL) {
      *FileDescriptor = Index;
      PrivateData->FilePool[Index] = FileHandle;
      Status = EFI_SUCCESS;
      break;
    }
  }

  return Status;
}

/**
  Release a standard file descriptor (integer) so that it can be reused later.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FileDescriptor  An integer mapped to given FileHandle.

  @retval EFI_SUCCESS           A FileDescriptor is released.
  @retval EFI_INVALID_PARAMETER Invalid FileDescriptor is given.

**/
STATIC
EFI_STATUS
EFIAPI
UsfUnmap (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      int                           FileDescriptor
)
{
  USF_PRIVATE_DATA      *PrivateData;

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);

  // 0, 1, 2 should not be unmapped.
  if (This == NULL || FileDescriptor < 3 || FileDescriptor >= ARRAY_SIZE(PrivateData->FilePool)) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData->FilePool[FileDescriptor] = NULL;

  return EFI_SUCCESS;
}

/**
  Return corresponding file handle for a given standard file descriptor.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FileDescriptor  An integer mapped to a opened file.
  @param  FileHandle      Handle of file mapped to given FileDescriptor.

  @retval EFI_SUCCESS           File handle found for given FileDescriptor.
  @retval EFI_NOT_FOUND         No handle mapped to given FileDescriptor.
  @retval EFI_INVALID_PARAMETER Invalid FileDescriptor is given.

**/
STATIC
EFI_STATUS
EFIAPI
UsfGetFileHandle (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      int                           FileDescriptor,
     OUT  EFI_FILE_HANDLE               *FileHandle
)
{
  USF_PRIVATE_DATA      *PrivateData;

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);
  *FileHandle = NULL;
  if (This == NULL ||
      FileDescriptor < 0 ||
      FileDescriptor >= ARRAY_SIZE(PrivateData->FilePool)) {
    return EFI_INVALID_PARAMETER;
  }

  *FileHandle = PrivateData->FilePool[FileDescriptor];
  if (*FileHandle == NULL) {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  Setup new STDIN, STDOUT and/or STDERR to support stdio redirection.

  Normally EFI_SCRIPT_FILE_PROTOCOL should setup default STDIN/STDOUT/STDERR
  during initialization.

  @param  This      A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  StdIn     A pointer to new STDIN. NULL means restoring to default.
  @param  StdOut    A pointer to new STDOUT. NULL means restoring to default.
  @param  StdErr    A pointer to new STDERR. NULL means restoring to default.

  @retval EFI_SUCCESS       A FileDescriptor is released.
  @retval EFI_UNSUPPORTED   The EFI_SCRIPT_FILE_PROTOCOL doesn't support
                            stdio redirection.

**/
EFI_STATUS
EFIAPI
UsfSetStdIo (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      EFI_FILE_HANDLE               StdIn,
  IN      EFI_FILE_HANDLE               StdOut,
  IN      EFI_FILE_HANDLE               StdErr
)
{
  USF_PRIVATE_DATA      *PrivateData;

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);

  if (StdIn == NULL) {
    StdIn = &mStdIn;
  }
  if (StdOut == NULL) {
    StdOut = &mStdOut;
  }
  if (StdErr == NULL) {
    StdErr = &mStdErr;
  }

  PrivateData->FilePool[STDIN_FILENO] = StdIn;
  PrivateData->FilePool[STDOUT_FILENO] = StdOut;
  PrivateData->FilePool[STDERR_FILENO] = StdErr;

  return EFI_SUCCESS;
}

/**
  Retrieve file systems (EFI_SIMPLE_FILE_SYSTEM_PROTOCOL) currently available.

  Note: The name of each file system returned is in the same scheme as
        consistent mapping in UEFI Shell Spec.

  @param  This          A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  Table         Array of string pointers, each pointing to a consistent
                        mapping name of a simple file system.
  @param  TableLength   Element number of Table.

  @retval EFI_SUCCESS     At least one file system found.
  @retval EFI_NOT_FOUND   No EFI_SIMPLE_FILE_SYSTEM_PROTOCOL instance located.

**/
EFI_STATUS
EFIAPI
UsfGetFileSystemTable (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
     OUT  CHAR16                        ***Table,
     OUT  UINTN                         *TableLength
)
{
  EFI_STATUS                  Status;
  UINTN                       Index;
  USF_PRIVATE_DATA            *PrivateData;
  UINTN                       HandleCount;
  EFI_HANDLE                  *HandleBuffer;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  CHAR16                      *FsName;
  UINTN                       FsIndex;

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR(Status) || HandleCount == 0) {
    *Table = NULL;
    *TableLength = 0;
    return EFI_NOT_FOUND;
  }

  *Table = AllocatePool(HandleCount * sizeof(CHAR16 *));
  ASSERT (*Table != NULL);

  for (Index = 0, FsIndex = 0; Index < HandleCount; ++Index) {
    DevicePath = DevicePathFromHandle (HandleBuffer[Index]);
    if (DevicePath != NULL) {
      FsName = GetConsistMappingName (DevicePath, PrivateData->FileSystemTable);
      if (FsName != NULL) {
        (*Table)[FsIndex++] = FsName;
      }
    }
  }

  *TableLength = FsIndex;
  FreePool(HandleBuffer);

  return EFI_SUCCESS;
}

/**
  Get current working directory.

  If gEfiShellProtocolGuid can be located, the current working directory of
  Shell should be returned. Otherwise, the directory of image file installed
  this protocol should be returned.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.

  @retval Valid pointer   Current directory is valid and found.
  @retval NULL            No file system found and switched to.

**/
CHAR16*
EFIAPI
UsfGetCurrentDirectory (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This
)
{
  EFI_STATUS                  Status;
  CHAR16                      *CurDir;
  CONST CHAR16                *Path;
  EFI_SHELL_PROTOCOL          *Shell;
  USF_PRIVATE_DATA            *PrivateData;

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);

  Status = gBS->LocateProtocol(&gEfiShellProtocolGuid, NULL, (VOID **)&Shell);
  if (!EFI_ERROR(Status)) {
    CurDir = NULL;
    Path = Shell->GetCurDir (NULL);
    if (Path != NULL) {
      Status = UsfFilePathNorm(This, Path, &CurDir, TRUE);
      if (EFI_ERROR (Status) || CurDir == NULL) {
        return NULL;
      }

      FREE_NON_NULL (PrivateData->CurrentDirectory);
      PrivateData->CurrentDirectory = CurDir;
    }
  }

  return StrDup(PrivateData->CurrentDirectory, 0);
}

/**
  Change current working directory.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  CurDir          String pointer to a directory path.



  @retval EFI_SUCCESS     Current directory is changed successfully.
  @retval EFI_NO_MAPPING  The corresponding file system is not found.
  @retval EFI_NOT_FOUND   The directory doesn't exist.

**/
EFI_STATUS
EFIAPI
UsfChangeCurrentDirectory (
  IN EFI_SCRIPT_FILE_PROTOCOL     *This,
  IN CONST CHAR16                 *CurDir
)
{
  EFI_STATUS                  Status;
  EFI_SHELL_PROTOCOL          *Shell;
  USF_PRIVATE_DATA            *PrivateData;
  CHAR16                      *Path;

  if (This == NULL || CurDir == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  PrivateData = USF_PRIVATE_DATA_FROM_SF(This);

  Status = UsfFilePathNorm (This, CurDir, &Path, TRUE);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gBS->LocateProtocol (&gEfiShellProtocolGuid, NULL, (VOID **)&Shell);
  if (!EFI_ERROR(Status)) {
    Shell->SetCurDir (NULL, Path);
  }

  FREE_NON_NULL (PrivateData->CurrentDirectory);
  PrivateData->CurrentDirectory = Path;

  return EFI_SUCCESS;
}

/**
  Convert a given device path (representing a file) to file path in default
  format.

  @param  This        A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  DevicePath  Pointer to a device path representing a file.

  @retval Valid pointer   If corresponding file system exists
  @retval NULL            The given device path has no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
                          installed.

**/
CHAR16*
EFIAPI
UsfGetFilePathFromDevicePath (
  IN        EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN  CONST EFI_DEVICE_PATH_PROTOCOL      *DevicePath
)
{
  EFI_STATUS                  Status;
  USF_PRIVATE_DATA            *PrivateData;
  CHAR16                      *Mapping;
  CHAR16                      *Path;
  EFI_HANDLE                  Handle;
  EFI_DEVICE_PATH_PROTOCOL    *RemainingPath;
  CHAR16                      *FilePath;
  UINTN                       FilePathLength;
  UINTN                       Length;

  if (DevicePath == NULL) {
    return NULL;
  }

  PrivateData   = USF_PRIVATE_DATA_FROM_SF(This);
  Handle        = NULL;
  RemainingPath = (EFI_DEVICE_PATH_PROTOCOL *)DevicePath;
  Status        = gBS->LocateDevicePath(
                        &gEfiSimpleFileSystemProtocolGuid,
                        &RemainingPath,
                        &Handle
                        );

  if (EFI_ERROR(Status)) {
    return NULL;
  }

  Mapping = GetConsistMappingName(DevicePath, PrivateData->FileSystemTable);
  if (Mapping == NULL) {
    return NULL;
  }

  if (RemainingPath != NULL && !IsDevicePathEnd(RemainingPath)) {
    FilePath = ConvertDevicePathToText(RemainingPath, FALSE, FALSE);
    FilePathLength = StrLen (FilePath);
  } else {
    FilePath = NULL;
    FilePathLength = 0;
  }

  Length = StrLen(EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL) + StrLen(Mapping)
           + FilePathLength + 3;
  Path = AllocatePool(Length * sizeof(CHAR16));
  ASSERT (Path != NULL);

  Path[0] = '\0';
  StrnCatS (Path, Length, EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL, StrLen (EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL));
  StrnCatS (Path, Length, Mapping, StrLen (Mapping));
  PathnCat (Path, FilePath, FilePathLength);

  return Path;
}

/**
  Convert a normal file path to corresponding device path.

  @param  This      A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  Path      Path string of a file.

  @retval Valid pointer   Corresponding file system device is found.
  @retval NULL            Corresponding file system device doesn't exist.

**/
EFI_DEVICE_PATH_PROTOCOL*
EFIAPI
UsfGetDevicePathFromFilePath (
  IN        EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN  CONST CHAR16                        *Path
)
{
  CHAR16                          *NewPath;
  CHAR16                          *MapName;
  CHAR16                          *FilePath;
  EFI_DEVICE_PATH_PROTOCOL        *DevicePath;
  EFI_HANDLE                      Handle;
  EFI_STATUS                      Status;

  if (Path == NULL) {
    return NULL;
  }

  MapName = NULL;
  NewPath = NULL;

  Status = UsfFilePathNorm (This, (CHAR16 *)Path, &NewPath, TRUE);
  if (EFI_ERROR(Status)) {
    return NULL;
  }

  SplitDrive (NewPath, &MapName, &FilePath);
  Handle = GetHandleFromMapName(MapName);
  if (Handle != NULL) {
    if (FilePath != NULL) {
      DevicePath = FileDevicePath(Handle, FilePath);
    } else {
      DevicePath = FileDevicePath(Handle, EFI_SCRIPT_FILE_PATH_SEP_STR);
    }
  } else {
    DevicePath = NULL;
  }

  FREE_NON_NULL (NewPath);
  FREE_NON_NULL (FilePath);
  FREE_NON_NULL (MapName);

  return DevicePath;
}

STATIC USF_PRIVATE_DATA mUsfData = {
  USF_PRIVATE_DATA_SIGNATURE,
  {
    EFI_SCRIPT_FILE_PATH_PREFIX,
    EFI_SCRIPT_FILE_PATH_SEP_CHAR,
    UsfOpen,
    UsfMap,
    UsfUnmap,
    UsfSetStdIo,
    UsfGetFileHandle,
    UsfGetFileSystemTable,
    UsfGetFilePathFromDevicePath,
    UsfGetDevicePathFromFilePath,
    UsfGetCurrentDirectory,
    UsfChangeCurrentDirectory
  },
  {
    0
  },
  NULL,
  NULL
};

STATIC BOOLEAN  mProtocolInstalled = FALSE;

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
  IN  EFI_HANDLE                  ImageHandle,
  OUT EFI_SCRIPT_FILE_PROTOCOL    **Protocol
)
{
  EFI_STATUS                  Status;

  *Protocol = NULL;
  Status = gBS->LocateProtocol (
                  &gEfiScriptFileProtocolGuid,
                  NULL,
                  (VOID **)Protocol
                  );
  if (!EFI_ERROR (Status)) {
    return EFI_ALREADY_STARTED;
  }

  SetMem (&mUsfData.FilePool, sizeof (mUsfData.FilePool), 0);

  mUsfData.FilePool[STDIN_FILENO]  = &mStdIn;
  mUsfData.FilePool[STDOUT_FILENO] = &mStdOut;
  mUsfData.FilePool[STDERR_FILENO] = &mStdErr;

  Status = ConsistMappingTableInit(&mUsfData.FileSystemTable);
  ASSERT_EFI_ERROR (Status);

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEfiScriptFileProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  &(mUsfData.ScriptFile)
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_STARTED;
  }

  mProtocolInstalled = TRUE;
  *Protocol = &mUsfData.ScriptFile;

  return EFI_SUCCESS;
}

/**
  Destroy and uninstall the script file protocol.

  @retval EFI_SUCCESS     The script file protocol is uninstalled successfully.
  @retval EFI_NOT_FOUND   The script file protocol has been already uninstalled.

**/
EFI_STATUS
EFIAPI
UsfDeinit (
  EFI_HANDLE        ImageHandle
)
{
  EFI_STATUS    Status;

  Status = EFI_SUCCESS;
  if (mProtocolInstalled && ImageHandle != NULL) {
    Status = gBS->UninstallProtocolInterface (
                    ImageHandle,
                    &gEfiScriptFileProtocolGuid,
                    &(mUsfData.ScriptFile)
                    );
    ConsistMappingTableDeinit(mUsfData.FileSystemTable);
  }

  return Status;
}

