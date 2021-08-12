#include "EfiInput.h"

EFI_SIMPLE_POINTER_PROTOCOL *gSimplePointer = NULL;

/**
 * Get the current position and state of the mouse
 * @param data store the mouse data here
 * @return false: because the points are not buffered, so no more data to be read
 */
bool mouse_read(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    EFI_STATUS Status;
    EFI_SIMPLE_POINTER_STATE State;
    STATIC lv_coord_t X = 0, Y = 0;
    
    if (gSimplePointer == NULL) {
        return false;
    }

    Status = gSimplePointer->GetState(gSimplePointer, &State);
    if (EFI_ERROR(Status)) {
        return false;
    }

    X += (lv_coord_t)(State.RelativeMovementX / (INT32)gSimplePointer->Mode->ResolutionX);
    Y += (lv_coord_t)(State.RelativeMovementY / (INT32)gSimplePointer->Mode->ResolutionY);

    X = MAX(X, 0);
    Y = MAX(Y, 0);

    /* Store the collected data */
    data->point.x = X;
    data->point.y = Y;
    data->state = State.LeftButton ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    return false;
}

/**
 * ConvertEfiKeyToLvgl
 */
uint32_t
ConvertEfiKeyToLvgl(
    EFI_INPUT_KEY *Key
)
{
    switch (Key->UnicodeChar)
    {
    case CHAR_CARRIAGE_RETURN:
        return LV_KEY_ENTER;
    case CHAR_BACKSPACE:
        return LV_KEY_BACKSPACE;
    case CHAR_TAB:
        return LV_KEY_NEXT;  
    default:
        break;
    }

    switch (Key->ScanCode)
    {
    case SCAN_UP:
        return LV_KEY_UP;
    case SCAN_DOWN:
        return LV_KEY_DOWN;
    case SCAN_RIGHT:
        return LV_KEY_RIGHT;
    case SCAN_LEFT:
        return LV_KEY_LEFT;
    case SCAN_DELETE:
        return LV_KEY_DEL;
    case SCAN_HOME:
        return LV_KEY_HOME;
    case SCAN_END:
        return LV_KEY_END;
    default:
        break;
    }

    return Key->UnicodeChar;
}

/**
 * Get the current key pressed
 * @param data store the keyboard data here
 * @return false: because the kee are not buffered, so no more data to be read
 */
bool keyboard_read(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    EFI_STATUS Status;
    EFI_INPUT_KEY Key;

	Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    if (EFI_ERROR(Status)) {
        data->state = LV_INDEV_STATE_REL;
        return false;
    }

    data->key = ConvertEfiKeyToLvgl(&Key);
    data->state = LV_INDEV_STATE_PR;

    return false;
}

/**
 * Init Function
 */
void input_init()
{
    EFI_STATUS Status;
    Status = gBS->HandleProtocol(gST->ConsoleInHandle, &gEfiSimplePointerProtocolGuid, &gSimplePointer);
    if (EFI_ERROR(Status)) {
        gSimplePointer = NULL;
    }
}