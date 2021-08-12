/** @file
  Implementation of Script Engine Protocol of MicroPython.

Copyright (c) 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <string.h>

#include <py/mpstate.h>
#include <py/nlr.h>
#include <py/compile.h>
#include <py/runtime.h>
#include <py/builtin.h>
#include <py/repl.h>
#include <py/gc.h>
#include <py/stackctrl.h>
#include <py/mphal.h>

#include <lib/mp-readline/readline.h>
#include <extmod/misc.h>

#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ScriptEngineProtocol.h>
#include <Protocol/ScriptFileProtocol.h>
#include <Protocol/PciIo.h>
#include <Protocol/Shell.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Protocol/LoadedImage.h>
#include <Library/DevicePathLib.h>

#include "genhdr/mpversion.h"
#include "upy.h"
#include "repl.h"

#define UPY_ASYNC_EXEC_TIMER_INTERVAL   ((100 * 1000 * 1000) / 100) // 100ms in 100ns unit

#define UPY_PRIVATE_DATA_FROM_SEP(a)    \
          CR (a, UPY_PRIVATE_DATA, ScriptEngine, EFI_SCRIPT_ENGINE_TYPE_MICROPYTHON)

typedef struct {
  EFI_HANDLE                      Handle;
  EFI_EVENT                       ExecEvent;
  EFI_STATUS                      ExecStatus;
  EFI_SCRIPT_ENGINE_PROTOCOL      *ScriptEngineProtocol;
  EFI_SCRIPT_FILE_PROTOCOL        *ScriptFileProtocol;
  UINT8                           *Script;
  UINTN                           ScriptLength;
  BASE_LIBRARY_JUMP_BUFFER        ExecutorContext;
  BASE_LIBRARY_JUMP_BUFFER        ScriptEngineContext;
  VOID                            *ScriptEngineStack;
  UINTN                           ScriptEngineStackSize;
  BOOLEAN                         IsRunning;
  BOOLEAN                         DeinitAfterwards;
} UPY_EXECUTOR_DATA;

typedef struct {
  CONST UINT32                Signature;
  EFI_SCRIPT_ENGINE_PROTOCOL  ScriptEngine;
} UPY_PRIVATE_DATA;

STATIC UPY_EXECUTOR_DATA  mExecutorData = {0};
STATIC UINTN              mStackSize = SIZE_256KB;
#if MICROPY_ENABLE_GC
STATIC VOID               *mHeap = NULL;
STATIC UINTN              mHeapSize = SIZE_64MB;
#endif

#define MPY_FEATURE_FLAGS_DYNAMIC ( \
    ((MICROPY_OPT_CACHE_MAP_LOOKUP_IN_BYTECODE_DYNAMIC) << 0) \
    | ((MICROPY_PY_BUILTINS_STR_UNICODE_DYNAMIC) << 1) \
    )

/**
  Helper method used to print error message.
**/
STATIC void stderr_print_strn(void *env, const char *str, size_t len)
{
  EFI_STATUS        Status;
  EFI_FILE_HANDLE   Output;

  Status = mExecutorData.ScriptFileProtocol->GetFileHandle (
                                               mExecutorData.ScriptFileProtocol,
                                               STDOUT_FILENO,
                                               &Output
                                               );
  if (EFI_ERROR (Status)) {
    Status = mExecutorData.ScriptFileProtocol->GetFileHandle (
                                                 mExecutorData.ScriptFileProtocol,
                                                 STDERR_FILENO,
                                                 &Output
                                                 );
  }

  if (Output != NULL) {
    Output->Write (Output, (UINTN *)&len, (VOID *)str);
  }
}


#define FORCED_EXIT (0x100)
const mp_print_t mp_stderr_print = {NULL, stderr_print_strn};


/**
  If exc is SystemExit, return value where FORCED_EXIT bit set,
  and lower 8 bits are SystemExit value. For all other exceptions,
  return 1.
**/
STATIC int handle_uncaught_exception(mp_obj_base_t *exc) {
  // check for SystemExit
  if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
    // None is an exit value of 0; an int is its value; anything else is 1
    mp_obj_t exit_val = mp_obj_exception_get_value(MP_OBJ_FROM_PTR(exc));
    mp_int_t val = 0;

    if (exit_val != mp_const_none && !mp_obj_get_int_maybe(exit_val, &val)) {
      val = 1;
    }
    return FORCED_EXIT | (val & 255);
  }

  // Report all other exceptions
  mp_obj_print_exception(&mp_stderr_print, MP_OBJ_FROM_PTR(exc));
  stderr_print_strn (NULL, "\r\n", 2);

  return 1;
}

/**
  Returns standard error codes: 0 for success, 1 for all other errors,
  except if FORCED_EXIT bit is set then script raised SystemExit and the
  value of the exit is in the lower 8 bits of the return value
**/
STATIC int execute_from_lexer(mp_lexer_t *lex, mp_parse_input_kind_t input_kind, bool is_repl) {
  nlr_buf_t nlr;

  if (nlr_push(&nlr) == 0) {
    qstr source_name = lex->source_name;

#if MICROPY_PY___FILE__
    if (input_kind == MP_PARSE_FILE_INPUT) {
      mp_store_global(MP_QSTR___file__, MP_OBJ_NEW_QSTR(source_name));
    }
#endif

    mp_parse_tree_t parse_tree = mp_parse(lex, input_kind);

#if MICROPY_DEBUG_PRINTERS
    mp_parse_node_print(parse_tree.root, 0);
#endif

    mp_obj_t module_fun = mp_compile(&parse_tree, source_name, MP_EMIT_OPT_NONE, is_repl);

    // execute it
    mp_call_function_0(module_fun);
    // check for pending exception
    if (MP_STATE_VM(mp_pending_exception) != MP_OBJ_NULL) {
      mp_obj_t obj = MP_STATE_VM(mp_pending_exception);

      MP_STATE_VM(mp_pending_exception) = MP_OBJ_NULL;
      nlr_raise(obj);
    }

    nlr_pop();
    return 0;

  } else {
    // uncaught exception
    return handle_uncaught_exception(nlr.ret_val);
  }
}

/**
  Helper method required by MicroPython to handle unrecoverable exception.
**/
void nlr_jump_fail(void *val) {
  DEBUG ((DEBUG_ERROR, "FATAL: uncaught NLR %p\n", val));
  ASSERT (FALSE);
  for (;;); // needed to silence compiler warning
}

/**
  Helper method required by MicroPython to implement file/package import.
**/
mp_import_stat_t mp_import_stat(const char *path)
{
  EFI_SCRIPT_FILE_PROTOCOL      *Fs;
  EFI_FILE_HANDLE               Fh;
  UINTN                         HandleCount;
  UINTN                         Index;
  EFI_HANDLE                    *HandleBuffer;
  EFI_STATUS                    Status;
  mp_import_stat_t              FileStat;
  EFI_FILE_INFO                 *FileInfo;
  CHAR16                        *FileFullPath;
  UINTN                         PathLength;

  PathLength = AsciiStrLen (path);
  FileFullPath = Utf8ToUnicode (path, NULL, &PathLength, FALSE);
  ASSERT(FileFullPath != NULL && PathLength > 0);

  // Use script file protocol to check the file stat
  HandleBuffer = NULL;
  HandleCount = 0;
  FileStat = MP_IMPORT_STAT_NO_EXIST;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiScriptFileProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (Status == EFI_SUCCESS && HandleCount > 0) {
    for (Index = 0; Index < HandleCount; ++Index) {
      Status = gBS->HandleProtocol(HandleBuffer[Index],
                                   &gEfiScriptFileProtocolGuid,
                                   (VOID *)&Fs);
      ASSERT_EFI_ERROR (Status);

      Status = Fs->Open(Fs, FileFullPath, EFI_FILE_MODE_READ, 0, &Fh);
      if (EFI_ERROR(Status)) {
        continue;
      }

      FileInfo = FileHandleGetInfo(Fh);
      if (FileInfo != NULL) {
        if (FileInfo->Attribute & EFI_FILE_DIRECTORY) {
          FileStat = MP_IMPORT_STAT_DIR;
        } else {
          FileStat = MP_IMPORT_STAT_FILE;
        }
        break;
      }

      FileHandleClose(Fh);
    }
  }

  FreePool(FileFullPath);
  if (HandleBuffer) {
    gBS->FreePool(HandleBuffer);
  }

  return FileStat;
}

/**
  Initialize MicroPython interpreter to be ready for its first line of code.
**/
EFI_STATUS
EFIAPI
UpyInit (
  VOID
)
{
  EFI_STATUS                  Status;
  EFI_LOADED_IMAGE_PROTOCOL   *LoadedImage;
  EFI_SCRIPT_FILE_PROTOCOL    *ScriptFile;
  EFI_DEVICE_PATH_PROTOCOL    *DevicePath;
  CHAR16                      *ImageFilePath;
  CHAR16                      *FileName;
  CHAR16                      *CurDir;
  CHAR8                       *AscPath;
  UINTN                       Length;


  CurDir = NULL;
  AscPath = NULL;
  FileName = NULL;
  ImageFilePath = NULL;

  DEBUG((DEBUG_INFO, "UpyInit()\r\n"));

  // MicroPython context common setup
  mp_stack_ctrl_init();
  mp_stack_set_limit (mStackSize);

#if MICROPY_ENABLE_GC
  mHeap = AllocateAlignedPages(EFI_SIZE_TO_PAGES(mHeapSize), 16);
  if (mHeap == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  gc_init(mHeap, (UINT8 *)mHeap + mHeapSize);
#endif

#if MICROPY_ENABLE_PYSTACK
  static mp_obj_t pystack[1024];
  mp_pystack_init(pystack, &pystack[MP_ARRAY_SIZE(pystack)]);
#endif

  mp_init();
  mp_obj_list_init(MP_OBJ_TO_PTR(mp_sys_argv), 0);
  mp_obj_list_init(MP_OBJ_TO_PTR(mp_sys_path), 0);

  if (mExecutorData.Handle != NULL) {
    moduefi_init(mExecutorData.Handle);
  }

  Status = gBS->OpenProtocol(
                  mExecutorData.Handle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **) &LoadedImage,
                  mExecutorData.Handle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  ASSERT_EFI_ERROR (Status);

  ScriptFile = mExecutorData.ScriptFileProtocol;
  ASSERT (ScriptFile != NULL);

  DevicePath = AppendDevicePath(
                DevicePathFromHandle(LoadedImage->DeviceHandle),
                LoadedImage->FilePath
                );
  ImageFilePath = ScriptFile->GetFilePath (ScriptFile, DevicePath);
  ASSERT (ImageFilePath != NULL);
  FREE_NON_NULL (DevicePath);

  //
  // Remove the file name part.
  //
  FileName = StrStrR (ImageFilePath, L"\\");
  if (FileName != NULL) {
    FileName[0] = '\0';
  }

  CurDir = ScriptFile->GetCurrentDirectory(ScriptFile);
  if (CurDir == NULL) {
    ScriptFile->ChangeCurrentDirectory(ScriptFile, ImageFilePath);
  }

  //
  // Add current directory to sys.path
  //
  if (CurDir != NULL && StrCmp (CurDir, ImageFilePath) != 0) {
    Length = StrLen(CurDir);
    AscPath = UnicodeToUtf8(CurDir, NULL, &Length);
    mp_obj_list_append(mp_sys_path, mp_obj_new_str(AscPath, (size_t)Length));

    FREE_NON_NULL (CurDir);
    FREE_NON_NULL (AscPath);
  }

  //
  // Always add image directory to sys.path
  //
  Length = StrLen(ImageFilePath);
  AscPath = UnicodeToUtf8(ImageFilePath, NULL, &Length);
  mp_obj_list_append(mp_sys_path, mp_obj_new_str(AscPath, (size_t)Length));

  FREE_NON_NULL (AscPath);
  FREE_NON_NULL (ImageFilePath);

  mExecutorData.ExecStatus = EFI_SUCCESS;

  return EFI_SUCCESS;
}

/**
  Destroy the MicroPython interpreter and release allocated resources.
**/
EFI_STATUS
EFIAPI
UpyDeinit (
)
{
  DEBUG((DEBUG_INFO, "UpyDeinit()\r\n"));

  mp_deinit();

#if MICROPY_ENABLE_GC
  if (mHeap != NULL) {
    FreePages(mHeap, EFI_SIZE_TO_PAGES(mHeapSize));
    mHeap = NULL;
  }
#endif

  moduefi_deinit();

  return EFI_SUCCESS;
}

/**
  Entry method to run MicroPython code in REPL (interactive shell).
**/
STATIC
int
DoRepl (
  VOID
)
{
  vstr_t    line;
  int       ret;
  int       parse_input_kind;

  vstr_init(&line, 128);
  ReplInit (&line);

  for (;;) {
    vstr_reset(&line);
    parse_input_kind = ReplLoop ();
    if (parse_input_kind == -1) {
      mp_hal_stdout_tx_str("\r\n");
      break;
    }

    if (line.len == 0) {
      continue;
    }

    mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, line.buf, line.len, false);
    ret = execute_from_lexer(lex, parse_input_kind, true);
    if (ret & 0x100) {
      return ret;
    }
  }

  return EFI_SUCCESS;
}

/**
  Entry method to execute MicroPython code in string.
**/
STATIC
int
DoStr (
  const char *str
)
{
  mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, str, AsciiStrLen(str), 0);
  return execute_from_lexer(lex, MP_PARSE_FILE_INPUT, false);
}

/**
  Entry method to execute MicroPython code in complied way.
**/
STATIC
int
DoMpy (
  const char *buf
)
{
  return 0;
}

/**
  Save MicroPython execution context and jump back to UEFI context.

  This method is used by MicroPython to proactively release its control to UEFI
  code. The value passed to UEFI code is the time (in milliseconds) used to tell
  UEFI code when it should return the control back to MicroPython.
**/
mp_obj_t
UpySuspend (
  mp_obj_t    ms
)
{
  UINTN   time = (UINTN)mp_obj_get_int(ms);

  if (mExecutorData.ExecEvent == NULL) {
    mp_raise_msg(&mp_type_RuntimeError, "Current interpreter is not running in async mode.");
  }

  if (SetJump(&mExecutorData.ScriptEngineContext) == 0) {
    //
    // Use LongJump to pass time value (in higher 24-bit or 56-bit)
    //
    DEBUG((DEBUG_INFO, "UpySuspend(): %dms\r\n", time));
    LongJump(&mExecutorData.ExecutorContext, (time << 8) | 1);
  }

  DEBUG((DEBUG_INFO, "UpySuspend(): woke up\r\n", time));

  return mp_const_none;
}

/**
  Entry method of MicroPython interpreter.

  This is basically the timer event handler in async mode. In normal mode,
  it's called directly with parameters of NULL value.

  @param[in]  Event       Event whose notification function is being invoked.
  @param[in]  Context     The pointer to the notification function's context,
                          which is implementation-dependent.

**/
EFI_STATUS
EFIAPI
UpyDoScript (
  EFI_SCRIPT_ENGINE_PROTOCOL      *This,
  UINT8                           *Source
)
{
  EFI_STATUS          Status;

  if (mExecutorData.ScriptEngineStack != NULL) {
    if (!mExecutorData.IsRunning) {
      mp_stack_set_top((UINT8 *)mExecutorData.ScriptEngineStack +
                       mExecutorData.ScriptEngineStackSize);
      //
      // MicroPython's stack check cannot take the code of UEFI part into
      // account. We should not set the limit to the whole stack but just
      // MicroPython part.
      //
      mp_stack_set_limit (mStackSize);
    }
  } else {
    mp_stack_ctrl_init();
    mp_stack_set_limit (mStackSize);
  }

  mExecutorData.IsRunning = TRUE;

  // Header of compiled Python script contains
  //  byte  'M'
  //  byte  version
  //  byte  feature flags
  //  byte  number of bits in a small int
  if (Source != NULL) {
    if (Source[0] == 'M' && Source[1] == 0 && Source[2] == MPY_FEATURE_FLAGS_DYNAMIC) {
      Status = DoMpy((const char *)Source);
    } else {
      Status = DoStr((const char *)Source);
    }
  } else {
    Status = DoRepl();
  }

  // We have to free the memory allocated for script here because of aync exec mode
  if (Source != NULL) {
    FreePool(Source);
  }

  if (Status != 0) {
    Status = EFI_LOAD_ERROR;
  }

  //
  // We have mixed memory management. Do once gc.collect before exit to let
  // those objects having __del__() method be called to release resources not
  // managed by MicroPython.
  //
  gc_collect();

  //
  // Normal exit from script interpreter. For asynchronous execution, we need
  // to LongJump back to timer event handler.
  //
  mExecutorData.ExecStatus = Status;
  if (mExecutorData.ScriptEngineStack != NULL) {
    LongJump(&mExecutorData.ExecutorContext, 2);
  }

  return Status;
}

/**
  Helper method used primarily for async execution mode for MicroPython.

  This is the handler of timer event in async mode. In normal mode, it's called
  directly with parameters of NULL value.

  @param[in]  Event       Event whose notification function is being invoked.
  @param[in]  Context     The pointer to the notification function's context,
                          which is implementation-dependent.

**/
VOID
EFIAPI
ScriptExecutor (
  IN EFI_EVENT        Event,
  IN VOID             *Context
)
{
  EFI_STATUS      Status;
  UINTN           Value;

  Value = SetJump(&mExecutorData.ExecutorContext);
  switch (Value & 0xF) {
  case 0:
    // Normal return from just above call of SetJump().
    DEBUG((DEBUG_INFO, "ScriptExecutor(): SetJump => 0: return from saving jump context\r\n"));
    //
    // If it's the first time to launch the script engine, we must preprare a
    // standalone stack for it in order to avoid using the stack of current
    // function, because this timer event handler has to exit, when necessary,
    // to give a chance to other modules to run.
    //
    if (!mExecutorData.IsRunning && mExecutorData.ScriptEngineProtocol != NULL) {
      // Use SwitchStack to call the entry of script engine
      SwitchStack(
        (SWITCH_STACK_ENTRY_POINT)UpyDoScript,
        mExecutorData.ScriptEngineProtocol,
        mExecutorData.Script,
        (UINT8 *)mExecutorData.ScriptEngineStack +
        mExecutorData.ScriptEngineStackSize
        );
    } else {
      // Simply restore the execution of script engine if it's still running.
      LongJump(&mExecutorData.ScriptEngineContext, 1);
    }
    break;

  case 1:
    // LongJump() return from the middle execution of script interpreter.
    // It means interpreter relinguish its contol of processor temporarily.
    // Do nothing special but wait for another timer expiration.
    DEBUG((DEBUG_INFO, "ScriptExecutor(): SetJump => 1: yield from script engine\r\n"));

    // Value returned by LongJump may contain the delay time value for another re-entry.
    Value = Value >> 8;
    Value = (Value == 0) ? UPY_ASYNC_EXEC_TIMER_INTERVAL : (Value * 1000 * 10);
    Status = gBS->SetTimer(
                    mExecutorData.ExecEvent,
                    TimerRelative,
                    Value
                    );
    ASSERT_EFI_ERROR(Status);
    break;

  default:
    // Exeuction of interpreter done. Let's do clean-up.
    DEBUG((DEBUG_INFO, "ScriptExecutor(): SetJump => %d: exit from script engine\r\n", Value));
    if (mExecutorData.ScriptEngineStack != NULL) {
      FreeAlignedPages(
        mExecutorData.ScriptEngineStack,
        EFI_SIZE_TO_PAGES (mExecutorData.ScriptEngineStackSize)
        );
      mExecutorData.ScriptEngineStack = NULL;
      mExecutorData.ScriptEngineStackSize = 0;
    }

    if (mExecutorData.DeinitAfterwards) {
      UpyDeinit();
      mExecutorData.ScriptEngineProtocol = NULL;
    }
    mExecutorData.IsRunning = FALSE;
    break;
  }
}

/**
  Get the type of current script engine instance.

  @param  This        A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.

  @retval UINT32 value used to identify the type of current script engine.

**/
UINT32
EFIAPI
UpyGetType (
  EFI_SCRIPT_ENGINE_PROTOCOL      *This
)
{
  UPY_PRIVATE_DATA    *PrivateData;

  PrivateData = UPY_PRIVATE_DATA_FROM_SEP(This);
  return PrivateData->Signature;
}

/**
  Execute script code given in string asynchronously. This API will return
  immediately after launching the script engine and feeding the script to it.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Script    A pointer to string buffer contaning the script code.
  @param  Length    Length of string buffer passed by parameter SCript.
  @param  Sharable  Value of TRUE is used to tell script engine not to destroy
                    itself after the script has been executed. Otherwise, the
                    script engine will initialize itself upon each calling of
                    this protocol API.

  @retval EFI_SUCCESS           The script is lauched successfully.
  @retval EFI_LOAD_ERROR        The script is lauched with error.
  @retval EFI_ALREADY_STARTED   There's another script in running.
  @retval EFI_OUT_OF_RESOURCES  Not enough resource to start the script engine.

**/
EFI_STATUS
EFIAPI
UpyExecuteAsync (
  EFI_SCRIPT_ENGINE_PROTOCOL      *This,
  UINT8                           *Source,
  UINTN                           Length,
  BOOLEAN                         Sharable
)
{
  EFI_STATUS          Status;

  if (Source == NULL || Length == 0) {
    return EFI_INVALID_PARAMETER;
  }

  if (mExecutorData.IsRunning) {
    return EFI_ALREADY_STARTED;
  }

  if (mExecutorData.ScriptEngineProtocol == NULL) {
    UpyInit();
    mExecutorData.ScriptEngineProtocol = This;
  }

  mExecutorData.DeinitAfterwards = !Sharable;
  mExecutorData.Script = Source;
  mExecutorData.ScriptLength = Length;

  //
  // Use separate stack for MicroPython.
  //
  mExecutorData.ScriptEngineStackSize = mStackSize;
  mExecutorData.ScriptEngineStack = AllocateAlignedPages(
                                      EFI_SIZE_TO_PAGES (
                                        mExecutorData.ScriptEngineStackSize
                                        ),
                                      CPU_STACK_ALIGNMENT
                                      );
  ASSERT (mExecutorData.ScriptEngineStack != NULL);

  //
  // Use a timer to schedule the running of script engine. Then we can go back
  // to caller of this method immediately.
  //
  Status = gBS->CreateEvent(
                 EVT_TIMER | EVT_NOTIFY_SIGNAL,
                 TPL_CALLBACK,
                 ScriptExecutor,
                 (VOID *)&mExecutorData,
                 &mExecutorData.ExecEvent
                 );
  ASSERT_EFI_ERROR(Status);

  //
  // Set the alarm.
  //
  Status = gBS->SetTimer(
                 mExecutorData.ExecEvent,
                 TimerRelative,
                 1
                 );
  ASSERT_EFI_ERROR(Status);

  return Status;
}

/**
  Execute script code given in string.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Script    A pointer to string buffer contaning the script code.
  @param  Length    Length of string buffer passed by parameter SCript.
  @param  Sharable  Value of TRUE is used to tell script engine not to destroy
                    itself after the script has been executed. Otherwise, the
                    script engine will initialize itself upon each calling of
                    this protocol API.

  @retval EFI_SUCCESS           The script is executed successfully.
  @retval EFI_LOAD_ERROR        The script is executed with error.
  @retval EFI_ALREADY_STARTED   There's another script in running.
  @retval EFI_OUT_OF_RESOURCES  Not enough resource to start the script engine.

**/
EFI_STATUS
EFIAPI
UpyExecute (
  EFI_SCRIPT_ENGINE_PROTOCOL      *This,
  UINT8                           *Source,
  UINTN                           Length,
  BOOLEAN                         Sharable
)
{
  EFI_STATUS      Status;

  if (mExecutorData.IsRunning) {
    return EFI_ALREADY_STARTED;
  }

  if (mExecutorData.ScriptEngineProtocol == NULL) {
    Status = UpyInit();
    if (EFI_ERROR(Status)) {
      return Status;
    }
    mExecutorData.ScriptEngineProtocol = This;
  }

  mExecutorData.DeinitAfterwards = !Sharable;
  mExecutorData.ExecEvent = NULL;
  mExecutorData.Script = Source;
  mExecutorData.ScriptLength = Length;

  //
  // Use separate stack for MicroPython.
  //
  mExecutorData.ScriptEngineStackSize = mStackSize;
  mExecutorData.ScriptEngineStack = AllocateAlignedPages(
                                      EFI_SIZE_TO_PAGES (mExecutorData.ScriptEngineStackSize),
                                      CPU_STACK_ALIGNMENT
                                      );
  ASSERT (mExecutorData.ScriptEngineStack != NULL);

  ScriptExecutor (NULL, NULL);

  return mExecutorData.ExecStatus;
}

/**
  Evaluate a line of script code and return the result.

  @param  This        A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  ScriptLine  A pointer to string buffer contaning the script code.
  @param  Result      Buffer used to hold the result of evaluation. The memory
                      layout of the buffer is script engine type dependent.

  @retval EFI_SUCCESS           The script is executed successfully.
  @retval EFI_LOAD_ERROR        The script is executed with error.
  @retval EFI_ALREADY_STARTED   There's another script in running.
  @retval EFI_OUT_OF_RESOURCES  Not enough resource to start the script engine.
  @retval EFI_UNSUPPORTED       The script engine doesn't support evaluation mode.

**/
EFI_STATUS
EFIAPI
UpyEvaluate (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL  *This,
  IN      CHAR8                       *ScriptLine,
  IN  OUT VOID                        *Result
)
{
  return EFI_UNSUPPORTED;
}

/**
  Set or unset an environment variable.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Name      The name of the environment variable.
  @param  Value     The string value of the environment variable. NULL means
                    unsetting.

  @retval EFI_SUCCESS           The environment variable is set successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support environment
                                variable.

**/
EFI_STATUS
EFIAPI
UpySetEnv (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      CHAR16                        *Name,
  IN      CHAR16                        *Value
)
{
  // MicroPython core does't support environment variables, which is supposed to
  // be done by os module according to CPython convention.
  return EFI_UNSUPPORTED;
}

/**
  Pass standard command line options to script engine.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Argc      Number of command line options.
  @param  Argv      Array of command line options.

  @retval EFI_SUCCESS           The operation is done successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support processing
                                command line options.

**/
EFI_STATUS
EFIAPI
UpySetArgs (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      UINTN                         Argc,
  IN      CONST CHAR16                  *Argv[]
)
{
  UINTN           Index;
  CHAR8           Buffer[1024];
  CHAR8           *ArgStr;
  UINTN           ArgSize;
  mp_obj_list_t   *SysArgv = MP_OBJ_TO_PTR(mp_sys_argv);

  if (This == NULL || Argv == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (mExecutorData.ScriptEngineProtocol == NULL) {
    UpyInit();
    mExecutorData.ScriptEngineProtocol = This;
  }

  if (SysArgv->len > 0) {
    // Clear the list if not empty
    SysArgv->len = 0;
    SysArgv->items = m_renew(mp_obj_t, SysArgv->items, SysArgv->alloc, Argc);
    SysArgv->alloc = Argc;
    mp_seq_clear(SysArgv->items, 0, SysArgv->alloc, sizeof(*SysArgv->items));
  }

  for (Index = 0; Index < Argc; Index++) {
    if (Argv[Index] != NULL) {
      ArgSize = StrLen(Argv[Index]);
      if (ArgSize >= ARRAY_SIZE(Buffer)) {
        ArgStr = NULL;
      } else {
        ArgStr = Buffer;
      }

      ArgStr = ToUpyString(Argv[Index], ArgStr, &ArgSize);
      mp_obj_list_append(mp_sys_argv, MP_OBJ_NEW_QSTR(qstr_from_str(ArgStr)));
      if (ArgStr != Buffer) {
        FreePool(ArgStr);
      }
    }
  }

  return EFI_SUCCESS;
}

/**
  Add a path to system PATH.

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  Path      A path string to be added. A NULL value means to clear
                    the system PATH.

  @retval EFI_SUCCESS           The path is added successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support system PATH.

**/
EFI_STATUS
EFIAPI
UpySetPath (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      CONST CHAR16                  *Path
)
{
  CHAR8           Buffer[1024];
  CHAR8           *PathStr;
  UINTN           PathSize;
  mp_obj_list_t   *SysPath = MP_OBJ_TO_PTR(mp_sys_path);

  if (This == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (mExecutorData.ScriptEngineProtocol == NULL) {
    UpyInit();
    mExecutorData.ScriptEngineProtocol = This;
  }

  if (Path == NULL && SysPath->len > 0) {
    // Clear the list if not empty
    SysPath->len = 0;
    SysPath->items = m_renew(mp_obj_t, SysPath->items, SysPath->alloc, 1);
    SysPath->alloc = 1;
    mp_seq_clear(SysPath->items, 0, SysPath->alloc, sizeof(*SysPath->items));
  }

  if (Path) {
    PathSize = StrLen(Path);
    if (PathSize >= ARRAY_SIZE(Buffer)) {
      PathStr = NULL;
    } else {
      PathStr = Buffer;
    }

    PathStr = ToUpyString(Path, PathStr, &PathSize);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(qstr_from_str(PathStr)));
    if (PathStr != Buffer) {
      FreePool(PathStr);
    }
  }

  return EFI_SUCCESS;
}

/**
  Set new STDIN, STDOUT and/or STDOUT for script engine.

  Script engine normally should setup its own STDIN/STDOUT/STDERR during
  initialization. This API allows the user of script engine to redirect
  stdio to different devices. Any NULL value passed in means restoring
  the default setting of corresponding stdio.

  All stdio must be file-compatible object (aka. type of EFI_FILE_HANDLE).

  @param  This      A pointer to the EFI_SCRIPT_ENGINE_PROTOCOL instance.
  @param  StdIn     A pointer to new STDIN.
  @param  StdOut    A pointer to new STDOUT.
  @param  StdErr    A pointer to new STDERR.

  @retval EFI_SUCCESS           The stdio is re-set successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support changing stdio.

**/
EFI_STATUS
EFIAPI
UpySetStdIo (
  IN      EFI_SCRIPT_ENGINE_PROTOCOL    *This,
  IN      EFI_FILE_HANDLE               StdIn,
  IN      EFI_FILE_HANDLE               StdOut,
  IN      EFI_FILE_HANDLE               StdErr
)
{
  EFI_STATUS                  Status;
  EFI_SCRIPT_FILE_PROTOCOL    *Sfp;

  Status = gBS->LocateProtocol(&gEfiScriptFileProtocolGuid, NULL, (VOID **)&Sfp);
  if (!EFI_ERROR(Status)) {
    Status = Sfp->SetStdIo (Sfp, StdIn, StdOut, StdErr);
  }

  return Status;
}

STATIC UPY_PRIVATE_DATA mUpyData = {
  EFI_SCRIPT_ENGINE_TYPE_MICROPYTHON,
  {
    UpyGetType,
    UpyExecute,
    UpyExecuteAsync,
    UpyEvaluate,
    UpySetEnv,
    UpySetArgs,
    UpySetPath,
    UpySetStdIo
  }
};

/**
  Initialize the script engine of MicroPython.

  @param  ImageHandle   Image handle of driver calling this method.

  @retval EFI_SUCCESS           The script engine is initialized successfully.
  @retval EFI_UNSUPPORTED       The script engine doesn't support environment
                                variable.

**/
EFI_STATUS
EFIAPI
UseInit (
  EFI_HANDLE        ImageHandle
)
{
  EFI_STATUS                  Status;

  mExecutorData.Handle = ImageHandle;

  Status = UsfInit (ImageHandle, &mExecutorData.ScriptFileProtocol);
  if (Status ==  EFI_SUCCESS || Status == EFI_ALREADY_STARTED) {
    Status = UpyInit ();
    if (!EFI_ERROR(Status)) {
      Status = gBS->InstallProtocolInterface (
                      &ImageHandle,
                      &gEfiScriptEngineProtocolGuid,
                      EFI_NATIVE_INTERFACE,
                      &(mUpyData.ScriptEngine)
                      );
      if (!EFI_ERROR(Status)) {
        mExecutorData.ScriptEngineProtocol = &mUpyData.ScriptEngine;
      } else {
        UpyDeinit();
      }
    }

    if (EFI_ERROR(Status)) {
      UsfDeinit(ImageHandle);
    }
  }

  return Status;
}

/**
  Destroy the script engine and release all resources allocated before.

  @retval EFI_SUCCESS     The script engine is destroied successfully.
  @retval EFI_NOT_FOUND   The script engine has been already destroied.

**/
EFI_STATUS
EFIAPI
UseDeinit (
  VOID
)
{
  EFI_STATUS    Status;

  if (mExecutorData.IsRunning) {
    DEBUG((DEBUG_WARN, "There's script in execution\r\n"));
  }

  UpyDeinit ();
  UsfDeinit (mExecutorData.Handle);

  Status = gBS->UninstallProtocolInterface (
                  mExecutorData.Handle,
                  &gEfiScriptEngineProtocolGuid,
                  &(mUpyData.ScriptEngine)
                  );
  SetMem (&mExecutorData, sizeof(mExecutorData), 0);
  return Status;
}

