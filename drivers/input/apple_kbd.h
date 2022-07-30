/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common code for handling Apple laptop keyboards
 *
 * Copyright The Asahi Linux Contributors
 */

#ifndef _APPLE_KBD_H
#define _APPLE_KBD_H

#include <input.h>

struct apple_kbd_report {
	u8 reportid;
	u8 modifiers;
	u8 reserved;
	u8 keycode[6];
	u8 fn;
};

struct apple_kbd_priv {
	struct apple_kbd_report old; /* previous keyboard input report */
	struct apple_kbd_report new; /* current keyboard input report */
};

int apple_kbd_handle_report(struct input_config *input,
			    struct apple_kbd_priv *priv,
			    void *data, size_t size);

#endif
