#include "EfiMonitor.h"

typedef struct {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    UINTN Width;
    UINTN Height;
} MONITOR_PRIVATE;

STATIC MONITOR_PRIVATE *tPrivate = NULL;

/**
 * Flush a buffer to the display. Calls 'lv_flush_ready()' when finished
 */
void monitor_flush(struct _disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    lv_coord_t w = area->x2 - area->x1 + 1;
    lv_coord_t h = area->y2 - area->y1 + 1;

    Gop = tPrivate->Gop;

#if LV_COLOR_DEPTH != 24 && LV_COLOR_DEPTH != 32    /*32 is valid but support 24 for backward compatibility too*/
    //
    // TODO: need convert to 32 bit color
    //
    ASSERT(FALSE);
#else

{
  EFI_STATUS Status;
  UINTN Index;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  EFI_HANDLE *HndlBuf;
  UINTN HndlNum;

  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &HndlNum,  &HndlBuf);
  if (EFI_ERROR(Status) || HndlNum == 0) {
    return;
  }

  for (Index = 0; Index < HndlNum; Index++) {

    Status = gBS->HandleProtocol (
                    HndlBuf[Index],
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **) &GraphicsOutput
                    );
    if (EFI_ERROR(Status)) {
      continue;
    }


    GraphicsOutput->Blt(
        GraphicsOutput,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)color_p,
        EfiBltBufferToVideo,
        0,
        0,
        area->x1,
        area->y1,
        w,
        h,
        0
    );
  }

  gBS->FreePool(HndlBuf);
}

/*
    Gop->Blt(
        Gop,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)color_p,
        EfiBltVideoFill,//EfiBltBufferToVideo,
        0,
        0,
        area->x1,
        area->y1,
        w,
        h,
        0
    );*/
#endif

    /*IMPORTANT! It must be called to tell the system the flush is ready*/
    lv_disp_flush_ready(disp_drv);
}


/**
 * Initialize the monitor
 */
void monitor_init(int w, int h)
{
    EFI_STATUS Status;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop;
    

    if (tPrivate != NULL) {
        //
        // Already inited
        //
        return;
    }

    tPrivate = (MONITOR_PRIVATE*)AllocateZeroPool(sizeof(MONITOR_PRIVATE));
    if (tPrivate == NULL) {
        ASSERT(FALSE);
        return;
    }

    /*Status = gBS->HandleProtocol (
                gST->ConsoleOutHandle,
                &gEfiGraphicsOutputProtocolGuid,
                (VOID**)&Gop
                );*/

	Status = gBS->LocateProtocol (
								&gEfiGraphicsOutputProtocolGuid,
								NULL,
								(VOID **)&Gop
								);
    if (EFI_ERROR(Status)) {
        ASSERT(FALSE);
        FreePool(tPrivate);
        return;
    }

    tPrivate->Gop = Gop;
    tPrivate->Width = MIN(tPrivate->Width, Gop->Mode->Info->HorizontalResolution);
    tPrivate->Height = MIN(tPrivate->Height, Gop->Mode->Info->VerticalResolution);

{
  EFI_STATUS Status;
  UINTN Index;
  EFI_GRAPHICS_OUTPUT_PROTOCOL   *GraphicsOutput;
  EFI_HANDLE *HndlBuf;
  UINTN HndlNum;

  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiGraphicsOutputProtocolGuid, NULL, &HndlNum,  &HndlBuf);
  if (EFI_ERROR(Status) || HndlNum == 0) {
    return;
  }

  for (Index = 0; Index < HndlNum; Index++) {

    
    Status = gBS->HandleProtocol (
                    HndlBuf[Index],
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **) &GraphicsOutput
                    );
    if (EFI_ERROR(Status)) {
      continue;
    }

    /*EFI_GRAPHICS_OUTPUT_BLT_PIXEL p;
    p.Blue = 255;
    GraphicsOutput->Blt(
        GraphicsOutput,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)&p,
        EfiBltVideoFill,
        0,
        0,
        0,
        0,
        100,
        100,
        0
    );*/
  }

    EFI_GRAPHICS_OUTPUT_BLT_PIXEL p;
    p.Blue = 255;
    Gop->Blt(
        Gop,
        (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)&p,
        EfiBltBufferToVideo,
        0,
        0,
        0,
        0,
        100,
        100,
        0
    );
}

}

/**
 * Deinit the monitor and close SDL
 */
void monitor_deinit(void)
{
    FreePool(tPrivate);
    tPrivate = NULL;
}