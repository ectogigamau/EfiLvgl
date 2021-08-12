/** @file
  Main file for script engine shell command functions.

  Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved. <BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "MicroPython.h"

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

  *ScriptProtocol = NULL;

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
  Read and return script content from given file.

  @param  ScriptFile        File path.
  @param  Script            Pointer to buffer containing the file content.
  @param  ScriptLength      Length of data read from file.

  @retval EFI_SUCCESS       The file was opened and read successfully.
  @retval non-EFI_SUCCESS   Failed to read the file.

**/
STATIC
EFI_STATUS
EFIAPI
GetScriptFromFile (
  IN      CHAR16                        *ScriptFile,
      OUT UINT8                         **Script,
      OUT UINTN                         *ScriptLength
)
{
  EFI_STATUS                      Status;
  EFI_SCRIPT_FILE_PROTOCOL        *Sfp;
  EFI_FILE_HANDLE                 Fh;
  UINT64                          Size;
  EFI_FILE_INFO                   *Info;

  *Script = NULL;
  *ScriptLength = 0;

  Status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&Sfp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Sfp->Open(Sfp, ScriptFile, EFI_FILE_MODE_READ, 0, &Fh);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = EFI_DEVICE_ERROR;
  Info = FileHandleGetInfo(Fh);
  if (Info != NULL && (Info->Attribute & EFI_FILE_DIRECTORY) == 0) {
    Status = FileHandleGetSize(Fh, &Size);
    if (!EFI_ERROR(Status)) {
      *ScriptLength = (UINTN)(Size + 1);
      *Script = AllocatePool(*ScriptLength);
      ASSERT (*Script != NULL);

      Status = FileHandleRead(Fh, ScriptLength, *Script);
      if (!EFI_ERROR(Status)) {
        (*Script)[*ScriptLength] = '\0';
      } else {
        FREE_NON_NULL(*Script);
        *ScriptLength = 0;
      }
    }
  }

  FREE_NON_NULL(Info);
  FileHandleClose(Fh);

  return Status;
}

/**
  Return the pointer to the first occurrence of any character from a list of characters.

  @param[in] String           the string to parse
  @param[in] CharacterList    the list of character to look for
  @param[in] EscapeCharacter  An escape character to skip

  @return the location of the first character in the string
  @retval CHAR_NULL no instance of any character in CharacterList was found in String
**/
STATIC
CONST CHAR16*
FindFirstCharacter(
  IN CONST CHAR16 *String,
  IN CONST CHAR16 *CharacterList,
  IN CONST CHAR16 EscapeCharacter
  )
{
  UINT32 WalkChar;
  UINT32 WalkStr;

  for (WalkStr = 0; WalkStr < StrLen(String); WalkStr++) {
    if (String[WalkStr] == EscapeCharacter) {
      WalkStr++;
      continue;
    }
    for (WalkChar = 0; WalkChar < StrLen(CharacterList); WalkChar++) {
      if (String[WalkStr] == CharacterList[WalkChar]) {
        return (&String[WalkStr]);
      }
    }
  }
  return (String + StrLen(String));
}

/**
  Return the next parameter's end from a command line string.

  @param[in] String        the string to parse
**/
STATIC
CONST CHAR16*
FindEndOfParameter(
  IN CONST CHAR16 *String
  )
{
  CONST CHAR16 *First;
  CONST CHAR16 *CloseQuote;

  First = FindFirstCharacter(String, L" \"", L'^');

  //
  // nothing, all one parameter remaining
  //
  if (*First == CHAR_NULL) {
    return (First);
  }

  //
  // If space before a quote (or neither found, i.e. both CHAR_NULL),
  // then that's the end.
  //
  if (*First == L' ') {
    return (First);
  }

  CloseQuote = FindFirstCharacter (First+1, L"\"", L'^');

  //
  // We did not find a terminator...
  //
  if (*CloseQuote == CHAR_NULL) {
    return (NULL);
  }

  return (FindEndOfParameter (CloseQuote+1));
}

/**
  Return the next parameter from a command line string.

  This function moves the next parameter from Walker into TempParameter and moves
  Walker up past that parameter for recursive calling.  When the final parameter
  is moved *Walker will be set to NULL;

  Temp Parameter must be large enough to hold the parameter before calling this
  function.

  This will also remove all remaining ^ characters after processing.

  @param[in, out] Walker          pointer to string of command line.  Adjusted to
                                  reminaing command line on return
  @param[in, out] TempParameter   pointer to string of command line item extracted.
  @param[in]      Length          buffer size of TempParameter.
  @param[in]      StripQuotation  if TRUE then strip the quotation marks surrounding
                                  the parameters.

  @return   EFI_INALID_PARAMETER  A required parameter was NULL or pointed to a NULL or empty string.
  @return   EFI_NOT_FOUND         A closing " could not be found on the specified string
**/
STATIC
EFI_STATUS
GetNextParameter(
  IN OUT CHAR16   **Walker,
  IN OUT CHAR16   **TempParameter,
  IN CONST UINTN  Length,
  IN BOOLEAN      StripQuotation
  )
{
  CONST CHAR16 *NextDelim;

  if (Walker           == NULL
    ||*Walker          == NULL
    ||TempParameter    == NULL
    ||*TempParameter   == NULL
    ){
    return (EFI_INVALID_PARAMETER);
  }


  //
  // make sure we dont have any leading spaces
  //
  while ((*Walker)[0] == L' ') {
      (*Walker)++;
  }

  //
  // make sure we still have some params now...
  //
  if (StrLen(*Walker) == 0) {
DEBUG_CODE_BEGIN();
    *Walker        = NULL;
DEBUG_CODE_END();
    return (EFI_INVALID_PARAMETER);
  }

  NextDelim = FindEndOfParameter(*Walker);

  if (NextDelim == NULL){
DEBUG_CODE_BEGIN();
    *Walker        = NULL;
DEBUG_CODE_END();
    return (EFI_NOT_FOUND);
  }

  StrnCpyS(*TempParameter, Length / sizeof(CHAR16), (*Walker), NextDelim - *Walker);

  //
  // Add a CHAR_NULL if we didnt get one via the copy
  //
  if (*NextDelim != CHAR_NULL) {
    (*TempParameter)[NextDelim - *Walker] = CHAR_NULL;
  }

  //
  // Update Walker for the next iteration through the function
  //
  *Walker = (CHAR16*)NextDelim;

  //
  // Remove any non-escaped quotes in the string
  // Remove any remaining escape characters in the string
  //
  for (NextDelim = FindFirstCharacter(*TempParameter, L"\"^", CHAR_NULL)
    ; *NextDelim != CHAR_NULL
    ; NextDelim = FindFirstCharacter(NextDelim, L"\"^", CHAR_NULL)
    ) {
    if (*NextDelim == L'^') {

      //
      // eliminate the escape ^
      //
      CopyMem ((CHAR16*)NextDelim, NextDelim + 1, StrSize (NextDelim + 1));
      NextDelim++;
    } else if (*NextDelim == L'\"') {

      //
      // eliminate the unescaped quote
      //
      if (StripQuotation) {
        CopyMem ((CHAR16*)NextDelim, NextDelim + 1, StrSize (NextDelim + 1));
    } else{
        NextDelim++;
    }
    }
  }

  return EFI_SUCCESS;
}

/**
  Remove spaces/tabes at the beginning and end of given string.

  @param  String        Buffer of the string.

  @retval EFI_SUCCESS   Spaces/tabs are removed.

**/
STATIC
EFI_STATUS
TrimSpaces(
  IN CHAR16 **String
  )
{
  ASSERT(String != NULL);
  ASSERT(*String!= NULL);
  //
  // Remove any spaces and tabs at the beginning of the (*String).
  //
  while (((*String)[0] == L' ') || ((*String)[0] == L'\t')) {
    CopyMem((*String), (*String)+1, StrSize((*String)) - sizeof((*String)[0]));
  }

  //
  // Remove any spaces and tabs at the end of the (*String).
  //
  while ((StrLen (*String) > 0) && (((*String)[StrLen((*String))-1] == L' ') || ((*String)[StrLen((*String))-1] == L'\t'))) {
    (*String)[StrLen((*String))-1] = CHAR_NULL;
  }

  return (EFI_SUCCESS);
}

/**
  Parse load options and convert them to standard C argc/argv.

  @param  LoadOptions       Pointer to load option string.
  @param  StripQuotation    Flag to indicate if or not strip the quotations.
  @param  Argv              Pointer to an array of options parsed.
  @param  Argc              Pointer to the number of entries in Argv.

  @retval EFI_SUCCESS           Parse the options successfully.
  @retval EFI_OUT_OF_RESOURCES  Not enough resource to do parse.

**/
STATIC
EFI_STATUS
ParseLoadOptionsToArgs(
  IN CONST CHAR16 *LoadOptions,
  IN BOOLEAN      StripQuotation,
  IN OUT CHAR16   ***Argv,
  IN OUT UINTN    *Argc
  )
{
  UINTN       Count;
  CHAR16      *TempParameter;
  CHAR16      *Walker;
  CHAR16      *NewParam;
  CHAR16      *NewLoadOptions;
  UINTN       Size;
  EFI_STATUS  Status;

  ASSERT(Argc != NULL);
  ASSERT(Argv != NULL);

  if (LoadOptions == NULL || StrLen(LoadOptions) == 0) {
    (*Argc) = 0;
    (*Argv) = NULL;
    return (EFI_SUCCESS);
  }

  NewLoadOptions = AllocateCopyPool(StrSize(LoadOptions), LoadOptions);
  if (NewLoadOptions == NULL){
    return (EFI_OUT_OF_RESOURCES);
  }

  TrimSpaces(&NewLoadOptions);
  Size = StrSize(NewLoadOptions);
  TempParameter = AllocateZeroPool(Size);
  if (TempParameter == NULL) {
    FREE_NON_NULL(NewLoadOptions);
    return (EFI_OUT_OF_RESOURCES);
  }

  for ( Count = 0
      , Walker = (CHAR16*)NewLoadOptions
      ; Walker != NULL && *Walker != CHAR_NULL
      ; Count++
      ) {
    if (EFI_ERROR(GetNextParameter(&Walker, &TempParameter, Size, TRUE))) {
      break;
    }
  }

  //
  // lets allocate the pointer array
  //
  (*Argv) = AllocateZeroPool((Count)*sizeof(CHAR16*));
  if (*Argv == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto Done;
  }

  *Argc = 0;
  Walker = (CHAR16*)NewLoadOptions;
  while(Walker != NULL && *Walker != CHAR_NULL) {
    SetMem16(TempParameter, Size, CHAR_NULL);
    if (EFI_ERROR(GetNextParameter(&Walker, &TempParameter, Size, StripQuotation))) {
      Status = EFI_INVALID_PARAMETER;
      goto Done;
    }

    NewParam = AllocateCopyPool(StrSize(TempParameter), TempParameter);
    if (NewParam == NULL){
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }
    ((CHAR16**)(*Argv))[(*Argc)] = NewParam;
    (*Argc)++;
  }
  ASSERT(Count >= (*Argc));
  Status = EFI_SUCCESS;

Done:
  FREE_NON_NULL(TempParameter);
  FREE_NON_NULL(NewLoadOptions);
  return (Status);
}

/**
  Add a path as system path.

  @param  ScriptProtocol    Pointer to script engine protocol.
  @param  Path              Pointer to a file system PATH.

  @retval EFI_SUCCESS             Path is added successfully.
  @retval EFI_INVALID_PARAMETER   Not a valid path.
  @retval EFI_UNSUPPORTED         The script engine doesn't support system PATH.

**/
STATIC
EFI_STATUS
EFIAPI
AddSysPath (
  EFI_SCRIPT_ENGINE_PROTOCOL      *ScriptProtocol,
  CHAR16                          *Path
)
{
  INTN                Index;
  EFI_STATUS          Status;

  for (Index = (INTN)(StrLen(Path) - 1); Index > 0 && Path[Index] != '\\'; --Index);
  if (Index <= 0) {
    return EFI_INVALID_PARAMETER;
  }

  Path[Index] = '\0';
  Status = ScriptProtocol->SetSysPath(ScriptProtocol, Path);
  Path[Index] = '\\';

  return Status;
}

/**
  Parse and validate arguments passed from shell command line or load options,
  and call corresponding gEfiScriptEngineProtocolGuid interfaces to do the real
  job.

  @param  Argc        Number of Argv entries.
  @param  Argv        Pointer to array of option strings.

  @retval EFI_SUCCESS           Options are valid and script is executed successfully.
  @retval EFI_UNSUPPORTED       No script engine protocol found for MicroPython.
  @retval EFI_ALREADY_STARTED   There's already a script running.
  @retval EFI_INVALID_PARAMETER There's invalid options passed in.

**/
STATIC
EFI_STATUS
EFIAPI
RunScript (
  UINTN                 Argc,
  CHAR16                *Argv[]
)
{
  UINTN                           Index;
  CHAR16                          *ScriptStringOrFile;
  BOOLEAN                         IsScriptFile;
  BOOLEAN                         IsInteractiveMode;
  BOOLEAN                         IsAsyncMode;
  BOOLEAN                         IsShareMode;
  UINT8                           *Script;
  UINTN                           ScriptLength;
  EFI_STATUS                      Status;
  EFI_SCRIPT_ENGINE_PROTOCOL      *ScriptProtocol;

  ScriptStringOrFile = NULL;
  IsScriptFile = TRUE;
  IsInteractiveMode = FALSE;
  IsAsyncMode = FALSE;
  IsShareMode = FALSE;

  Status = FindScriptProtocolByType(EFI_SCRIPT_ENGINE_TYPE_MICROPYTHON, &ScriptProtocol);
  if (EFI_ERROR(Status) || ScriptProtocol == NULL) {
    return EFI_UNSUPPORTED;
  }

  if (Argc > 1) {
    EFI_SHELL_PROTOCOL  *Shell;
    if (EFI_ERROR(gBS->LocateProtocol(&gEfiShellProtocolGuid, NULL, (VOID **)&Shell))) {
      // Boot manager will just pass LoadOptions part to application
      Index = 0;
    } else {
      // Shell will put application as the first Argv. Just skip it.
      Index = 1;
    }
    // Check parameters passed through shell command line
    for (; Index < Argc; ++Index) {
      if (Argv[Index][0] == '-') {
        switch (Argv[Index][1]) {
        case 'a':
          IsAsyncMode = TRUE;
          IsInteractiveMode = FALSE;
          break;

        case 'c':
          IsScriptFile = FALSE;
          break;

        case 'i':
          IsInteractiveMode = TRUE;
          break;

        case 's':
          IsShareMode = TRUE;
          break;

        default:
          DEBUG((DEBUG_ERROR, "Unsupported command line option: %s\r\n", Argv[Index]));
          break;
        }
      } else if (ScriptStringOrFile == NULL) {
        ScriptStringOrFile = Argv[Index];
      } else {
        ScriptProtocol->SetSysArgs(ScriptProtocol, Argc - Index, (CONST CHAR16 **)&Argv[Index]);
        break;
      }
    }
  } else {
    IsInteractiveMode = TRUE;
  }

  if (IsAsyncMode) {
    IsInteractiveMode = FALSE;
  }

  Script = NULL;
  ScriptLength = 0;
  if (ScriptStringOrFile != NULL) {
    if (IsScriptFile) {
      Status = GetScriptFromFile(ScriptStringOrFile, &Script, &ScriptLength);
      if (!EFI_ERROR(Status)) {
        AddSysPath(ScriptProtocol, ScriptStringOrFile);
      } else {
        Print(L"upy: cannot open '%s': %r\r\n", ScriptStringOrFile, Status);
      }
    } else {
      ScriptLength = StrLen(ScriptStringOrFile);
      Script = AllocatePool(ScriptLength + 1);
      ASSERT(Script != NULL);

      Status = UnicodeStrToAsciiStrS(ScriptStringOrFile, (CHAR8 *)Script, ScriptLength + 1);
    }
  }

  if (!EFI_ERROR(Status)) {
    if (Script != NULL && ScriptLength > 0) {
      do {
        gBS->Stall(100);
        if (IsAsyncMode) {
          Status = ScriptProtocol->ExecuteAsync(ScriptProtocol, Script, ScriptLength, IsShareMode);
        } else {
          Status = ScriptProtocol->Execute(ScriptProtocol, Script, ScriptLength, IsShareMode||IsInteractiveMode);
        }
      } while (Status == EFI_ALREADY_STARTED);
    }

    if (IsInteractiveMode) {
      do {
        gBS->Stall(100);
        Status = ScriptProtocol->Execute(ScriptProtocol, NULL, 0, IsShareMode);
      } while (Status == EFI_ALREADY_STARTED);
    }
  }

  if (IsAsyncMode || IsShareMode) {
    Status = EFI_ALREADY_STARTED;
  } else {
    Status = EFI_SUCCESS;
  }

  return Status;
}

/**
  The application entry point.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
MicroPythonAppMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
)
{
  EFI_STATUS                    Status;
  EFI_LOADED_IMAGE_PROTOCOL     *LoadedImage;
  UINTN                         Argc;
  CHAR16                        **Argv;
  EFI_SCRIPT_ENGINE_PROTOCOL    *ScriptProtocol;
  UINTN                         DestMax;
  CHAR16                        *FilePath;
  CHAR16                        *UpyDxePath;
  EFI_HANDLE                    UpyDxeImage;
  EFI_DEVICE_PATH_PROTOCOL      *UpyDxeDev;
  UINTN                         Index;
  UINTN                         ExitDataSize;
  CHAR16                        *ExitData;

  Argc = 0;
  Argv = NULL;

  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **) &LoadedImage,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);

  UpyDxeImage = NULL;
  Status = FindScriptProtocolByType(EFI_SCRIPT_ENGINE_TYPE_MICROPYTHON, &ScriptProtocol);
  if ((LoadedImage->FilePath != NULL) && (EFI_ERROR(Status) || ScriptProtocol == NULL)) {
    FilePath = ConvertDevicePathToText(LoadedImage->FilePath, FALSE, FALSE);
    ASSERT(FilePath != NULL);

    Index = StrLen(FilePath);
    while (Index > 0) {
      if (FilePath[--Index] == '\\') {
        break;
      }
    }
    FilePath[Index + 1] = '\0';

    DestMax = StrLen(FilePath) + StrLen(MICROPYTHON_DRIVER_NAME) + 1;
    UpyDxePath = AllocatePool(DestMax * 2);
    ASSERT(UpyDxePath != NULL);

    UpyDxePath[0] = '\0';
    StrCatS(UpyDxePath, DestMax, FilePath);
    StrCatS(UpyDxePath, DestMax, MICROPYTHON_DRIVER_NAME);
    UpyDxeDev = FileDevicePath(LoadedImage->DeviceHandle, UpyDxePath);

    Status = gBS->LoadImage(FALSE, gImageHandle, UpyDxeDev, NULL, 0, &UpyDxeImage);
    if (!EFI_ERROR(Status)) {
      Status = gBS->StartImage(UpyDxeImage, &ExitDataSize, &ExitData);
    }

    if (EFI_ERROR(Status)) {
      DEBUG((DEBUG_ERROR, "Failed to load/start %u\r\n", UpyDxePath));
    }

    FreePool(FilePath);
    FreePool(UpyDxePath);
  }

  if (EFI_ERROR(Status)) {
    return EFI_NOT_STARTED;
  }

  //
  // Extract application parameters
  //
  Status = ParseLoadOptionsToArgs(LoadedImage->LoadOptions, TRUE, &Argv, &Argc);
  if (!EFI_ERROR(Status)) {
    Status = RunScript(Argc, Argv);
    if (Argv != NULL) {
      for (Index = 0; Index < Argc; ++Index) {
        if (Argv[Index] != NULL) {
          FreePool(Argv[Index]);
        }
      }

      FreePool(Argv);
    }

    if (!EFI_ERROR(Status) && UpyDxeImage != NULL) {
      gBS->UnloadImage (UpyDxeImage);
    }
  } else {
    DEBUG((DEBUG_ERROR, "Invalid options: %s\r\n", LoadedImage->LoadOptions));
    if (UpyDxeImage != NULL) {
      gBS->UnloadImage(UpyDxeImage);
    }
    return RETURN_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

