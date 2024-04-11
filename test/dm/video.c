// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2014 Google, Inc
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <common.h>
#include <bzlib.h>
#include <dm.h>
#include <gzip.h>
#include <log.h>
#include <malloc.h>
#include <mapmem.h>
#include <os.h>
#include <video.h>
#include <video_console.h>
#include <asm/test.h>
#include <asm/sdl.h>
#include <dm/test.h>
#include <dm/uclass-internal.h>
#include <test/test.h>
#include <test/ut.h>

/*
 * These tests use the standard sandbox frame buffer, the resolution of which
 * is defined in the device tree. This only supports 16bpp so the tests only
 * test that code path. It would be possible to adjust this fairly easily,
 * by adjusting the bpix value in struct sandbox_sdl_plat. However the code
 * in sandbox_sdl_sync() would also need to change to handle the different
 * surface depth.
 */
/* Basic test of the video uclass */
static int dm_test_video_base(struct unit_test_state *uts)
{
	struct video_priv *priv;
	struct udevice *dev;

	ut_assertok(uclass_get_device(UCLASS_VIDEO, 0, &dev));
	ut_asserteq(1366, video_get_xsize(dev));
	ut_asserteq(768, video_get_ysize(dev));
	priv = dev_get_uclass_priv(dev);
	ut_asserteq(priv->fb_size, 1366 * 768 * 2);

	return 0;
}
DM_TEST(dm_test_video_base, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/**
 * compress_frame_buffer() - Compress the frame buffer and return its size
 *
 * We want to write tests which perform operations on the video console and
 * check that the frame buffer ends up with the correct contents. But it is
 * painful to store 'known good' images for comparison with the frame
 * buffer. As an alternative, we can compress the frame buffer and check the
 * size of the compressed data. This provides a pretty good level of
 * certainty and the resulting tests need only check a single value.
 *
 * @uts:	Test state
 * @dev:	Video device
 * @use_copy:	Use copy frame buffer if available
 * Return: compressed size of the frame buffer, or -ve on error
 */
static int compress_frame_buffer(struct unit_test_state *uts,
				 struct udevice *dev,
				 bool use_copy)
{
	struct video_priv *priv = dev_get_uclass_priv(dev);
	uint destlen;
	void *dest;
	int ret;

	if (!IS_ENABLED(CONFIG_VIDEO_COPY))
		use_copy = false;

	destlen = priv->fb_size;
	dest = malloc(priv->fb_size);
	if (!dest)
		return -ENOMEM;
	ret = BZ2_bzBuffToBuffCompress(dest, &destlen,
				       use_copy ? priv->copy_fb : priv->fb,
				       priv->fb_size,
				       3, 0, 0);
	free(dest);
	if (ret)
		return ret;

	return destlen;
}

/**
 * check_copy_frame_buffer() - Compare main frame buffer to copy
 *
 * If the copy frame buffer is enabled, this compares it to the main
 * frame buffer. Normally they should have the same contents after a
 * sync.
 *
 * @uts:	Test state
 * @dev:	Video device
 * Return: 0, or -ve on error
 */
static int check_copy_frame_buffer(struct unit_test_state *uts,
				   struct udevice *dev)
{
	struct video_priv *priv = dev_get_uclass_priv(dev);

	if (!IS_ENABLED(CONFIG_VIDEO_COPY))
		return 0;

	ut_assertf(!memcmp(priv->fb, priv->copy_fb, priv->fb_size),
		   "Copy framebuffer does not match fb");

	return 0;
}

/*
 * Call this function at any point to halt and show the current display. Be
 * sure to run the test with the -l flag.
 */
static void __maybe_unused see_output(void)
{
	video_sync_all();
	while (1);
}

/* Select the video console driver to use for a video device */
static int select_vidconsole(struct unit_test_state *uts, const char *drv_name)
{
	struct sandbox_sdl_plat *plat;
	struct udevice *dev;

	ut_assertok(uclass_find_device(UCLASS_VIDEO, 0, &dev));
	ut_assert(!device_active(dev));
	plat = dev_get_plat(dev);
	plat->vidconsole_drv_name = "vidconsole0";

	return 0;
}

/**
 * video_get_nologo() - Disable the logo on the video device and return it
 *
 * @uts: Test state
 * @devp: Returns video device
 * Return: 0 if OK, -ve on error
 */
static int video_get_nologo(struct unit_test_state *uts, struct udevice **devp)
{
	struct video_uc_plat *uc_plat;
	struct udevice *dev;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	uc_plat = dev_get_uclass_plat(dev);
	uc_plat->hide_logo = true;

	/* now probe it */
	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	*devp = dev;

	return 0;
}

/* Test text output works on the video console */
static int dm_test_video_text(struct unit_test_state *uts)
{
	struct udevice *dev, *con;
	int i;

#define WHITE		0xffff
#define SCROLL_LINES	100

	ut_assertok(select_vidconsole(uts, "vidconsole0"));
	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	ut_assertok(vidconsole_select_font(con, "8x16", 0));
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	vidconsole_putc_xy(con, 0, 0, 'a');
	ut_asserteq(79, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	vidconsole_putc_xy(con, 0, 0, ' ');
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	for (i = 0; i < 20; i++)
		vidconsole_putc_xy(con, VID_TO_POS(i * 8), 0, ' ' + i);
	ut_asserteq(273, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	vidconsole_set_row(con, 0, WHITE);
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	for (i = 0; i < 20; i++)
		vidconsole_putc_xy(con, VID_TO_POS(i * 8), 0, ' ' + i);
	ut_asserteq(273, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_text, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

static int dm_test_video_text_12x22(struct unit_test_state *uts)
{
	struct udevice *dev, *con;
	int i;

#define WHITE		0xffff
#define SCROLL_LINES	100

	ut_assertok(select_vidconsole(uts, "vidconsole0"));
	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	ut_assertok(vidconsole_select_font(con, "12x22", 0));
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	vidconsole_putc_xy(con, 0, 0, 'a');
	ut_asserteq(89, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	vidconsole_putc_xy(con, 0, 0, ' ');
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	for (i = 0; i < 20; i++)
		vidconsole_putc_xy(con, VID_TO_POS(i * 8), 0, ' ' + i);
	ut_asserteq(363, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	vidconsole_set_row(con, 0, WHITE);
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	for (i = 0; i < 20; i++)
		vidconsole_putc_xy(con, VID_TO_POS(i * 8), 0, ' ' + i);
	ut_asserteq(363, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_text_12x22, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test handling of special characters in the console */
static int dm_test_video_chars(struct unit_test_state *uts)
{
	struct udevice *dev, *con;
	const char *test_string = "Well\b\b\b\bxhe is\r \n\ta very \amodest  \bman\n\t\tand Has much to\b\bto be modest about.";

	ut_assertok(select_vidconsole(uts, "vidconsole0"));
	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	ut_assertok(vidconsole_select_font(con, "8x16", 0));
	vidconsole_put_string(con, test_string);
	ut_asserteq(466, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_chars, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

#ifdef CONFIG_VIDEO_ANSI
#define ANSI_ESC "\x1b"
/* Test handling of ANSI escape sequences */
static int dm_test_video_ansi(struct unit_test_state *uts)
{
	struct udevice *dev, *con;

	ut_assertok(select_vidconsole(uts, "vidconsole0"));
	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	ut_assertok(vidconsole_select_font(con, "8x16", 0));

	/* reference clear: */
	video_clear(con->parent);
	video_sync(con->parent, false);
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	/* test clear escape sequence: [2J */
	vidconsole_put_string(con, "A\tB\tC"ANSI_ESC"[2J");
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	/* test set-cursor: [%d;%df */
	vidconsole_put_string(con, "abc"ANSI_ESC"[2;2fab"ANSI_ESC"[4;4fcd");
	ut_asserteq(143, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	/* test colors (30-37 fg color, 40-47 bg color) */
	vidconsole_put_string(con, ANSI_ESC"[30;41mfoo"); /* black on red */
	vidconsole_put_string(con, ANSI_ESC"[33;44mbar"); /* yellow on blue */
	ut_asserteq(272, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_ansi, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);
#endif

/**
 * check_vidconsole_output() - Run a text console test
 *
 * @uts:	Test state
 * @rot:	Console rotation (0=normal orientation, 1=90 degrees clockwise,
 *		2=upside down, 3=90 degree counterclockwise)
 * @wrap_size:	Expected size of compressed frame buffer for the wrap test
 * @scroll_size: Same for the scroll test
 * Return: 0 on success
 */
static int check_vidconsole_output(struct unit_test_state *uts, int rot,
				   int wrap_size, int scroll_size)
{
	struct udevice *dev, *con;
	struct sandbox_sdl_plat *plat;
	int i;

	ut_assertok(uclass_find_device(UCLASS_VIDEO, 0, &dev));
	ut_assert(!device_active(dev));
	plat = dev_get_plat(dev);
	plat->rot = rot;

	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	ut_assertok(vidconsole_select_font(con, "8x16", 0));
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	/* Check display wrap */
	for (i = 0; i < 120; i++)
		vidconsole_put_char(con, 'A' + i % 50);
	ut_asserteq(wrap_size, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	/* Check display scrolling */
	for (i = 0; i < SCROLL_LINES; i++) {
		vidconsole_put_char(con, 'A' + i % 50);
		vidconsole_put_char(con, '\n');
	}
	ut_asserteq(scroll_size, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	/* If we scroll enough, the screen becomes blank again */
	for (i = 0; i < SCROLL_LINES; i++)
		vidconsole_put_char(con, '\n');
	ut_asserteq(46, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}

/* Test text output through the console uclass */
static int dm_test_video_context(struct unit_test_state *uts)
{
	ut_assertok(select_vidconsole(uts, "vidconsole0"));
	ut_assertok(check_vidconsole_output(uts, 0, 788, 453));

	return 0;
}
DM_TEST(dm_test_video_context, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test rotated text output through the console uclass */
static int dm_test_video_rotation1(struct unit_test_state *uts)
{
	ut_assertok(check_vidconsole_output(uts, 1, 1112, 680));

	return 0;
}
DM_TEST(dm_test_video_rotation1, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test rotated text output through the console uclass */
static int dm_test_video_rotation2(struct unit_test_state *uts)
{
	ut_assertok(check_vidconsole_output(uts, 2, 783, 445));

	return 0;
}
DM_TEST(dm_test_video_rotation2, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test rotated text output through the console uclass */
static int dm_test_video_rotation3(struct unit_test_state *uts)
{
	ut_assertok(check_vidconsole_output(uts, 3, 1134, 681));

	return 0;
}
DM_TEST(dm_test_video_rotation3, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Read a file into memory and return a pointer to it */
static int read_file(struct unit_test_state *uts, const char *fname,
		     ulong *addrp)
{
	int buf_size = 100000;
	ulong addr = 0;
	int size, fd;
	char *buf;

	buf = map_sysmem(addr, 0);
	ut_assert(buf != NULL);
	fd = os_open(fname, OS_O_RDONLY);
	ut_assert(fd >= 0);
	size = os_read(fd, buf, buf_size);
	os_close(fd);
	ut_assert(size >= 0);
	ut_assert(size < buf_size);
	*addrp = addr;

	return 0;
}

/* Test drawing a bitmap file */
static int dm_test_video_bmp(struct unit_test_state *uts)
{
	struct udevice *dev;
	ulong addr;

	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(read_file(uts, "tools/logos/denx.bmp", &addr));

	ut_assertok(video_bmp_display(dev, addr, 0, 0, false));
	ut_asserteq(1368, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_bmp, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a bitmap file on a 8bpp display */
static int dm_test_video_bmp8(struct unit_test_state *uts)
{
	struct udevice *dev;
	ulong addr;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	ut_assertok(sandbox_sdl_set_bpp(dev, VIDEO_BPP8));

	ut_assertok(read_file(uts, "tools/logos/denx.bmp", &addr));

	ut_assertok(video_bmp_display(dev, addr, 0, 0, false));
	ut_asserteq(1247, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_bmp8, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a bitmap file on a 16bpp display */
static int dm_test_video_bmp16(struct unit_test_state *uts)
{
	ulong src, src_len = ~0UL;
	uint dst_len = ~0U;
	struct udevice *dev;
	ulong dst = 0x10000;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	ut_assertok(sandbox_sdl_set_bpp(dev, VIDEO_BPP16));

	ut_assertok(read_file(uts, "tools/logos/denx-16bpp.bmp.gz", &src));
	ut_assertok(gunzip(map_sysmem(dst, 0), dst_len, map_sysmem(src, 0),
			   &src_len));

	ut_assertok(video_bmp_display(dev, dst, 0, 0, false));
	ut_asserteq(3700, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_bmp16, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a 24bpp bitmap file on a 16bpp display */
static int dm_test_video_bmp24(struct unit_test_state *uts)
{
	ulong src, src_len = ~0UL;
	uint dst_len = ~0U;
	struct udevice *dev;
	ulong dst = 0x10000;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	ut_assertok(sandbox_sdl_set_bpp(dev, VIDEO_BPP16));

	ut_assertok(read_file(uts, "tools/logos/denx-24bpp.bmp.gz", &src));
	ut_assertok(gunzip(map_sysmem(dst, 0), dst_len, map_sysmem(src, 0),
			   &src_len));

	ut_assertok(video_bmp_display(dev, dst, 0, 0, false));
	ut_asserteq(3656, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_bmp24, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a 24bpp bitmap file on a 32bpp display */
static int dm_test_video_bmp24_32(struct unit_test_state *uts)
{
	ulong src, src_len = ~0UL;
	uint dst_len = ~0U;
	struct udevice *dev;
	ulong dst = 0x10000;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	ut_assertok(sandbox_sdl_set_bpp(dev, VIDEO_BPP32));

	ut_assertok(read_file(uts, "tools/logos/denx-24bpp.bmp.gz", &src));
	ut_assertok(gunzip(map_sysmem(dst, 0), dst_len, map_sysmem(src, 0),
			   &src_len));

	ut_assertok(video_bmp_display(dev, dst, 0, 0, false));
	ut_asserteq(6827, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_bmp24_32, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a bitmap file on a 32bpp display */
static int dm_test_video_bmp32(struct unit_test_state *uts)
{
	struct udevice *dev;
	ulong addr;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	ut_assertok(sandbox_sdl_set_bpp(dev, VIDEO_BPP32));
	ut_assertok(read_file(uts, "tools/logos/denx.bmp", &addr));

	ut_assertok(video_bmp_display(dev, addr, 0, 0, false));
	ut_asserteq(2024, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_bmp32, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a compressed bitmap file */
static int dm_test_video_bmp_comp(struct unit_test_state *uts)
{
	struct udevice *dev;
	ulong addr;

	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(read_file(uts, "tools/logos/denx-comp.bmp", &addr));

	ut_assertok(video_bmp_display(dev, addr, 0, 0, false));
	ut_asserteq(1368, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_bmp_comp, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a bitmap file on a 32bpp display */
static int dm_test_video_comp_bmp32(struct unit_test_state *uts)
{
	struct udevice *dev;
	ulong addr;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	ut_assertok(sandbox_sdl_set_bpp(dev, VIDEO_BPP32));

	ut_assertok(read_file(uts, "tools/logos/denx.bmp", &addr));

	ut_assertok(video_bmp_display(dev, addr, 0, 0, false));
	ut_asserteq(2024, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_comp_bmp32, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test drawing a bitmap file on a 8bpp display */
static int dm_test_video_comp_bmp8(struct unit_test_state *uts)
{
	struct udevice *dev;
	ulong addr;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	ut_assertok(sandbox_sdl_set_bpp(dev, VIDEO_BPP8));

	ut_assertok(read_file(uts, "tools/logos/denx.bmp", &addr));

	ut_assertok(video_bmp_display(dev, addr, 0, 0, false));
	ut_asserteq(1247, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_comp_bmp8, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test TrueType console */
static int dm_test_video_truetype(struct unit_test_state *uts)
{
	struct udevice *dev, *con;
	const char *test_string = "Criticism may not be agreeable, but it is necessary. It fulfils the same function as pain in the human body. It calls attention to an unhealthy state of things. Some see private enterprise as a predatory target to be shot, others as a cow to be milked, but few are those who see it as a sturdy horse pulling the wagon. The \aprice OF\b\bof greatness\n\tis responsibility.\n\nBye";

	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	vidconsole_put_string(con, test_string);
	ut_asserteq(12174, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_truetype, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test scrolling TrueType console */
static int dm_test_video_truetype_scroll(struct unit_test_state *uts)
{
	struct sandbox_sdl_plat *plat;
	struct udevice *dev, *con;
	const char *test_string = "Criticism may not be agreeable, but it is necessary. It fulfils the same function as pain in the human body. It calls attention to an unhealthy state of things. Some see private enterprise as a predatory target to be shot, others as a cow to be milked, but few are those who see it as a sturdy horse pulling the wagon. The \aprice OF\b\bof greatness\n\tis responsibility.\n\nBye";

	ut_assertok(uclass_find_device(UCLASS_VIDEO, 0, &dev));
	ut_assert(!device_active(dev));
	plat = dev_get_plat(dev);
	plat->font_size = 100;

	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	vidconsole_put_string(con, test_string);
	ut_asserteq(34287, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_truetype_scroll, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test TrueType backspace, within and across lines */
static int dm_test_video_truetype_bs(struct unit_test_state *uts)
{
	struct sandbox_sdl_plat *plat;
	struct udevice *dev, *con;
	const char *test_string = "...Criticism may or may\b\b\b\b\b\bnot be agreeable, but seldom it is necessary\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\bit is necessary. It fulfils the same function as pain in the human body. It calls attention to an unhealthy state of things.";

	ut_assertok(uclass_find_device(UCLASS_VIDEO, 0, &dev));
	ut_assert(!device_active(dev));
	plat = dev_get_plat(dev);
	plat->font_size = 100;

	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	vidconsole_put_string(con, test_string);
	ut_asserteq(29471, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_truetype_bs, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test partial rendering onto hardware frame buffer */
static int dm_test_video_copy(struct unit_test_state *uts)
{
	struct sandbox_sdl_plat *plat;
	struct video_uc_plat *uc_plat;
	struct udevice *dev, *con;
	struct video_priv *priv;
	const char *test_string = "\n\tCriticism may not be agreeable, but it is necessary.\t";
	ulong addr;

	if (!IS_ENABLED(CONFIG_VIDEO_COPY))
		return -EAGAIN;

	ut_assertok(uclass_find_first_device(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	uc_plat = dev_get_uclass_plat(dev);
	uc_plat->hide_logo = true;
	plat = dev_get_plat(dev);
	plat->font_size = 32;
	ut_assert(!device_active(dev));
	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	ut_assertnonnull(dev);
	priv = dev_get_uclass_priv(dev);

	ut_assertok(read_file(uts, "tools/logos/denx.bmp", &addr));
	ut_assertok(video_bmp_display(dev, addr, 0, 0, false));

	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	vidconsole_put_string(con, "\n\n\n\n\n");
	vidconsole_put_string(con, test_string);
	vidconsole_put_string(con, test_string);

	ut_asserteq(6678, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	/*
	 * Secretly clear the hardware frame buffer, but in a different
	 * color (black) to see which parts will be overwritten.
	 */
	memset(priv->copy_fb, 0, priv->fb_size);

	/*
	 * We should have the full content on the main buffer, but only
	 * the new content should have been copied to the copy buffer.
	 */
	vidconsole_put_string(con, test_string);
	vidconsole_put_string(con, test_string);
	ut_asserteq(7589, compress_frame_buffer(uts, dev, false));
	ut_asserteq(5278, compress_frame_buffer(uts, dev, true));

	return 0;
}
DM_TEST(dm_test_video_copy, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);

/* Test video damage tracking */
static int dm_test_video_damage(struct unit_test_state *uts)
{
	struct sandbox_sdl_plat *plat;
	struct udevice *dev, *con;
	struct video_priv *priv;
	const char *test_string_1 = "Criticism may not be agreeable, ";
	const char *test_string_2 = "but it is necessary.";
	const char *test_string_3 = "It fulfils the same function as pain in the human body.";

	if (!IS_ENABLED(CONFIG_VIDEO_DAMAGE))
		return -EAGAIN;

	ut_assertok(uclass_find_device(UCLASS_VIDEO, 0, &dev));
	ut_assert(!device_active(dev));
	plat = dev_get_plat(dev);
	plat->font_size = 32;

	ut_assertok(video_get_nologo(uts, &dev));
	ut_assertok(uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con));
	priv = dev_get_uclass_priv(dev);

	vidconsole_position_cursor(con, 14, 10);
	vidconsole_put_string(con, test_string_2);
	ut_asserteq(449, priv->damage.xstart);
	ut_asserteq(325, priv->damage.ystart);
	ut_asserteq(661, priv->damage.xend);
	ut_asserteq(350, priv->damage.yend);

	vidconsole_position_cursor(con, 7, 5);
	vidconsole_put_string(con, test_string_1);
	ut_asserteq(225, priv->damage.xstart);
	ut_asserteq(164, priv->damage.ystart);
	ut_asserteq(661, priv->damage.xend);
	ut_asserteq(350, priv->damage.yend);

	vidconsole_position_cursor(con, 21, 15);
	vidconsole_put_string(con, test_string_3);
	ut_asserteq(225, priv->damage.xstart);
	ut_asserteq(164, priv->damage.ystart);
	ut_asserteq(1280, priv->damage.xend);
	ut_asserteq(510, priv->damage.yend);

	video_sync(dev, false);
	ut_asserteq(priv->xsize, priv->damage.xstart);
	ut_asserteq(priv->ysize, priv->damage.ystart);
	ut_asserteq(0, priv->damage.xend);
	ut_asserteq(0, priv->damage.yend);

	ut_asserteq(7339, compress_frame_buffer(uts, dev, false));
	ut_assertok(check_copy_frame_buffer(uts, dev));

	return 0;
}
DM_TEST(dm_test_video_damage, UT_TESTF_SCAN_PDATA | UT_TESTF_SCAN_FDT);
