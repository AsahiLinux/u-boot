#ifndef __CONFIG_H
#define __CONFIG_H

#include <linux/sizes.h>

/* Environment */
#define ENV_DEVICE_SETTINGS \
	"stdin=serial,usbkbd,spikbd\0" \
	"stdout=serial,vidconsole\0" \
	"stderr=serial,vidconsole\0"

#if CONFIG_IS_ENABLED(CMD_NVME)
	#define BOOT_TARGET_NVME(func) func(NVME, nvme, 0)
#else
	#define BOOT_TARGET_NVME(func)
#endif

#if CONFIG_IS_ENABLED(CMD_USB)
	#define BOOT_TARGET_USB(func) func(USB, usb, 0)
#else
	#define BOOT_TARGET_USB(func)
#endif

#define BOOT_TARGET_DEVICES(func) \
	BOOT_TARGET_NVME(func) \
	BOOT_TARGET_USB(func)

#include <config_distro_bootcmd.h>

#if CONFIG_IS_ENABLED(CMD_NVME) && CONFIG_IS_ENABLED(CMD_USB)
#define BOOTENV_APPLE                                                          \
	"scan_dev_for_boot_partuuid="                                          \
		"part list ${devtype} ${devnum} -bootable devplist; "          \
		"env exists devplist || setenv devplist 1; "                   \
		"for distro_bootpart in ${devplist}; do "                      \
			"part uuid ${devtype} ${devnum}:${distro_bootpart} "   \
					"partuuid; "                           \
			"if test U${partuuid} = U${esp_partuuid} ; then "      \
				"if fstype ${devtype} "                        \
						"${devnum}:${distro_bootpart} "\
						"bootfstype; then "            \
					"run scan_dev_for_boot; "              \
				"fi; "                                         \
			"fi; "                                                 \
		"done; "                                                       \
		"setenv partuuid; "                                            \
		"setenv devplist\0" \
	"nvme_boot_partuuid="                                                  \
		"run nvme_init; "                                              \
		"if nvme dev ${devnum}; then "                                 \
			"devtype=nvme; run scan_dev_for_boot_partuuid;"        \
		" fi\0" \
	"bootcmd_apple_nvme0=devnum=0; run nvme_boot_partuuid\0" \
	"apple_bootcfg="                                                       \
		"if fdt addr ${fdtcontroladdr}; then "                         \
			"fdt get value esp_partuuid /chosen asahi,efi-system-partition; "\
		"fi\0" \
	"apple_bootcmd="                                                       \
		"setenv nvme_need_init; run apple_bootcfg; "                   \
		"if test -n ${esp_partuuid}; then "                            \
			"run bootcmd_apple_nvme0; "                            \
			"echo Failed to boot from ESP with UUID ${esp_partuuid}; " \
			"echo Trying to boot from USB ...; " \
		"fi; "                                                         \
		"run bootcmd_usb0; "                                           \
		"echo Failed to find boot partition `run distro_bootcmd` to boot from any partition\0" \
	"bootcmd=run apple_bootcmd\0"
#else
#define BOOTENV_APPLE
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
	ENV_DEVICE_SETTINGS \
	BOOTENV \
	BOOTENV_APPLE

#endif
