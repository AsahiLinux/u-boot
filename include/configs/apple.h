#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

/* Environment */
#define ENV_DEVICE_SETTINGS \
	"stdin=serial,usbkbd,spikbd,mtpkbd\0" \
	"stdout=vidconsole,serial\0" \
	"stderr=vidconsole,serial\0"

#define BOOT_TARGETS	"nvme usb"

#define CFG_EXTRA_ENV_SETTINGS \
	ENV_DEVICE_SETTINGS \
	"boot_env_padding=echo padding for developement purposes -       " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                                                " \
	"                                          - nothing to see here!\0" \
	"boot_targets=" BOOT_TARGETS "\0"

#endif
