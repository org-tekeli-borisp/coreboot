/* SPDX-License-Identifier: GPL-2.0-only */

#include <console/console.h>
#include <acpi/acpi.h>
#include <acpi/acpigen.h>
#include <arch/cpu.h>
#include <cpu/x86/msr.h>
#include <cpu/intel/speedstep.h>
#include <cpu/intel/turbo.h>
#include <device/device.h>
#include <stdint.h>

#include "model_206ax.h"
#include "chip.h"

/*
 * List of supported C-states in this processor
 *
 * Latencies are typical worst-case package exit time in uS
 * taken from the SandyBridge BIOS specification.
 */
static const acpi_cstate_t cstate_map[] = {
	{	/* 0: C0 */
	}, {	/* 1: C1 */
		.latency = 1,
		.power = 1000,
		.resource = {
			.addrl = 0x00,	/* MWAIT State 0 */
			.space_id = ACPI_ADDRESS_SPACE_FIXED,
			.bit_width = ACPI_FFIXEDHW_VENDOR_INTEL,
			.bit_offset = ACPI_FFIXEDHW_CLASS_MWAIT,
			.access_size = ACPI_FFIXEDHW_FLAG_HW_COORD,
		}
	},
	{	/* 2: C1E */
		.latency = 1,
		.power = 1000,
		.resource = {
			.addrl = 0x01,	/* MWAIT State 0 Sub-state 1 */
			.space_id = ACPI_ADDRESS_SPACE_FIXED,
			.bit_width = ACPI_FFIXEDHW_VENDOR_INTEL,
			.bit_offset = ACPI_FFIXEDHW_CLASS_MWAIT,
			.access_size = ACPI_FFIXEDHW_FLAG_HW_COORD,
		}
	},
	{	/* 3: C3 */
		.latency = 63,
		.power = 500,
		.resource = {
			.addrl = 0x10,	/* MWAIT State 1 */
			.space_id = ACPI_ADDRESS_SPACE_FIXED,
			.bit_width = ACPI_FFIXEDHW_VENDOR_INTEL,
			.bit_offset = ACPI_FFIXEDHW_CLASS_MWAIT,
			.access_size = ACPI_FFIXEDHW_FLAG_HW_COORD,
		}
	},
	{	/* 4: C6 */
		.latency = 87,
		.power = 350,
		.resource = {
			.addrl = 0x20,	/* MWAIT State 2 */
			.space_id = ACPI_ADDRESS_SPACE_FIXED,
			.bit_width = ACPI_FFIXEDHW_VENDOR_INTEL,
			.bit_offset = ACPI_FFIXEDHW_CLASS_MWAIT,
			.access_size = ACPI_FFIXEDHW_FLAG_HW_COORD,
		}
	},
	{	/* 5: C7 */
		.latency = 90,
		.power = 200,
		.resource = {
			.addrl = 0x30,	/* MWAIT State 3 */
			.space_id = ACPI_ADDRESS_SPACE_FIXED,
			.bit_width = ACPI_FFIXEDHW_VENDOR_INTEL,
			.bit_offset = ACPI_FFIXEDHW_CLASS_MWAIT,
			.access_size = ACPI_FFIXEDHW_FLAG_HW_COORD,
		}
	},
	{	/* 6: C7S */
		.latency = 90,
		.power = 200,
		.resource = {
			.addrl = 0x31,	/* MWAIT State 3 Sub-state 1 */
			.space_id = ACPI_ADDRESS_SPACE_FIXED,
			.bit_width = ACPI_FFIXEDHW_VENDOR_INTEL,
			.bit_offset = ACPI_FFIXEDHW_CLASS_MWAIT,
			.access_size = ACPI_FFIXEDHW_FLAG_HW_COORD,
		}
	},
};

static int get_logical_cores_per_package(void)
{
	msr_t msr = rdmsr(MSR_CORE_THREAD_COUNT);
	return msr.lo & 0xffff;
}

static void generate_C_state_entries(void)
{
	struct device *lapic;
	struct cpu_intel_model_206ax_config *conf = NULL;

	/* Find the SpeedStep CPU in the device tree using magic APIC ID */
	lapic = dev_find_lapic(SPEEDSTEP_APIC_MAGIC);
	if (!lapic)
		return;
	conf = lapic->chip_info;
	if (!conf)
		return;

	const int acpi_cstates[3] = { conf->acpi_c1, conf->acpi_c2, conf->acpi_c3 };

	acpi_cstate_t acpi_cstate_map[ARRAY_SIZE(acpi_cstates)] = { 0 };

	/* Count number of active C-states */
	int count = 0;

	for (int i = 0; i < ARRAY_SIZE(acpi_cstates); i++) {
		if (acpi_cstates[i] > 0 && acpi_cstates[i] < ARRAY_SIZE(cstate_map)) {
			acpi_cstate_map[count] = cstate_map[acpi_cstates[i]];
			acpi_cstate_map[count].ctype = i + 1;
			count++;
		}
	}
	acpigen_write_CST_package(acpi_cstate_map, count);
}

static acpi_tstate_t tss_table_fine[] = {
	{ 100, 1000, 0, 0x00, 0 },
	{ 94, 940, 0, 0x1f, 0 },
	{ 88, 880, 0, 0x1e, 0 },
	{ 82, 820, 0, 0x1d, 0 },
	{ 75, 760, 0, 0x1c, 0 },
	{ 69, 700, 0, 0x1b, 0 },
	{ 63, 640, 0, 0x1a, 0 },
	{ 57, 580, 0, 0x19, 0 },
	{ 50, 520, 0, 0x18, 0 },
	{ 44, 460, 0, 0x17, 0 },
	{ 38, 400, 0, 0x16, 0 },
	{ 32, 340, 0, 0x15, 0 },
	{ 25, 280, 0, 0x14, 0 },
	{ 19, 220, 0, 0x13, 0 },
	{ 13, 160, 0, 0x12, 0 },
};

static acpi_tstate_t tss_table_coarse[] = {
	{ 100, 1000, 0, 0x00, 0 },
	{ 88, 875, 0, 0x1f, 0 },
	{ 75, 750, 0, 0x1e, 0 },
	{ 63, 625, 0, 0x1d, 0 },
	{ 50, 500, 0, 0x1c, 0 },
	{ 38, 375, 0, 0x1b, 0 },
	{ 25, 250, 0, 0x1a, 0 },
	{ 13, 125, 0, 0x19, 0 },
};

static void generate_T_state_entries(int core, int cores_per_package)
{
	/* Indicate SW_ALL coordination for T-states */
	acpigen_write_TSD_package(core, cores_per_package, SW_ALL);

	/* Indicate FFixedHW so OS will use MSR */
	acpigen_write_empty_PTC();

	/* Set a T-state limit that can be modified in NVS */
	acpigen_write_TPC("\\TLVL");

	/*
	 * CPUID.(EAX=6):EAX[5] indicates support
	 * for extended throttle levels.
	 */
	if (cpuid_eax(6) & (1 << 5))
		acpigen_write_TSS_package(
			ARRAY_SIZE(tss_table_fine), tss_table_fine);
	else
		acpigen_write_TSS_package(
			ARRAY_SIZE(tss_table_coarse), tss_table_coarse);
}

static int calculate_power(int tdp, int p1_ratio, int ratio)
{
	u32 m;
	u32 power;

	/*
	 * M = ((1.1 - ((p1_ratio - ratio) * 0.00625)) / 1.1) ^ 2
	 *
	 * Power = (ratio / p1_ratio) * m * tdp
	 */

	m = (110000 - ((p1_ratio - ratio) * 625)) / 11;
	m = (m * m) / 1000;

	power = ((ratio * 100000 / p1_ratio) / 100);
	power *= (m / 100) * (tdp / 1000);
	power /= 1000;

	return (int)power;
}

static void generate_P_state_entries(int core, int cores_per_package)
{
	int ratio_min, ratio_max, ratio_turbo, ratio_step;
	int coord_type, power_max, power_unit, num_entries;
	int ratio, power, clock, clock_max;
	msr_t msr;

	/* Determine P-state coordination type from MISC_PWR_MGMT[0] */
	msr = rdmsr(MSR_MISC_PWR_MGMT);
	if (msr.lo & MISC_PWR_MGMT_EIST_HW_DIS)
		coord_type = SW_ANY;
	else
		coord_type = HW_ALL;

	/* Get bus ratio limits and calculate clock speeds */
	msr = rdmsr(MSR_PLATFORM_INFO);
	ratio_min = (msr.hi >> (40-32)) & 0xff; /* Max Efficiency Ratio */

	/* Determine if this CPU has configurable TDP */
	if (cpu_config_tdp_levels()) {
		/* Set max ratio to nominal TDP ratio */
		msr = rdmsr(MSR_CONFIG_TDP_NOMINAL);
		ratio_max = msr.lo & 0xff;
	} else {
		/* Max Non-Turbo Ratio */
		ratio_max = (msr.lo >> 8) & 0xff;
	}
	clock_max = ratio_max * SANDYBRIDGE_BCLK;

	/* Calculate CPU TDP in mW */
	msr = rdmsr(MSR_PKG_POWER_SKU_UNIT);
	power_unit = 2 << ((msr.lo & 0xf) - 1);
	msr = rdmsr(MSR_PKG_POWER_SKU);
	power_max = ((msr.lo & 0x7fff) / power_unit) * 1000;

	/* Write _PCT indicating use of FFixedHW */
	acpigen_write_empty_PCT();

	/* Write _PPC with no limit on supported P-state */
	acpigen_write_PPC_NVS();

	/* Write PSD indicating configured coordination type */
	acpigen_write_PSD_package(core, cores_per_package, coord_type);

	/* Add P-state entries in _PSS table */
	acpigen_write_name("_PSS");

	/* Determine ratio points */
	ratio_step = PSS_RATIO_STEP;
	num_entries = (ratio_max - ratio_min) / ratio_step;
	while (num_entries > PSS_MAX_ENTRIES-1) {
		ratio_step <<= 1;
		num_entries >>= 1;
	}

	/* P[T] is Turbo state if enabled */
	if (get_turbo_state() == TURBO_ENABLED) {
		/* _PSS package count including Turbo */
		acpigen_write_package(num_entries + 2);

		msr = rdmsr(MSR_TURBO_RATIO_LIMIT);
		ratio_turbo = msr.lo & 0xff;

		/* Add entry for Turbo ratio */
		acpigen_write_PSS_package(
			clock_max + 1,		/*MHz*/
			power_max,		/*mW*/
			PSS_LATENCY_TRANSITION,	/*lat1*/
			PSS_LATENCY_BUSMASTER,	/*lat2*/
			ratio_turbo << 8,	/*control*/
			ratio_turbo << 8);	/*status*/
	} else {
		/* _PSS package count without Turbo */
		acpigen_write_package(num_entries + 1);
	}

	/* First regular entry is max non-turbo ratio */
	acpigen_write_PSS_package(
		clock_max,		/*MHz*/
		power_max,		/*mW*/
		PSS_LATENCY_TRANSITION,	/*lat1*/
		PSS_LATENCY_BUSMASTER,	/*lat2*/
		ratio_max << 8,		/*control*/
		ratio_max << 8);	/*status*/

	/* Generate the remaining entries */
	for (ratio = ratio_min + ((num_entries - 1) * ratio_step);
	     ratio >= ratio_min; ratio -= ratio_step) {
		/* Calculate power at this ratio */
		power = calculate_power(power_max, ratio_max, ratio);
		clock = ratio * SANDYBRIDGE_BCLK;

		acpigen_write_PSS_package(
			clock,			/*MHz*/
			power,			/*mW*/
			PSS_LATENCY_TRANSITION,	/*lat1*/
			PSS_LATENCY_BUSMASTER,	/*lat2*/
			ratio << 8,		/*control*/
			ratio << 8);		/*status*/
	}

	/* Fix package length */
	acpigen_pop_len();
}

void generate_cpu_entries(const struct device *device)
{
	int coreID, cpuID;
	int totalcores = dev_count_cpu();
	int cores_per_package = get_logical_cores_per_package();
	int numcpus = totalcores/cores_per_package;

	printk(BIOS_DEBUG, "Found %d CPU(s) with %d core(s) each.\n",
	       numcpus, cores_per_package);

	for (cpuID = 1; cpuID <= numcpus; cpuID++) {
		for (coreID = 1; coreID <= cores_per_package; coreID++) {
			/* Generate processor \_SB.CPUx */
			acpigen_write_processor(
				(cpuID-1)*cores_per_package+coreID-1, 0, 0);

			/* Generate P-state tables */
			generate_P_state_entries(
				cpuID-1, cores_per_package);

			/* Generate C-state tables */
			generate_C_state_entries();

			/* Generate T-state tables */
			generate_T_state_entries(
				cpuID-1, cores_per_package);

			acpigen_pop_len();
		}
	}

	/* PPKG is usually used for thermal management
	   of the first and only package. */
	acpigen_write_processor_package("PPKG", 0, cores_per_package);

	/* Add a method to notify processor nodes */
	acpigen_write_processor_cnot(cores_per_package);
}

struct chip_operations cpu_intel_model_206ax_ops = {
	CHIP_NAME("Intel SandyBridge/IvyBridge CPU")
};
