
#include "intel_opregion.h"
//TODO CONVVERT to EFI_STATUS RETURN TYPEs

//#include <string.h>
//#include <stdlib.h>

/*
 * The child device config, aka the display device data structure, provides a
 * description of a port and its configuration on the platform.
 *
 * The child device config size has been increased, and fields have been added
 * and their meaning has changed over time. Care must be taken when accessing
 * basically any of the fields to ensure the correct interpretation for the BDB
 * version in question.
 *
 * When we copy the child device configs to dev_priv->vbt.child_dev, we reserve
 * space for the full structure below, and initialize the tail not actually
 * present in VBT to zeros. Accessing those fields is fine, as long as the
 * default zero is taken into account, again according to the BDB version.
 *
 * BDB versions 155 and below are considered legacy, and version 155 seems to be
 * a baseline for some of the VBT documentation. When adding new fields, please
 * include the BDB version when the field was added, if it's above that.
 */
struct child_device_config
{
	UINT16 handle;
	UINT16 device_type; /* See DEVICE_TYPE_* above */

	union
	{
		UINT8 device_id[10]; /* ascii string */
		struct
		{
			UINT8 i2c_speed;
			UINT8 dp_onboard_redriver;			/* 158 */
			UINT8 dp_ondock_redriver;			/* 158 */
			UINT8 hdmi_level_shifter_value : 5; /* 169 */
			UINT8 hdmi_max_data_rate : 3;		/* 204 */
			UINT16 dtd_buf_ptr;					/* 161 */
			UINT8 edidless_efp : 1;				/* 161 */
			UINT8 compression_enable : 1;		/* 198 */
			UINT8 compression_method : 1;		/* 198 */
			UINT8 ganged_edp : 1;				/* 202 */
			UINT8 reserved0 : 4;
			UINT8 compression_structure_index : 4; /* 198 */
			UINT8 reserved1 : 4;
			UINT8 slave_port; /* 202 */
			UINT8 reserved2;
		} __packed;
	} __packed;

	UINT16 addin_offset;
	UINT8 dvo_port; /* See DEVICE_PORT_* and DVO_PORT_* above */
	UINT8 i2c_pin;
	UINT8 slave_addr;
	UINT8 ddc_pin;
	UINT16 edid_ptr;
	UINT8 dvo_cfg; /* See DEVICE_CFG_* above */

	union
	{
		struct
		{
			UINT8 dvo2_port;
			UINT8 i2c2_pin;
			UINT8 slave2_addr;
			UINT8 ddc2_pin;
		} __packed;
		struct
		{
			UINT8 efp_routed : 1;	  /* 158 */
			UINT8 lane_reversal : 1;  /* 184 */
			UINT8 lspcon : 1;		  /* 192 */
			UINT8 iboost : 1;		  /* 196 */
			UINT8 hpd_invert : 1;	  /* 196 */
			UINT8 use_vbt_vswing : 1; /* 218 */
			UINT8 flag_reserved : 2;
			UINT8 hdmi_support : 1; /* 158 */
			UINT8 dp_support : 1;	/* 158 */
			UINT8 tmds_support : 1; /* 158 */
			UINT8 support_reserved : 5;
			UINT8 aux_channel;
			UINT8 dongle_detect;
		} __packed;
	} __packed;

	UINT8 pipe_cap : 2;
	UINT8 sdvo_stall : 1; /* 158 */
	UINT8 hpd_status : 2;
	UINT8 integrated_encoder : 1;
	UINT8 capabilities_reserved : 2;
	UINT8 dvo_wiring; /* See DEVICE_WIRE_* above */

	union
	{
		UINT8 dvo2_wiring;
		UINT8 mipi_bridge_type; /* 171 */
	} __packed;

	UINT16 extended_type;
	UINT8 dvo_function;
	UINT8 dp_usb_type_c : 1;			 /* 195 */
	UINT8 tbt : 1;						 /* 209 */
	UINT8 flags2_reserved : 2;			 /* 195 */
	UINT8 dp_port_trace_length : 4;		 /* 209 */
	UINT8 dp_gpio_index;				 /* 195 */
	UINT16 dp_gpio_pin_num;				 /* 195 */
	UINT8 dp_iboost_level : 4;			 /* 196 */
	UINT8 hdmi_iboost_level : 4;		 /* 196 */
	UINT8 dp_max_link_rate : 2;			 /* 216 CNL+ */
	UINT8 dp_max_link_rate_reserved : 6; /* 216 */
} __packed;

/* Get BDB block size given a pointer to Block ID. */
static UINT32 _get_blocksize(const UINT8 *block_base)
{
	/* The MIPI Sequence Block v3+ has a separate size field. */
	if (*block_base == BDB_MIPI_SEQUENCE && *(block_base + 3) >= 3)
		return *((const UINT32 *)(block_base + 4));
	else
		return *((const UINT16 *)(block_base + 1));
}

static struct bdb_block *find_section(struct context *context, int section_id)
{
	const struct bdb_header *bdb = context->bdb;
	int length = context->size;
	struct bdb_block *block;
	const UINT8 *base = (const UINT8 *)bdb;
	int index = 0;
	UINT32 total, current_size;
	unsigned char current_id;

	/* skip to first section */
	index += bdb->header_size;
	total = bdb->bdb_size;
	if (total > length)
		total = length;
	DebugPrint(EFI_D_ERROR, "i915: finding section %d\n", section_id);
	//block = malloc(sizeof(*block));

	/* walk the sections looking for section_id */
	while (index + 3 < total)
	{
		current_id = *(base + index);
		current_size = _get_blocksize(base + index);
		index += 3;
		//DebugPrint(EFI_D_ERROR, "i915: current id %d; index: %d; location  0x%04x; current_size: %d\n", current_id, index, (base + index), current_size);

		if (index + current_size > total)
			return NULL;

		if (current_id == section_id)
		{
			block = (struct bdb_block *)AllocatePool(sizeof(block));
			if (!block)
			{
				DebugPrint(EFI_D_ERROR, "i915: out of memory");
				//TODO CONVVERT to EFI_STATUS RETURN TYPEs
				//	exit(EXIT_FAILURE);
			}
			block->id = current_id;
			block->size = current_size;
			block->data = base + index;
			return block;
		}

		index += current_size;
	}

	FreePool(block);
	return NULL;
}
static const char *dvo_port_names[] = {
	[DVO_PORT_HDMIA] = "HDMI-A",
	[DVO_PORT_HDMIB] = "HDMI-B",
	[DVO_PORT_HDMIC] = "HDMI-C",
	[DVO_PORT_HDMID] = "HDMI-D",
	[DVO_PORT_LVDS] = "LVDS",
	[DVO_PORT_TV] = "TV",
	[DVO_PORT_CRT] = "CRT",
	[DVO_PORT_DPB] = "DP-B",
	[DVO_PORT_DPC] = "DP-C",
	[DVO_PORT_DPD] = "DP-D",
	[DVO_PORT_DPA] = "DP-A",
	[DVO_PORT_DPE] = "DP-E",
	[DVO_PORT_HDMIE] = "HDMI-E",
	[DVO_PORT_MIPIA] = "MIPI-A",
	[DVO_PORT_MIPIB] = "MIPI-B",
	[DVO_PORT_MIPIC] = "MIPI-C",
	[DVO_PORT_MIPID] = "MIPI-D",
};

static const char *dvo_port(UINT8 type)
{
	if (type < ARRAY_SIZE(dvo_port_names) && dvo_port_names[type])
		return dvo_port_names[type];
	else
		return "unknown";
}
#define DEVICE_HANDLE_CRT 0x01
#define DEVICE_HANDLE_EFP1 0x04
#define DEVICE_HANDLE_EFP2 0x40
#define DEVICE_HANDLE_EFP3 0x20
#define DEVICE_HANDLE_EFP4 0x10
#define DEVICE_HANDLE_LPF1 0x08
#define DEVICE_HANDLE_LFP2 0x80

#define DEVICE_TYPE_DP_DVI 0x68d6
#define DEVICE_TYPE_DVI 0x68d2
#define DEVICE_TYPE_MIPI 0x7cc2
static const struct
{
	unsigned char handle;
	const char *name;
} child_device_handles[] = {
	{DEVICE_HANDLE_CRT, "CRT"},
	{DEVICE_HANDLE_EFP1, "EFP 1 (HDMI/DVI/DP)"},
	{DEVICE_HANDLE_EFP2, "EFP 2 (HDMI/DVI/DP)"},
	{DEVICE_HANDLE_EFP3, "EFP 3 (HDMI/DVI/DP)"},
	{DEVICE_HANDLE_EFP4, "EFP 4 (HDMI/DVI/DP)"},
	{DEVICE_HANDLE_LPF1, "LFP 1 (eDP)"},
	{DEVICE_HANDLE_LFP2, "LFP 2 (eDP)"},
};
static const int num_child_device_handles =
	sizeof(child_device_handles) / sizeof(child_device_handles[0]);

static const char *child_device_handle(unsigned char handle)
{
	int i;

	for (i = 0; i < num_child_device_handles; i++)
		if (child_device_handles[i].handle == handle)
			return child_device_handles[i].name;

	return "unknown";
}
static const struct
{
	unsigned short type;
	const char *name;
} child_device_types[] = {
	{DEVICE_TYPE_NONE, "none"},
	{DEVICE_TYPE_CRT, "CRT"},
	{DEVICE_TYPE_TV, "TV"},
	{DEVICE_TYPE_EFP, "EFP"},
	{DEVICE_TYPE_LFP, "LFP"},
	{DEVICE_TYPE_CRT_DPMS, "CRT"},
	{DEVICE_TYPE_CRT_DPMS_HOTPLUG, "CRT"},
	{DEVICE_TYPE_TV_COMPOSITE, "TV composite"},
	{DEVICE_TYPE_TV_MACROVISION, "TV"},
	{DEVICE_TYPE_TV_RF_COMPOSITE, "TV"},
	{DEVICE_TYPE_TV_SVIDEO_COMPOSITE, "TV S-Video"},
	{DEVICE_TYPE_TV_SCART, "TV SCART"},
	{DEVICE_TYPE_TV_CODEC_HOTPLUG_PWR, "TV"},
	{DEVICE_TYPE_EFP_HOTPLUG_PWR, "EFP"},
	{DEVICE_TYPE_EFP_DVI_HOTPLUG_PWR, "DVI"},
	{DEVICE_TYPE_EFP_DVI_I, "DVI-I"},
	{DEVICE_TYPE_EFP_DVI_D_DUAL, "DL-DVI-D"},
	{DEVICE_TYPE_EFP_DVI_D_HDCP, "DVI-D"},
	{DEVICE_TYPE_OPENLDI_HOTPLUG_PWR, "OpenLDI"},
	{DEVICE_TYPE_OPENLDI_DUALPIX, "OpenLDI"},
	{DEVICE_TYPE_LFP_PANELLINK, "PanelLink"},
	{DEVICE_TYPE_LFP_CMOS_PWR, "CMOS LFP"},
	{DEVICE_TYPE_LFP_LVDS_PWR, "LVDS"},
	{DEVICE_TYPE_LFP_LVDS_DUAL, "LVDS"},
	{DEVICE_TYPE_LFP_LVDS_DUAL_HDCP, "LVDS"},
	{DEVICE_TYPE_INT_LFP, "LFP"},
	{DEVICE_TYPE_INT_TV, "TV"},
	{DEVICE_TYPE_DP, "DisplayPort"},
	{DEVICE_TYPE_DP_DUAL_MODE, "DisplayPort/HDMI/DVI"},
	{DEVICE_TYPE_DP_DVI, "DisplayPort/DVI"},
	{DEVICE_TYPE_HDMI, "HDMI/DVI"},
	{DEVICE_TYPE_DVI, "DVI"},
	{DEVICE_TYPE_eDP, "eDP"},
	{DEVICE_TYPE_MIPI, "MIPI"},
};
static const int num_child_device_types =
	sizeof(child_device_types) / sizeof(child_device_types[0]);

static const char *child_device_type(unsigned short type)
{
	int i;

	for (i = 0; i < num_child_device_types; i++)
		if (child_device_types[i].type == type)
			return child_device_types[i].name;

	return "unknown";
}
static const struct
{
	unsigned short mask;
	const char *name;
} child_device_type_bits[] = {
	{DEVICE_TYPE_CLASS_EXTENSION, "Class extension"},
	{DEVICE_TYPE_POWER_MANAGEMENT, "Power management"},
	{DEVICE_TYPE_HOTPLUG_SIGNALING, "Hotplug signaling"},
	{DEVICE_TYPE_INTERNAL_CONNECTOR, "Internal connector"},
	{DEVICE_TYPE_NOT_HDMI_OUTPUT, "HDMI output"}, /* decoded as inverse */
	{DEVICE_TYPE_MIPI_OUTPUT, "MIPI output"},
	{DEVICE_TYPE_COMPOSITE_OUTPUT, "Composite output"},
	{DEVICE_TYPE_DUAL_CHANNEL, "Dual channel"},
	{1 << 7, "Content protection"},
	{DEVICE_TYPE_HIGH_SPEED_LINK, "High speed link"},
	{DEVICE_TYPE_LVDS_SIGNALING, "LVDS signaling"},
	{DEVICE_TYPE_TMDS_DVI_SIGNALING, "TMDS/DVI signaling"},
	{DEVICE_TYPE_VIDEO_SIGNALING, "Video signaling"},
	{DEVICE_TYPE_DISPLAYPORT_OUTPUT, "DisplayPort output"},
	{DEVICE_TYPE_DIGITAL_OUTPUT, "Digital output"},
	{DEVICE_TYPE_ANALOG_OUTPUT, "Analog output"},
};

static void dump_child_device_type_bits(UINT16 type)
{
	int i;

	type ^= DEVICE_TYPE_NOT_HDMI_OUTPUT;

	for (i = 0; i < ARRAY_SIZE(child_device_type_bits); i++)
	{
		if (child_device_type_bits[i].mask & type)
			DebugPrint(EFI_D_ERROR, "i915: \t\t\t%s\n", child_device_type_bits[i].name);
	}
}
static const char *mipi_bridge_type(UINT8 type)
{
	switch (type)
	{
	case 1:
		return "ASUS";
	case 2:
		return "Toshiba";
	case 3:
		return "Renesas";
	default:
		return "unknown";
	}
}
static void dump_child_device(struct context *context,
							  const struct child_device_config *child)
{
	if (!child->device_type)
		return;

	DebugPrint(EFI_D_ERROR, "i915: Child device info:\n");
	DebugPrint(EFI_D_ERROR, "i915: \tDevice handle: 0x%04x (%s)\n", child->handle,
			   child_device_handle(child->handle));
	DebugPrint(EFI_D_ERROR, "i915: \tDevice type: 0x%04x (%s)\n", child->device_type,
			   child_device_type(child->device_type));
	dump_child_device_type_bits(child->device_type);

	if (context->bdb->version < 152)
	{
		DebugPrint(EFI_D_ERROR, "i915: \tSignature: %.*s\n", (int)sizeof(child->device_id), child->device_id);
	}
	else
	{
		DebugPrint(EFI_D_ERROR, "i915: \tI2C speed: 0x%02x\n", child->i2c_speed);
		DebugPrint(EFI_D_ERROR, "i915: \tDP onboard redriver: 0x%02x\n", child->dp_onboard_redriver);
		DebugPrint(EFI_D_ERROR, "i915: \tDP ondock redriver: 0x%02x\n", child->dp_ondock_redriver);
		DebugPrint(EFI_D_ERROR, "i915: \tHDMI level shifter value: 0x%02x\n", child->hdmi_level_shifter_value);
		//	dump_hmdi_max_data_rate(child->hdmi_max_data_rate);
		DebugPrint(EFI_D_ERROR, "i915: \tOffset to DTD buffer for edidless CHILD: 0x%02x\n", child->dtd_buf_ptr);
		DebugPrint(EFI_D_ERROR, "i915: \tEdidless EFP: %s\n", YESNO(child->edidless_efp));
		DebugPrint(EFI_D_ERROR, "i915: \tCompression enable: %s\n", YESNO(child->compression_enable));
		DebugPrint(EFI_D_ERROR, "i915: \tCompression method CPS: %s\n", YESNO(child->compression_method));
		DebugPrint(EFI_D_ERROR, "i915: \tDual pipe ganged eDP: %s\n", YESNO(child->ganged_edp));
		DebugPrint(EFI_D_ERROR, "i915: \tCompression structure index: 0x%02x)\n", child->compression_structure_index);
		DebugPrint(EFI_D_ERROR, "i915: \tSlave DDI port: 0x%02x (%s)\n", child->slave_port, dvo_port(child->slave_port));
	}

	DebugPrint(EFI_D_ERROR, "i915: \tAIM offset: %d\n", child->addin_offset);
	DebugPrint(EFI_D_ERROR, "i915: \tDVO Port: 0x%02x (%s)\n", child->dvo_port, dvo_port(child->dvo_port));

	DebugPrint(EFI_D_ERROR, "i915: \tAIM I2C pin: 0x%02x\n", child->i2c_pin);
	DebugPrint(EFI_D_ERROR, "i915: \tAIM Slave address: 0x%02x\n", child->slave_addr);
	DebugPrint(EFI_D_ERROR, "i915: \tDDC pin: 0x%02x\n", child->ddc_pin);
	DebugPrint(EFI_D_ERROR, "i915: \tEDID buffer ptr: 0x%02x\n", child->edid_ptr);
	DebugPrint(EFI_D_ERROR, "i915: \tDVO config: 0x%02x\n", child->dvo_cfg);

	if (context->bdb->version < 155)
	{
		DebugPrint(EFI_D_ERROR, "i915: \tDVO2 Port: 0x%02x (%s)\n", child->dvo2_port, dvo_port(child->dvo2_port));
		DebugPrint(EFI_D_ERROR, "i915: \tI2C2 pin: 0x%02x\n", child->i2c2_pin);
		DebugPrint(EFI_D_ERROR, "i915: \tSlave2 address: 0x%02x\n", child->slave2_addr);
		DebugPrint(EFI_D_ERROR, "i915: \tDDC2 pin: 0x%02x\n", child->ddc2_pin);
	}
	else
	{
		DebugPrint(EFI_D_ERROR, "i915: \tEFP routed through dock: %s\n", YESNO(child->efp_routed));
		DebugPrint(EFI_D_ERROR, "i915: \tLane reversal: %s\n", YESNO(child->lane_reversal));
		DebugPrint(EFI_D_ERROR, "i915: \tOnboard LSPCON: %s\n", YESNO(child->lspcon));
		DebugPrint(EFI_D_ERROR, "i915: \tIboost enable: %s\n", YESNO(child->iboost));
		DebugPrint(EFI_D_ERROR, "i915: \tHPD sense invert: %s\n", YESNO(child->hpd_invert));
		DebugPrint(EFI_D_ERROR, "i915: \tHDMI compatible? %s\n", YESNO(child->hdmi_support));
		DebugPrint(EFI_D_ERROR, "i915: \tDP compatible? %s\n", YESNO(child->dp_support));
		DebugPrint(EFI_D_ERROR, "i915: \tTMDS compatible? %s\n", YESNO(child->tmds_support));
		DebugPrint(EFI_D_ERROR, "i915: \tAux channel: 0x%02x\n", child->aux_channel);
		DebugPrint(EFI_D_ERROR, "i915: \tDongle detect: 0x%02x\n", child->dongle_detect);
	}

	DebugPrint(EFI_D_ERROR, "i915: \tPipe capabilities: 0x%02x\n", child->pipe_cap);
	DebugPrint(EFI_D_ERROR, "i915: \tSDVO stall signal available: %s\n", YESNO(child->sdvo_stall));
	DebugPrint(EFI_D_ERROR, "i915: \tHotplug connect status: 0x%02x\n", child->hpd_status);
	DebugPrint(EFI_D_ERROR, "i915: \tIntegrated encoder instead of SDVO: %s\n", YESNO(child->integrated_encoder));
	DebugPrint(EFI_D_ERROR, "i915: \tDVO wiring: 0x%02x\n", child->dvo_wiring);

	if (context->bdb->version < 171)
	{
		DebugPrint(EFI_D_ERROR, "i915: \tDVO2 wiring: 0x%02x\n", child->dvo2_wiring);
	}
	else
	{
		DebugPrint(EFI_D_ERROR, "i915: \tMIPI bridge type: %02x (%s)\n", child->mipi_bridge_type,
				   mipi_bridge_type(child->mipi_bridge_type));
	}

	DebugPrint(EFI_D_ERROR, "i915: \tDevice class extension: 0x%02x\n", child->extended_type);
	DebugPrint(EFI_D_ERROR, "i915: \tDVO function: 0x%02x\n", child->dvo_function);

	if (context->bdb->version >= 195)
	{
		DebugPrint(EFI_D_ERROR, "i915: \tDP USB type C support: %s\n", YESNO(child->dp_usb_type_c));
		DebugPrint(EFI_D_ERROR, "i915: \t2X DP GPIO index: 0x%02x\n", child->dp_gpio_index);
		DebugPrint(EFI_D_ERROR, "i915: \t2X DP GPIO pin number: 0x%02x\n", child->dp_gpio_pin_num);
	}

	if (context->bdb->version >= 196)
	{
		DebugPrint(EFI_D_ERROR, "i915: \tIBoost level for HDMI: 0x%02x\n", child->hdmi_iboost_level);
		DebugPrint(EFI_D_ERROR, "i915: \tIBoost level for DP/eDP: 0x%02x\n", child->dp_iboost_level);
	}
}
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

static void dump_child_devices(struct context *context, const UINT8 *devices,
							   UINT8 child_dev_num, UINT8 child_dev_size)
{
	struct child_device_config *child;
	int i;

	/*
	 * Use a temp buffer so dump_child_device() doesn't have to worry about
	 * accessing the struct beyond child_dev_size. The tail, if any, remains
	 * initialized to zero.
	 */
	child = (struct child_device_config *)AllocateZeroPool(sizeof(*child));

	//child = calloc(1, sizeof(*child));

	for (i = 0; i < child_dev_num; i++)
	{
		CopyMem(child, devices + i * child_dev_size,
				min(sizeof(*child), child_dev_size));

		dump_child_device(context, child);
	}

	FreePool(child);
}
static void dumpNull(struct context *context,
					 const struct bdb_block *block)
{
	DebugPrint(EFI_D_ERROR, "i915: undefined block \n");
}
static void dump_general_definitions(struct context *context,
									 const struct bdb_block *block)
{
	const struct bdb_general_definitions *defs = block->data;
	int child_dev_num;

	child_dev_num = (block->size - sizeof(*defs)) / defs->child_dev_size;

	DebugPrint(EFI_D_ERROR, "i915: CRT DDC GMBUS addr: 0x%02x\n", defs->crt_ddc_gmbus_pin);
	DebugPrint(EFI_D_ERROR, "i915: Use ACPI DPMS CRT power states: %s\n",
			   YESNO(defs->dpms_acpi));
	DebugPrint(EFI_D_ERROR, "i915: Skip CRT detect at boot: %s\n",
			   YESNO(defs->skip_boot_crt_detect));
	DebugPrint(EFI_D_ERROR, "i915: Use DPMS on AIM devices: %s\n", YESNO(defs->dpms_aim));
	DebugPrint(EFI_D_ERROR, "i915: Boot display type: 0x%02x%02x\n", defs->boot_display[1],
			   defs->boot_display[0]);
	DebugPrint(EFI_D_ERROR, "i915: Child device size: %d\n", defs->child_dev_size);
	DebugPrint(EFI_D_ERROR, "i915: Child device count: %d\n", child_dev_num);

	dump_child_devices(context, defs->devices,
					   child_dev_num, defs->child_dev_size);
}

static void dump_legacy_child_devices(struct context *context,
									  const struct bdb_block *block)
{
	const struct bdb_legacy_child_devices *defs = block->data;
	int child_dev_num;

	child_dev_num = (block->size - sizeof(*defs)) / defs->child_dev_size;

	DebugPrint(EFI_D_ERROR, "i915: Child device size: %d\n", defs->child_dev_size);
	DebugPrint(EFI_D_ERROR, "i915: Child device count: %d\n", child_dev_num);

	dump_child_devices(context, defs->devices,
					   child_dev_num, defs->child_dev_size);
}
struct dumper dumpers[] = {
	{
		.id = BDB_GENERAL_FEATURES,
		.name = "General features block",
		.dump = dumpNull,
	},
	{
		.id = BDB_GENERAL_DEFINITIONS,
		.name = "General definitions block",
		.dump = dump_general_definitions,
	},
	{
		.id = BDB_CHILD_DEVICE_TABLE,
		.name = "Legacy child devices block",
		.dump = dump_legacy_child_devices,
	},
	{
		.id = BDB_LVDS_OPTIONS,
		.name = "LVDS options block",
		.dump = dumpNull,
	},
	{
		.id = BDB_LVDS_LFP_DATA_PTRS,
		.name = "LVDS timing pointer data",
		.dump = dumpNull,
	},
	{
		.id = BDB_LVDS_LFP_DATA,
		.name = "LVDS panel data block",
		.dump = dumpNull,
	},
	{
		.id = BDB_LVDS_BACKLIGHT,
		.name = "Backlight info block",
		.dump = dumpNull,
	},
	{
		.id = BDB_SDVO_LVDS_OPTIONS,
		.name = "SDVO LVDS options block",
		.dump = dumpNull,
	},
	{
		.id = BDB_SDVO_PANEL_DTDS,
		.name = "SDVO panel dtds",
		.dump = dumpNull,
	},
	{
		.id = BDB_DRIVER_FEATURES,
		.name = "Driver feature data block",
		.dump = dumpNull,
	},
	{
		.id = BDB_EDP,
		.name = "eDP block",
		.dump = dumpNull,
	},
	{
		.id = BDB_PSR,
		.name = "PSR block",
		.dump = dumpNull,
	},
	{
		.id = BDB_MIPI_CONFIG,
		.name = "MIPI configuration block",
		.dump = dumpNull,
	},
	{
		.id = BDB_MIPI_SEQUENCE,
		.name = "MIPI sequence block",
		.dump = dumpNull,
	},
	{
		.id = BDB_COMPRESSION_PARAMETERS,
		.name = "Compression parameters block",
		.dump = dumpNull,
	},
};
static BOOLEAN dump_section(struct context *context, int section_id)
{
	struct dumper *dumper = NULL;
	struct bdb_block *block;
	int i;

	block = find_section(context, section_id);
	if (!block)
		return FALSE;

	for (i = 0; i < ARRAY_SIZE(dumpers); i++)
	{
		if (block->id == dumpers[i].id)
		{
			dumper = &dumpers[i];
			break;
		}
	}

	if (dumper && dumper->name)
		DebugPrint(EFI_D_ERROR, "BDB block %d - %s:\n", block->id, dumper->name);
	else
		DebugPrint(EFI_D_ERROR, "BDB block %d - Unknown, no decoding available:\n",
				   block->id);

	//if (context->hexdump)
	//	hex_dump_block(block);
	if (dumper && dumper->dump)
		dumper->dump(context, block);
	DebugPrint(EFI_D_ERROR, "\n");

	//FreePool (block);

	return TRUE;
}
EFI_STATUS decodeVBT(struct vbt_header *vbt, int vbt_off, UINT8 *VBIOS)
{
	//UINT8 *VBIOS;
	//	int index;
	//	int fd;
	//struct vbt_header *vbt = NULL;
	int i, bdb_off;
	//	const char *filename = NULL;
	//int size = 8192;
	struct context context = {
		.panel_type = -1,
	};
	//	int block_number = -1;
	//BOOLEAN header_only = FALSE, describe = FALSE;

	/* Scour memory looking for the VBT signature */
	/* Scour memory looking for the VBT signature */
	// for (i = 0; i + 4 < size; i++) {
	// 	if (!CompareMem (VBIOS + i, "$VBT", 4)) {
	// 		vbt_off = i;
	// 		vbt = (struct vbt_header *)(VBIOS + i);
	// 		DebugPrint(EFI_D_ERROR,  "VBT signature Found, sig: %.4s at 0x%04x\n", *(VBIOS + i), i);

	// 		break;
	// 	} else {

	// 	}
	// }

	// if (!vbt) {
	// 	DebugPrint(EFI_D_ERROR,  "VBT signature missing\n");
	// //	return EXIT_FAILURE;
	// }

	context.vbt = vbt;
	bdb_off = vbt_off + vbt->bdb_offset;

	context.bdb = (const struct bdb_header *)(VBIOS + bdb_off);
	context.size = 8192;
	DebugPrint(EFI_D_ERROR, "i915: vbt: 0x%04x, bdb: 0x%04x, sig: %s, bsig: %s \n", context.vbt, context.bdb, context.vbt->signature, context.bdb->signature);
	/* 	if (!context.devid) {
		const char *devid_string = getenv("DEVICE");
		if (devid_string)
			context.devid = strtoul(devid_string, NULL, 16);
	}
	if (!context.devid)
		context.devid = get_device_id(VBIOS, size);
	if (!context.devid)
		DebugPrint(EFI_D_ERROR, "Warning: could not find PCI device ID!\n");

	if (context.panel_type == -1)
		context.panel_type = get_panel_type(&context);
	if (context.panel_type == -1) {
		DebugPrint(EFI_D_ERROR, "Warning: panel type not set, using 0\n");
		context.panel_type = 0;
	} */

	/* 	if (describe) {
		print_description(&context);
	} else if (header_only) {
		dump_headers(&context);
	} else if (block_number != -1) {
		 dump specific section only 
		if (!dump_section(&context, block_number)) {
			DebugPrint(EFI_D_ERROR, "Block %d not found\n", block_number);
			return EXIT_FAILURE;
		}
	} else { */
	//	dump_headers(&context);

	/* dump all sections  */
	for (i = 0; i < 256; i++)
		dump_section(&context, i);
	//	}

	return 0;
}