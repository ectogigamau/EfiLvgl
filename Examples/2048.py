import lvgl as lv
import efidirect as ed
import uctypes
import Lib.Uefi.uefi as uefi
import urandom

SCR_WIDTH = 800
SCR_HEIGHT = 600

# Size of item array N*N 
N=4

CONTAINER_SIZE_PX = 500
PADDING_PX = 15
ANIM_MOVE_TIME = 500
ANIM_INIT_TIME = 500 * 3
DEFAULT_VALUE = 2
WIN_VALUE = 2048

CONTROL_BUTTON_SIZE = 50

ELEMENT_COLOR = {2 : 0xeee4da, 
				4 : 0xede0c8,
				8 : 0xf2b179,
				16 : 0xf59563,
				32 : 0xf67c60,
				64 : 0xf65e3b,
				128 : 0xedcf73,
				256 : 0xedcc62,
				512 : 0xedc850,
				1024 : 0xedc53f,
				2048 : 0xedc22d,
				4096 : 0x5578f7,
				8192 : 0x833fba,
				}

GUI_ITEM_SIZE_PX = (CONTAINER_SIZE_PX - (N + 1) * PADDING_PX) // N

#
# Exit from script
#
CLOSE_FLAG = False

#
# WORKAROUND:
#  Lvgl does not have a global KeyUp/KeyDown event.
#  Emulate it! Read the last key pressed (see. get_last_key) = "KeyDown"
#  and "KeyUp" = lv.EVENT.KEY for selected object (see update_key_handler)
#
key_up_event = False

#
# Board object
#
board = None

#
# Common Functions
#

def update_key_handler(evt):
	global key_up_event
	if evt ==  lv.EVENT.KEY:
		key_up_event = True

def close_event_handler(source,evt):
	global CLOSE_FLAG
	update_key_handler(evt)
	if evt == lv.EVENT.CLICKED:
		CLOSE_FLAG = True

def anim_x_cb(object, v):
	object.set_x(v)

def anim_y_cb(object, v):
	object.set_y(v)

def anim_w_cb(object, v):
	object.set_width(v)

def anim_h_cb(object, v):
	object.set_height(v)

def anim_del_cb(object, v):
	object.delete()

def mbox_event_cb(obj, evt):
	if evt == lv.EVENT.VALUE_CHANGED:
		# a button was clicked 
		obj.start_auto_close(0)

#
# Screen
#
class Screen():
	#
	# Keyboard handling
	#
	kb_group = None
	kb_indev = None

	scr = None

	def __init__(self):
		ed.init(w = SCR_WIDTH, h = SCR_HEIGHT)
		lv.init()

		# Register EFI display driver.

		disp_buf1 = lv.disp_buf_t()
		buf1_1 = bytearray(SCR_WIDTH*SCR_HEIGHT*4)
		disp_buf1.init(buf1_1, None, len(buf1_1)//4)
		disp_drv = lv.disp_drv_t()
		disp_drv.init()
		disp_drv.buffer = disp_buf1
		disp_drv.flush_cb = ed.monitor_flush
		disp_drv.hor_res = SCR_WIDTH
		disp_drv.ver_res = SCR_HEIGHT
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
		self.kb_indev = kb_indev

		kb_group = lv.group_create()
		kb_indev.set_group(kb_group)
		self.kb_group = kb_group
		
		#
		# Init mouse
		#
		img = lv.img(lv.scr_act(), None)
		img.set_src(lv.SYMBOL.PLAY)
		mouse_indev.set_cursor(img)

		self.scr = lv.obj()

	def get_obj(self):
		return self.scr

	def add_keyboard(self, obj):
		self.kb_group.add_obj(obj)

	def get_last_key(self):
		return lv.indev_t.get_key(self.kb_indev)

	def run(self):
		global key_up_event
		# Load the screen
		lv.scr_load(self.scr)

		while(1):
			lv.task_handler()
			lv.tick_inc(10)
			uefi.bs.Stall(1000)
			last_key = self.get_last_key()

			if key_up_event:
				set_key(last_key)
				key_up_event = False
			if CLOSE_FLAG:
				break

#
# Gui Board
#
class GuiBoard:
	board_cont = None
	def __init__(self, scr):
		# create a container
		cont = lv.cont(scr,None)
		cont.set_width(CONTAINER_SIZE_PX)
		cont.set_height(CONTAINER_SIZE_PX)
		cont.align_mid(None,lv.ALIGN.CENTER,0,0) 
		cont.add_style(lv.obj.PART.MAIN, self.get_container_style())

		style = self.get_back_panel_style()
		for x in range(N):
			for y in range(N):
				panel = lv.obj(cont)
				panel.set_width(GUI_ITEM_SIZE_PX)
				panel.set_height(GUI_ITEM_SIZE_PX)
				panel.set_x(PADDING_PX + x * (GUI_ITEM_SIZE_PX + PADDING_PX))
				panel.set_y(PADDING_PX + y * (GUI_ITEM_SIZE_PX + PADDING_PX))
				panel.add_style(lv.obj.PART.MAIN, style)

		self.board_cont = cont;

	def get_back_panel_style(self):
		# Back panel style

		style_back_panel =  lv.style_t()
		style_back_panel.init()
		style_back_panel.set_radius(lv.STATE.DEFAULT, 8);
		style_back_panel.set_bg_opa(lv.STATE.DEFAULT, lv.OPA.COVER);
		style_back_panel.set_bg_color(lv.STATE.DEFAULT, lv.color_hex(0xcdc1b4))
		style_back_panel.set_border_width(lv.STATE.DEFAULT, 0);
		return style_back_panel

	def get_container_style(self):
		# Container style

		style_cont =  lv.style_t()
		style_cont.init()
		style_cont.set_bg_color(lv.STATE.DEFAULT, lv.color_hex(0xbbada0))
		return style_cont

#
# Gui Item
#
class GuiItem():
	panel = None
	def __init__(self, board, i, j):
		panel = lv.cont(board.board_cont)
		panel.set_width(GUI_ITEM_SIZE_PX)
		panel.set_height(GUI_ITEM_SIZE_PX)
		panel.set_x(PADDING_PX + i * (GUI_ITEM_SIZE_PX + PADDING_PX))
		panel.set_y(PADDING_PX + j * (GUI_ITEM_SIZE_PX + PADDING_PX))
		panel.set_layout(lv.LAYOUT.CENTER);
		label = lv.label(panel)
		label.set_text("0")
		panel.add_style(lv.obj.PART.MAIN, self.get_style())

		a = lv.anim_t()
		a.init()
		a.set_var(panel)
		a.set_values(panel.get_x() + GUI_ITEM_SIZE_PX//2, panel.get_x())
		a.set_time(ANIM_INIT_TIME)
		a.set_custom_exec_cb(lambda a,val: anim_x_cb(panel,val))
		lv.anim_t.start(a)

		a = lv.anim_t()
		a.init()
		a.set_var(panel)
		a.set_values(panel.get_y() + GUI_ITEM_SIZE_PX//2, panel.get_y())
		a.set_time(ANIM_INIT_TIME)
		a.set_custom_exec_cb(lambda a,val: anim_y_cb(panel,val))
		lv.anim_t.start(a)

		a = lv.anim_t()
		a.init()
		a.set_var(panel)
		a.set_values(0, GUI_ITEM_SIZE_PX)
		a.set_time(ANIM_INIT_TIME)
		a.set_custom_exec_cb(lambda a,val: anim_w_cb(panel,val))
		lv.anim_t.start(a)

		a = lv.anim_t()
		a.init()
		a.set_var(panel)
		a.set_values(0, GUI_ITEM_SIZE_PX)
		a.set_time(ANIM_INIT_TIME)
		a.set_custom_exec_cb(lambda a,val: anim_h_cb(panel,val))
		lv.anim_t.start(a)

		self.panel = panel

	def get_style(self):
		# Front panel style

		style =  lv.style_t()
		style.init()
		style.set_radius(lv.STATE.DEFAULT, 8);
		style.set_bg_opa(lv.STATE.DEFAULT, lv.OPA.COVER);
		style.set_bg_color(lv.STATE.DEFAULT, lv.color_hex(0xeee4da))
		style.set_border_width(lv.STATE.DEFAULT, 0);
		return style

	def set_pos(self, i, j):
		#
		# Set position with animation:
		#  self.panel.set_x(PADDING_PX + i * (GUI_ITEM_SIZE_PX + PADDING_PX))
		#  self.panel.set_y(PADDING_PX + j * (GUI_ITEM_SIZE_PX + PADDING_PX))
		#
		self.panel.set_x(PADDING_PX + i * (GUI_ITEM_SIZE_PX + PADDING_PX))
		self.panel.set_y(PADDING_PX + j * (GUI_ITEM_SIZE_PX + PADDING_PX))
		#a = lv.anim_t()
		#a.init()
		#a.set_var(self.panel)
		#a.set_values(self.panel.get_x(), PADDING_PX + i * (GUI_ITEM_SIZE_PX + PADDING_PX))
		#a.set_time(ANIM_MOVE_TIME)
		#a.set_custom_exec_cb(lambda a,val: anim_x_cb(self.panel,val))
		#lv.anim_t.start(a)
#
		#a = lv.anim_t()
		#a.init()
		#a.set_var(self.panel)
		#a.set_values(self.panel.get_y(), PADDING_PX + j * (GUI_ITEM_SIZE_PX + PADDING_PX))
		#a.set_time(ANIM_MOVE_TIME)
		#a.set_custom_exec_cb(lambda a,val: anim_y_cb(self.panel,val))
		#lv.anim_t.start(a)

	def set_number(self, num):
		color = ELEMENT_COLOR.get(num, 0xFFFFFF)
		self.panel.set_style_local_bg_color(lv.obj.PART.MAIN, lv.STATE.DEFAULT, lv.color_hex(color))
		label = self.panel.get_child(None)
		label.set_text(str(num))

	def __del__(self):
		self.panel.delete()

#
# Logic
#

class Item:
	value = 0
	gui_item = None

	def __init__(self, i, j, gui_board):
		self.gui_item = GuiItem(gui_board, i, j);

	def get_value(self):
		return self.value

	def set_value(self, value):
		self.value = value
		self.gui_item.set_number(value)

	def set_pos(self, x, y):
		self.gui_item.set_pos(x, y)


class Board:
	gui_board = None
	items = []

	# Flags
	item_moved = False
	game_overed = False

	def __init__(self, scr):
		#init array
		for x in range(N):
			line = []
			for y in range(N):
				line.append(None)
			self.items.append(line)
		
		self.gui_board = GuiBoard(scr)
		
	def compress_x(self, left, y):
		if left:
			# Compress           
			for x in range(1, N):
				if self.exist(x, y):
					for i in reversed(range(0, x)):
						if not self.exist(i, y):
							self.drag(i, y, i + 1, y)
		else:
			for x in reversed(range(0, N - 1)):
				if self.exist(x, y):
					for i in range(x + 1, N):
						if not self.exist(i, y):
							self.drag(i, y, i - 1, y)

	def test_and_join(self, x1, y1, x2, y2):
		if self.exist(x1, y1) and self.exist(x2, y2):
				left_value = self.items[x1][y1].get_value()
				right_value = self.items[x2][y2].get_value()
				if left_value == right_value:
					self.join(x1, y1, x2, y2)

	def move_left(self):
		for y in range(N):

			self.compress_x(True, y)

			# Join neighbors
			for x in range(0, N - 1):
				self.test_and_join(x, y, x + 1, y);

			self.compress_x(True, y)

	def move_right(self):
		for y in range(N):

			# Compress           
			self.compress_x(False, y)

			# Join neighbors
			for x in reversed(range(1, N)):
				self.test_and_join(x, y, x - 1, y);

			# Compress           
			self.compress_x(False, y)
		
	def compress_y(self, up, x):
		if up:
			# Compress           
			for y in range(1, N):
				if self.exist(x, y):
					for i in reversed(range(0, y)):
						if not self.exist(x, i):
							self.drag(x, i, x, i + 1)
		else:
			for y in reversed(range(0, N - 1)):
				if self.exist(x, y):
					for i in range(y + 1, N):
						if not self.exist(x, i):
							self.drag(x, i, x,  i - 1)

	def move_down(self):
		for x in range(N):
			# Compress           
			self.compress_y(False, x)

			# Join neighbors
			for y in reversed(range(1, N)):
				self.test_and_join(x, y, x, y - 1)

			# Compress           
			self.compress_y(False, x)

	def move_up(self):
		for x in range(N):
			# Compress
			self.compress_y(True, x)

			# Join neighbors
			for y in range(0, N - 1):
				self.test_and_join(x, y, x, y  + 1)

			# Compress
			self.compress_y(True, x)

	def move(self, c):
		# disable "move" if game overed
		if self.game_overed:
			return

		self.item_moved = False

		move_func = {'u': self.move_up, 'd': self.move_down, 'l': self.move_left, 'r': self.move_right}
		move_func[c]()

		if self.item_moved:
			self.check_win()
			self.check_game_over()
			self.add_random()

	def exist(self, x, y):
		return self.items[x][y] != None

	def no_free_item(self):
		count = 0
		for x in range(N):
			for y in range(N):
				if self.exist(x, y):
					count += 1
		return count == N*N

	def add(self, x, y):
		item = Item(x, y, self.gui_board)
		item.set_value(DEFAULT_VALUE)
		self.items[x][y] = item

	def add_random(self):
		# Save the empty items to list
		item_list = []
		for x in range(N):
			for y in range(N):
				if not self.exist(x, y):
					item_list.append([x, y])

		# Get random the empty item from list
		index = urandom.randint(0, len(item_list) - 1)

		x = item_list[index][0]
		y = item_list[index][1]
		self.add(x, y)  

	def delete(self, x, y):
		item = self.items[x][y]
		#force delete gui object
		item.gui_item.panel.delete()
		item.gui_item.panel = None
		del item
		
		self.items[x][y] = None

	def join(self, new_x, new_y, x, y):    
		if self.items[new_x][new_y].get_value() != self.items[x][y].get_value():
			return

		self.item_moved = True

		item = self.items[x][y]
		#print("del: " + str(x) + " " + str(y))
		item.set_pos(new_x, new_y)
		self.items[new_x][new_y].set_value(self.items[new_x][new_y].get_value() + item.get_value())
		
		self.delete(x, y)

	def drag(self, new_x, new_y, x, y):
		if new_x == x and new_y == y:
			return

		self.item_moved = True

		item = self.items[x][y]
		item.set_pos(new_x, new_y)
		self.items[new_x][new_y] = item
		self.items[x][y] = None

	def clear(self):
		for x in range(N):
			for y in range(N):
				if self.exist(x, y):
					self.delete(x, y)

	def new_game(self):
		self.clear()
		self.add_random()
		self.add_random()

	def check_win(self):
		win = False
		for x in range(N):
			for y in range(N):
				if self.exist(x, y) and self.items[x][y].get_value() == WIN_VALUE:
					win = True
					break

		if win:
			self.game_overed = True
			btns = ["OK", ""]
			mbox = lv.msgbox(lv.scr_act())
			mbox.set_text("You win!")
			mbox.add_btns(btns)
			mbox.set_width(200)
			mbox.set_event_cb(mbox_event_cb)
			mbox.align(None, lv.ALIGN.CENTER, 0, 0)  # Align to the corner

	def check_game_over(self):
		count = 0
		for x in range(N):
			for y in range(N):
				if self.exist(x, y):
					count += 1
		
		if count == N * N:
			self.game_overed = True
			btns = ["OK", ""]
			mbox1 = lv.msgbox(lv.scr_act())
			mbox1.set_text("Game over!")
			mbox1.add_btns(btns)
			mbox1.set_width(200)
			mbox1.set_event_cb(self.mbox_event_cb)
			mbox1.align(None, lv.ALIGN.CENTER, 0, 0)  # Align to the corner


	def debug_out(self):
		print("------------------------------------------------")
		for y in range(N):
			for x in range(N):
				if not self.exist(x, y):
					print("[    ]", end="")
				else:
					print("[%.4d]" % self.items[x][y].get_value(), end="")
			print("")

def create_close_button(parent):
	# Cancel button
	btn = lv.btn(parent)
	btn.set_width(CONTROL_BUTTON_SIZE)
	btn.set_height(CONTROL_BUTTON_SIZE)
	btn.align(None,lv.ALIGN.IN_TOP_RIGHT,-10,10) 
	label = lv.label(btn)
	label.set_text(lv.SYMBOL.CLOSE)
	btn.set_event_cb(close_event_handler)
	style_close_btn =  lv.style_t()
	style_close_btn.init()
	style_close_btn.set_radius(lv.STATE.DEFAULT, 16)
	style_close_btn.set_bg_color(lv.STATE.DEFAULT, lv.color_hex(0xcdc1b4))
	style_close_btn.set_border_width(lv.STATE.DEFAULT, 2)
	btn.add_style(lv.obj.PART.MAIN, style_close_btn)
	return btn

def create_reset_button(parent):
	# Cancel button
	btn = lv.btn(parent)
	btn.set_width(CONTROL_BUTTON_SIZE)
	btn.set_height(CONTROL_BUTTON_SIZE)
	btn.align(None,lv.ALIGN.IN_TOP_RIGHT,-10 * 2 - CONTROL_BUTTON_SIZE ,10) 
	label = lv.label(btn)
	label.set_text(lv.SYMBOL.REFRESH)
	btn.set_event_cb(reset_event_handler)
	style_close_btn =  lv.style_t()
	style_close_btn.init()
	style_close_btn.set_radius(lv.STATE.DEFAULT, 16)
	style_close_btn.set_bg_color(lv.STATE.DEFAULT, lv.color_hex(0xcdc1b4))
	style_close_btn.set_border_width(lv.STATE.DEFAULT, 2)
	btn.add_style(lv.obj.PART.MAIN, style_close_btn)
	return btn


def set_key(key):
	global board
	if key == lv.KEY.UP:
		board.move('u')
	if key == lv.KEY.DOWN:
		board.move('d')
	if key == lv.KEY.LEFT:
		board.move('l')
	if key == lv.KEY.RIGHT:
		board.move('r')

def reset_event_handler(source, evt):
	global board
	update_key_handler(evt)
	if evt == lv.EVENT.CLICKED:
		board.new_game()

def main():
	global board
	screen = Screen()
	scr = screen.get_obj()

	close_btn = create_close_button(scr)
	screen.add_keyboard(close_btn)

	reset_btn = create_reset_button(scr)
	screen.add_keyboard(reset_btn)

	# Init board
	board = Board(scr)
	board.new_game()

	screen.run()

if __name__ == "__main__":
	main()