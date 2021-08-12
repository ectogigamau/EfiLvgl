/** @file
  Glub interface definitions of Oniguruma library.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#include "oniguruma.h"

#define CHAR_ENCODING   ONIG_ENCODING_UTF8
#define SYNTAX_TYPE     ONIG_SYNTAX_PERL

#define ONIG_OPTION_ASCII     (ONIG_OPTION_WORD_IS_ASCII  |   \
                               ONIG_OPTION_DIGIT_IS_ASCII |   \
                               ONIG_OPTION_SPACE_IS_ASCII |   \
                               ONIG_OPTION_POSIX_IS_ASCII)

VOID
OnigurumaRegionFree (
  IN  OnigRegion          *Region
  );

VOID
OnigurumaFree (
  IN  regex_t               *Regex
  );

EFI_STATUS
OnigurumaCompile (
  IN  CONST CHAR8     *Pattern,
  IN  UINTN           Flag,
  OUT regex_t         **Regex
  );

EFI_STATUS
OnigurumaMatch (
  IN  regex_t               *Regex,
  IN  CONST CHAR8           *String,
  OUT OnigRegion            **MatchRegion
  );

EFI_STATUS
OnigurumaSplit (
  IN  regex_t         *Regex,
  IN  CONST CHAR8     *String,
  IN  int             (*Callback)(int, int, OnigRegion*, void*),
  IN  void            *CallbackArg
  );

