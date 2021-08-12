/** @file
  
  Module to rewrite stdlib references within Oniguruma

  (C) Copyright 2014-2015 Hewlett Packard Enterprise Development LP<BR>

  This program and the accompanying materials are licensed and made available
  under the terms and conditions of the BSD License that accompanies this
  distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, WITHOUT
  WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/
#ifndef ONIGURUMA_UEFI_PORT_H
#define ONIGURUMA_UEFI_PORT_H

#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#define HAVE_STDARG_PROTOTYPES

#undef ONIGURUMA_EXPORT
#define ONIG_EXTERN   extern

#define SIZEOF_LONG sizeof(long)
#define SIZEOF_INT  sizeof(int)
typedef UINTN size_t;

extern void *m_malloc(size_t num_bytes);

#define malloc(n)   AllocatePool(n)
#define calloc(n,s) AllocateZeroPool((n)*(s))
#define _alloca(x)  m_malloc(x)
#define alloca(x)   m_malloc(x)
#define realloc(OldPtr,NewSize) ReallocatePool(NewSize,NewSize,OldPtr)

#define free(p)             \
  do {                      \
    VOID *EvalOnce;         \
                            \
    EvalOnce = (p);         \
    if (EvalOnce != NULL) { \
      FreePool (EvalOnce);  \
    }                       \
  } while (FALSE)


#define _TRUNCATE     ((size_t)-1)
#define FILE          VOID
#define stdout        NULL
#define fprintf(...)
#define fputs(a,b)
#define fflush(fp)
#define _vsnprintf    vsnprintf
#define snprintf      AsciiSPrint
#if !defined(__clang__)
#define vsnprintf     (int)AsciiVSPrint
#else
int EFIAPI vsnprintf(char *buf, size_t size, const char *fmt, va_list args);
#endif

#if !defined(__GNUC__)
#define __attribute__(attr)
#endif

#define MAX_STRING_SIZE 0x1000
#define strlen_s(String,MaxSize)            AsciiStrnLenS (String, MaxSize)
#define strcat_s(Dest,MaxSize,Src)          AsciiStrCatS (Dest, MaxSize, Src)
#define strncpy_s(Dest,MaxSize,Src,Length)  AsciiStrnCpyS (Dest, MaxSize, Src, Length)
#define strncpy(Dest,Src,Length)            AsciiStrnCpy (Dest, Src, Length)
#define strcat(Dest,Src)                    AsciiStrCat (Dest, Src)
#define _vsnprintf_s(buf,size,_TRUNCATE,fmt,args) (int)AsciiVSPrint(buf, size, fmt, args)

int EFIAPI sprintf_s (char *str, size_t sizeOfBuffer, char const *fmt, ...);

//#define exit(n) ASSERT(FALSE);

#endif // !ONIGURUMA_UEFI_PORT_H
