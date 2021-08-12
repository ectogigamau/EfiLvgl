/** @file
  Edk2 version of time module for MicroPythhon.

Copyright (c) 2017 - 2018, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "py/mpconfig.h"

#if MICROPY_PY_UTIME

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include <py/runtime.h>
#include <py/smallint.h>
#include <py/mphal.h>
#include <extmod/utime_mphal.h>

#include <Library/UefiLib.h>
#include <Library/TimerLib.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <lib/timeutils/timeutils.h>


/******************************************************************************
DECLARE EXPORTED DATA
 ******************************************************************************/
const char mpexception_value_invalid_arguments[] = "invalid argument(s) value";
const char mpexception_num_type_invalid_arguments[] = "invalid argument(s) num/type";
const char mpexception_uncaught[] = "uncaught exception";

STATIC mp_uint_t   mClockSeconds = 0;
STATIC mp_uint_t   mClockNanoSeconds = 0;

STATIC mp_obj_t mod_time_time (void)
{
  EFI_STATUS    Status;
  EFI_TIME      Time;

  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    return mp_const_none;
  }

  return mp_obj_new_int_from_uint (
           timeutils_seconds_since_2000 (
             Time.Year,
             Time.Month,
             Time.Day,
             Time.Hour,
             Time.Minute,
             Time.Second
             )
           );
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0 (mod_time_time_obj, mod_time_time);

// Note: this is deprecated since CPy3.3, but pystone still uses it.
// This function returns wall-clock seconds elapsed since the first call to this
// function, as a floating point number, based on the gRT->GetTime().
STATIC mp_obj_t mod_time_clock (void)
{
  EFI_STATUS    Status;
  EFI_TIME      Time;
  mp_uint_t     Seconds;
  mp_uint_t     NanoSeconds;

  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    return mp_const_none;
  }

  Seconds = timeutils_seconds_since_2000 (
              Time.Year,
              Time.Month,
              Time.Day,
              Time.Hour,
              Time.Minute,
              Time.Second
              );
  NanoSeconds = Time.Nanosecond;

  if (mClockSeconds == 0 && mClockNanoSeconds == 0) {
    mClockSeconds     = Seconds;
    mClockNanoSeconds = NanoSeconds;
  }

  Seconds       -= mClockSeconds;
  if (NanoSeconds < mClockNanoSeconds) {
    Seconds     -= 1;
    NanoSeconds += 1000000000;
  }
  NanoSeconds   -= mClockSeconds;

  return mp_obj_new_int_from_uint (Seconds);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0 (mod_time_clock_obj, mod_time_clock);


STATIC mp_obj_t time_sleep_ms (mp_obj_t arg)
{
  /* Delay for given number of milliseconds, should be positive or 0 */
  mp_int_t ms = mp_obj_get_int(arg);
  if (ms > 0) {
    MicroSecondDelay (ms * 1000);
  }

  return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1 (mp_utime_sleep_ms_obj, time_sleep_ms);

STATIC mp_obj_t time_sleep_us (mp_obj_t arg)
{
  /* The number of microseconds to stall execution. */
  /* Delay for given number of milliseconds, should be positive or 0 */
  mp_int_t us = mp_obj_get_int(arg);
  if (us > 0) {
    MicroSecondDelay (us);
  }

  return mp_const_none;

}
MP_DEFINE_CONST_FUN_OBJ_1 (mp_utime_sleep_us_obj, time_sleep_us);

STATIC mp_obj_t time_ticks_ms (void)
{
  EFI_TIME            Time;
  EFI_STATUS          Status;

  //
  // Use the EFI Runtime Services Table to get the current time and data.
  //
  Status = gRT->GetTime(&Time, NULL);
  if (EFI_ERROR (Status)) {
    return mp_const_none;
  }

  return MP_OBJ_NEW_SMALL_INT((Time.Second * 1000 + Time.Nanosecond / 1000000));
}
MP_DEFINE_CONST_FUN_OBJ_0 (mp_utime_ticks_ms_obj, time_ticks_ms);

STATIC mp_obj_t time_ticks_us (void) {
  EFI_TIME            Time;
  EFI_STATUS          Status;

  //
  // Use the EFI Runtime Services Table to get the current time and data.
  //
  Status = gRT->GetTime(&Time, NULL);
  if (EFI_ERROR (Status)) {
    return mp_const_none;
  }

  return mp_obj_new_int_from_uint(Time.Second * 1000000 + Time.Nanosecond / 1000);

}
MP_DEFINE_CONST_FUN_OBJ_0 (mp_utime_ticks_us_obj, time_ticks_us);

STATIC mp_obj_t time_ticks_cpu (void) {
  return mp_obj_new_int_from_ull(GetPerformanceCounter());
}
MP_DEFINE_CONST_FUN_OBJ_0 (mp_utime_ticks_cpu_obj, time_ticks_cpu);

STATIC mp_obj_t time_ticks_diff (mp_obj_t end_in, mp_obj_t start_in) {
  // we assume that the arguments come from ticks_xx so are small ints
  mp_uint_t start = MP_OBJ_SMALL_INT_VALUE(start_in);
  mp_uint_t end = MP_OBJ_SMALL_INT_VALUE(end_in);
  // Optimized formula avoiding if conditions. We adjust difference "forward",
  // wrap it around and adjust back.
  mp_int_t diff = ((end - start + MICROPY_PY_UTIME_TICKS_PERIOD / 2) & (MICROPY_PY_UTIME_TICKS_PERIOD - 1))
                  - MICROPY_PY_UTIME_TICKS_PERIOD / 2;
  return MP_OBJ_NEW_SMALL_INT(diff);
}
MP_DEFINE_CONST_FUN_OBJ_2 (mp_utime_ticks_diff_obj, time_ticks_diff);


STATIC mp_obj_t mod_time_sleep (mp_obj_t arg)
{
  /*
  Sleep for the given number of seconds. Some boards may accept seconds as a floating-point
  number to sleep for a fractional number of seconds. Note that other boards may not accept a
  floating-point argument, for compatibility with them use sleep_ms() and sleep_us() functions.
  */
  mp_int_t seconds = mp_obj_get_int(arg);
  if (seconds > 0) {
    MicroSecondDelay(seconds * 1000 * 1000);
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1 (mod_time_sleep_obj, mod_time_sleep);



STATIC mp_obj_t time_mktime (mp_obj_t tuple)
{
  size_t len;
  mp_obj_t *elem;

  mp_obj_get_array(tuple, &len, &elem);

  // localtime generates a tuple of len 8. CPython uses 9, so we accept both.
  if (len < 8 || len > 9) {
    mp_raise_TypeError(mpexception_num_type_invalid_arguments);
  }

  return mp_obj_new_int_from_uint(timeutils_mktime(mp_obj_get_int(elem[0]), mp_obj_get_int(elem[1]), mp_obj_get_int(elem[2]),
                                                   mp_obj_get_int(elem[3]), mp_obj_get_int(elem[4]), mp_obj_get_int(elem[5])));
}
MP_DEFINE_CONST_FUN_OBJ_1 (time_mktime_obj, time_mktime);

STATIC mp_obj_t time_ticks_add (mp_obj_t ticks_in, mp_obj_t delta_in) {
  // we assume that first argument come from ticks_xx so is small int
  mp_uint_t ticks = MP_OBJ_SMALL_INT_VALUE(ticks_in);
  mp_uint_t delta = mp_obj_get_int(delta_in);
  return MP_OBJ_NEW_SMALL_INT((ticks + delta) & (MICROPY_PY_UTIME_TICKS_PERIOD - 1));
}
MP_DEFINE_CONST_FUN_OBJ_2 (mp_utime_ticks_add_obj, time_ticks_add);

int dayofweek (int year, int month, int day)
{ // calculate the  day of the week based on Zeller's Congruence
  if (month == 1 || month == 2) {
    month += 12;
    year--;
  }
  int c = year / 100;
  int y = year % 100;
  int m = month;
  int d = day;
  int W = c / 4 - 2 * c + y + y / 4 + 26 * (m + 1) / 10 + d - 1;
  if (W < 0) return (W + (-W / 7 + 1) * 7) % 7;
  return W % 7;
}




STATIC mp_obj_t mod_time_localtime (size_t n_args, const mp_obj_t *args)
{
  EFI_TIME Time;
  struct tm tm;
  EFI_STATUS  Status;

  if (n_args == 0) {
    //
    // Use the EFI Runtime Services Table to get the current time and data.
    //

    Status = gRT->GetTime(&Time, NULL);
    if (Status != EFI_SUCCESS) {
       return mp_const_none;
    }

    tm.tm_year  = Time.Year;
    tm.tm_mon   = Time.Month;
    tm.tm_mday  = Time.Day;
    tm.tm_hour  = Time.Hour;
    tm.tm_min   = Time.Minute;
    tm.tm_sec   = Time.Second;
    tm.tm_isdst = Time.Daylight;
    tm.tm_wday  = dayofweek(Time.Year, Time.Month, Time.Day);

    tm.tm_yday = timeutils_year_day(Time.Year, Time.Month, Time.Day);


    mp_obj_t ret = mp_obj_new_tuple(8, NULL);

    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(ret);
    tuple->items[0] = MP_OBJ_NEW_SMALL_INT(tm.tm_year);
    tuple->items[1] = MP_OBJ_NEW_SMALL_INT(tm.tm_mon);
    tuple->items[2] = MP_OBJ_NEW_SMALL_INT(tm.tm_mday);
    tuple->items[3] = MP_OBJ_NEW_SMALL_INT(tm.tm_hour);
    tuple->items[4] = MP_OBJ_NEW_SMALL_INT(tm.tm_min);
    tuple->items[5] = MP_OBJ_NEW_SMALL_INT(tm.tm_sec);
    int wday = tm.tm_wday - 1;
    if (wday < 0) {
      wday = 6;
    }
    tuple->items[6] = MP_OBJ_NEW_SMALL_INT(wday);
    tuple->items[7] = MP_OBJ_NEW_SMALL_INT(tm.tm_yday);


    return ret;
  } else {
    mp_int_t seconds = mp_obj_get_int(args[0]);
    timeutils_struct_time_t tm;
    timeutils_seconds_since_2000_to_struct_time(seconds, &tm);
    mp_obj_t tuple[8] = {
      tuple[0] = mp_obj_new_int(tm.tm_year),
      tuple[1] = mp_obj_new_int(tm.tm_mon),
      tuple[2] = mp_obj_new_int(tm.tm_mday),
      tuple[3] = mp_obj_new_int(tm.tm_hour),
      tuple[4] = mp_obj_new_int(tm.tm_min),
      tuple[5] = mp_obj_new_int(tm.tm_sec),
      tuple[6] = mp_obj_new_int(tm.tm_wday),
      tuple[7] = mp_obj_new_int(tm.tm_yday),
    };
    return mp_obj_new_tuple(8, tuple);
  }

}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN (mod_time_localtime_obj, 0, 1, mod_time_localtime);

STATIC const mp_rom_map_elem_t mp_module_time_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_utime)},
  { MP_ROM_QSTR(MP_QSTR_clock), MP_ROM_PTR(&mod_time_clock_obj)},
  { MP_ROM_QSTR(MP_QSTR_sleep), MP_ROM_PTR(&mod_time_sleep_obj)},
  { MP_ROM_QSTR(MP_QSTR_sleep_ms), MP_ROM_PTR(&mp_utime_sleep_ms_obj)},
  { MP_ROM_QSTR(MP_QSTR_sleep_us), MP_ROM_PTR(&mp_utime_sleep_us_obj)},
  { MP_ROM_QSTR(MP_QSTR_time), MP_ROM_PTR(&mod_time_time_obj)},
  { MP_ROM_QSTR(MP_QSTR_ticks_ms), MP_ROM_PTR(&mp_utime_ticks_ms_obj)},
  { MP_ROM_QSTR(MP_QSTR_ticks_us), MP_ROM_PTR(&mp_utime_ticks_us_obj)},
  { MP_ROM_QSTR(MP_QSTR_ticks_cpu), MP_ROM_PTR(&mp_utime_ticks_cpu_obj)},
  { MP_ROM_QSTR(MP_QSTR_ticks_add), MP_ROM_PTR(&mp_utime_ticks_add_obj)},
  { MP_ROM_QSTR(MP_QSTR_ticks_diff), MP_ROM_PTR(&mp_utime_ticks_diff_obj)},
  { MP_ROM_QSTR(MP_QSTR_localtime), MP_ROM_PTR(&mod_time_localtime_obj)},
  { MP_ROM_QSTR(MP_QSTR_mktime),   MP_ROM_PTR(&time_mktime_obj)},
};

STATIC MP_DEFINE_CONST_DICT (mp_module_time_globals, mp_module_time_globals_table);

const mp_obj_module_t mp_module_time = {
  .base = {&mp_type_module },
  .globals = (mp_obj_dict_t *)&mp_module_time_globals,
};

#endif // MICROPY_PY_UTIME
