/** @file
  Glue code to wrap Oniguruma library interfaces.

  Copyright (c) 2018, Intel Corporation. All rights reserved. <BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "config.h"
#include "onigurumaglue.h"

int EFIAPI sprintf_s(char *str, size_t sizeOfBuffer, char const *fmt, ...)
{
  VA_LIST Marker;
  int   NumberOfPrinted;

  VA_START (Marker, fmt);
  NumberOfPrinted = (int)AsciiVSPrint (str, sizeOfBuffer, fmt, Marker);
  VA_END (Marker);

  return NumberOfPrinted;
}

int OnigStrCmp (char* Str1, char* Str2)
{
  return (int)AsciiStrCmp (Str1, Str2);
}

VOID
OnigurumaRegionFree (
  IN  OnigRegion          *Region
  )
{
  onig_region_free (Region, 1);
}


VOID
OnigurumaFree (
  IN  regex_t               *Regex
  )
{
  onig_free (Regex);
}

EFI_STATUS
OnigurumaCompile (
  IN  CONST CHAR8     *Pattern,
  IN  UINTN           Flag,
  OUT regex_t         **Regex
)
{
  INT32           OnigResult;
  OnigErrorInfo   ErrorInfo;
  OnigUChar       ErrorMessage[ONIG_MAX_ERROR_MESSAGE_LEN];
  OnigUChar       *Start;

  //
  // Compile pattern
  //
  Start = (OnigUChar*)Pattern;
  OnigResult = onig_new (
                 Regex,
                 Start,
                 Start + onigenc_str_bytelen_null (CHAR_ENCODING, Start),
                 Flag,
                 CHAR_ENCODING,
                 SYNTAX_TYPE,
                 &ErrorInfo
                 );
  if (OnigResult != ONIG_NORMAL) {
    onig_error_code_to_str (ErrorMessage, OnigResult, &ErrorInfo);
    DEBUG ((DEBUG_ERROR, "Regex compilation failed: %a\n", ErrorMessage));
    return EFI_INVALID_LANGUAGE;
  }

  return EFI_SUCCESS;
}

/**
  Call the Oniguruma regex match API.

  Same parameters as RegularExpressionMatch, except SyntaxType is required.

  @param String         A pointer to a NULL terminated string to match against the
                        regular expression string specified by Pattern.

  @param Pattern        A pointer to a NULL terminated string that represents the
                        regular expression.
  @param SyntaxType     A pointer to the EFI_REGEX_SYNTAX_TYPE that identifies the
                        regular expression syntax type to use. May be NULL in which
                        case the function will use its default regular expression
                        syntax type.

  @param Result         On return, points to TRUE if String fully matches against
                        the regular expression Pattern using the regular expression
                        SyntaxType. Otherwise, points to FALSE.

  @param Captures       A Pointer to an array of EFI_REGEX_CAPTURE objects to receive
                        the captured groups in the event of a match. The full
                        sub-string match is put in Captures[0], and the results of N
                        capturing groups are put in Captures[1:N]. If Captures is
                        NULL, then this function doesn't allocate the memory for the
                        array and does not build up the elements. It only returns the
                        number of matching patterns in CapturesCount. If Captures is
                        not NULL, this function returns a pointer to an array and
                        builds up the elements in the array. CapturesCount is also
                        updated to the number of matching patterns found. It is the
                        caller's responsibility to free the memory pool in Captures
                        and in each CapturePtr in the array elements.

  @param CapturesCount  On output, CapturesCount is the number of matching patterns
                        found in String. Zero means no matching patterns were found
                        in the string.

  @retval  EFI_SUCCESS       Regex compilation and match completed successfully.
  @retval  EFI_DEVICE_ERROR  Regex compilation failed.

**/
EFI_STATUS
OnigurumaMatch (
  IN  regex_t               *Regex,
  IN  CONST CHAR8           *String,
  OUT OnigRegion            **MatchRegion
  )
{
  OnigRegion      *Region;
  INT32           OnigResult;
  OnigUChar       ErrorMessage[ONIG_MAX_ERROR_MESSAGE_LEN];
  OnigUChar       *Start;
  EFI_STATUS      Status;


  Status = EFI_SUCCESS;
  *MatchRegion = NULL;

  //
  // Try to match
  //
  Start = (OnigUChar*)String;
  Region = onig_region_new ();
  if (Region == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  OnigResult = onig_search (
                 Regex,
                 Start,
                 Start + onigenc_str_bytelen_null (CHAR_ENCODING, Start),
                 Start,
                 Start + onigenc_str_bytelen_null (CHAR_ENCODING, Start),
                 Region,
                 ONIG_OPTION_NONE
                 );
  if (OnigResult >= 0) {
    *MatchRegion = Region;
  } else {
    if (OnigResult != ONIG_MISMATCH) {
      onig_error_code_to_str (ErrorMessage, OnigResult);
      DEBUG ((DEBUG_ERROR, "Regex match failed: %a\n", ErrorMessage));
      onig_region_free (Region, 1);
    }
    Status = EFI_NOT_FOUND;
  }

  return Status;
}

EFI_STATUS
OnigurumaSplit (
  IN  regex_t         *Regex,
  IN  CONST CHAR8     *String,
  IN  int             (*Callback)(int, int, OnigRegion*, void*),
  IN  void            *CallbackArg
  )
{
  OnigRegion      *Region;
  INT32           OnigResult;
  OnigUChar       ErrorMessage[ONIG_MAX_ERROR_MESSAGE_LEN];
  OnigUChar       *Start;
  EFI_STATUS      Status;


  Status = EFI_SUCCESS;
  //
  // Try to match
  //
  Start = (OnigUChar*)String;
  Region = onig_region_new ();
  if (Region == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  OnigResult = onig_scan (
                 Regex,
                 Start,
                 Start + onigenc_str_bytelen_null (CHAR_ENCODING, Start),
                 Region,
                 ONIG_OPTION_NONE,
                 Callback,
                 CallbackArg
                 );
  if (OnigResult < 0) {
    if (OnigResult != ONIG_MISMATCH) {
      onig_error_code_to_str (ErrorMessage, OnigResult);
      DEBUG ((DEBUG_ERROR, "Regex match failed: %a\n", ErrorMessage));
    }
    Status = EFI_NOT_FOUND;
  }


  onig_region_free (Region, 1);

  return Status;
}
