/**@file

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

Module Name:

  ScriptEngineProtocol.h

Abstract:



**/

#ifndef   UEFI_SCRIPT_ENGIEN_PROTOCOL_H
#define   UEFI_SCRIPT_ENGIEN_PROTOCOL_H

#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>

///
/// Global ID for the Script Engine Protocol
///
#define EFI_SCRIPT_ENGINE_PROTOCOL_GUID \
  { 0x63e9dd8c, 0xbe55, 0x430a, 0xae, 0xb1, 0x1d, 0x9d, 0xc1, 0x25, 0xaf, 0x7d }

//
// Script engine type for MicroPython
//
#define EFI_SCRIPT_ENGINE_TYPE_MICROPYTHON   SIGNATURE_32('u', 'p', 'y', '\0')

///
/// Declare forward reference for the Script Engine Protocol
///
typedef struct _EFI_SCRIPT_ENGINE_PROTOCOL    EFI_SCRIPT_ENGINE_PROTOCOL;

/**
  Execute script code given in string.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Script    A pointer to string buffer contaning the script code.
  @param  Length    Length of string buffer passed by parameter SCript.
  @param  Sharable  Value of TRUE is used to tell script engine not to destroy
                    itself after the script has been executed. Otherwise, the
                    script engine will initialize itself upon each calling of
                    this protocol API.

  @retval EFI_SUCCESS           The script is executed successfully.
  @retval EFI_LOAD_ERROR        The script is executed with error.
  @retval EFI_ALREADY_STARTED   There's another script in running.
  @retval EFI_OUT_OF_RESOURCES  Not enough resource to start the script engine.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_ENGINE_EXECUTE) (
  IN EFI_SCRIPT_ENGINE_PROTOCOL   *This,
  IN UINT8                        *Script,
  IN UINTN                        Length,
  IN BOOLEAN                      Sharable
  );

/**
  Evaluate a line of script code and return the result.

  @param  This        A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  ScriptLine  A pointer to string buffer contaning the script code.
  @param  Result      Buffer used to hold the result of evaluation. The memory
                      layout of the buffer is script engine type dependent.

  @retval EFI_SUCCESS           The script is executed successfully.
  @retval EFI_LOAD_ERROR        The script is executed with error.
  @retval EFI_ALREADY_STARTED   There's another script in running.
  @retval EFI_OUT_OF_RESOURCES  Not enough resource to start the script engine.
  @retval EFI_UNSUPPORTED       The script engine doesn't support evaluation mode.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_ENGINE_EVALUATE) (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL  *This,
  IN      CHAR8                       *ScriptLine,
  IN  OUT VOID                        *Result
  );

/**
  Execute script code given in string asynchronously. This API will return
  immediately after launching the script engine and feeding the script to it.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Script    A pointer to string buffer contaning the script code.
  @param  Length    Length of string buffer passed by parameter SCript.
  @param  Sharable  Value of TRUE is used to tell script engine not to destroy
                    itself after the script has been executed. Otherwise, the
                    script engine will initialize itself upon each calling of
                    this protocol API.

  @retval EFI_SUCCESS           The script is lauched successfully.
  @retval EFI_LOAD_ERROR        The script is lauched with error.
  @retval EFI_ALREADY_STARTED   There's another script in running.
  @retval EFI_OUT_OF_RESOURCES  Not enough resource to start the script engine.
  @retval EFI_UNSUPPORTED       The script engine doesn't support async mode.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_ENGINE_EXECUTE_ASYNC) (
  IN EFI_SCRIPT_ENGINE_PROTOCOL   *This,
  IN UINT8                        *Script,
  IN UINTN                        Length,
  IN BOOLEAN                      Sharable
  );

/**
  Get the type of current script engine instance.

  @param  This        A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.

  @retval UINT32 value used to identify the type of current script engine.

**/
typedef
UINT32
(EFIAPI *EFI_SCRIPT_ENGINE_GET_TYPE) (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This
  );

/**
  Set or unset an environment variable.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Name      The name of the environment variable.
  @param  Value     The string value of the environment variable. NULL means
                    unsetting.

  @retval EFI_SUCCESS           The environment variable is set successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support environment
                                variable.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_ENGINE_SET_ENV) (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      CHAR16                        *Name,
  IN      CHAR16                        *Value
  );

/**
  Set new STDIN, STDOUT and/or STDOUT for script engine.

  Script engine normally should setup its own STDIN/STDOUT/STDERR during
  initialization. This API allows the user of script engine to redirect
  stdio to different devices. Any NULL value passed in means restoring
  the default setting of corresponding stdio.

  All stdio must be file-compatible object (aka. type of EFI_FILE_HANDLE).

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  StdIn     A pointer to new STDIN.
  @param  StdOut    A pointer to new STDOUT.
  @param  StdErr    A pointer to new STDERR.

  @retval EFI_SUCCESS           The stdio is re-set successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support changing stdio.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_ENGINE_SET_STDIO) (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      EFI_FILE_HANDLE               StdIn,
  IN      EFI_FILE_HANDLE               StdOut,
  IN      EFI_FILE_HANDLE               StdErr
  );

/**
  Pass standard command line options to script engine.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Argc      Number of command line options.
  @param  Argv      Array of command line options.

  @retval EFI_SUCCESS           The operation is done successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support processing
                                command line options.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_ENGINE_SET_ARGS) (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      UINTN                         Argc,
  IN      CONST CHAR16                  *Argv[]
  );

/**
  Add a path to system PATH.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Path      A path string to be added. A NULL value means to clear
                    the system PATH.

  @retval EFI_SUCCESS           The path is added successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support system PATH.

**/
typedef
EFI_STATUS
(EFIAPI *EFI_SCRIPT_ENGINE_SET_PATH) (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      CONST CHAR16                  *Path
  );

///
/// The EFI_SCRIPT_ENGINE_PROTOCOL provides a capability to execute script
/// code in UEFI BIOS environment, which facilitates the faster and easier
/// testing, debugging and diagnosing during UEFI BIOS development and
/// deployment.
///
struct _EFI_SCRIPT_ENGINE_PROTOCOL {
  EFI_SCRIPT_ENGINE_GET_TYPE        GetType;
  EFI_SCRIPT_ENGINE_EXECUTE         Execute;
  EFI_SCRIPT_ENGINE_EXECUTE_ASYNC   ExecuteAsync;
  EFI_SCRIPT_ENGINE_EVALUATE        Evaluate;
  EFI_SCRIPT_ENGINE_SET_ENV         SetSysEnv;
  EFI_SCRIPT_ENGINE_SET_ARGS        SetSysArgs;
  EFI_SCRIPT_ENGINE_SET_PATH        SetSysPath;
  EFI_SCRIPT_ENGINE_SET_STDIO       SetSysStdIo;
};

extern EFI_GUID gEfiScriptEngineProtocolGuid;

#endif /* UEFI_SCRIPT_ENGIEN_PROTOCOL_H */

