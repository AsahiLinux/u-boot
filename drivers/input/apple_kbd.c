// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
 */

#include <common.h>
#include <dm.h>
#include <keyboard.h>
#include <spi.h>
#include <stdio_dev.h>
#include <asm-generic/gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include "apple_kbd.h"

/*
 * The Apple keyboard controllers implement a protocol that
 * closely resembles HID Keyboard Boot protocol.  The key codes are
 * mapped according to the HID Keyboard/Keypad Usage Table.
 */

/* Modifier key bits */
#define HID_MOD_LEFTCTRL	BIT(0)
#define HID_MOD_LEFTSHIFT	BIT(1)
#define HID_MOD_LEFTALT		BIT(2)
#define HID_MOD_LEFTGUI		BIT(3)
#define HID_MOD_RIGHTCTRL	BIT(4)
#define HID_MOD_RIGHTSHIFT	BIT(5)
#define HID_MOD_RIGHTALT	BIT(6)
#define HID_MOD_RIGHTGUI	BIT(7)

static const u8 hid_kbd_keymap[] = {
	KEY_RESERVED, 0xff, 0xff, 0xff,
	KEY_A, KEY_B, KEY_C, KEY_D,
	KEY_E, KEY_F, KEY_G, KEY_H,
	KEY_I, KEY_J, KEY_K, KEY_L,
	KEY_M, KEY_N, KEY_O, KEY_P,
	KEY_Q, KEY_R, KEY_S, KEY_T,
	KEY_U, KEY_V, KEY_W, KEY_X,
	KEY_Y, KEY_Z, KEY_1, KEY_2,
	KEY_3, KEY_4, KEY_5, KEY_6,
	KEY_7, KEY_8, KEY_9, KEY_0,
	KEY_ENTER, KEY_ESC, KEY_BACKSPACE, KEY_TAB,
	KEY_SPACE, KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE,
	KEY_RIGHTBRACE, KEY_BACKSLASH, 0xff, KEY_SEMICOLON,
	KEY_APOSTROPHE, KEY_GRAVE, KEY_COMMA, KEY_DOT,
	KEY_SLASH, KEY_CAPSLOCK, KEY_F1, KEY_F2,
	KEY_F3, KEY_F4, KEY_F5, KEY_F6,
	KEY_F7, KEY_F8, KEY_F9, KEY_F10,
	KEY_F11, KEY_F12, KEY_SYSRQ, KEY_SCROLLLOCK,
	KEY_PAUSE, KEY_INSERT, KEY_HOME, KEY_PAGEUP,
	KEY_DELETE, KEY_END, KEY_PAGEDOWN, KEY_RIGHT,
	KEY_LEFT, KEY_DOWN, KEY_UP, KEY_NUMLOCK,
	KEY_KPSLASH, KEY_KPASTERISK, KEY_KPMINUS, KEY_KPPLUS,
	KEY_KPENTER, KEY_KP1, KEY_KP2, KEY_KP3,
	KEY_KP4, KEY_KP5, KEY_KP6, KEY_KP7,
	KEY_KP8, KEY_KP9, KEY_KP0, KEY_KPDOT,
	KEY_BACKSLASH, KEY_COMPOSE, KEY_POWER, KEY_KPEQUAL,
};

/* Report ID used for keyboard input reports. */
#define KBD_REPORTID	0x01

static void apple_kbd_service_modifiers(struct input_config *input,
					struct apple_kbd_priv *priv)
{
	u8 new = priv->new.modifiers;
	u8 old = priv->old.modifiers;

	if ((new ^ old) & HID_MOD_LEFTCTRL)
		input_add_keycode(input, KEY_LEFTCTRL,
				  old & HID_MOD_LEFTCTRL);
	if ((new ^ old) & HID_MOD_RIGHTCTRL)
		input_add_keycode(input, KEY_RIGHTCTRL,
				  old & HID_MOD_RIGHTCTRL);
	if ((new ^ old) & HID_MOD_LEFTSHIFT)
		input_add_keycode(input, KEY_LEFTSHIFT,
				  old & HID_MOD_LEFTSHIFT);
	if ((new ^ old) & HID_MOD_RIGHTSHIFT)
		input_add_keycode(input, KEY_RIGHTSHIFT,
				  old & HID_MOD_RIGHTSHIFT);
	if ((new ^ old) & HID_MOD_LEFTALT)
		input_add_keycode(input, KEY_LEFTALT,
				  old & HID_MOD_LEFTALT);
	if ((new ^ old) & HID_MOD_RIGHTALT)
		input_add_keycode(input, KEY_RIGHTALT,
				  old & HID_MOD_RIGHTALT);
	if ((new ^ old) & HID_MOD_LEFTGUI)
		input_add_keycode(input, KEY_LEFTMETA,
				  old & HID_MOD_LEFTGUI);
	if ((new ^ old) & HID_MOD_RIGHTGUI)
		input_add_keycode(input, KEY_RIGHTMETA,
				  old & HID_MOD_RIGHTGUI);
}

static void apple_kbd_service_key(struct input_config *input,
				  struct apple_kbd_priv *priv,
				  int i, int released)
{
	u8 *new;
	u8 *old;

	if (released) {
		new = priv->new.keycode;
		old = priv->old.keycode;
	} else {
		new = priv->old.keycode;
		old = priv->new.keycode;
	}

	if (memscan(new, old[i], sizeof(priv->new.keycode)) ==
	    new + sizeof(priv->new.keycode) &&
	    old[i] < ARRAY_SIZE(hid_kbd_keymap))
		input_add_keycode(input, hid_kbd_keymap[old[i]], released);
}

int apple_kbd_handle_report(struct input_config *input,
			    struct apple_kbd_priv *priv,
			    void *data, size_t size)
{
	struct apple_kbd_report *report = data;
	int i;

	if (size < sizeof(struct apple_kbd_report))
		return -1;

	if (report->reportid != KBD_REPORTID)
		return 0;

	memcpy(&priv->new, report, sizeof(struct apple_kbd_report));
	apple_kbd_service_modifiers(input, priv);
	for (i = 0; i < sizeof(priv->new.keycode); i++) {
		apple_kbd_service_key(input, priv, i, 1);
		apple_kbd_service_key(input, priv, i, 0);
	}
	memcpy(&priv->old, &priv->new, sizeof(struct apple_kbd_report));

	return 1;
}
