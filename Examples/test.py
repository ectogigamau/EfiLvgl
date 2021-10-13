import lvgl as lv
import efidirect as ed
import uctypes
import Lib.Uefi.uefi as uefi

scr_width = 800
scr_height = 600

close_flag = False

def mbox_event_cb(obj, evt):
    if evt == lv.EVENT.VALUE_CHANGED:
        # a button was clicked 
        obj.start_auto_close(0)

def event_handler(source,evt):
	global close_flag
	if evt == lv.EVENT.CLICKED:
		print("Clicked")
		close_flag = True


def event_handler_hello(source,evt):
	global close_flag
	if evt == lv.EVENT.CLICKED:
		btns = ["Close", ""]

		mbox1 = lv.msgbox(lv.scr_act())
		mbox1.set_text("Hi!");
		mbox1.add_btns(btns)
		mbox1.set_width(200)
		mbox1.set_event_cb(mbox_event_cb)
		mbox1.align(None, lv.ALIGN.CENTER, 0, 0)  # Align to the corner


ed.init(w = scr_width, h = scr_height)
lv.init()

# Register EFI display driver.

disp_buf1 = lv.disp_buf_t()
buf1_1 = bytearray(scr_width*scr_height*4)
disp_buf1.init(buf1_1, None, len(buf1_1)//4)
disp_drv = lv.disp_drv_t()
disp_drv.init()
disp_drv.buffer = disp_buf1
disp_drv.flush_cb = ed.monitor_flush
disp_drv.hor_res = scr_width
disp_drv.ver_res = scr_height
disp_drv.register()

# Regsiter EFI mouse driver

indev_drv = lv.indev_drv_t()
indev_drv.init() 
indev_drv.type = lv.INDEV_TYPE.POINTER;
indev_drv.read_cb = ed.mouse_read
mouse_indev = indev_drv.register();

indev_drv = lv.indev_drv_t()
indev_drv.init() 
indev_drv.type = lv.INDEV_TYPE.KEYPAD;
indev_drv.read_cb = ed.keyboard_read
kb_indev = indev_drv.register();

kb_group = lv.group_create()
kb_indev.set_group(kb_group)

scr = lv.obj()

btn = lv.btn(scr)
btn.align(lv.scr_act(), lv.ALIGN.CENTER, 0, 0)
label = lv.label(btn)
label.set_text("Cancel")
btn.set_event_cb(event_handler)

kb_group.add_obj(btn)

btn = lv.btn(scr)
btn.align(lv.scr_act(), lv.ALIGN.CENTER, 0, 50)
label = lv.label(btn)
label.set_text("Hello World!")
kb_group.add_obj(btn)
btn.set_event_cb(event_handler_hello)


# Load the screen

lv.scr_load(scr)

img = lv.img(lv.scr_act(), None)
img.set_src(lv.SYMBOL.PLAY)
mouse_indev.set_cursor(img)

while(1):
	lv.task_handler()
	lv.tick_inc(10)
	uefi.bs.Stall(1000)
	key = lv.indev_t.get_key(kb_indev)
	if close_flag:
		break
