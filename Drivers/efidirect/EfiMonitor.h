/**
 * @file monitor.h
 *
 */

#ifndef MONITOR_H
#define MONITOR_H

/*********************
 *      INCLUDES
 *********************/
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>

#include "../../lv_binding_micropython/lvgl/lvgl.h"
    
#include "lvgl/src/lv_misc/lv_color.h"

extern void lv_disp_flush_ready(lv_disp_drv_t * disp_drv);

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/
void monitor_init(int w, int h);
void monitor_deinit(void);
void monitor_flush(struct _disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

 #endif /* MONITOR_H */