/** @file
  File object for MicroPython on EDK2/UEFI.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <py/nlr.h>
#include <py/runtime.h>
#include <py/stream.h>
#include <py/builtin.h>
#include <py/mphal.h>

#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ScriptEngineProtocol.h>

#include "fdfile.h"
#include "upy.h"

#if MICROPY_PY_IO

#define IS_STD_FILE(o)  (o->fd == STDIN_FILENO || o->fd == STDOUT_FILENO || o->fd == STDERR_FILENO)

extern const mp_obj_type_t mp_type_fileio;
extern const mp_obj_type_t mp_type_textio;

STATIC EFI_STATUS sfp_open_by_fd(mp_obj_fdfile_t *self, int fd)
{
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  EFI_STATUS                    status;

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (!EFI_ERROR(status)) {
    status = sfp->GetFileHandle(sfp, fd, &self->fh);
    if (!EFI_ERROR(status)) {
      self->fd = fd;
      self->sfp = sfp;
    }
  }

  return status;
}

STATIC EFI_STATUS sfp_open(mp_obj_fdfile_t *self, const char *fname, UINT64 mode) {
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  EFI_FILE_HANDLE               fh;
  EFI_FILE_INFO                 *info;
  EFI_STATUS                    status;
  CHAR16                        *uniname;
  UINTN                         len;

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (!EFI_ERROR(status)) {
    len = strlen(fname);
    uniname = Utf8ToUnicode(fname, NULL, &len, FALSE);
    ASSERT(uniname != NULL);

    status = sfp->Open(sfp, uniname, mode, 0, &fh);
    if (!EFI_ERROR(status) && fh != NULL) {
      self->fh = fh;
      self->sfp = sfp;

      status = sfp->Map(sfp, fh, &self->fd);
      if (!EFI_ERROR(status)) {
        info = FileHandleGetInfo(fh);
        //
        // open() should not operate on folders.
        //
        if (info == NULL || (info->Attribute & EFI_FILE_DIRECTORY) != 0) {
          fh->Close(fh);
          status = EFI_ACCESS_DENIED;
        }
      } else {
        fh->Close(fh);
      }
    }

    FREE_NON_NULL(uniname);
  }

  return status;
}

STATIC void check_fd_is_open(const mp_obj_fdfile_t *o) {
#ifdef MICROPY_CPYTHON_COMPAT
  if (o->fd < 0 || (o->fh == NULL || o->sfp == NULL)) {
    nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "I/O operation on closed file"));
  }
#endif
}

STATIC void fdfile_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
  (void)kind;
  mp_obj_fdfile_t *self = MP_OBJ_TO_PTR(self_in);
  mp_printf(print, "<io.%s %d>", mp_obj_get_type_str(self_in), self->fd);
}

STATIC mp_uint_t fdfile_read(mp_obj_t o_in, void *buf, mp_uint_t size, int *errcode) {
  mp_obj_fdfile_t *o = MP_OBJ_TO_PTR(o_in);
  mp_obj_fdfile_t stdio;
  EFI_STATUS      status;

  if (IS_STD_FILE(o)) {
    sfp_open_by_fd(&stdio, o->fd);
    o = &stdio;
  }
  check_fd_is_open(o);

  status = FileHandleRead(o->fh, &size, buf);
  if (EFI_ERROR(status)) {
    *errcode = status;
    return MP_STREAM_ERROR;
  }

  return size;
}

STATIC mp_uint_t fdfile_write(mp_obj_t o_in, const void *buf, mp_uint_t size, int *errcode) {
  mp_obj_fdfile_t *o = MP_OBJ_TO_PTR(o_in);
  EFI_STATUS      status;
  mp_obj_fdfile_t stdio;

  if (IS_STD_FILE(o)) {
    sfp_open_by_fd(&stdio, o->fd);
    o = &stdio;
  }
  check_fd_is_open (o);

#if MICROPY_PY_OS_DUPTERM
  if (o->fd <= STDERR_FILENO) {
    mp_hal_stdout_tx_strn(buf, size);
    return size;
  }
#endif

  status = FileHandleWrite(o->fh, &size,(VOID *)buf);
  if (EFI_ERROR(status)) {
    *errcode = status;
    return MP_STREAM_ERROR;
  }

  return size;
}

STATIC mp_uint_t fdfile_ioctl(mp_obj_t o_in, mp_uint_t request, uintptr_t arg, int *errcode) {
  mp_obj_fdfile_t *o = MP_OBJ_TO_PTR(o_in);
  EFI_STATUS      status;
  mp_obj_fdfile_t stdio;

  if (IS_STD_FILE(o)) {
    sfp_open_by_fd(&stdio, o->fd);
    o = &stdio;
  }
  check_fd_is_open(o);

  switch (request) {
  case MP_STREAM_SEEK:
    {
      struct mp_stream_seek_t *s = (struct mp_stream_seek_t *)arg;
      UINT64                  pos;

      status = EFI_SUCCESS;
      switch (s->whence) {
      case SEEK_SET:
        pos = s->offset;
        break;

      case SEEK_CUR:
        status = FileHandleGetPosition(o->fh, &pos);
        if (!EFI_ERROR(status)) {
          pos += s->offset;
        }
        break;

      case SEEK_END:
        status = FileHandleGetSize(o->fh, &pos);
        if (!EFI_ERROR(status)) {
          pos += s->offset;
        }
        break;

      default:
        status = EFI_INVALID_PARAMETER;
        break;
      }

      if (!EFI_ERROR(status)) {
        status = FileHandleSetPosition(o->fh, pos);
      }

      if (EFI_ERROR(status)) {
        *errcode = status;
        return MP_STREAM_ERROR;
      }

      s->offset = 0;
      return 0;
    }

  case MP_STREAM_FLUSH:
    status = FileHandleFlush(o->fh);
    if (EFI_ERROR(status)) {
      *errcode = status;
      return MP_STREAM_ERROR;
    }
    return 0;

  default:
    *errcode = EINVAL;
    return MP_STREAM_ERROR;
  }
}

STATIC mp_obj_t fdfile_close(mp_obj_t self_in) {
  mp_obj_fdfile_t           *self = MP_OBJ_TO_PTR(self_in);

  if (self->fh) {
    FileHandleClose (self->fh);
  }

  if (self->sfp != NULL) {
    self->sfp->Unmap (self->sfp, self->fd);
  }

#ifdef MICROPY_CPYTHON_COMPAT
  self->fd = -1;
#endif
  self->fh = NULL;
  self->sfp = NULL;

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(fdfile_close_obj, fdfile_close);

STATIC mp_obj_t fdfile___exit__(size_t n_args, const mp_obj_t *args) {
  (void)n_args;
  return fdfile_close(args[0]);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(fdfile___exit___obj, 4, 4, fdfile___exit__);

STATIC mp_obj_t fdfile_fileno(mp_obj_t self_in) {
  mp_obj_fdfile_t *self = MP_OBJ_TO_PTR(self_in);
  check_fd_is_open(self);
  return MP_OBJ_NEW_SMALL_INT(self->fd);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(fdfile_fileno_obj, fdfile_fileno);

// Note: encoding is ignored for now; it's also not a valid kwarg for CPython's FileIO,
// but by adding it here we can use one single mp_arg_t array for open() and FileIO's constructor
STATIC const mp_arg_t file_open_args[] = {
  { MP_QSTR_file, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
  { MP_QSTR_mode, MP_ARG_OBJ, {.u_obj = MP_OBJ_NEW_QSTR(MP_QSTR_r)} },
  { MP_QSTR_buffering, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
  { MP_QSTR_encoding, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
};
#define FILE_OPEN_NUM_ARGS MP_ARRAY_SIZE(file_open_args)

STATIC mp_obj_t fdfile_open(const mp_obj_type_t *type, mp_arg_val_t *args) {
  mp_obj_fdfile_t *o = m_new_obj(mp_obj_fdfile_t);
  const char      *mode_s = mp_obj_str_get_str(args[1].u_obj);
  UINT64          mode;
  BOOLEAN         append;
  BOOLEAN         truncate;
  UINT64          size;
  EFI_STATUS      status;

  append = FALSE;
  truncate = FALSE;
  mode = 0;
  while (*mode_s) {
    switch (*mode_s++) {
    case 'r':
      mode |= EFI_FILE_MODE_READ;
      break;

    case 'w':
      mode |= (EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE);
      truncate = TRUE;
      break;

    case 'a':
      mode |= (EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE);
      append = TRUE;
      break;

    case '+':
      mode |= (EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE);
      break;

#if MICROPY_PY_IO_FILEIO
      // If we don't have io.FileIO, then files are in text mode implicitly
    case 'b':
      type = &mp_type_fileio;
      break;
    case 't':
      type = &mp_type_textio;
      break;
#endif
    }
  }

  o->base.type = type;

  if (MP_OBJ_IS_SMALL_INT(args[0].u_obj)) {
    status = sfp_open_by_fd(o, MP_OBJ_SMALL_INT_VALUE(args[0].u_obj));
  } else {
    status = sfp_open(o, mp_obj_str_get_str(args[0].u_obj), mode);
  }

  if (EFI_ERROR(status)) {
    mp_raise_OSError(ENOENT);
  }

  if (o->fh) {
    status = FileHandleGetSize(o->fh, &size);
    if (!EFI_ERROR(status) && size > 0) {
      if (append) {
        FileHandleSetPosition(o->fh, size);
      }

      if (truncate) {
        status = FileHandleSetSize(o->fh, 0);
      }
    }
  }

  return MP_OBJ_FROM_PTR(o);
}

STATIC mp_obj_t fdfile_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
  mp_arg_val_t arg_vals[FILE_OPEN_NUM_ARGS];
  mp_arg_parse_all_kw_array(n_args, n_kw, args, FILE_OPEN_NUM_ARGS, file_open_args, arg_vals);
  return fdfile_open(type, arg_vals);
}

STATIC const mp_rom_map_elem_t rawfile_locals_dict_table[] = {
  { MP_ROM_QSTR(MP_QSTR_fileno), MP_ROM_PTR(&fdfile_fileno_obj) },
  { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
  { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
  { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
  { MP_ROM_QSTR(MP_QSTR_readlines), MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj) },
  { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
  { MP_ROM_QSTR(MP_QSTR_seek), MP_ROM_PTR(&mp_stream_seek_obj) },
  { MP_ROM_QSTR(MP_QSTR_tell), MP_ROM_PTR(&mp_stream_tell_obj) },
  { MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
  { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&fdfile_close_obj) },
  { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
  { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&fdfile___exit___obj) },
  { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&fdfile___exit___obj) },
};

STATIC MP_DEFINE_CONST_DICT(rawfile_locals_dict, rawfile_locals_dict_table);

#if MICROPY_PY_IO_FILEIO
STATIC const mp_stream_p_t fileio_stream_p = {
  .read = fdfile_read,
  .write = fdfile_write,
  .ioctl = fdfile_ioctl,
};

const mp_obj_type_t mp_type_fileio = {
  { &mp_type_type },
  .name = MP_QSTR_FileIO,
  .print = fdfile_print,
  .make_new = fdfile_make_new,
  .getiter = mp_identity_getiter,
  .iternext = mp_stream_unbuffered_iter,
  .protocol = &fileio_stream_p,
  .locals_dict = (mp_obj_dict_t*)&rawfile_locals_dict,
};
#endif

STATIC const mp_stream_p_t textio_stream_p = {
  .read = fdfile_read,
  .write = fdfile_write,
  .ioctl = fdfile_ioctl,
  .is_text = true,
};

const mp_obj_type_t mp_type_textio = {
  { &mp_type_type },
  .name = MP_QSTR_TextIOWrapper,
  .print = fdfile_print,
  .make_new = fdfile_make_new,
  .getiter = mp_identity_getiter,
  .iternext = mp_stream_unbuffered_iter,
  .protocol = &textio_stream_p,
  .locals_dict = (mp_obj_dict_t*)&rawfile_locals_dict,
};

// Factory function for I/O stream classes
mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs) {
  // TODO: analyze buffering args and instantiate appropriate type
  mp_arg_val_t arg_vals[FILE_OPEN_NUM_ARGS];
  mp_arg_parse_all(n_args, args, kwargs, FILE_OPEN_NUM_ARGS, file_open_args, arg_vals);
  return fdfile_open(&mp_type_textio, arg_vals);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);

const mp_obj_fdfile_t mp_sys_stdin_obj  = { .base = {&mp_type_textio}, .fd = STDIN_FILENO };
const mp_obj_fdfile_t mp_sys_stdout_obj = { .base = {&mp_type_textio}, .fd = STDOUT_FILENO };
const mp_obj_fdfile_t mp_sys_stderr_obj = { .base = {&mp_type_textio}, .fd = STDERR_FILENO };

#endif // MICROPY_PY_IO
