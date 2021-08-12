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


/** The strncpy function copies not more than n characters (characters that
    follow a null character are not copied) from the array pointed to by s2 to
    the array pointed to by s1. If copying takes place between objects that
    overlap, the behavior is undefined.

    If the array pointed to by s2 is a string that is shorter than n
    characters, null characters are appended to the copy in the array pointed
    to by s1, until n characters in all have been written.

    @return   The strncpy function returns the value of s1.
**/
char     *strncpy(char * __restrict s1, const char * __restrict s2, size_t n)
{
  return AsciiStrnCpyS( s1, n, s2, n);
}

/** The strcpy function copies the string pointed to by s2 (including the
    terminating null character) into the array pointed to by s1. If copying
    takes place between objects that overlap, the behavior is undefined.

    @return   The strcpy function returns the value of s1.
**/
char *
strcpy(char * __restrict s1, const char * __restrict s2)
{
  return AsciiStrCpyS( s1, AsciiStrSize(s2), s2);
}

/** The strcat function appends a copy of the string pointed to by s2
    (including the terminating null character) to the end of the string pointed
    to by s1. The initial character of s2 overwrites the null character at the
    end of s1. If copying takes place between objects that overlap, the
    behavior is undefined.

    @return   The strcat function returns the value of s1.
**/
char *
strcat(char * __restrict s1, const char * __restrict s2)
{
  return AsciiStrCatS( s1, AsciiStrSize(s1) + AsciiStrSize(s2), s2);
}