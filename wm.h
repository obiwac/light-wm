// basically just functions & callbacks to handle wm stuff, because would clutter up the main file otherwise

static lwm_win_t* search_win(lwm_t* wm, uint64_t _win) {
	for (object_t* elem = wm->win_list->head; elem; elem = elem->next) {
		lwm_win_t* win = (void*) elem;

		if (win->win.win == _win) {
			return win;
		}
	}

	return NULL;
}

// callbacks

static int cb_create(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = lwm_win_new(wm, _win);

	if (!win) {
		return -1;
	}

	return 0;
}

static int cb_show(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	return lwm_win_show(win);
}

static int cb_hide(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	return lwm_win_hide(win);
}

static int cb_modify(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	return lwm_win_modify(win);
}

static int cb_delete(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	del(win);
	return 0;
}

static int cb_focus(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	return lwm_win_focus(win);
}

static int cb_state(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	return lwm_win_change_state(win);
}

static int cb_caption(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	return lwm_win_change_caption(win);
}

static int cb_dwd(uint64_t _wm, uint64_t _win, uint64_t _self) {
	lwm_t* wm = (void*) _self;
	lwm_win_t* win = search_win(wm, _win);

	if (!win) {
		return -1;
	}

	return lwm_win_dwd(win);
}

static int cb_click(uint64_t _x, uint64_t _y, uint64_t _self) {
	float x = *(float*) &_x;
	float y = *(float*) &_y;

	lwm_t* wm = (void*) _self;

	// TODO cf. lwm_win_sync & lwm_win_process_click

	int vx = x * wm->x_res;
	int vy = wm->y_res - y * wm->y_res;

	// go through each window front-to-back
	// this is to ensure that, when a window is obscured by another, we don't process clicks on it

	for (object_t* elem = wm->win_list->tail; elem; elem = elem->prev) {
		lwm_win_t* win = (void*) elem;

		// make sure the click happened within the window on the X axis

		if (vx < win->vx_pos || vx > win->vx_pos + win->win.x_res) {
			continue;
		}

		// make sure the click happened within the window on the Y axis (including the titlebar)

		if (vy < win->vy_pos || vy > win->vy_pos + win->win.y_res) {
			continue;
		}

		win->clicked = 1;

		// we don't wanna process windows beneath, so stop here & reject click if clicking on window contents (when lwm_win_process_click = 0)

		return lwm_win_process_click(win, x, y, 1);
	}

	return 0;
}

static void register_cbs(lwm_t* wm) {
	int (*CB_LUT[]) (uint64_t _wm, uint64_t _win, uint64_t _self) = {
		[WM_CB_CREATE ] = cb_create,
		[WM_CB_SHOW   ] = cb_show,
		[WM_CB_HIDE   ] = cb_hide,
		[WM_CB_MODIFY ] = cb_modify,
		[WM_CB_DELETE ] = cb_delete,
		[WM_CB_FOCUS  ] = cb_focus,
		[WM_CB_STATE  ] = cb_state,
		[WM_CB_DWD    ] = cb_dwd,
		[WM_CB_CAPTION] = cb_caption,
	};

	for (int i = 0; i < sizeof(CB_LUT) / sizeof(*CB_LUT); i++) {
		if (!CB_LUT[i]) {
			continue;
		}

		wm_register_cb(wm->wm, i, CB_LUT[i], wm);
	}

	// WM_CB_CLICK is a little different

	wm_register_cb_click(wm->wm, cb_click, wm);
}
