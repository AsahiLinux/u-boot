// SPDX-License-Identifier: GPL-2.0 OR MIT
/*
 * ASMedia xHCI firmware loader
 * Copyright (C) The Asahi Linux Contributors
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <fs_loader.h>
#include <linux/iopoll.h>
#include <pci.h>
#include <usb.h>
#include <usb/xhci.h>

/* Configuration space registers */
#define ASMT_CFG_CONTROL		0xe0
#define ASMT_CFG_CONTROL_WRITE		BIT(1)
#define ASMT_CFG_CONTROL_READ		BIT(0)

#define ASMT_CFG_SRAM_ADDR		0xe2

#define ASMT_CFG_SRAM_ACCESS		0xef
#define ASMT_CFG_SRAM_ACCESS_READ	BIT(6)
#define ASMT_CFG_SRAM_ACCESS_ENABLE	BIT(7)

#define ASMT_CFG_DATA_READ0		0xf0
#define ASMT_CFG_DATA_READ1		0xf4

#define ASMT_CFG_DATA_WRITE0		0xf8
#define ASMT_CFG_DATA_WRITE1		0xfc

#define ASMT_CMD_GET_FWVER		0x8000060840
#define ASMT_FWVER_ROM			0x010250090816

/* BAR0 registers */
#define ASMT_REG_ADDR			0x3000

#define ASMT_REG_DATA			0x3004

#define ASMT_REG_STATUS			0x3009
#define ASMT_REG_STATUS_BUSY		BIT(7)

#define ASMT_REG_WDATA			0x3010
#define ASMT_REG_RDATA			0x3018

#define TIMEOUT_USEC			10000
#define RESET_TIMEOUT_USEC		500000

static int asmedia_mbox_tx(struct udevice *pdev, u64 data)
{
	u8 op;
	int i;

	for (i = 0; i < TIMEOUT_USEC; i++) {
		dm_pci_read_config8(pdev, ASMT_CFG_CONTROL, &op);
		if (!(op & ASMT_CFG_CONTROL_WRITE))
			break;
		udelay(1);
	}

	if (op & ASMT_CFG_CONTROL_WRITE) {
		dev_err(pdev,
			"Timed out on mailbox tx: 0x%llx\n",
			data);
		return -ETIMEDOUT;
	}

	dm_pci_write_config32(pdev, ASMT_CFG_DATA_WRITE0, data);
	dm_pci_write_config32(pdev, ASMT_CFG_DATA_WRITE1, data >> 32);
	dm_pci_write_config8(pdev, ASMT_CFG_CONTROL,
			     ASMT_CFG_CONTROL_WRITE);

	return 0;
}

static int asmedia_mbox_rx(struct udevice *pdev, u64 *data)
{
	u8 op;
	u32 low, high;
	int i;

	for (i = 0; i < TIMEOUT_USEC; i++) {
		dm_pci_read_config8(pdev, ASMT_CFG_CONTROL, &op);
		if (op & ASMT_CFG_CONTROL_READ)
			break;
		udelay(1);
	}

	if (!(op & ASMT_CFG_CONTROL_READ)) {
		dev_err(pdev, "Timed out on mailbox rx\n");
		return -ETIMEDOUT;
	}

	dm_pci_read_config32(pdev, ASMT_CFG_DATA_READ0, &low);
	dm_pci_read_config32(pdev, ASMT_CFG_DATA_READ1, &high);
	dm_pci_write_config8(pdev, ASMT_CFG_CONTROL,
			     ASMT_CFG_CONTROL_READ);

	*data = ((u64)high << 32) | low;
	return 0;
}

static int asmedia_get_fw_version(struct udevice *pdev, u64 *version)
{
	int err = 0;
	u64 cmd;

	err = asmedia_mbox_tx(pdev, ASMT_CMD_GET_FWVER);
	if (err)
		return err;
	err = asmedia_mbox_tx(pdev, 0);
	if (err)
		return err;

	err = asmedia_mbox_rx(pdev, &cmd);
	if (err)
		return err;
	err = asmedia_mbox_rx(pdev, version);
	if (err)
		return err;

	if (cmd != ASMT_CMD_GET_FWVER) {
		dev_err(pdev, "Unexpected reply command 0x%llx\n", cmd);
		return -EIO;
	}

	return 0;
}

static bool asmedia_check_firmware(struct udevice *pdev)
{
	u64 fwver;
	int ret;

	ret = asmedia_get_fw_version(pdev, &fwver);
	if (ret)
		return ret;

	dev_info(pdev, "Firmware version: 0x%llx\n", fwver);

	return fwver != ASMT_FWVER_ROM;
}

static int asmedia_wait_reset(struct udevice *pdev, struct xhci_hcor *hcor)
{
	u32 val;
	int ret;

	ret = readl_poll_sleep_timeout(&hcor->or_usbcmd,
				       val, !(val & CMD_RESET),
				       1000, RESET_TIMEOUT_USEC);

	if (!ret)
		return 0;

	dev_err(pdev, "Reset timed out, trying to kick it\n");

	dm_pci_write_config8(pdev, ASMT_CFG_SRAM_ACCESS,
			     ASMT_CFG_SRAM_ACCESS_ENABLE);

	dm_pci_write_config8(pdev, ASMT_CFG_SRAM_ACCESS, 0);

	ret = readl_poll_sleep_timeout(&hcor->or_usbcmd,
				       val, !(val & CMD_RESET),
				       1000, RESET_TIMEOUT_USEC);

	if (ret)
		dev_err(pdev, "Reset timed out, giving up\n");

	return ret;
}

static void asmedia_write_reg(struct udevice *pdev, struct xhci_hccr *hccr,
			      u16 addr, u8 data) {
	void __iomem *regs = (void *)hccr;
	u8 status;
	int ret;

	writew_relaxed(addr, regs + ASMT_REG_ADDR);

	ret = readb_poll_sleep_timeout(regs + ASMT_REG_STATUS, status,
				       !(status & ASMT_REG_STATUS_BUSY),
				       1000, TIMEOUT_USEC);

	if (ret)
		dev_err(pdev,
			"Write addr timed out ([%04x] = %02x)\n",
			addr, data);

	writeb_relaxed(data, regs + ASMT_REG_DATA);

	ret = readb_poll_sleep_timeout(regs + ASMT_REG_STATUS, status,
				       !(status & ASMT_REG_STATUS_BUSY),
				       1000, TIMEOUT_USEC);

	if (ret)
		dev_err(pdev,
			"Write data timed out ([%04x] = %02x)\n",
			addr, data);
}

static int asmedia_load_fw(struct udevice *pdev, struct xhci_hccr *hccr,
			   struct xhci_hcor *hcor, void *fwdata, size_t fwsize)
{
	void __iomem *regs = (void *)hccr;
	const u16 *fw_data = (const u16 *)fwdata;
	u32 data;
	size_t index = 0, addr = 0;
	size_t words = fwsize >> 1;
	int ret;

	asmedia_write_reg(pdev, hccr, 0x5040, 2);
	asmedia_write_reg(pdev, hccr, 0x5042, 1);

	ret = asmedia_wait_reset(pdev, hcor);
	if (ret) {
		dev_err(pdev, "Failed pre-upload reset\n");
		return ret;
	}

	asmedia_write_reg(pdev, hccr, 0x500e, 1);

	dm_pci_write_config8(pdev, ASMT_CFG_SRAM_ACCESS,
			     ASMT_CFG_SRAM_ACCESS_ENABLE);

	/* The firmware upload is interleaved in 0x4000 word blocks */
	addr = index = 0;
	while (index < words) {
		data = fw_data[index];
		if ((index | 0x4000) < words)
			data |= fw_data[index | 0x4000] << 16;

		dm_pci_write_config16(pdev, ASMT_CFG_SRAM_ADDR,
				      addr);

		writel_relaxed(data, regs + ASMT_REG_WDATA);

		if (++index & 0x4000)
			index += 0x4000;
		addr += 2;
	}

	asmedia_write_reg(pdev, hccr, 0x5040, 3);

	dm_pci_write_config8(pdev, ASMT_CFG_SRAM_ACCESS, 0);

	asmedia_write_reg(pdev, hccr, 0x500e, 0);

	ret = asmedia_wait_reset(pdev, hcor);
	if (ret) {
		dev_err(pdev, "Failed post-upload reset\n");
		return ret;
	}

	return 0;
}

int asmedia_xhci_check_request_fw(struct udevice *pdev,
				  struct xhci_hccr *hccr,
				  struct xhci_hcor *hcor)
{
	const char *fw_name = "vendorfw/u-boot/asmedia/asm2214a-apple.bin";
	struct udevice *fsdev;
	void *data;
	int ret;

	/* Check if device has firmware, if so skip everything */
	ret = asmedia_check_firmware(pdev);
	if (ret < 0)
		return ret;
	else if (ret == 1)
		return 0;

	ret = get_fs_loader(&fsdev);
	if (ret) {
		dev_err(pdev, "Could not load firmware %s: %d\n",
			fw_name, ret);
		return ret;
	}

	data = malloc(SZ_256K);
	ret = request_firmware_into_buf(fsdev, fw_name, data, SZ_256K, 0);
	if (ret < 0) {
		dev_err(pdev, "Could not load firmware %s: %d\n",
			fw_name, ret);
		return ret;
	}

	ret = asmedia_load_fw(pdev, hccr, hcor, data, ret);
	if (ret) {
		dev_err(pdev, "Firmware upload failed: %d\n", ret);
		goto err;
	}

	ret = asmedia_check_firmware(pdev);
	if (ret < 0) {
		goto err;
	} else if (ret != 1) {
		dev_err(pdev, "Firmware version is too old after upload\n");
		ret = -EIO;
	} else {
		ret = 0;
	}

err:
	free(data);
	return ret;
}
