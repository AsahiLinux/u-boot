// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2026 Yureka <yureka@cyberchaos.dev>
 */

#include <dm.h>
#include <power-domain-uclass.h>

static const struct udevice_id apple_pmp_report_ids[] = {
	{ .compatible = "apple,t6000-pmp-v2-report" },
	{ .compatible = "apple,t6020-pmp-v2-report" },
	{ .compatible = "apple,t6030-pmp-v2-report" },
	{ .compatible = "apple,t8112-pmp-v2-report" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(apple_pmp_report) = {
	.name = "apple_pmp_report",
	.id = UCLASS_NOP,
	.of_match = apple_pmp_report_ids,
	.bind = dm_scan_fdt_dev,
};

static int apple_pmp_report_entry_of_xlate(struct power_domain *power_domain,
					   struct ofnode_phandle_args *args)
{
	return 0;
}

static const struct udevice_id apple_pmp_report_entry_ids[] = {
	{ .compatible = "apple,t6000-pmp-v2-report-entry" },
	{ .compatible = "apple-pmp-report-entry" },
	{ /* sentinel */ }
};

struct power_domain_ops apple_pmp_report_entry_ops = {
	.of_xlate = apple_pmp_report_entry_of_xlate,
};

U_BOOT_DRIVER(apple_pmp_report_entry) = {
	.name = "apple_pmp_report_entry",
	.id = UCLASS_POWER_DOMAIN,
	.of_match = apple_pmp_report_entry_ids,
	.ops = &apple_pmp_report_entry_ops,
};
