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

/* Keyboard device. */
#define KBD_DEVICE	0x01

/* The controller sends us fixed-size packets of 256 bytes. */
struct apple_spi_kbd_packet {
	u8 flags;
#define PACKET_READ	0x20
	u8 device;
	u16 offset;
	u16 remaining;
	u16 len;
	u8 data[246];
	u16 crc;
};

/* Packets contain a single variable-sized message. */
struct apple_spi_kbd_msg {
	u8 type;
#define MSG_REPORT	0x10
	u8 device;
	u8 unknown;
	u8 msgid;
	u16 rsplen;
	u16 cmdlen;
	u8 data[0];
};

struct apple_spi_kbd_priv {
	struct gpio_desc enable;
	struct apple_kbd_priv kbd;
};

static int apple_spi_kbd_check(struct input_config *input)
{
	struct udevice *dev = input->dev;
	struct apple_spi_kbd_priv *priv = dev_get_priv(dev);
	struct apple_spi_kbd_packet packet;
	struct apple_spi_kbd_msg *msg;
	struct apple_spi_kbd_report *report;
	int ret;

	memset(&packet, 0, sizeof(packet));

	ret = dm_spi_claim_bus(dev);
	if (ret < 0)
		return ret;

	/*
	 * The keyboard controller needs delays after asserting CS#
	 * and before deasserting CS#.
	 */
	ret = dm_spi_xfer(dev, 0, NULL, NULL, SPI_XFER_BEGIN);
	if (ret < 0)
		goto fail;
	udelay(100);
	ret = dm_spi_xfer(dev, sizeof(packet) * 8, NULL, &packet, 0);
	if (ret < 0)
		goto fail;
	udelay(100);
	ret = dm_spi_xfer(dev, 0, NULL, NULL, SPI_XFER_END);
	if (ret < 0)
		goto fail;

	dm_spi_release_bus(dev);

	/*
	 * The keyboard controller needs a delay between subsequent
	 * SPI transfers.
	 */
	udelay(250);

	msg = (struct apple_spi_kbd_msg *)packet.data;
	report = (struct apple_spi_kbd_report *)msg->data;
	if (packet.flags == PACKET_READ && packet.device == KBD_DEVICE &&
	    msg->type == MSG_REPORT && msg->device == KBD_DEVICE)
		return apple_kbd_handle_report(input, &priv->kbd, msg->data, msg->cmdlen);

	return 0;

fail:
	/*
	 * Make sure CS# is deasserted. If this fails there is nothing
	 * we can do, so ignore any errors.
	 */
	dm_spi_xfer(dev, 0, NULL, NULL, SPI_XFER_END);
	dm_spi_release_bus(dev);
	return ret;
}

static int apple_spi_kbd_probe(struct udevice *dev)
{
	struct apple_spi_kbd_priv *priv = dev_get_priv(dev);
	struct keyboard_priv *uc_priv = dev_get_uclass_priv(dev);
	struct stdio_dev *sdev = &uc_priv->sdev;
	struct input_config *input = &uc_priv->input;
	int ret;

	ret = gpio_request_by_name(dev, "spien-gpios", 0, &priv->enable,
				   GPIOD_IS_OUT);
	if (ret < 0)
		return ret;

	/* Reset the keyboard controller. */
	dm_gpio_set_value(&priv->enable, 1);
	udelay(5000);
	dm_gpio_set_value(&priv->enable, 0);
	udelay(5000);

	/* Enable the keyboard controller. */
	dm_gpio_set_value(&priv->enable, 1);

	input->dev = dev;
	input->read_keys = apple_spi_kbd_check;
	input_add_tables(input, false);
	strcpy(sdev->name, "spikbd");

	return input_stdio_register(sdev);
}

static const struct keyboard_ops apple_spi_kbd_ops = {
};

static const struct udevice_id apple_spi_kbd_of_match[] = {
	{ .compatible = "apple,spi-hid-transport" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(apple_spi_kbd) = {
	.name = "apple_spi_kbd",
	.id = UCLASS_KEYBOARD,
	.of_match = apple_spi_kbd_of_match,
	.probe = apple_spi_kbd_probe,
	.priv_auto = sizeof(struct apple_spi_kbd_priv),
	.ops = &apple_spi_kbd_ops,
};
