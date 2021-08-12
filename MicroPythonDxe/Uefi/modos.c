/** @file
  Edk2 version of os module for MicroPythhon.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Protocol/SimpleFileSystem.h>
#include <Protocol/ScriptFileProtocol.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/Smbios.h>
#include <Protocol/Rng.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FileHandleLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <py/mpconfig.h>
#include <py/nlr.h>
#include <py/runtime.h>
#include <py/objtuple.h>
#include <py/mphal.h>
#include <extmod/misc.h>
#include <py/objstr.h>
#include <py/objarray.h>

#include <lib/timeutils/timeutils.h>

#include "genhdr/mpversion.h"
#include "objuefi.h"
#include "upy.h"

#define FILE_TYPE_DIR     0x4000
#define FILE_TYPE_FILE    0x8000

typedef struct _mp_obj_listdir_t {
    mp_obj_base_t     base;
    mp_fun_1_t        iternext;
    EFI_FILE_HANDLE   fh;
    EFI_FILE_INFO     *openlist;
} mp_obj_listdir_t;

typedef struct _file_list_t {
  LIST_ENTRY      link;
  mp_obj_t        info;
} file_list_t;

typedef struct _mp_obj_stat_result_t {
  mp_obj_base_t base;
  UINT64        st_mode;
  UINT64        st_ino;
  UINT64        st_dev;
  UINT64        st_nlink;
  UINT64        st_uid;
  UINT64        st_gid;
  UINT64        st_size;
  UINT64        st_atime;
  UINT64        st_mtime;
  UINT64        st_ctime;
} mp_obj_stat_result_t;

STATIC const mp_obj_type_t mp_type_stat_result;


STATIC void stat_result_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind)
{
  mp_obj_stat_result_t    *self = MP_OBJ_TO_PTR(o_in);

  mp_printf(print,
            "os.stat_result("
            "st_mode=%d, st_ino=%d, st_dev=%d, st_nlink=%d, st_uid=%d, "
            "st_gid=%d, st_size=%d, st_atime=%d, st_mtime=%d, st_ctime=%d)",
            self->st_mode, self->st_ino, self->st_dev, self->st_nlink,
            self->st_uid, self->st_gid, self->st_size, self->st_atime,
            self->st_mtime, self->st_ctime);
}

STATIC void stat_result_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest)
{
  mp_obj_stat_result_t      *self = MP_OBJ_TO_PTR(self_in);

  switch (attr) {
  case MP_QSTR_st_mode:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = mp_obj_new_int_from_ull(self->st_mode);
    }
    break;

  case MP_QSTR_st_ino:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = MP_OBJ_NEW_SMALL_INT(0);
    }
    break;

  case MP_QSTR_st_dev:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = MP_OBJ_NEW_SMALL_INT(0);
    }
    break;

  case MP_QSTR_st_nlink:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = MP_OBJ_NEW_SMALL_INT(0);
    }
    break;

  case MP_QSTR_st_uid:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = MP_OBJ_NEW_SMALL_INT(0);
    }
    break;

  case MP_QSTR_st_gid:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = MP_OBJ_NEW_SMALL_INT(0);
    }
    break;

  case MP_QSTR_st_size:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = mp_obj_new_int_from_ull(self->st_size);
    }
    break;

  case MP_QSTR_st_atime:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = mp_obj_new_int_from_ull(self->st_atime);
    }
    break;

  case MP_QSTR_st_mtime:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = mp_obj_new_int_from_ull(self->st_mtime);
    }
    break;

  case MP_QSTR_st_ctime:
    if (dest[0] == MP_OBJ_NULL) {
      dest[0] = mp_obj_new_int_from_ull(self->st_ctime);
    }
    break;

  default:
    nlr_raise(
      mp_obj_new_exception_msg_varg(
        &mp_type_AttributeError,
        "Non-existing attribute"
        ));
  }
}

STATIC mp_obj_t mod_os_system (mp_obj_t cmd_in)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  EFI_LOADED_IMAGE_PROTOCOL     *imageinfo;
  EFI_DEVICE_PATH_PROTOCOL      *devpath;
  EFI_HANDLE                    image;
  const char                    *cmd_str;
  CHAR16                        *cmd_uni;
  CHAR16                        *options;
  UINTN                         len;

  cmd_uni = NULL;
  devpath = NULL;

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  cmd_str = mp_obj_str_get_str(cmd_in);
  len     = strlen(cmd_str);
  cmd_uni = Utf8ToUnicode(cmd_str, NULL, &len, FALSE);
  options = StrStr(cmd_uni, L".efi");
  if (options == NULL) {
    status = EFI_UNSUPPORTED;
    goto Exit;
  }

  options += StrLen(L".efi");
  if (*options != L'\0') {
    *options++ = L'\0';
  }

  devpath = sfp->GetDevicePath(sfp, cmd_uni);
  if (devpath == NULL) {
    status = EFI_NOT_FOUND;
    goto Exit;
  }

  status  = gBS->LoadImage(FALSE, gImageHandle, devpath, NULL, 0, &image);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  //
  // Pass load options to image
  //
  if (*options != L'\0') {
    status = gBS->HandleProtocol(image, &gEfiLoadedImageProtocolGuid, (VOID **)&imageinfo);
    if (EFI_ERROR(status)) {
      goto Exit;
    }

    imageinfo->LoadOptionsSize  = StrLen(options) + 1;
    imageinfo->LoadOptions      = StrDup(options, imageinfo->LoadOptionsSize);
  }

  status = gBS->StartImage(image, NULL, NULL);

Exit:
  FREE_NON_NULL(cmd_uni);
  FREE_NON_NULL(devpath);
  RAISE_UEFI_EXCEPTION_ON_ERROR(status);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mod_os_system_obj, mod_os_system);


STATIC mp_obj_t mod_os_mkdir (mp_obj_t path)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CONST CHAR8                   *path_asc;
  CHAR16                        *path_uni;
  UINTN                         len;
  EFI_FILE_HANDLE               fh;

  fh       = NULL;
  path_uni = NULL;

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  path_asc  = mp_obj_str_get_str(path);
  len       = strlen (path_asc);
  path_uni  = Utf8ToUnicode (path_asc, NULL, &len, FALSE);
  status    = sfp->Open (sfp, path_uni, EFI_FILE_MODE_READ, 0, &fh);
  if (status == EFI_SUCCESS) {
    status = EEXIST;
    goto Exit;
  }

  status = sfp->Open (sfp,
                      path_uni,
                      EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,
                      EFI_FILE_DIRECTORY,
                      &fh);

Exit:
  CLOSE_FILE(fh);
  FREE_NON_NULL(path_uni);
  RAISE_UEFI_EXCEPTION_ON_ERROR(status);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mod_os_mkdir_obj, mod_os_mkdir);


STATIC mp_obj_t mod_os_stat (mp_obj_t path)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CONST CHAR8                   *path_asc;
  CHAR16                        *path_uni;
  UINTN                         len;
  EFI_FILE_HANDLE               fh;
  EFI_FILE_INFO                 *info;
  mp_obj_stat_result_t          *stat;

  fh        = NULL;
  info      = NULL;
  path_uni  = NULL;
  stat      = (mp_obj_stat_result_t *)&mp_const_none_obj;

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  path_asc  = mp_obj_str_get_str(path);
  len       = strlen (path_asc);
  path_uni  = Utf8ToUnicode (path_asc, NULL, &len, FALSE);
  status    = sfp->Open (sfp, path_uni, EFI_FILE_MODE_READ, 0, &fh);
  if (status != EFI_SUCCESS) {
    goto Exit;
  }

  info = FileHandleGetInfo(fh);
  if (info != NULL) {
    stat = m_new_obj(mp_obj_stat_result_t);

    stat->base.type = &mp_type_stat_result;
    stat->st_mode   = info->Attribute;
    stat->st_ino    = 0;
    stat->st_dev    = 0;
    stat->st_nlink  = 0;
    stat->st_uid    = 0;
    stat->st_gid    = 0;
    stat->st_size   = info->FileSize;
    stat->st_atime  = timeutils_seconds_since_2000 (
                        info->LastAccessTime.Year,
                        info->LastAccessTime.Month,
                        info->LastAccessTime.Day,
                        info->LastAccessTime.Hour,
                        info->LastAccessTime.Minute,
                        info->LastAccessTime.Second
                        );
    stat->st_mtime  = timeutils_seconds_since_2000 (
                        info->ModificationTime.Year,
                        info->ModificationTime.Month,
                        info->ModificationTime.Day,
                        info->ModificationTime.Hour,
                        info->ModificationTime.Minute,
                        info->ModificationTime.Second
                        );
    stat->st_ctime  = timeutils_seconds_since_2000 (
                        info->CreateTime.Year,
                        info->CreateTime.Month,
                        info->CreateTime.Day,
                        info->CreateTime.Hour,
                        info->CreateTime.Minute,
                        info->CreateTime.Second
                        );
  }

Exit:
  CLOSE_FILE(fh);
  FREE_NON_NULL(info);
  FREE_NON_NULL(path_uni);

  return MP_OBJ_FROM_PTR(stat);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mod_os_stat_obj, mod_os_stat);


STATIC mp_obj_t mod_os_remove (mp_obj_t path)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CONST CHAR8                   *path_asc;
  CHAR16                        *path_uni;
  UINTN                         len;
  EFI_FILE_HANDLE               fh;
  EFI_FILE_INFO                 *info;

  fh       = NULL;
  info     = NULL;
  path_asc = mp_obj_str_get_str(path);
  len      = strlen (path_asc);
  path_uni = Utf8ToUnicode (path_asc, NULL, &len, FALSE);

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  status = sfp->Open (sfp, path_uni, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE, 0, &fh);
  if (status != EFI_SUCCESS) {
    goto Exit;
  }

  status = EFI_ACCESS_DENIED;
  info = FileHandleGetInfo(fh);
  if (info != NULL && (info->Attribute & EFI_FILE_DIRECTORY) == 0) {
    status = fh->Delete (fh);
    fh = NULL;
  }

Exit:
  CLOSE_FILE(fh);
  FREE_NON_NULL(info);
  FREE_NON_NULL(path_uni);
  RAISE_UEFI_EXCEPTION_ON_ERROR(status);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mp_vfs_remove_obj, mod_os_remove);


STATIC mp_obj_t mod_os_rename (mp_obj_t src, mp_obj_t dst)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CONST CHAR8                   *src_asc;
  CONST CHAR8                   *dst_asc;
  CHAR16                        *src_uni;
  CHAR16                        *dst_uni;
  UINTN                         src_len;
  UINTN                         dst_len;
  EFI_FILE_HANDLE               src_fh;
  EFI_FILE_INFO                 *src_info;
  EFI_FILE_INFO                 *dst_info;

  src_info= NULL;
  dst_info= NULL;
  src_fh  = NULL;
  src_asc = mp_obj_str_get_str(src);
  dst_asc = mp_obj_str_get_str(dst);
  src_len = strlen (src_asc);
  dst_len = strlen (dst_asc);
  src_uni = Utf8ToUnicode (src_asc, NULL, &src_len, FALSE);
  dst_uni = Utf8ToUnicode (dst_asc, NULL, &dst_len, FALSE);

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  //
  // Source file or directory must exist.
  //
  status = sfp->Open (sfp, src_uni, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE, 0, &src_fh);
  if (status != EFI_SUCCESS) {
    goto Exit;
  }

  src_info = FileHandleGetInfo(src_fh);
  if (src_info != NULL) {
    dst_info = AllocatePool(sizeof(EFI_FILE_INFO) + dst_len * sizeof(CHAR16));
    ASSERT (dst_info != NULL);

    CopyMem (dst_info, src_info, sizeof(EFI_FILE_INFO));
    dst_info->FileName[0] = L'\0';
    StrnCatS(dst_info->FileName, dst_len + 1, dst_uni, dst_len);

    status = FileHandleSetInfo(src_fh, dst_info);
  }

Exit:
  CLOSE_FILE(src_fh);
  FREE_NON_NULL(src_uni);
  FREE_NON_NULL(dst_uni);
  FREE_NON_NULL(src_info);
  FREE_NON_NULL(dst_info);
  RAISE_UEFI_EXCEPTION_ON_ERROR(status);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2 (mp_vfs_rename_obj, mod_os_rename);


STATIC mp_obj_t mod_os_chdir (mp_obj_t path)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CONST CHAR8                   *path_asc;
  CHAR16                        *path_uni;
  UINTN                         len;
  EFI_FILE_HANDLE               fh;

  fh        = NULL;
  path_asc  = mp_obj_str_get_str(path);
  len       = strlen (path_asc);
  path_uni  = Utf8ToUnicode (path_asc, NULL, &len, FALSE);

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  status = sfp->Open (sfp, path_uni, EFI_FILE_MODE_READ, 0, &fh);
  if (status != EFI_SUCCESS) {
    goto Exit;
  }

  sfp->ChangeCurrentDirectory(sfp, path_uni);

Exit:
  CLOSE_FILE(fh);
  FREE_NON_NULL(path_uni);
  RAISE_UEFI_EXCEPTION_ON_ERROR(status);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mod_os_chdir_obj, mod_os_chdir);


EFI_STATUS
remove_dir (
  EFI_FILE_HANDLE     dir,
  BOOLEAN             quiet
  )
{
  EFI_STATUS        status;
  EFI_FILE_HANDLE   fh;
  EFI_FILE_INFO     *info;
  BOOLEAN           nofile;

  info = FileHandleGetInfo(dir);
  if (info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  if ((info->Attribute & EFI_FILE_DIRECTORY) == 0) {
    FREE_NON_NULL (info);
    return EFI_ACCESS_DENIED;
  }
  FREE_NON_NULL (info);

  nofile = FALSE;
  for (status = FileHandleFindFirstFile(dir, &info);
        status == EFI_SUCCESS && info != NULL && !nofile;
        status = FileHandleFindNextFile (dir, info, &nofile)) {
    //
    // Skip '.' and '..'
    //
    if (info->FileName[0] == L'.' &&
        (info->FileName[1] == L'\0' || (info->FileName[1] == L'.' &&
                                        info->FileName[2] == L'\0'))) {
      continue;
    }

    status = dir->Open(dir, &fh, info->FileName, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE, 0);
    if (status != EFI_SUCCESS) {
      return status;
    }

    if ((info->Attribute & EFI_FILE_DIRECTORY) == 0) {
      status = FileHandleDelete(fh);
    } else {
      status = remove_dir(fh, quiet);
    }

    if (status != EFI_SUCCESS) {
      fh->Close(fh);
      return status;
    }
  }

  return FileHandleDelete(dir);
}

STATIC mp_obj_t mod_os_rmdir (mp_obj_t path)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CONST CHAR8                   *path_asc;
  CHAR16                        *path_uni;
  UINTN                         len;
  EFI_FILE_HANDLE               fh;

  fh        = NULL;
  path_asc  = mp_obj_str_get_str(path);
  len       = strlen (path_asc);
  path_uni  = Utf8ToUnicode (path_asc, NULL, &len, FALSE);

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    goto Exit;
  }

  status = sfp->Open (sfp, path_uni, EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE, 0, &fh);
  if (status != EFI_SUCCESS) {
    goto Exit;
  }

  status = remove_dir (fh, TRUE);
  if (status == EFI_SUCCESS) {
    fh = NULL;
  }

Exit:
  CLOSE_FILE(fh);
  FREE_NON_NULL(path_uni);
  RAISE_UEFI_EXCEPTION_ON_ERROR(status);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mp_vfs_rmdir_obj, mod_os_rmdir);


STATIC mp_obj_t mod_os_getcwd (void)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CONST CHAR16                  *cwd_uni;
  char                          *cwd;
  UINTN                         len;
  mp_obj_t                      strobj;

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    return mp_const_none;
  }

  cwd_uni = sfp->GetCurrentDirectory (sfp);
  if (cwd_uni == NULL) {
    return mp_const_none;
  }

  len     = StrLen (cwd_uni);
  cwd     = UnicodeToUtf8 (cwd_uni, NULL, &len);
  strobj  =  mp_obj_new_str(cwd, (size_t)len);

  FREE_NON_NULL(cwd_uni);
  FREE_NON_NULL(cwd);

  return strobj;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0 (mod_os_getcwd_obj, mod_os_getcwd);


STATIC mp_obj_t listdir_next (mp_obj_t self_in)
{
  EFI_STATUS            status;
  CHAR8                 *fname = NULL;
  mp_obj_listdir_t      *it = MP_OBJ_TO_PTR(self_in);
  BOOLEAN               nofile;
  UINTN                 len;
  mp_obj_tuple_t        *finfo;

  nofile = FALSE;
  if (it->openlist == NULL) {
    status = FileHandleFindFirstFile(it->fh, &it->openlist);
  } else {
    status = FileHandleFindNextFile(it->fh, it->openlist, &nofile);
  }

  if (EFI_ERROR(status) || nofile || it->openlist == NULL) {
    it->fh->Close(it->fh);
    return MP_OBJ_STOP_ITERATION;
  }

  len = StrLen(it->openlist->FileName);
  fname = UnicodeToUtf8(it->openlist->FileName, NULL, &len);

  finfo = MP_OBJ_TO_PTR(mp_obj_new_tuple(4, NULL));
  finfo->items[0] = mp_obj_new_str(fname, len);
  if ((it->openlist->Attribute & EFI_FILE_DIRECTORY) != 0) {
    finfo->items[1] = MP_OBJ_NEW_SMALL_INT(FILE_TYPE_DIR);
  } else {
    finfo->items[1] = MP_OBJ_NEW_SMALL_INT(FILE_TYPE_FILE);
  }
  finfo->items[2] = MP_OBJ_NEW_SMALL_INT(0);
  finfo->items[3] = mp_obj_new_int_from_ull(it->openlist->FileSize);

  FREE_NON_NULL (fname);

  return MP_OBJ_FROM_PTR(finfo);
}

STATIC mp_obj_t mod_os_ilistdir (size_t n_args, const mp_obj_t *args)
{
  EFI_STATUS                    status;
  EFI_SCRIPT_FILE_PROTOCOL      *sfp;
  CHAR16                        *path_uni;
  EFI_FILE_HANDLE               fh = NULL;
  mp_obj_listdir_t              *itobj;
  const char                    *path;
  UINTN                         len;

  status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&sfp);
  if (EFI_ERROR(status)) {
    return mp_const_none;
  }

  if (n_args > 0) {
    path = mp_obj_str_get_str(args[0]);
    len = strlen (path);
    path_uni = Utf8ToUnicode (path, NULL, &len, FALSE);
  } else {
    path_uni = sfp->GetCurrentDirectory (sfp);
    if (path_uni == NULL) {
      return mp_const_none;
    }
  }

  itobj = (mp_obj_listdir_t *)mp_const_empty_tuple;
  status = sfp->Open (sfp, path_uni, EFI_FILE_MODE_READ, 0, &fh);
  if (!EFI_ERROR(status)) {
    itobj = m_new_obj(mp_obj_listdir_t);

    itobj->base.type = &mp_type_polymorph_iter;
    itobj->fh        = fh;
    itobj->openlist  = NULL;
    itobj->iternext  = listdir_next;
  }

  if (EFI_ERROR(status) && fh != NULL) {
    fh->Close(fh);
  }

  FREE_NON_NULL (path_uni);

  return MP_OBJ_FROM_PTR(itobj);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (mod_os_ilistdir_obj, 0, 1, mod_os_ilistdir);


STATIC mp_obj_t mod_os_listdir (size_t n_args, const mp_obj_t *args)
{
  mp_obj_t              itobj;
  mp_obj_t              finfo;
  UINTN                 fnum;
  UINTN                 index;
  file_list_t           flist;
  file_list_t           *file;
  mp_obj_tuple_t        *ftuple;

  itobj = mod_os_ilistdir (n_args, args);
  if (itobj == mp_const_none) {
    return mp_const_empty_tuple;
  }

  fnum = 0;
  InitializeListHead (&flist.link);
  while (TRUE) {
    finfo = listdir_next(itobj);
    if (finfo == MP_OBJ_STOP_ITERATION) {
      break;
    }

    file = AllocatePool (sizeof(file_list_t));
    ASSERT (file != NULL);

    file->info = finfo;
    InsertTailList(&flist.link, &file->link);
    ++fnum;
  }

  if (fnum > 0) {
    ftuple = MP_OBJ_TO_PTR(mp_obj_new_tuple(fnum, NULL));
    for (index = 0; index < fnum; ++index) {
      file = (file_list_t *)GetFirstNode (&flist.link);
      ftuple->items[index] = file->info;
      RemoveEntryList(&file->link);
      FreePool (file);
    }
  } else {
    ftuple = mp_const_empty_tuple;
  }

  return MP_OBJ_FROM_PTR(ftuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (mod_os_listdir_obj, 0, 1, mod_os_listdir);

STATIC char *get_smbios_string(SMBIOS_STRUCTURE *header, UINTN str_index)
{
  UINTN         index;
  char          *buf;
  UINTN         len;

  if (str_index < 1) {
    return "";
  }

  str_index -= 1;
  buf = (char *)header + header->Length;
  for (index = 0; index < str_index; ++index) {
    len = strlen(buf);
    if (len == 0) {
      break;
    }
    buf += len + 1;
  }

  if (index == str_index) {
    return buf;
  }

  return "";
}

STATIC mp_obj_t mod_os_uname (void)
{
  EFI_STATUS                status;
  mp_obj_tuple_t            *uname;
  EFI_SMBIOS_PROTOCOL       *sbp;
  EFI_SMBIOS_HANDLE         handle;
  EFI_SMBIOS_TYPE           type;
  SMBIOS_TABLE_TYPE0        *bios_info;
  SMBIOS_TABLE_TYPE4        *cpu_info;
  CHAR8                     tmp_str[64];
  CHAR8                     *str;
  UINTN                     len;

  uname = mp_obj_new_tuple(5, NULL);
  uname->items[0] = mp_obj_new_str(MICROPY_PY_SYS_PLATFORM, strlen(MICROPY_PY_SYS_PLATFORM));
  uname->items[1] = mp_const_empty_bytes; // nodename: the network name (can be the same as sysname)
  uname->items[2] = mp_const_empty_bytes; // release: the version of the underlying system
  uname->items[3] = mp_obj_new_str(MICROPY_VERSION_STRING, strlen(MICROPY_VERSION_STRING));
  uname->items[4] = mp_const_empty_bytes; // machine: an identifier for the underlying hardware (eg board, CPU)

  status = gBS->LocateProtocol (&gEfiSmbiosProtocolGuid, NULL, (VOID **)&sbp);
  if (!EFI_ERROR(status)) {
    handle = 0xFFFE;
    type = EFI_SMBIOS_TYPE_BIOS_INFORMATION;
    status = sbp->GetNext (sbp, &handle, &type, (EFI_SMBIOS_TABLE_HEADER **)&bios_info, NULL);
    if (!EFI_ERROR(status)) {
      str = get_smbios_string(&bios_info->Hdr, bios_info->BiosVersion);
      len = AsciiSPrint(tmp_str, sizeof(tmp_str), "%a(%d.%d)", str,
                        bios_info->SystemBiosMajorRelease,
                        bios_info->SystemBiosMinorRelease);
      uname->items[2] = mp_obj_new_str(tmp_str, len);
    }

    handle = 0xFFFE;
    type = EFI_SMBIOS_TYPE_PROCESSOR_INFORMATION;
    status = sbp->GetNext (sbp, &handle, &type, (EFI_SMBIOS_TABLE_HEADER **)&cpu_info, NULL);
    if (!EFI_ERROR(status)) {
      str = get_smbios_string(&cpu_info->Hdr, cpu_info->ProcessorVersion);
      uname->items[4] = mp_obj_new_str(str, strlen(str));
    }
  }


  return MP_OBJ_FROM_PTR(uname);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0 (mod_os_uname_obj, mod_os_uname);


STATIC mp_obj_t mod_os_urandom (mp_obj_t n_in)
{
  EFI_STATUS            status;
  EFI_RNG_PROTOCOL      *rng;
  UINTN                 number;
  UINT8                 *rand;
  mp_obj_array_t        *bytes;
  BOOLEAN               Done;

  number = mp_obj_get_int_truncated(n_in);
  rand = AllocateZeroPool (number);
  ASSERT (rand != NULL);

  status = gBS->LocateProtocol(&gEfiRngProtocolGuid, NULL, (VOID **)&rng);
  if (EFI_ERROR(status)) {

    if (number <= 2) {
      Done = AsmRdRand16((UINT16 *)rand);
    } else if (number <= 4) {
      Done = AsmRdRand32((UINT32 *)rand);
    } else if (number <= 8) {
      Done = AsmRdRand64((UINT64 *)rand);
    } else {
      Done = FALSE;
    }

    if (!Done) {
      FREE_NON_NULL (rand);
      RAISE_UEFI_EXCEPTION_ON_ERROR(status);
    }

  } else {

    status = rng->GetRNG (rng, NULL, number, rand);
    if (EFI_ERROR(status)) {
      FREE_NON_NULL (rand);
      RAISE_UEFI_EXCEPTION_ON_ERROR(status);
    }

  }

  bytes = mp_obj_new_bytearray(number, rand);
  FREE_NON_NULL (rand);

  return MP_OBJ_FROM_PTR(bytes);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mod_os_urandom_obj, mod_os_urandom);


STATIC const mp_obj_type_t mp_type_stat_result = {
    { &mp_type_type },
    .name = MP_QSTR_stat_result,
    .print = stat_result_print,
    .attr = stat_result_attr,
};

STATIC const mp_rom_map_elem_t mp_module_os_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__),  MP_ROM_QSTR(MP_QSTR_uos)},
  { MP_ROM_QSTR(MP_QSTR_stat),      MP_ROM_PTR(&mod_os_stat_obj)},
  { MP_ROM_QSTR(MP_QSTR_system),    MP_ROM_PTR(&mod_os_system_obj)},
  { MP_ROM_QSTR(MP_QSTR_mkdir),     MP_ROM_PTR(&mod_os_mkdir_obj)},
  { MP_ROM_QSTR(MP_QSTR_listdir),   MP_ROM_PTR(&mod_os_listdir_obj)},
  { MP_ROM_QSTR(MP_QSTR_ilistdir),  MP_ROM_PTR(&mod_os_ilistdir_obj)},
  { MP_ROM_QSTR(MP_QSTR_remove),    MP_ROM_PTR(&mp_vfs_remove_obj)},
  { MP_ROM_QSTR(MP_QSTR_rename),    MP_ROM_PTR(&mp_vfs_rename_obj)},
  { MP_ROM_QSTR(MP_QSTR_rmdir),     MP_ROM_PTR(&mp_vfs_rmdir_obj)},
  { MP_ROM_QSTR(MP_QSTR_chdir),     MP_ROM_PTR(&mod_os_chdir_obj)},
  { MP_ROM_QSTR(MP_QSTR_getcwd),    MP_ROM_PTR(&mod_os_getcwd_obj)},
  { MP_ROM_QSTR(MP_QSTR_uname),     MP_ROM_PTR(&mod_os_uname_obj)},
  { MP_ROM_QSTR(MP_QSTR_urandom),   MP_ROM_PTR(&mod_os_urandom_obj)},
};

STATIC MP_DEFINE_CONST_DICT (mp_module_os_globals, mp_module_os_globals_table);

const mp_obj_module_t mp_module_os = {
  .base = {&mp_type_module },
  .globals = (mp_obj_dict_t *)&mp_module_os_globals,
};

