/**
 * @file mouse.h
 *
 */

#ifndef INPUT_H
#define INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Protocol/SimplePointer.h>

#include "../../lv_binding_micropython/lvgl/lvgl.h"
    
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the mouse
 */
void mouse_init(void);
/**
 * Get the current position and state of the mouse
 * @param data store the mouse data here
 * @return false: because the points are not buffered, so no more data to be read
 */
bool mouse_read(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**
 * Get the current position and state of the mouse
 * @param data store the mouse data here
 * @return false: because the points are not buffered, so no more data to be read
 */
bool keyboard_read(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

/**
 * Init Function
 */
void input_init();

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INPUT_H */
