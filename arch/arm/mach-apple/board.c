// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2021 Mark Kettenis <kettenis@openbsd.org>
 */

#include <common.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <efi_config.h>
#include <efi_loader.h>
#include <efi_variable.h>
#include <lmb.h>
#include <malloc.h>
#include <nvme.h>
#include <part.h>

#include <asm/armv8/mmu.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/system.h>

DECLARE_GLOBAL_DATA_PTR;

/* Apple M1/M2 */

static struct mm_region t8103_mem_map[] = {
	{
		/* I/O */
		.virt = 0x200000000,
		.phys = 0x200000000,
		.size = 2UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x380000000,
		.phys = 0x380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x500000000,
		.phys = 0x500000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x680000000,
		.phys = 0x680000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x6a0000000,
		.phys = 0x6a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x6c0000000,
		.phys = 0x6c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x800000000,
		.phys = 0x800000000,
		.size = 8UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Framebuffer */
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

/* Apple M1 Pro/Max */

static struct mm_region t6000_mem_map[] = {
	{
		/* I/O */
		.virt = 0x280000000,
		.phys = 0x280000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x380000000,
		.phys = 0x380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x580000000,
		.phys = 0x580000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5a0000000,
		.phys = 0x5a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5c0000000,
		.phys = 0x5c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x700000000,
		.phys = 0x700000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xb00000000,
		.phys = 0xb00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xf00000000,
		.phys = 0xf00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x1300000000,
		.phys = 0x1300000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x10000000000,
		.phys = 0x10000000000,
		.size = 16UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Framebuffer */
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

/* Apple M1 Ultra */

static struct mm_region t6002_mem_map[] = {
	{
		/* I/O */
		.virt = 0x280000000,
		.phys = 0x280000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x380000000,
		.phys = 0x380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x580000000,
		.phys = 0x580000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5a0000000,
		.phys = 0x5a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5c0000000,
		.phys = 0x5c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x700000000,
		.phys = 0x700000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xb00000000,
		.phys = 0xb00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xf00000000,
		.phys = 0xf00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x1300000000,
		.phys = 0x1300000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2280000000,
		.phys = 0x2280000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2380000000,
		.phys = 0x2380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2580000000,
		.phys = 0x2580000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x25a0000000,
		.phys = 0x25a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x25c0000000,
		.phys = 0x25c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2700000000,
		.phys = 0x2700000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2b00000000,
		.phys = 0x2b00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2f00000000,
		.phys = 0x2f00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x3300000000,
		.phys = 0x3300000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x10000000000,
		.phys = 0x10000000000,
		.size = 16UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Framebuffer */
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

/* Apple M2 Pro/Max */

static struct mm_region t6020_mem_map[] = {
	{
		/* I/O */
		.virt = 0x280000000,
		.phys = 0x280000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x340000000,
		.phys = 0x340000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x380000000,
		.phys = 0x380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x400000000,
		.phys = 0x400000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE |
			PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x480000000,
		.phys = 0x480000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE |
			PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x580000000,
		.phys = 0x580000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5a0000000,
		.phys = 0x5a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5c0000000,
		.phys = 0x5c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x700000000,
		.phys = 0x700000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xb00000000,
		.phys = 0xb00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xf00000000,
		.phys = 0xf00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x1300000000,
		.phys = 0x1300000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x10000000000,
		.phys = 0x10000000000,
		.size = 16UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Framebuffer */
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

/* Apple M2 Ultra */

static struct mm_region t6022_mem_map[] = {
	{
		/* I/O */
		.virt = 0x280000000,
		.phys = 0x280000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x340000000,
		.phys = 0x340000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x380000000,
		.phys = 0x380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x400000000,
		.phys = 0x400000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE |
			PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x480000000,
		.phys = 0x480000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE |
			PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x580000000,
		.phys = 0x580000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5a0000000,
		.phys = 0x5a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x5c0000000,
		.phys = 0x5c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x700000000,
		.phys = 0x700000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xb00000000,
		.phys = 0xb00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0xf00000000,
		.phys = 0xf00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x1300000000,
		.phys = 0x1300000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2280000000,
		.phys = 0x2280000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2340000000,
		.phys = 0x2340000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2380000000,
		.phys = 0x2380000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2400000000,
		.phys = 0x2400000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE |
			PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2480000000,
		.phys = 0x2480000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			PTE_BLOCK_NON_SHARE |
			PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2580000000,
		.phys = 0x2580000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x25a0000000,
		.phys = 0x25a0000000,
		.size = SZ_512M,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* PCIE */
		.virt = 0x25c0000000,
		.phys = 0x25c0000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRE) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2700000000,
		.phys = 0x2700000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2b00000000,
		.phys = 0x2b00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x2f00000000,
		.phys = 0x2f00000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* I/O */
		.virt = 0x3300000000,
		.phys = 0x3300000000,
		.size = SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x10000000000,
		.phys = 0x10000000000,
		.size = 16UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Framebuffer */
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL_NC) |
			 PTE_BLOCK_INNER_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map;

int board_init(void)
{
	return 0;
}

int dram_init(void)
{
	return fdtdec_setup_mem_size_base();
}

int dram_init_banksize(void)
{
	return fdtdec_setup_memory_banksize();
}

extern long fw_dtb_pointer;

void *board_fdt_blob_setup(int *err)
{
	/* Return DTB pointer passed by m1n1 */
	*err = 0;
	return (void *)fw_dtb_pointer;
}

void build_mem_map(void)
{
	ofnode node;
	fdt_addr_t base;
	fdt_size_t size;
	int i;

	if (of_machine_is_compatible("apple,t8103") ||
	    of_machine_is_compatible("apple,t8112"))
		mem_map = t8103_mem_map;
	else if (of_machine_is_compatible("apple,t6000") ||
		 of_machine_is_compatible("apple,t6001"))
		mem_map = t6000_mem_map;
	else if (of_machine_is_compatible("apple,t6002"))
		mem_map = t6002_mem_map;
	else if (of_machine_is_compatible("apple,t6020") ||
		 of_machine_is_compatible("apple,t6021"))
		mem_map = t6020_mem_map;
	else if (of_machine_is_compatible("apple,t6022"))
		mem_map = t6022_mem_map;
	else
		panic("Unsupported SoC\n");

	/* Find list terminator. */
	for (i = 0; mem_map[i].size || mem_map[i].attrs; i++)
		;

	/* Align RAM mapping to page boundaries */
	base = gd->bd->bi_dram[0].start;
	size = gd->bd->bi_dram[0].size;
	size += (base - ALIGN_DOWN(base, SZ_4K));
	base = ALIGN_DOWN(base, SZ_4K);
	size = ALIGN(size, SZ_4K);

	/* Update RAM mapping */
	mem_map[i - 2].virt = base;
	mem_map[i - 2].phys = base;
	mem_map[i - 2].size = size;

	node = ofnode_path("/chosen/framebuffer");
	if (!ofnode_valid(node))
		return;

	base = ofnode_get_addr_size(node, "reg", &size);
	if (base == FDT_ADDR_T_NONE)
		return;

	/* Align framebuffer mapping to page boundaries */
	size += (base - ALIGN_DOWN(base, SZ_4K));
	base = ALIGN_DOWN(base, SZ_4K);
	size = ALIGN(size, SZ_4K);

	/* Add framebuffer mapping */
	mem_map[i - 1].virt = base;
	mem_map[i - 1].phys = base;
	mem_map[i - 1].size = size;
}

void enable_caches(void)
{
	build_mem_map();

	icache_enable();
	dcache_enable();
}

u64 get_page_table_size(void)
{
	return SZ_256K;
}

static char *asahi_esp_devpart(void)
{
	struct disk_partition info;
	struct blk_desc *nvme_blk;
	const char *uuid = NULL;
	int devnum, len, p, part, ret;
	static char devpart[64];
	struct udevice *dev;
	ofnode node;

	if (devpart[0])
		return devpart;

	node = ofnode_path("/chosen");
	if (ofnode_valid(node)) {
		uuid = ofnode_get_property(node, "asahi,efi-system-partition",
					   &len);
	}

	nvme_scan_namespace();
	for (devnum = 0, part = 0;; devnum++) {
		nvme_blk = blk_get_devnum_by_uclass_id(UCLASS_NVME, devnum);
		if (nvme_blk == NULL)
			break;

		dev = dev_get_parent(nvme_blk->bdev);
		if (!device_is_compatible(dev, "apple,nvme-ans2"))
			continue;

		for (p = 1; p <= MAX_SEARCH_PARTITIONS; p++) {
			ret = part_get_info(nvme_blk, p, &info);
			if (ret < 0)
				break;

			if (info.bootable & PART_EFI_SYSTEM_PARTITION) {
				if (uuid && strcasecmp(uuid, info.uuid) == 0) {
					part = p;
					break;
				}
				if (part == 0)
					part = p;
			}
		}

		if (part > 0)
			break;
	}

	if (part > 0) {
		snprintf(devpart, sizeof(devpart), "%d:%d", devnum, part);
		efi_system_partition.uclass_id = UCLASS_NVME;
		efi_system_partition.devnum = devnum;
		efi_system_partition.part = part;
	}

	return devpart;
}

const char *env_fat_get_intf(void)
{
	return blk_get_uclass_name(UCLASS_NVME);
}

char *env_fat_get_dev_part(void)
{
	return asahi_esp_devpart();
}

#define KERNEL_COMP_SIZE	SZ_128M

static void efi_seed_boot_options(void)
{
	struct efi_load_option lo;
	struct blk_desc *desc;
	efi_uintn_t size;
	efi_status_t ret;
	u16 *bootorder;
	u32 boot_index;
	u16 varname[9];
	u16 devname[BOOTMENU_DEVICE_NAME_MAX];
	char buf[BOOTMENU_DEVICE_NAME_MAX];
	char *optional_data;
	u16 *tmp = devname;
	void *p;

	/*
	 * Don't create a boot option if we don't have a designated
	 * EFI System Partition.
	 */
	if (efi_system_partition.uclass_id == UCLASS_INVALID)
		return;

	ret = efi_init_obj_list();
	if (ret != EFI_SUCCESS)
		return;

	/*
	 * If the "BootOrder" UEFI variable already exists we don't
	 * need to create a boot option.
	 */
	bootorder = efi_get_var(u"BootOrder", &efi_global_variable_guid,
				&size);
	if (bootorder)
		return;

	desc = blk_get_devnum_by_uclass_id(efi_system_partition.uclass_id,
					   efi_system_partition.devnum);
	if (desc == NULL)
		return;

	snprintf(buf, sizeof(buf), "%s %d:%d",
		 blk_get_uclass_name(efi_system_partition.uclass_id),
		 efi_system_partition.devnum, efi_system_partition.part);
	utf8_utf16_strcpy(&tmp, buf);

	/*
	 * Create a boot option for the designated EFI System
	 * Partition.  This makes sure that the EFI boot manager will
	 * be used and that it will attempt to boot from this
	 * partition first.  Mark the boot option as automatically
	 * generated to prevent eficonfig_enumerate_boot_option() from
	 * generating another one for the same partition.
	 */
	lo.label = devname;
	lo.file_path = efi_dp_from_part(desc, efi_system_partition.part);
	lo.file_path_length = efi_dp_size(lo.file_path) + sizeof(END);
	lo.attributes = LOAD_OPTION_ACTIVE;
	lo.optional_data = "1234567";
	size = efi_serialize_load_option(&lo, (u8 **)&p);
	if (size == 0)
		return;
	optional_data = (char *)p + (size - u16_strsize(u"1234567"));
	memcpy(optional_data, &efi_guid_bootmenu_auto_generated,
	       sizeof(efi_guid_t));

	ret = eficonfig_get_unused_bootoption(varname, sizeof(varname),
					      &boot_index);
	if (ret != EFI_SUCCESS)
		return;

	ret = efi_set_variable_int(varname, &efi_global_variable_guid,
				   EFI_VARIABLE_NON_VOLATILE |
				   EFI_VARIABLE_BOOTSERVICE_ACCESS |
				   EFI_VARIABLE_RUNTIME_ACCESS,
				   size, p, false);
	free(p);
	if (ret != EFI_SUCCESS)
		return;

	eficonfig_append_bootorder(boot_index);
}

int board_late_init(void)
{
	struct lmb lmb;
	u32 status = 0;

	status |= env_set("storage_interface",
			  blk_get_uclass_name(UCLASS_NVME));
	status |= env_set("fw_dev_part", asahi_esp_devpart());

	lmb_init_and_reserve(&lmb, gd->bd, (void *)gd->fdt_blob);

	/* somewhat based on the Linux Kernel boot requirements:
	 * align by 2M and maximal FDT size 2M
	 */
	status |= env_set_hex("loadaddr", lmb_alloc(&lmb, SZ_1G, SZ_2M));
	status |= env_set_hex("fdt_addr_r", lmb_alloc(&lmb, SZ_2M, SZ_2M));
	status |= env_set_hex("kernel_addr_r", lmb_alloc(&lmb, SZ_128M, SZ_2M));
	status |= env_set_hex("ramdisk_addr_r", lmb_alloc(&lmb, SZ_1G, SZ_2M));
	status |= env_set_hex("kernel_comp_addr_r",
			      lmb_alloc(&lmb, KERNEL_COMP_SIZE, SZ_2M));
	status |= env_set_hex("kernel_comp_size", KERNEL_COMP_SIZE);
	status |= env_set_hex("scriptaddr", lmb_alloc(&lmb, SZ_4M, SZ_2M));
	status |= env_set_hex("pxefile_addr_r", lmb_alloc(&lmb, SZ_4M, SZ_2M));

	if (status)
		log_warning("late_init: Failed to set run time variables\n");

	efi_seed_boot_options();

	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	struct udevice *dev;
	const char *stdoutname;
	int node, ret;

	/*
	 * Modify the "stdout-path" property under "/chosen" to point
	 * at "/chosen/framebuffer if a keyboard is available and
	 * we're not running under the m1n1 hypervisor.
	 * Developers can override this behaviour by dropping
	 * "vidconsole" from the "stdout" environment variable.
	 */

	/* EL1 means we're running under the m1n1 hypervisor. */
	if (current_el() == 1)
		return 0;

	ret = uclass_find_device(UCLASS_KEYBOARD, 0, &dev);
	if (ret < 0)
		return 0;

	stdoutname = env_get("stdout");
	if (!stdoutname || !strstr(stdoutname, "vidconsole"))
		return 0;

	/* Make sure we actually have a framebuffer. */
	node = fdt_path_offset(blob, "/chosen/framebuffer");
	if (node < 0 || !fdtdec_get_is_enabled(blob, node))
		return 0;

	node = fdt_path_offset(blob, "/chosen");
	if (node < 0)
		return 0;
	fdt_setprop_string(blob, node, "stdout-path", "/chosen/framebuffer");

	return 0;
}
