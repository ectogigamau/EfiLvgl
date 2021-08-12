/** @file
  UEFI file support for MicroPython lexer.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>

#include <py/mpconfig.h>
#include <py/lexer.h>
#include <py/reader.h>

#include <Protocol/ScriptEngineProtocol.h>
#include <Protocol/ScriptFileProtocol.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include "upy.h"

typedef struct _mp_lexer_file_buf_t {
  EFI_FILE_PROTOCOL *fh;
  byte              buf[128];
  mp_uint_t         len;
  mp_uint_t         pos;
} mp_lexer_file_buf_t;

STATIC mp_uint_t file_buf_next_byte(void *data)
{
  mp_uint_t           n;
  EFI_STATUS          status;
  mp_lexer_file_buf_t *fb;

  fb = data;
  if (fb->pos >= fb->len) {
    if (fb->len == 0) {
      return MP_READER_EOF;
    } else {
      n = sizeof(fb->buf);
      status = FileHandleRead(fb->fh, &n, fb->buf);

      if (n <= 0 || EFI_ERROR(status)) {
        fb->len = 0;
        return MP_READER_EOF;
      }

      fb->len = n;
      fb->pos = 0;
    }
  }

  return fb->buf[fb->pos++];
}

STATIC void file_buf_close(void *fb)
{
  FileHandleClose(((mp_lexer_file_buf_t *)fb)->fh);
  m_del_obj(mp_lexer_file_buf_t, fb);
}

mp_lexer_t* mp_lexer_new_from_fh(qstr filename, EFI_FILE_HANDLE fh)
{
  mp_lexer_file_buf_t *fb = m_new_obj_maybe(mp_lexer_file_buf_t);
  mp_uint_t           n;
  EFI_STATUS          status;

  if (fb == NULL) {
    FileHandleClose(fh);
    return NULL;
  }

  fb->fh = fh;
  n = sizeof(fb->buf);
  status = FileHandleRead(fb->fh, &n, fb->buf);
  if (EFI_ERROR(status)) {
    return NULL;
  }
  fb->len = n;
  fb->pos = 0;

  mp_reader_t reader = {fb,file_buf_next_byte,file_buf_close};

  return mp_lexer_new(filename, reader);
}

EFI_FILE_HANDLE mp_lexer_open_file(const char *filename)
{
  EFI_SCRIPT_FILE_PROTOCOL      *Fs;
  EFI_FILE_HANDLE               Fh;
  CHAR16                        *UniPath;
  UINTN                         Length;
  EFI_FILE_INFO                 *Info;
  EFI_STATUS                    Status;

  // Check script file protocols
  UniPath = NULL;
  Fh = NULL;
  Status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&Fs);
  ASSERT_EFI_ERROR(Status);

  Length = AsciiStrLen(filename);
  UniPath = Utf8ToUnicode(filename, NULL, &Length, FALSE);
  ASSERT (UniPath != NULL);

  Status = Fs->Open(Fs, UniPath, EFI_FILE_MODE_READ, 0, &Fh);
  if (!EFI_ERROR (Status)) {
    Info = FileHandleGetInfo(Fh);
     if (Info == NULL || (Info->Attribute & EFI_FILE_DIRECTORY) != 0) {
       FileHandleClose(Fh);
       Fh = NULL;
     }

     FREE_NON_NULL(Info);
  }

  FREE_NON_NULL (UniPath);

  return Fh;
}

mp_lexer_t* mp_lexer_new_from_file(const char *filename)
{
  EFI_FILE_HANDLE   fh = mp_lexer_open_file(filename);

  if (fh == NULL) {
    return NULL;
  }
  return mp_lexer_new_from_fh(qstr_from_str(filename), fh);
}

