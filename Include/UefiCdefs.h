/** @file
  Standard C APIs, types, macros needed by MicroPython for UEFI.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/
#ifndef _UEFI_CDEFS_H
#define _UEFI_CDEFS_H

#include <Base.h>
#include <Library/DebugLib.h>

// Types used to replace long so that it will have constant length regardless of compiler.
typedef  INT32   LONG32;
typedef UINT32  ULONG32;
typedef  INT64   LONG64;
typedef UINT64  ULONG64;

typedef   INTN    LONGN;
typedef  UINTN   ULONGN;
typedef  ULONGN  ULONG_PTR;


////////////////////////////////////////////////////////////////////////////////
///
///  machine/endian.h, sys/endian.h
///
#define LITTLE_ENDIAN   1234
#define BIG_ENDIAN      4321
#define BYTE_ORDER      LITTLE_ENDIAN


////////////////////////////////////////////////////////////////////////////////
///
/// sys/types.h
///
typedef INT8    int8_t;
typedef INT16   int16_t;
typedef INT32   int32_t;
typedef INT64   int64_t;

typedef UINT8   u_int8_t;
typedef UINT16  u_int16_t;
typedef UINT32  u_int32_t;
typedef UINT64  u_int64_t;
typedef UINTN   u_int;

typedef UINT8   uint8_t;
typedef UINT16  uint16_t;
typedef UINT32  uint32_t;
typedef UINT64  uint64_t;

typedef UINT8   u_char;
typedef UINT16  u_short;
typedef UINTN   u_int;
typedef ULONGN  u_long;

typedef UINTN   size_t;
typedef INT32   time_t;
typedef UINT64  useconds_t;
typedef INT64   suseconds_t;


////////////////////////////////////////////////////////////////////////////////
///
/// sys/stdint.h, machine/int_types.h
///
typedef INTN      intptr_t;
typedef UINTN     uintptr_t;


////////////////////////////////////////////////////////////////////////////////
///
/// assert.h
///
#ifdef  NDEBUG
#define assert(Expression)
#else
#define assert(Expression)   \
  ((Expression) ? (VOID)0 : DebugAssert(__FILE__, __LINE__, #Expression))
#endif


////////////////////////////////////////////////////////////////////////////////
///
/// ctype.h
///
#define isalpha(c)    ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
#define isalnum(c)    ((c >= '0' && c <= '9') || isalpha(c))
#define islower(c)    (c >= 'a' && c <= 'z')
#define isspace(c)    (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == '\f')
#define toupper(c)    (islower(c) ? (c - 0x20) : c)


////////////////////////////////////////////////////////////////////////////////
///
///  errno.h
///
#define EPERM           1
#define ENOENT          2
#define EIO             5
#define EBADF           9
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define EEXIST          17
#define ENODEV          19
#define EISDIR          21
#define EINVAL          22
#define EFBIG           27
#define EPIPE           32
#define EDOM            33
#define ERANGE          34
#define EAGAIN          35
#define EWOULDBLOCK     35
#define EINPROGRESS     36
#define EALREADY        37
#define ENOTSOCK        38
#define EDESTADDRREQ    39
#define EPROTOTYPE      41
#define ENOPROTOOPT     42
#define EPROTONOSUPPORT 43
#define EOPNOTSUPP      45
#define ENOTSUP         45
#define EAFNOSUPPORT    47
#define EADDRINUSE      48
#define EADDRNOTAVAIL   49
#define ENETDOWN        50
#define ENETUNREACH     51
#define ECONNABORTED    53
#define ECONNRESET      54
#define ENOBUFS         55
#define EISCONN         56
#define ENOTCONN        57
#define ETIMEDOUT       60
#define ECONNREFUSED    61
#define EHOSTUNREACH    65

extern int errno;


////////////////////////////////////////////////////////////////////////////////
///
///  limits.h, macchine/int_limits.h
///
#define MB_LEN_MAX      4
#define PATH_MAX        1024

#if defined(__GNUC__)
  #define LONG_MIN      MIN_INTN
  #define LONG_MAX      MAX_INTN
  #define ULONG_MAX     MAX_UINTN
#else /* Non __GNUC__  */
  #define LONG_MIN      MIN_INT32
  #define LONG_MAX      MAX_INT32
  #define ULONG_MAX     MAX_UINT32
#endif

#define LLONG_MIN       MIN_INT64
#define LLONG_MAX       MAX_INT64
#define ULLONG_MAX      MAX_UINT64

//
// MAX_INT64 has a typecast in the macro definition, which cannot be used in
// preprocessor statement. We cannot reuse it in following definition.
//
#define INT64_MAX       0x7fffffffffffffffLL
#define INT32_MAX       0x7fffffff
#define INT_MAX         0x7fffffff


////////////////////////////////////////////////////////////////////////////////
///
///  setjmp.h
///
#include <Library/BaseLib.h>

typedef BASE_LIBRARY_JUMP_BUFFER jmp_buf[1];

#define setjmp(env)       (INTN)SetJump((env))
#define longjmp(env, val) LongJump(env, (UINTN)((val == 0) ? 1 : val))


////////////////////////////////////////////////////////////////////////////////
///
///  stdarg.h
///
#if defined(__clang__)

typedef __builtin_va_list   va_list;
#define va_start            __builtin_va_start
#define va_arg              __builtin_va_arg
#define va_end              __builtin_va_end

#else

#define va_list             VA_LIST
#define va_start            VA_START
#define va_arg              VA_ARG
#define va_end              VA_END

#endif


////////////////////////////////////////////////////////////////////////////////
///
///  stdbool.h
///
#define bool        BOOLEAN
#define true        TRUE
#define false       FALSE


////////////////////////////////////////////////////////////////////////////////
///
///  stddef.h
///
#define offsetof(StrucName, Member)  OFFSET_OF(StrucName, Member)


////////////////////////////////////////////////////////////////////////////////
///
///  stdio.h
///
int printf (const char * format, ...);


////////////////////////////////////////////////////////////////////////////////
///
///  string.h
///
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *str);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);


////////////////////////////////////////////////////////////////////////////////
///
///  time.h
///
struct tm {
  int     tm_year;      // years since 1900
  int     tm_mon;       // months since January  [0, 11]
  int     tm_mday;      // day of the month  [1, 31]
  int     tm_hour;      // hours since midnight  [0, 23]
  int     tm_min;       // minutes after the hour  [0, 59]
  int     tm_sec;       // seconds after the minute  [0, 60]
  int     tm_wday;      // days since Sunday  [0, 6]
  int     tm_yday;      // days since January 1  [0, 365]
  int     tm_isdst;     // Daylight Saving Time flag
  int     tm_zoneoff;   // EFI TimeZone offset, -1440 to 1440 or 2047
  int     tm_daylight;  // EFI Daylight flags
  UINT32  tm_Nano;      // EFI Nanosecond value
};

struct timeval {
  LONG32    tv_sec;   /* seconds */
  LONG32    tv_usec;  /* and microseconds */
};


////////////////////////////////////////////////////////////////////////////////
///
///  unistd.h
///
int usleep(useconds_t);

#define STDIN_FILENO  0 /**< standard input file descriptor */
#define STDOUT_FILENO 1 /**< standard output file descriptor */
#define STDERR_FILENO 2 /**< standard error file descriptor */

#ifndef SEEK_SET
  #define SEEK_SET  0 /**< set file offset to offset */
#endif
#ifndef SEEK_CUR
  #define SEEK_CUR  1 /**< set file offset to current plus offset */
#endif
#ifndef SEEK_END
  #define SEEK_END  2 /**< set file offset to EOF plus offset */
#endif


#endif  /* _UEFI_CDEFS_H */
