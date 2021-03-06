/*
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <fuse.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <thermal.h>
#include <dm/device-internal.h>
#include <dm/device.h>

DECLARE_GLOBAL_DATA_PTR;

#define SITES_MAX	16

#define TMR_DISABLE	0x0
#define TMR_ME		0x80000000
#define TMR_ALPF	0x0c000000
#define TMTMIR_DEFAULT	0x00000002
#define TIER_DISABLE	0x0

/*
 * NXP TMU Registers
 */
struct nxp_tmu_site_regs {
	u32 tritsr;		/* Immediate Temperature Site Register */
	u32 tratsr;		/* Average Temperature Site Register */
	u8 res0[0x8];
};

struct nxp_tmu_regs {
	u32 tmr;		/* Mode Register */
	u32 tsr;		/* Status Register */
	u32 tmtmir;		/* Temperature measurement interval Register */
	u8 res0[0x14];
	u32 tier;		/* Interrupt Enable Register */
	u32 tidr;		/* Interrupt Detect Register */
	u32 tiscr;		/* Interrupt Site Capture Register */
	u32 ticscr;		/* Interrupt Critical Site Capture Register */
	u8 res1[0x10];
	u32 tmhtcrh;		/* High Temperature Capture Register */
	u32 tmhtcrl;		/* Low Temperature Capture Register */
	u8 res2[0x8];
	u32 tmhtitr;		/* High Temperature Immediate Threshold */
	u32 tmhtatr;		/* High Temperature Average Threshold */
	u32 tmhtactr;	/* High Temperature Average Crit Threshold */
	u8 res3[0x24];
	u32 ttcfgr;		/* Temperature Configuration Register */
	u32 tscfgr;		/* Sensor Configuration Register */
	u8 res4[0x78];
	struct nxp_tmu_site_regs site[SITES_MAX];
	u8 res5[0x9f8];
	u32 ipbrr0;		/* IP Block Revision Register 0 */
	u32 ipbrr1;		/* IP Block Revision Register 1 */
	u8 res6[0x310];
	u32 ttr0cr;		/* Temperature Range 0 Control Register */
	u32 ttr1cr;		/* Temperature Range 1 Control Register */
	u32 ttr2cr;		/* Temperature Range 2 Control Register */
	u32 ttr3cr;		/* Temperature Range 3 Control Register */
};

struct nxp_tmu_plat {
	int critical;
	int alert;
	int polling_delay;
	int id;
	bool zone_node;
	struct nxp_tmu_regs *regs;
};

static int read_temperature(struct udevice *dev, int *temp)
{
	struct nxp_tmu_plat *pdata = dev_get_platdata(dev);
	u32 val;

	do {
		val = readl(&pdata->regs->site[pdata->id].tritsr);
	} while (!(val & 0x80000000));

	*temp = (val & 0xff) * 1000;

	return 0;
}

#define SNVS_LPCR 0x38
static void mx8_snvs_poweroff(void)
{
	u32 value;
	void __iomem *mx8_snvs_base = (void __iomem *)SNVS_HP_BASE_ADDR;

	value = readl(mx8_snvs_base + SNVS_LPCR);
	/* Set TOP and DP_EN bit to put the PMIC into dumb mode and
	 * explicity disable.*/
	writel(value | 0x60, mx8_snvs_base + SNVS_LPCR);
	/* The PMIC will pull power and execution won't continue */
}

int nxp_tmu_get_temp(struct udevice *dev, int *temp)
{
	struct nxp_tmu_plat *pdata = dev_get_platdata(dev);
	int cpu_tmp = 0;
	int ret;

	ret = read_temperature(dev, &cpu_tmp);
	if (ret)
		return ret;


	while (cpu_tmp >= pdata->alert) {
		if (cpu_tmp >= pdata->critical) {
			printf("Critical temperature hit. Shutting down, "
					"a power cycle will be necessary\n");
			mx8_snvs_poweroff();
		}

		printf("CPU Temperature (%dC) has exceeded alert (%dC), close to critical (%dC)",
		       cpu_tmp, pdata->alert, pdata->critical);
		puts(" waiting...\n");
		mdelay(pdata->polling_delay);
		ret = read_temperature(dev, &cpu_tmp);
		if (ret)
			return ret;
	}

	*temp = cpu_tmp / 1000;

	return 0;
}

static const struct dm_thermal_ops nxp_tmu_ops = {
	.get_temp	= nxp_tmu_get_temp,
};

static int nxp_tmu_calibration(struct udevice *dev)
{
	int i, val, len, ret;
	u32 range[4];
	const fdt32_t *calibration;
	struct nxp_tmu_plat *pdata = dev_get_platdata(dev);

	debug("%s\n", __func__);

	ret = fdtdec_get_int_array(gd->fdt_blob, dev_of_offset(dev),
		"fsl,tmu-range", range, 4);
	if (ret) {
		printf("TMU: missing calibration range, ret = %d.\n", ret);
		return ret;
	}

	/* Init temperature range registers */
	writel(range[0], &pdata->regs->ttr0cr);
	writel(range[1], &pdata->regs->ttr1cr);
	writel(range[2], &pdata->regs->ttr2cr);
	writel(range[3], &pdata->regs->ttr3cr);

	calibration = fdt_getprop(gd->fdt_blob, dev_of_offset(dev),
		"fsl,tmu-calibration", &len);
	if (calibration == NULL || len % 8) {
		printf("TMU: invalid calibration data.\n");
		return -ENODEV;
	}

	for (i = 0; i < len; i += 8, calibration += 2) {
		val = fdt32_to_cpu(*calibration);
		writel(val, &pdata->regs->ttcfgr);
		val = fdt32_to_cpu(*(calibration + 1));
		writel(val, &pdata->regs->tscfgr);
	}

	return 0;
}

static void nxp_tmu_init(struct nxp_tmu_plat *pdata)
{
	debug("%s\n", __func__);

	/* Disable monitoring */
	writel(TMR_DISABLE, &pdata->regs->tmr);

	/* Disable interrupt, using polling instead */
	writel(TIER_DISABLE, &pdata->regs->tier);

	/* Set update_interval */
	writel(TMTMIR_DEFAULT, &pdata->regs->tmtmir);
}

static int nxp_tmu_enable_msite(struct udevice *dev)
{
	struct nxp_tmu_plat *pdata = dev_get_platdata(dev);
	u32 reg;

	debug("%s\n", __func__);

	if (!pdata->regs)
		return -EIO;

	/* Clear the ME before setting MSITE and ALPF*/
	reg = readl(&pdata->regs->tmr);
	reg &= ~TMR_ME;
	writel(reg, &pdata->regs->tmr);

	reg |= 1 << (15 - pdata->id);
	reg |= TMR_ALPF;
	writel(reg, &pdata->regs->tmr);

	/* Enable ME */
	reg |= TMR_ME;
	writel(reg, &pdata->regs->tmr);

	return 0;
}

static int nxp_tmu_probe(struct udevice *dev)
{
	struct nxp_tmu_plat *pdata = dev_get_platdata(dev);

	debug("%s dev name %s\n", __func__, dev->name);

	if (pdata->zone_node) {
		nxp_tmu_init(pdata);
		nxp_tmu_calibration(dev);
	} else {
		nxp_tmu_enable_msite(dev);
	}

	return 0;
}

static int nxp_tmu_bind(struct udevice *dev)
{
	int ret;
	int offset;
	const char *name;
	const void *prop;

	struct nxp_tmu_plat *pdata = dev_get_platdata(dev);

	debug("%s dev name %s\n", __func__, dev->name);

	prop = fdt_getprop(gd->fdt_blob, dev_of_offset(dev), "compatible", NULL);
	if (!prop)
		return 0;
	else
		pdata->zone_node = 1;

	offset = fdt_subnode_offset(gd->fdt_blob, 0, "thermal-zones");
	fdt_for_each_subnode(offset, gd->fdt_blob, offset) {
		/* Bind the subnode to this driver */
		name = fdt_get_name(gd->fdt_blob, offset, NULL);

		ret = device_bind_with_driver_data(dev, dev->driver, name,
						   dev->driver_data, offset, NULL);
		if (ret)
			printf("Error binding driver '%s': %d\n", dev->driver->name,
				ret);
	}
	return 0;
}

static int nxp_tmu_ofdata_to_platdata(struct udevice *dev)
{
	int ret;
	int trips_np;

	struct nxp_tmu_plat *pdata = dev_get_platdata(dev);
	struct fdtdec_phandle_args args;

	debug("%s dev name %s\n", __func__, dev->name);

	if (pdata->zone_node) {
		pdata->regs = (struct nxp_tmu_regs *)fdtdec_get_addr(gd->fdt_blob, dev_of_offset(dev), "reg");

		if ((fdt_addr_t)pdata->regs == FDT_ADDR_T_NONE)
			return -EINVAL;
		return 0;
	} else {
		struct nxp_tmu_plat *p_parent_data = dev_get_platdata(dev->parent);
		if (p_parent_data->zone_node)
			pdata->regs = p_parent_data->regs;
	}

	ret = fdtdec_parse_phandle_with_args(gd->fdt_blob, dev_of_offset(dev), "thermal-sensors",
					 "#thermal-sensor-cells",
					 0, 0, &args);
	if (ret)
		return ret;

	if (args.node != dev_of_offset(dev->parent))
		return -EFAULT;

	if (args.args_count >= 1)
		pdata->id = args.args[0];
	else
		pdata->id = 0;

	debug("args.args_count %d, id %d\n", args.args_count, pdata->id);

	pdata->polling_delay = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev), "polling-delay", 1000);

	trips_np = fdt_subnode_offset(gd->fdt_blob, dev_of_offset(dev), "trips");
	fdt_for_each_subnode(trips_np, gd->fdt_blob, trips_np) {
		const char *type;
		type = fdt_getprop(gd->fdt_blob, trips_np, "type", NULL);
		if (type) {
			if (strcmp(type, "critical") == 0)
				pdata->critical = fdtdec_get_int(gd->fdt_blob, trips_np, "temperature", 85);
			else if (strcmp(type, "passive") == 0)
				pdata->alert = fdtdec_get_int(gd->fdt_blob, trips_np, "temperature", 80);
		}
	}

	debug("id %d polling_delay %d, critical %d, alert %d\n",
		pdata->id, pdata->polling_delay, pdata->critical, pdata->alert);

	return 0;
}

static const struct udevice_id nxp_tmu_ids[] = {
	{ .compatible = "fsl,imx8mq-tmu", },
	{ }
};

U_BOOT_DRIVER(nxp_tmu) = {
	.name	= "nxp_tmu",
	.id	= UCLASS_THERMAL,
	.ops	= &nxp_tmu_ops,
	.of_match = nxp_tmu_ids,
	.bind = nxp_tmu_bind,
	.probe	= nxp_tmu_probe,
	.ofdata_to_platdata = nxp_tmu_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct nxp_tmu_plat),
	.flags  = DM_FLAG_PRE_RELOC,
};
