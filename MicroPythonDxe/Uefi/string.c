/** @file
  Wrapper of required std c lib.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <stdint.h>
#include <string.h>

#include "mpconfigport.h"

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>

#if defined(__clang__) && !defined(__APPLE__)

static __attribute__((__used__))
void *__memcpy(void *dst, const void *src, size_t n) {
  return CopyMem(dst, src, n);
}

__attribute__((__alias__("__memcpy")))
void *memcpy(void *dst, const void *src, size_t n);

#else

void *memcpy(void *dst, const void *src, size_t n) {
  return CopyMem(dst, src, n);
}

#endif

void *memmove(void *dst, const void *src, size_t n) {
  return CopyMem(dst, src, n);
}

void *memset(void *s, int c, size_t n) {
  return SetMem(s, (UINTN)n, (UINT8)c);
}

int memcmp(const void *s1, const void *s2, size_t n) {
  return CompareMem(s1, s2, n);
}

size_t strlen(const char *str) {
  return AsciiStrLen(str);
}

int strcmp(const char *s1, const char *s2) {
  return AsciiStrCmp(s1, s2);
}

int strncmp(const char *s1, const char *s2, size_t n) {
  return AsciiStrnCmp(s1, s2, n);
}

char *strchr(const char *s, int c)
{
  char pattern[2];
  pattern[0] = c;
  pattern[1] = 0;
  return AsciiStrStr (s, pattern);
}

char *strstr(const char *haystack, const char *needle) {
  return AsciiStrStr(haystack, needle);
}
