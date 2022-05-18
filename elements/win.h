#include <math.h>

// custom window type
// "resz" is shorthand for "resize"

typedef enum {
	CT_NONE        = 0b00000,
	CT_MOVE        = 0b00001,
	CT_RESZ        = 0b11110,

	CT_RESZ_LEFT   = 0b00010,
	CT_RESZ_RIGHT  = 0b00100,
	CT_RESZ_TOP    = 0b01000,
	CT_RESZ_BOTTOM = 0b10000,
} ct_t; // click type

typedef enum {
	MAX_LEVEL_NONE,
	MAX_LEVEL_TILED,
	MAX_LEVEL_FULL,
	MAX_LEVEL_MAX
} max_level_t;

typedef struct {
	object_t obj;

	lwm_t* wm;
	win_t win;

	int vx_pos, vy_pos;
	char* caption;

	// other grabby bits & clicking & dragging stuff

	unsigned clicked;

	int resize_border;
	ct_t ct; // click type

	float drag_init_mx;
	float drag_init_my;

	// move stuff

	unsigned moving;

	float move_init_x;
	float move_init_y;

	// resize stuff

	unsigned resizing;

	float resize_init_x;
	float resize_init_y;

	// state stuff

	unsigned transient;
	unsigned fullscreen;

	// fullscreen stuff

	max_level_t max_level;

	float mini_x_pos, mini_y_pos;
	unsigned mini_x_res, mini_y_res;
} lwm_win_t;

#define RESIZE_BORDER 5

static type_t lwm_win_type; // forward declaration

static inline void __lwm_win_process_vpos(lwm_win_t* win) {
	// TODO perhaps make utility functions in the UI lib for converting these float coordinates which are standard across AQUA devices to VX/VY UI-friendly coordinates?

	win->vx_pos = win->win.x_pos * win->win.wm_x_res;
	win->vy_pos = win->win.wm_y_res - win->win.y_pos * win->win.wm_y_res - win->win.y_res;
}

static int lwm_win_sync(lwm_win_t* win) {
	win_sync(&win->win);
	__lwm_win_process_vpos(win);

	return 0;
}

static void lwm_win_commit(lwm_win_t* win) {
	lwm_t* wm = win->wm;
	win_modify(&win->win, win->win.x_pos, win->win.y_pos, win->win.x_res, win->win.y_res);
}

static int lwm_win_fullscreen(lwm_win_t* win, unsigned fullscreen) {
	if (win->fullscreen == fullscreen) {
		return 0;
	}

	lwm_t* wm = win->wm;

	if (!fullscreen) {
		win->fullscreen = 0;

		win->win.x_pos = win->mini_x_pos;
		win->win.y_pos = win->mini_y_pos;

		win->win.x_res = win->mini_x_res;
		win->win.y_res = win->mini_y_res;

		lwm_win_commit(win); // modifications must all be done before this call to avoid potential race conditions
		return 0;
	}

	// convert window centre to wm provider-space

	int cx = win->vx_pos + win->win.x_res / 2;
	int cy = win->vy_pos + win->win.y_res / 2;

	WM_ITERATE_PROVIDERS_BEGIN(wm->wm)
		// are we outside of this provider?

		if (cx < x || cy > x + x_res || cy < y || cy > y + y_res) {
			continue;
		}

		// found provider! fullscreen!

		win->fullscreen = 1;

		win->mini_x_pos = win->win.x_pos;
		win->mini_y_pos = win->win.y_pos;

		win->mini_x_res = win->win.x_res;
		win->mini_y_res = win->win.y_res;

		win->win.x_pos = (float) x / wm->x_res;
		win->win.y_pos = (float) (wm->y_res - y - y_res) / wm->y_res;

		win->win.x_res = x_res;
		win->win.y_res = y_res;

		lwm_win_commit(win); // modifications must all be done before this call to avoid potential race conditions
		break;
	WM_ITERATE_PROVIDERS_END

	return 0;
}

static int lwm_win_change_state(lwm_win_t* win) {
	win_state_t state = win_get_state(&win->win);

	win->transient = state & WIN_STATE_TRANSIENT;
	lwm_win_fullscreen(win, state & WIN_STATE_FULLSCREEN);

	return 0;
}

static int lwm_win_change_caption(lwm_win_t* win) {
	char* caption = win_get_caption(&win->win);

	if (!caption || !*caption) {
		return 0;
	}

	if (win->caption) {
		free(win->caption);
	}

	win->caption = caption;
	return 0;
}

static lwm_win_t* lwm_win_new(lwm_t* wm, uint64_t _win) {
	lwm_win_t* win = batch_alloc(&lwm_win_type);
	win->wm = wm;

	win->win.win = _win;
	lwm_win_sync(win);

	win->resize_border = RESIZE_BORDER;

	lwm_win_change_caption(win);

	// push myself to the window list

	push(wm->win_list, win);

	return win;
}

static int lwm_win_focus(lwm_win_t* win) {
	lwm_t* wm = win->wm;
	object_t* elem = (void*) win;

	_list_detach(wm->win_list, elem);
	push(wm->win_list, elem);

	return 0;
}

static inline void __lwm_win_update_visual(lwm_win_t* win) {
	__lwm_win_process_vpos(win);
}

static int lwm_win_dwd(lwm_win_t* win) {
	__lwm_win_update_visual(win);
	return 0;
}

static int lwm_win_modify(lwm_win_t* win) {
	if (lwm_win_sync(win) < 0) {
		return -1;
	}

	// update window visually

	__lwm_win_update_visual(win);

	return 0;
}

static int lwm_win_hide(lwm_win_t* win) {
	return 0;
}

static int lwm_win_close(lwm_win_t* win) {
	return 0;
}

static int lwm_win_show(lwm_win_t* win) {
	// if (lwm_win_sync(win) < 0) {
	// 	return -1;
	// }

	// update the state

	if (lwm_win_change_state(win) < 0) {
		return -1;
	}

	// update the caption

	lwm_win_change_caption(win);

	// centre window around cursor if it's at (0, 0)

	if ((win->vx_pos < 2 && win->vy_pos < 2) || !win->transient) {
		float x = win->wm->mx - win->win.width  / 2;
		float y = win->wm->my - win->win.height / 2;

		if (x != x || y != y) {
			x = 0.5 - win->win.width  / 2;
			y = 0.5 - win->win.height / 2;
		}

		// in case for some reason the coordinates end up being negative

		x = fabs(x);
		y = fabs(y);

		win_modify(&win->win, x, y, win->win.x_res, win->win.y_res);
	}

	// make ui visible

	int rv = lwm_win_modify(win);
	lwm_win_focus(win);

	return rv;
}

static void lwm_win_max(lwm_win_t* win, max_level_t max_level /* TODO for now, this parameter is ignored */) {
	lwm_t* wm = win->wm;

	// don't wanna maximize if the window already is maximized

	if (max_level > MAX_LEVEL_NONE && win->max_level > MAX_LEVEL_NONE) {
		return;
	}

	// convert wm->mx/my to wm provider-space

	int mx = wm->mx * wm->x_res;
	int my = (1 - wm->my) * wm->y_res;

	WM_ITERATE_PROVIDERS_BEGIN(wm->wm)
		// are we outside of this provider?

		if (mx < x || mx > x + x_res || my < y || my > y + y_res) {
			continue;
		}

		// found provider! maximize!
		// TODO should provider-space not also start at the bottom-left?

		win->max_level = max_level;

		win->win.x_pos = (float) x / wm->x_res;
		win->win.y_pos = (float) (wm->y_res - y - y_res) / wm->y_res;

		win->win.x_res = x_res;
		win->win.y_res = y_res;

		lwm_win_commit(win); // modifications must all be done before this call to avoid potential race conditions
		break;
	WM_ITERATE_PROVIDERS_END
}

// a bit of a self-bet here, but I bet this function's signature will never ever need to be modified to return an int

static void lwm_win_process_snapping(lwm_win_t* win) {
	lwm_t* wm = win->wm;

	// just to make sure ...

	if (!win->moving) {
		return;
	}

	// if (wm->my > 1.0 - (float) SNAPPING_SENSITIVITY / wm->y_res) {
	// 	lwm_win_max(win, MAX_LEVEL_FULL);
	// }
}

// return 0 when clicking on content, 1 when clicking a grabby bit
// assume vx/vy are coordinates inside the window
// dry is whether or not we want to set the windows click type (win->ct)

static unsigned lwm_win_process_click(lwm_win_t* win, float x, float y, unsigned dry) {
	lwm_t* wm = win->wm;
	win->ct = CT_NONE;

	// TODO cf. lwm_win_sync & cb_click

	int vx = x * wm->x_res;
	int vy = wm->y_res - y * wm->y_res;

	// was the click on a window border? (resize window)
	// TODO disregard resize grabs when window is not resizable

	unsigned left   = vx <= win->vx_pos + win->resize_border;
	unsigned right  = vx >= win->vx_pos - win->resize_border + win->win.x_res;

	unsigned top    = vy <= win->vy_pos + win->resize_border;
	unsigned bottom = vy >= win->vy_pos - win->resize_border + win->win.y_res;

	if (left || right || top || bottom) {
		if (!dry) {
			win->ct |= CT_RESZ_LEFT   * left;
			win->ct |= CT_RESZ_RIGHT  * right;
			win->ct |= CT_RESZ_TOP    * top;
			win->ct |= CT_RESZ_BOTTOM * bottom;
		}

		return 1;
	}

	// was the click on the titlebar? (move window)

	if (vy >= win->vy_pos && vy < win->vy_pos) {
		if (!dry) {
			win->ct = CT_MOVE;
		}

		return 1;
	}

	// otherwise we clicked on the window content

	return 0;
}

static unsigned lwm_win_movable(lwm_win_t* win) {
	if (win->max_level > MAX_LEVEL_NONE) {
		return 0;
	}

	if (win->fullscreen) {
		return 0;
	}

	return 1;
}

static inline void __lwm_win_init_drag(lwm_win_t* win) {
	lwm_t* wm = win->wm;

	win->move_init_x = win->win.x_pos;
	win->move_init_y = win->win.y_pos;

	win->drag_init_mx = wm->mx;
	win->drag_init_my = wm->my;
}

static inline int __lwm_win_process_move(lwm_win_t* win, unsigned pressed) {
	lwm_t* wm = win->wm;

	// initiate a move
	// TODO what happens if we become unmovable while moving?

	if (!win->moving && pressed && lwm_win_movable(win)) {
		win->moving = 1;

		__lwm_win_init_drag(win);

		return 1;
	}

	// if not moving, end things here

	if (!win->moving) {
		return 0;
	}

	// stop moving and actually move the window to its new position

	if (!wm->pressed) {
		win->moving = 0;

		lwm_win_commit(win);

		return 1;
	}

	// in the middle of a move

	win->win.x_pos = win->move_init_x + wm->mx - win->drag_init_mx;
	win->win.y_pos = win->move_init_y + wm->my - win->drag_init_my;

	lwm_win_process_snapping(win);
	__lwm_win_update_visual(win);

	return 1;
}

static inline int __lwm_win_process_resize(lwm_win_t* win, unsigned pressed) {
	lwm_t* wm = win->wm;

	// initiate a resize

	if (!win->resizing && pressed) {
		win->resizing = win->ct;

		win->resize_init_x = win->win.width;
		win->resize_init_y = win->win.height;

		__lwm_win_init_drag(win);

		return 1;
	}

	// if not resizing, end things here

	if (!win->resizing) {
		return 0;
	}

	// stop resizing and actually resize the window to its new size

	if (!wm->pressed) {
		win->resizing = 0;

		lwm_win_commit(win);

		return 1;
	}

	// in the middle of a resize

	if (win->resizing & CT_RESZ_RIGHT) {
		win->win.width = win->resize_init_x + wm->mx - win->drag_init_mx;
	}

	else if (win->resizing & CT_RESZ_LEFT) {
		win->win.width = win->resize_init_x - wm->mx + win->drag_init_mx;
	}

	if (win->resizing & CT_RESZ_TOP) {
		win->win.height = win->resize_init_y + wm->my - win->drag_init_my;
	}

	else if (win->resizing & CT_RESZ_BOTTOM) {
		win->win.height = win->resize_init_y - wm->my + win->drag_init_my;
	}

	// make sure window doesn't end up too small
	// TODO make this like 1000* cleaner

	if (win->win.width < 0.05) {
		win->win.width = 0.05;
	}

	if (win->win.height < 0.1) {
		win->win.height = 0.1;
	}

	// in the case of resizing from the left or the bottom, we'll equally need to change the window's position

	if (win->resizing & CT_RESZ_LEFT) {
		win->win.x_pos = win->move_init_x - win->win.width + win->resize_init_x;
	}

	if (win->resizing & CT_RESZ_BOTTOM) {
		win->win.y_pos = win->move_init_y - win->win.height + win->resize_init_y;
	}

	// __lwm_win_update_visual & lwm_win_commit use the window's resolution, not size, so convert that

	win->win.x_res = win->win.width  * wm->x_res;
	win->win.y_res = win->win.height * wm->y_res;

	__lwm_win_update_visual(win);
	lwm_win_commit(win);

	return 1;
}

// returns 1 if an interaction took place, 0 otherwise

static int lwm_win_update(lwm_win_t* win) {
	lwm_t* wm = win->wm;

	unsigned pressed = 0; // ui_pressed(win->section);

	// have we just initiated a click? process what type it is

	if (win->clicked) {
		win->clicked = 0;
		lwm_win_process_click(win, wm->mx, wm->my, 0);
	}

	// if we're pressed and were not at the top of the window stack, grab focus

	if (pressed && win != (void*) wm->win_list->tail) {
		lwm_win_focus(win);
		win_grab_focus(&win->win);
	}

	// processing moving & resizing

	if (win->ct & CT_RESZ) {
		return __lwm_win_process_resize(win, pressed);
	}

	else {
		return __lwm_win_process_move(win, pressed);
	}

	return pressed;
}

static void lwm_win_del(lwm_win_t* win) {
	lwm_t* wm = win->wm;
	object_t* elem = (void*) win;

	_list_detach(wm->win_list, elem);

	if (win->caption) {
		free(win->caption);
	}
}

static type_t lwm_win_type = {
	.name = "LWMWindow",
	.size = sizeof(lwm_win_t),

	.del = (void*) lwm_win_del,
};
