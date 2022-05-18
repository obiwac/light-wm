#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <aquabsd/alps/wm.h>

#include <aquabsd/alps/mouse.h>
#include <aquabsd/alps/kbd.h>

#include <types/list.h>

typedef struct {
	wm_t wm;
	win_t* win;

	unsigned x_res, y_res;

	// input stuff

	float mx, my;

	unsigned clicked;
	unsigned pressed;

	mouse_t mouse;
	kbd_t kbd;

	// window management

	list_t* win_list;
} lwm_t;

#include "elements/win.h"
#include "wm.h"

#include <stdbool.h>
bool upped = false;

static int cb_draw(uint64_t _, uint64_t _wm) {
	lwm_t* wm = (void*) _wm;

	// process inputs

	kbd_update(wm->kbd);

	if (kbd_poll_button(wm->kbd, KBD_BUTTON_ESC)) {
		return 1; // exit
	}

	if (kbd_poll_button(wm->kbd, KBD_BUTTON_UP)) {
		if (!upped) {
			upped = true;
			system("xterm &");
		}
	}

	else {
		upped = false;
	}

	wm->mx = mouse_poll_axis(wm->mouse, MOUSE_AXIS_X);
	wm->my = mouse_poll_axis(wm->mouse, MOUSE_AXIS_Y);

	unsigned pressed = mouse_poll_button(wm->mouse, MOUSE_BUTTON_LEFT);

	wm->clicked = !wm->pressed && pressed;
	wm->pressed = pressed;

	// call cb_click if we clicked (this is also called by the WM when we're unsure of if a click is ours)

	if (wm->clicked) {
		cb_click(*(uint64_t*) &wm->mx, *(uint64_t*) &wm->my, (uint64_t) wm);
	}

	// go through each window front to back

	for (object_t* elem = wm->win_list->tail; elem; elem = elem->prev) {
		lwm_win_t* win = (void*) elem;

		if (lwm_win_update(win)) {
			break;
		}
	}

	return 0;
}

int main(void) {
	lwm_t* wm = calloc(1, sizeof *wm);

	wm->wm  = wm_create();
	wm->win = wm_get_root(wm->wm);

	wm->x_res = wm->win->x_res;
	wm->y_res = wm->win->y_res;

	printf("Total WM resolution: %dx%d\n", wm->x_res, wm->y_res);

	// input stuff

	wm->mouse = mouse_get_default();
	wm->kbd = kbd_get_default();

	// window management

	wm->win_list = list_new(0, NULL);

	// wm/win stuff & main loop

	wm_set_name(wm->wm, "Light WM");

	register_cbs(wm);

	win_register_cb(wm->win, WIN_CB_DRAW, cb_draw, wm);
	win_loop(wm->win);

	// free

	wm_delete(wm->wm);
	del(wm->win_list);

	return 0;
}
