/**@file

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Name:

  ScriptFileProtocol.h

Abstract:



**/

#ifndef   UEFI_SCRIPT_FILE_PROTOCOL_H
#define   UEFI_SCRIPT_FILE_PROTOCOL_H

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>

///
/// Global ID for the Script File Protocol
///
///   (4FFB124A-6A81-4982-A67E-C4EF57E5F074)
///
#define EFI_SCRIPT_FILE_PROTOCOL_GUID \
  { 0x4ffb124a, 0x6a81, 0x4982, 0xa6, 0x7e, 0xc4, 0xef, 0x57, 0xe5, 0xf0, 0x74 }

//
// The default of path formats supported by EFI_SCRIPT_FILE_PROTOCOL
//
#define EFI_SCRIPT_FILE_PATH_PREFIX              L"\\\\"
#define EFI_SCRIPT_FILE_DRIVE_CHAR               L':'
#define EFI_SCRIPT_FILE_DRIVE_STR                L":"
#define EFI_SCRIPT_FILE_PATH_SEP_CHAR            L'\\'
#define EFI_SCRIPT_FILE_PATH_SEP_STR             L"\\"
#define EFI_SCRIPT_FILE_ROOT_TYPE_INTERNAL       L"\\\\:"
#define EFI_SCRIPT_FILE_ROOT_TYPE_LABEL          L"\\\\LABEL:"

///
/// Declare forward reference
///
typedef struct _EFI_SCRIPT_FILE_PROTOCOL      EFI_SCRIPT_FILE_PROTOCOL;

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
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_FILE_OPEN) (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      CHAR16                        *FilePath,
  IN      UINT64                        Mode,
  IN      UINT64                        Attributes,
     OUT  EFI_FILE_HANDLE               *FileHandle
  );

/**
  Return corresponding file handle for a given standard file descriptor.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FileDescriptor  An integer mapped to a opened file.
  @param  FileHandle      Handle of file mapped to given FileDescriptor.

  @retval EFI_SUCCESS           File handle found for given FileDescriptor.
  @retval EFI_NOT_FOUND         No handle mapped to given FileDescriptor.
  @retval EFI_INVALID_PARAMETER Invalid FileDescriptor is given.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_FILE_GET_FH) (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      int                           FileDescriptor,
     OUT  EFI_FILE_HANDLE               *FileHandle
  );

/**
  Reserve a standard file descriptor (integer) for given file handle.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FileHandle      Handle of an opened file.
  @param  FileDescriptor  An integer mapped to given FileHandle.

  @retval EFI_SUCCESS           A FileDescriptor is mapped to given FileHandle.
  @retval EFI_NOT_FOUND         Too many opened files.
  @retval EFI_INVALID_PARAMETER Invalid FileHandle is given.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_FILE_MAP) (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      EFI_FILE_HANDLE               FileHandle,
     OUT  int                           *FileDescriptor
  );

/**
  Release a standard file descriptor (integer) so that it can be reused later.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FileDescriptor  An integer mapped to given FileHandle.

  @retval EFI_SUCCESS           A FileDescriptor is released.
  @retval EFI_INVALID_PARAMETER Invalid FileDescriptor is given.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_FILE_UNMAP) (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN      int                           FileDescriptor
  );

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
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_FILE_SET_STDIO) (
  IN      EFI_SCRIPT_FILE_PROTOCOL    *This,
  IN      EFI_FILE_HANDLE             StdIn,
  IN      EFI_FILE_HANDLE             StdOut,
  IN      EFI_FILE_HANDLE             StdErr
  );

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
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_FILE_GET_FST) (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This,
     OUT  CHAR16                        ***Table,
     OUT  UINTN                         *TableLength
  );

/**
  Convert a given device path (representing a file) to file path in default
  format.

  @param  This        A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  DevicePath  Pointer to a device path representing a file.

  @retval Valid pointer   If corresponding file system exists
  @retval NULL            The given device path has no EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
                          installed.

**/
typedef
CHAR16*
(EFIAPI *EFI_SCRIPT_FILE_GET_FP) (
  IN        EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN  CONST EFI_DEVICE_PATH_PROTOCOL      *DevicePath
);

/**
  Convert a normal file path to corresponding device path.

  @param  This      A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  FilePath  Path string of a file.

  @retval Valid pointer   Corresponding file system device is found.
  @retval NULL            Corresponding file system device doesn't exist.

**/
typedef
EFI_DEVICE_PATH_PROTOCOL*
(EFIAPI *EFI_SCRIPT_FILE_GET_DP) (
  IN        EFI_SCRIPT_FILE_PROTOCOL      *This,
  IN  CONST CHAR16                        *FilePath
);

/**
  Get current working directory.

  If gEfiShellProtocolGuid can be located, the current working directory of
  Shell should be returned. Otherwise, the directory of image file installed
  this protocol should be returned.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.

  @retval Valid pointer   Current directory is valid and found.
  @retval NULL            No file system found and switched to.

**/
typedef
CHAR16*
(EFIAPI *EFI_SCRIPT_FILE_GET_CWD) (
  IN      EFI_SCRIPT_FILE_PROTOCOL      *This
);

/**
  Change current working directory.

  @param  This            A pointer to the EFI_SCRIPT_FILE_PROTOCOL instance.
  @param  CurDir          String pointer to a directory path.



  @retval EFI_SUCCESS     Current directory is changed successfully.
  @retval EFI_NO_MAPPING  The corresponding file system is not found.
  @retval EFI_NOT_FOUND   The directory doesn't exist.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_FILE_CH_CWD) (
  IN EFI_SCRIPT_FILE_PROTOCOL     *This,
  IN CONST CHAR16                 *CurDir
);

///
/// The EFI_SCRIPT_FILE_PROTOCOL provides a way to fill the gap among script
/// engine, UEFI supported file system and UEFI internal device representation,
/// so that the script engine can keep its own way (usually standard c lib way)
/// to do file operations, without much adapting work.
///
struct _EFI_SCRIPT_FILE_PROTOCOL {
  CONST CHAR16        *Prefix;
  CONST CHAR16        Separator;

  EFI_SCRIPT_FILE_OPEN            Open;
  EFI_SCRIPT_FILE_MAP             Map;
  EFI_SCRIPT_FILE_UNMAP           Unmap;
  EFI_SCRIPT_FILE_SET_STDIO       SetStdIo;
  EFI_SCRIPT_FILE_GET_FH          GetFileHandle;
  EFI_SCRIPT_FILE_GET_FST         GetFileSystemTable;
  EFI_SCRIPT_FILE_GET_FP          GetFilePath;
  EFI_SCRIPT_FILE_GET_DP          GetDevicePath;
  EFI_SCRIPT_FILE_GET_CWD         GetCurrentDirectory;
  EFI_SCRIPT_FILE_CH_CWD          ChangeCurrentDirectory;
};

extern EFI_GUID gEfiScriptFileProtocolGuid;

#endif /* UEFI_SCRIPT_FILE_PROTOCOL_H */

