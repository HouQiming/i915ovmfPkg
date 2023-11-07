
#include "intel_opregion.h"
//TODO CONVVERT to EFI_STATUS RETURN TYPEs

//#include <string.h>
//#include <stdlib.h>

/* Get BDB block size given a pointer to Block ID. */
static UINT32 _get_blocksize(const UINT8 *block_base)
{
	/* The MIPI Sequence Block v3+ has a separate size field. */
	if (*block_base == BDB_MIPI_SEQUENCE && *(block_base + 3) >= 3)
		return *((const UINT32 *)(block_base + 4));
	else
		return *((const UINT16 *)(block_base + 1));
}

// /* Get BDB block size give a pointer to data after Block ID and Block Size. */
// static u32 get_blocksize(const void *block_data)
// {
// 	return _get_blocksize(block_data - 3);
// }

static const void *
find_section_vbt(const void *_bdb, int section_id)
{
	const struct bdb_header *bdb = _bdb;
	const u8 *base = _bdb;
	int index = 0;
	u32 total, current_size;
	u8 current_id;

	/* skip to first section */
	index += bdb->header_size;
	total = bdb->bdb_size;

	/* walk the sections looking for section_id */
	while (index + 3 < total)
	{
		current_id = *(base + index);
		current_size = _get_blocksize(base + index);
		index += 3;

		if (index + current_size > total)
			return NULL;

		if (current_id == section_id)
			return base + index;

		index += current_size;
	}

	return NULL;
}
static EFI_STATUS find_section(struct context *context, int section_id, struct bdb_block *block)
{
	const struct bdb_header *bdb = context->bdb;
	int length = context->size;
	const UINT8 *base = (const UINT8 *)bdb;
	int index = 0;
	UINT32 total, current_size;
	unsigned char current_id;

	/* skip to first section */
	index += bdb->header_size;
	total = bdb->bdb_size;
	if (total > length)
		total = length;
	PRINT_DEBUG(EFI_D_ERROR, "finding section %d\n", section_id);
	//block = malloc(sizeof(*block));

	/* walk the sections looking for section_id */
	while (index + 3 < total)
	{
		current_id = *(base + index);
		current_size = _get_blocksize(base + index);
		index += 3;
		//PRINT_DEBUG(EFI_D_ERROR,"current id %d; index: %d; location  0x%04x; current_size: %d\n", current_id, index, (base + index), current_size);

		if (index + current_size > total)
			return EFI_NOT_FOUND;

		if (current_id == section_id)
		{
			if (!block)
			{
				PRINT_DEBUG(EFI_D_ERROR, "out of memory");
				//TODO CONVVERT to EFI_STATUS RETURN TYPEs
				//	exit(EXIT_FAILURE);
			}
			block->id = current_id;
			block->size = current_size;
			block->data = base + index;
			return EFI_SUCCESS;
		}

		index += current_size;
	}

	return EFI_NOT_FOUND;
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
			PRINT_DEBUG(EFI_D_ERROR, "\t\t\t%s\n", child_device_type_bits[i].name);
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
	//if (!child->device_type)
	return;

	PRINT_DEBUG(EFI_D_ERROR, "Child device info:\n");
	PRINT_DEBUG(EFI_D_ERROR, "\tDevice handle: 0x%04x (%s)\n", child->handle,
				child_device_handle(child->handle));
	PRINT_DEBUG(EFI_D_ERROR, "\tDevice type: 0x%04x (%s)\n", child->device_type,
				child_device_type(child->device_type));
	dump_child_device_type_bits(child->device_type);

	if (context->bdb->version < 152)
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tSignature: %.*s\n", (int)sizeof(child->device_id), child->device_id);
	}
	else
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tI2C speed: 0x%02x\n", child->i2c_speed);
		PRINT_DEBUG(EFI_D_ERROR, "\tDP onboard redriver: 0x%02x\n", child->dp_onboard_redriver);
		PRINT_DEBUG(EFI_D_ERROR, "\tDP ondock redriver: 0x%02x\n", child->dp_ondock_redriver);
		PRINT_DEBUG(EFI_D_ERROR, "\tHDMI level shifter value: 0x%02x\n", child->hdmi_level_shifter_value);
		//	dump_hmdi_max_data_rate(child->hdmi_max_data_rate);
		PRINT_DEBUG(EFI_D_ERROR, "\tOffset to DTD buffer for edidless CHILD: 0x%02x\n", child->dtd_buf_ptr);
		PRINT_DEBUG(EFI_D_ERROR, "\tEdidless EFP: %s\n", YESNO(child->edidless_efp));
		PRINT_DEBUG(EFI_D_ERROR, "\tCompression enable: %s\n", YESNO(child->compression_enable));
		PRINT_DEBUG(EFI_D_ERROR, "\tCompression method CPS: %s\n", YESNO(child->compression_method));
		PRINT_DEBUG(EFI_D_ERROR, "\tDual pipe ganged eDP: %s\n", YESNO(child->ganged_edp));
		PRINT_DEBUG(EFI_D_ERROR, "\tCompression structure index: 0x%02x)\n", child->compression_structure_index);
		PRINT_DEBUG(EFI_D_ERROR, "\tSlave DDI port: 0x%02x (%s)\n", child->slave_port, dvo_port(child->slave_port));
	}

	PRINT_DEBUG(EFI_D_ERROR, "\tAIM offset: %d\n", child->addin_offset);
	PRINT_DEBUG(EFI_D_ERROR, "\tDVO Port: 0x%02x (%s)\n", child->dvo_port, dvo_port(child->dvo_port));

	PRINT_DEBUG(EFI_D_ERROR, "\tAIM I2C pin: 0x%02x\n", child->i2c_pin);
	PRINT_DEBUG(EFI_D_ERROR, "\tAIM Slave address: 0x%02x\n", child->slave_addr);
	PRINT_DEBUG(EFI_D_ERROR, "\tDDC pin: 0x%02x\n", child->ddc_pin);
	PRINT_DEBUG(EFI_D_ERROR, "\tEDID buffer ptr: 0x%02x\n", child->edid_ptr);
	PRINT_DEBUG(EFI_D_ERROR, "\tDVO config: 0x%02x\n", child->dvo_cfg);

	if (context->bdb->version < 155)
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tDVO2 Port: 0x%02x (%s)\n", child->dvo2_port, dvo_port(child->dvo2_port));
		PRINT_DEBUG(EFI_D_ERROR, "\tI2C2 pin: 0x%02x\n", child->i2c2_pin);
		PRINT_DEBUG(EFI_D_ERROR, "\tSlave2 address: 0x%02x\n", child->slave2_addr);
		PRINT_DEBUG(EFI_D_ERROR, "\tDDC2 pin: 0x%02x\n", child->ddc2_pin);
	}
	else
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tEFP routed through dock: %s\n", YESNO(child->efp_routed));
		PRINT_DEBUG(EFI_D_ERROR, "\tLane reversal: %s\n", YESNO(child->lane_reversal));
		PRINT_DEBUG(EFI_D_ERROR, "\tOnboard LSPCON: %s\n", YESNO(child->lspcon));
		PRINT_DEBUG(EFI_D_ERROR, "\tIboost enable: %s\n", YESNO(child->iboost));
		PRINT_DEBUG(EFI_D_ERROR, "\tHPD sense invert: %s\n", YESNO(child->hpd_invert));
		PRINT_DEBUG(EFI_D_ERROR, "\tHDMI compatible? %s\n", YESNO(child->hdmi_support));
		PRINT_DEBUG(EFI_D_ERROR, "\tDP compatible? %s\n", YESNO(child->dp_support));
		PRINT_DEBUG(EFI_D_ERROR, "\tTMDS compatible? %s\n", YESNO(child->tmds_support));
		PRINT_DEBUG(EFI_D_ERROR, "\tAux channel: 0x%02x\n", child->aux_channel);
		PRINT_DEBUG(EFI_D_ERROR, "\tDongle detect: 0x%02x\n", child->dongle_detect);
	}

	PRINT_DEBUG(EFI_D_ERROR, "\tPipe capabilities: 0x%02x\n", child->pipe_cap);
	PRINT_DEBUG(EFI_D_ERROR, "\tSDVO stall signal available: %s\n", YESNO(child->sdvo_stall));
	PRINT_DEBUG(EFI_D_ERROR, "\tHotplug connect status: 0x%02x\n", child->hpd_status);
	PRINT_DEBUG(EFI_D_ERROR, "\tIntegrated encoder instead of SDVO: %s\n", YESNO(child->integrated_encoder));
	PRINT_DEBUG(EFI_D_ERROR, "\tDVO wiring: 0x%02x\n", child->dvo_wiring);

	if (context->bdb->version < 171)
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tDVO2 wiring: 0x%02x\n", child->dvo2_wiring);
	}
	else
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tMIPI bridge type: %02x (%s)\n", child->mipi_bridge_type,
					mipi_bridge_type(child->mipi_bridge_type));
	}

	PRINT_DEBUG(EFI_D_ERROR, "\tDevice class extension: 0x%02x\n", child->extended_type);
	PRINT_DEBUG(EFI_D_ERROR, "\tDVO function: 0x%02x\n", child->dvo_function);

	if (context->bdb->version >= 195)
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tDP USB type C support: %s\n", YESNO(child->dp_usb_type_c));
		PRINT_DEBUG(EFI_D_ERROR, "\t2X DP GPIO index: 0x%02x\n", child->dp_gpio_index);
		PRINT_DEBUG(EFI_D_ERROR, "\t2X DP GPIO pin number: 0x%02x\n", child->dp_gpio_pin_num);
	}

	if (context->bdb->version >= 196)
	{
		PRINT_DEBUG(EFI_D_ERROR, "\tIBoost level for HDMI: 0x%02x\n", child->hdmi_iboost_level);
		PRINT_DEBUG(EFI_D_ERROR, "\tIBoost level for DP/eDP: 0x%02x\n", child->dp_iboost_level);
	}
}
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

static EFI_STATUS get_child_devices(struct context *context, const UINT8 *devices,
									UINT8 child_dev_num, UINT8 child_dev_size)
{
	struct child_device_config *child;
	struct child_device_config *children = (struct child_device_config *)AllocateZeroPool(child_dev_num * sizeof(*child));
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
		children[i] = *child;
		//dump_child_device(context, child);
	}
	context->children = children;
	context->numChildren = child_dev_num;
	FreePool(child);
	return EFI_SUCCESS;
}

static void
parse_edp(i915_CONTROLLER *controller, const struct bdb_header *bdb)
{
	const struct bdb_edp *edp;
	const struct edp_power_seq *edp_pps;
	const struct edp_fast_link_params *edp_link_params;
	int panel_type = controller->vbt.panel_type;

	edp = find_section_vbt(bdb, BDB_EDP);
	if (!edp)
		return;

	switch ((edp->color_depth >> (panel_type * 2)) & 3)
	{
	case EDP_18BPP:
		controller->vbt.edp.bpp = 18;
		break;
	case EDP_24BPP:
		controller->vbt.edp.bpp = 24;
		break;
	case EDP_30BPP:
		controller->vbt.edp.bpp = 30;
		break;
	}

	/* Get the eDP sequencing and link info */
	edp_pps = &edp->power_seqs[panel_type];
	edp_link_params = &edp->fast_link_params[panel_type];

	controller->vbt.edp.pps = *edp_pps;

	switch (edp_link_params->rate)
	{
	case EDP_RATE_1_62:
		controller->vbt.edp.rate = DP_LINK_BW_1_62;
		break;
	case EDP_RATE_2_7:
		controller->vbt.edp.rate = DP_LINK_BW_2_7;
		break;
	default:
		PRINT_DEBUG(EFI_D_ERROR, "VBT has unknown eDP link rate value %u\n",
					edp_link_params->rate);
		break;
	}

	switch (edp_link_params->lanes)
	{
	case EDP_LANE_1:
		controller->vbt.edp.lanes = 1;
		break;
	case EDP_LANE_2:
		controller->vbt.edp.lanes = 2;
		break;
	case EDP_LANE_4:
		controller->vbt.edp.lanes = 4;
		break;
	default:
		PRINT_DEBUG(EFI_D_ERROR, "VBT has unknown eDP lane count value %u\n",
					edp_link_params->lanes);
		break;
	}

	switch (edp_link_params->preemphasis)
	{
	case EDP_PREEMPHASIS_NONE:
		controller->vbt.edp.preemphasis = DP_TRAIN_PRE_EMPH_LEVEL_0;
		break;
	case EDP_PREEMPHASIS_3_5dB:
		controller->vbt.edp.preemphasis = DP_TRAIN_PRE_EMPH_LEVEL_1;
		break;
	case EDP_PREEMPHASIS_6dB:
		controller->vbt.edp.preemphasis = DP_TRAIN_PRE_EMPH_LEVEL_2;
		break;
	case EDP_PREEMPHASIS_9_5dB:
		controller->vbt.edp.preemphasis = DP_TRAIN_PRE_EMPH_LEVEL_3;
		break;
	default:
		PRINT_DEBUG(EFI_D_ERROR, "VBT has unknown eDP pre-emphasis value %u\n",
					edp_link_params->preemphasis);
		break;
	}

	switch (edp_link_params->vswing)
	{
	case EDP_VSWING_0_4V:
		controller->vbt.edp.vswing = DP_TRAIN_VOLTAGE_SWING_LEVEL_0;
		break;
	case EDP_VSWING_0_6V:
		controller->vbt.edp.vswing = DP_TRAIN_VOLTAGE_SWING_LEVEL_1;
		break;
	case EDP_VSWING_0_8V:
		controller->vbt.edp.vswing = DP_TRAIN_VOLTAGE_SWING_LEVEL_2;
		break;
	case EDP_VSWING_1_2V:
		controller->vbt.edp.vswing = DP_TRAIN_VOLTAGE_SWING_LEVEL_3;
		break;
	default:
		PRINT_DEBUG(EFI_D_ERROR, "VBT has unknown eDP voltage swing value %u\n",
					edp_link_params->vswing);
		break;
	}

	if (bdb->version >= 173)
	{
		u8 vswing;

		/* Don't read from VBT if module parameter has valid value*/
		// if (i915_modparams.edp_vswing)
		// {
		//     controller->vbt.edp.low_vswing =
		//         i915_modparams.edp_vswing == 1;
		// }
		// else
		{
			vswing = (edp->edp_vswing_preemph >> (panel_type * 4)) & 0xF;
			controller->vbt.edp.low_vswing = vswing == 0;
		}
	}
}

static enum port get_port_by_ddc_pin(i915_CONTROLLER *i915, u8 ddc_pin)
{
	const struct ddi_vbt_port_info *info;
	enum port port;

	for_each_port(port)
	{
		info = &i915->vbt.ddi_port_info[port];

		if (info->child && ddc_pin == info->alternate_ddc_pin)
			return port;
	}

	return PORT_NONE;
}

static void sanitize_ddc_pin(i915_CONTROLLER *controller,
							 enum port port)
{
	struct ddi_vbt_port_info *info = &controller->vbt.ddi_port_info[port];
	enum port p;

	if (!info->alternate_ddc_pin)
		return;

	p = get_port_by_ddc_pin(controller, info->alternate_ddc_pin);
	if (p != PORT_NONE)
	{
		PRINT_DEBUG(EFI_D_ERROR,
					"port %c trying to use the same DDC pin (0x%x) as port %c, "
					"disabling port %c DVI/HDMI support\n",
					port_name(port), info->alternate_ddc_pin,
					port_name(p), port_name(p));

		/*
		 * If we have multiple ports supposedly sharing the
		 * pin, then dvi/hdmi couldn't exist on the shared
		 * port. Otherwise they share the same ddc bin and
		 * system couldn't communicate with them separately.
		 *
		 * Give inverse child device order the priority,
		 * last one wins. Yes, there are real machines
		 * (eg. Asrock B250M-HDV) where VBT has both
		 * port A and port E with the same AUX ch and
		 * we must pick port E :(
		 */
		info = &controller->vbt.ddi_port_info[p];

		info->supports_dvi = false;
		info->supports_hdmi = false;
		info->alternate_ddc_pin = 0;
	}
}

static enum port get_port_by_aux_ch(i915_CONTROLLER *i915, u8 aux_ch)
{
	const struct ddi_vbt_port_info *info;
	enum port port;

	for_each_port(port)
	{
		info = &i915->vbt.ddi_port_info[port];

		if (info->child && aux_ch == info->alternate_aux_channel)
			return port;
	}

	return PORT_NONE;
}

static void sanitize_aux_ch(i915_CONTROLLER *controller,
							enum port port)
{
	struct ddi_vbt_port_info *info = &controller->vbt.ddi_port_info[port];
	enum port p;

	if (!info->alternate_aux_channel)
		return;

	p = get_port_by_aux_ch(controller, info->alternate_aux_channel);
	if (p != PORT_NONE)
	{
		PRINT_DEBUG(EFI_D_ERROR,
					"port %c trying to use the same AUX CH (0x%x) as port %c, disabling port %c DP support\n",
					port_name(port), info->alternate_aux_channel,
					port_name(p), port_name(p));

		/*
		 * If we have multiple ports supposedlt sharing the
		 * aux channel, then DP couldn't exist on the shared
		 * port. Otherwise they share the same aux channel
		 * and system couldn't communicate with them separately.
		 *
		 * Give inverse child device order the priority,
		 * last one wins. Yes, there are real machines
		 * (eg. Asrock B250M-HDV) where VBT has both
		 * port A and port E with the same AUX ch and
		 * we must pick port E :(
		 */
		info = &controller->vbt.ddi_port_info[p];

		info->supports_dp = false;
		info->alternate_aux_channel = 0;
	}
}

// static const u8 cnp_ddc_pin_map[] = {
// 	[0] = 0, /* N/A */
// 	[DDC_BUS_DDI_B] = GMBUS_PIN_1_BXT,
// 	[DDC_BUS_DDI_C] = GMBUS_PIN_2_BXT,
// 	[DDC_BUS_DDI_D] = GMBUS_PIN_4_CNP, /* sic */
// 	[DDC_BUS_DDI_F] = GMBUS_PIN_3_BXT, /* sic */
// };

/* static const u8 icp_ddc_pin_map[] = {
	[ICL_DDC_BUS_DDI_A] = GMBUS_PIN_1_BXT,
	[ICL_DDC_BUS_DDI_B] = GMBUS_PIN_2_BXT,
	[TGL_DDC_BUS_DDI_C] = GMBUS_PIN_3_BXT,
	[ICL_DDC_BUS_PORT_1] = GMBUS_PIN_9_TC1_ICP,
	[ICL_DDC_BUS_PORT_2] = GMBUS_PIN_10_TC2_ICP,
	[ICL_DDC_BUS_PORT_3] = GMBUS_PIN_11_TC3_ICP,
	[ICL_DDC_BUS_PORT_4] = GMBUS_PIN_12_TC4_ICP,
	[TGL_DDC_BUS_PORT_5] = GMBUS_PIN_13_TC5_TGP,
	[TGL_DDC_BUS_PORT_6] = GMBUS_PIN_14_TC6_TGP,
}; */

static u8 map_ddc_pin(i915_CONTROLLER *controller, u8 vbt_pin)
{
	return vbt_pin;
}

static enum port __dvo_port_to_port(int n_ports, int n_dvo,
									const int port_mapping[][3], u8 dvo_port)
{
	enum port port;
	int i;

	for (port = PORT_A; port < n_ports; port++)
	{
		for (i = 0; i < n_dvo; i++)
		{
			if (port_mapping[port][i] == -1)
				break;

			if (dvo_port == port_mapping[port][i])
				return port;
		}
	}

	return PORT_NONE;
}
static u8 translate_iboost(u8 val)
{
	static const u8 mapping[] = {1, 3, 7}; /* See VBT spec */

	if (val >= ARRAY_SIZE(mapping))
	{
		PRINT_DEBUG(EFI_D_ERROR, "Unsupported I_boost value found in VBT (%d), display may not work properly\n", val);
		return 0;
	}
	return mapping[val];
}
static enum port dvo_port_to_port(i915_CONTROLLER *controller,
								  u8 dvo_port)
{
	/*
	 * Each DDI port can have more than one value on the "DVO Port" field,
	 * so look for all the possible values for each port.
	 */
	static const int port_mapping[][3] = {
		[PORT_A] = {DVO_PORT_HDMIA, DVO_PORT_DPA, -1},
		[PORT_B] = {DVO_PORT_HDMIB, DVO_PORT_DPB, -1},
		[PORT_C] = {DVO_PORT_HDMIC, DVO_PORT_DPC, -1},
		[PORT_D] = {DVO_PORT_HDMID, DVO_PORT_DPD, -1},
		[PORT_E] = {DVO_PORT_HDMIE, DVO_PORT_DPE, DVO_PORT_CRT},
		[PORT_F] = {DVO_PORT_HDMIF, DVO_PORT_DPF, -1},
		[PORT_G] = {DVO_PORT_HDMIG, DVO_PORT_DPG, -1},
		[PORT_H] = {DVO_PORT_HDMIH, DVO_PORT_DPH, -1},
		[PORT_I] = {DVO_PORT_HDMII, DVO_PORT_DPI, -1},
	};
	/*
	 * Bspec lists the ports as A, B, C, D - however internally in our
	 * driver we keep them as PORT_A, PORT_B, PORT_D and PORT_E so the
	 * registers in Display Engine match the right offsets. Apply the
	 * mapping here to translate from VBT to internal convention.
	 */
	/* 	static const int rkl_port_mapping[][3] = {
		[PORT_A] = { DVO_PORT_HDMIA, DVO_PORT_DPA, -1 },
		[PORT_B] = { DVO_PORT_HDMIB, DVO_PORT_DPB, -1 },
		[PORT_C] = { -1 },
		[PORT_D] = { DVO_PORT_HDMIC, DVO_PORT_DPC, -1 },
		[PORT_E] = { DVO_PORT_HDMID, DVO_PORT_DPD, -1 },
	};
 */

	return __dvo_port_to_port(ARRAY_SIZE(port_mapping),
							  ARRAY_SIZE(port_mapping[0]),
							  port_mapping,
							  dvo_port);
}

static void parse_ddi_port(i915_CONTROLLER *controller,
						   const struct child_device_config *child,
						   UINT8 bdb_version)
{
	struct ddi_vbt_port_info *info;
	BOOLEAN is_dvi, is_hdmi, is_dp, is_edp, is_crt;
	enum port port;
	port = dvo_port_to_port(controller, child->dvo_port);
	if (port == PORT_NONE)
		return;

	info = &controller->vbt.ddi_port_info[port];
	info->port = port;

	if (info->child)
	{
		PRINT_DEBUG(EFI_D_ERROR,
					"More than one child device for port %c in VBT, using the first.\n",
					port_name(port));
		return;
	}

	is_dvi = (child->device_type & DEVICE_TYPE_TMDS_DVI_SIGNALING) != 0;
	is_dp = (child->device_type & DEVICE_TYPE_DISPLAYPORT_OUTPUT) != 0;
	is_crt = (child->device_type & DEVICE_TYPE_ANALOG_OUTPUT) != 0;
	is_hdmi = is_dvi && ((child->device_type & DEVICE_TYPE_NOT_HDMI_OUTPUT) == 0);
	is_edp = is_dp && ((child->device_type & DEVICE_TYPE_INTERNAL_CONNECTOR) != 0);

	if (port == PORT_A && is_dvi)
	{
		PRINT_DEBUG(EFI_D_ERROR,
					"VBT claims port A supports DVI%s, ignoring\n",
					is_hdmi ? "/HDMI" : "");
		is_dvi = FALSE;
		is_hdmi = FALSE;
	}

	info->supports_dvi = is_dvi;
	info->supports_hdmi = is_hdmi;
	info->supports_dp = is_dp;
	info->supports_edp = is_edp;

	if (bdb_version >= 195)
		info->supports_typec_usb = child->dp_usb_type_c;

	if (bdb_version >= 209)
		info->supports_tbt = child->tbt;

	PRINT_DEBUG(EFI_D_ERROR,
				"Port %c VBT info: CRT:%d DVI:%d HDMI:%d DP:%d eDP:%d USB-Type-C:%d TBT:%d type:%04x\n",
				port_name(port), is_crt, is_dvi, is_hdmi, is_dp, is_edp,
				info->supports_typec_usb, info->supports_tbt, child->device_type);

	if (is_dvi)
	{
		UINT8 ddc_pin;

		ddc_pin = map_ddc_pin(controller, child->ddc_pin);
		//	if (intel_gmbus_is_valid_pin(controller, ddc_pin)) {
		info->alternate_ddc_pin = ddc_pin;
		sanitize_ddc_pin(controller, port);
		/* } else {
			DebugPrint(EFI_D_ERROR, 
				    "Port %c has invalid DDC pin %d, "
				    "sticking to defaults\n",
				    port_name(port), ddc_pin);
		} */
	}

	if (is_dp)
	{
		info->alternate_aux_channel = child->aux_channel;

		sanitize_aux_ch(controller, port);
	}

	if (bdb_version >= 158)
	{
		/* The VBT HDMI level shift values match the table we have. */
		UINT8 hdmi_level_shift = child->hdmi_level_shifter_value;
		PRINT_DEBUG(EFI_D_ERROR,
					"VBT HDMI level shift for port %c: %d\n",
					port_name(port),
					hdmi_level_shift);
		info->hdmi_level_shift = hdmi_level_shift;
		info->hdmi_level_shift_set = TRUE;
	}

	/* 	if (bdb_version >= 204) {
		int max_tmds_clock;

		switch (child->hdmi_max_data_rate) {
		default:
			MISSING_CASE(child->hdmi_max_data_rate);
			fallthrough;
		case HDMI_MAX_DATA_RATE_PLATFORM:
			max_tmds_clock = 0;
			break;
		case HDMI_MAX_DATA_RATE_297:
			max_tmds_clock = 297000;
			break;
		case HDMI_MAX_DATA_RATE_165:
			max_tmds_clock = 165000;
			break;
		}

		if (max_tmds_clock)
			DebugPrint(EFI_D_ERROR, 
				    "VBT HDMI max TMDS clock for port %c: %d kHz\n",
				    port_name(port), max_tmds_clock);
		info->max_tmds_clock = max_tmds_clock;
	} */

	/* Parse the I_boost config for SKL and above */
	if (bdb_version >= 196 && child->iboost)
	{
		info->dp_boost_level = translate_iboost(child->dp_iboost_level);
		PRINT_DEBUG(EFI_D_ERROR,
					"VBT (e)DP boost level for port %c: %d\n",
					port_name(port), info->dp_boost_level);
		info->hdmi_boost_level = translate_iboost(child->hdmi_iboost_level);
		PRINT_DEBUG(EFI_D_ERROR,
					"VBT HDMI boost level for port %c: %d\n",
					port_name(port), info->hdmi_boost_level);
	}

	/* DP max link rate for CNL+ */
	if (bdb_version >= 216)
	{
		switch (child->dp_max_link_rate)
		{
		default:
		case VBT_DP_MAX_LINK_RATE_HBR3:
			info->dp_max_link_rate = 810000;
			break;
		case VBT_DP_MAX_LINK_RATE_HBR2:
			info->dp_max_link_rate = 540000;
			break;
		case VBT_DP_MAX_LINK_RATE_HBR:
			info->dp_max_link_rate = 270000;
			break;
		case VBT_DP_MAX_LINK_RATE_LBR:
			info->dp_max_link_rate = 162000;
			break;
		}
		PRINT_DEBUG(EFI_D_ERROR,
					"VBT DP max link rate for port %c: %d\n",
					port_name(port), info->dp_max_link_rate);
	}

	info->child = child;
}

void parse_ddi_ports(i915_CONTROLLER *controller, UINT8 bdb_version)
{
	/* 	struct display_device_data *devdata;

	if (!HAS_DDI(controller) && !IS_CHERRYVIEW(controller))
		return;
 */
	if (bdb_version < 155)
		return;
	struct context context;
	struct bdb_header bdb;
	bdb.version = bdb_version;
	context.bdb = &bdb;
	for (int i = 0; i < controller->opRegion->numChildren; i++)
	{
		dump_child_device(&context, &controller->opRegion->children[i]);
		parse_ddi_port(controller, &controller->opRegion->children[i], bdb_version); //TODO Update to dyn version
	}
	//list_for_each_entry(devdata, &controller->vbt.display_devices, node)
}
/* Common defaults which may be overridden by VBT. */
static void
init_vbt_defaults(i915_CONTROLLER *controller)
{
	//enum port port;

	// controller->vbt.crt_ddc_pin = GMBUS_PIN_VGADDC;

	// /* Default to having backlight */
	// controller->vbt.backlight.present = true;

	// /* LFP panel data */
	// controller->vbt.lvds_dither = 1;

	// /* SDVO panel data */
	// controller->vbt.sdvo_lvds_vbt_mode = NULL;

	// /* general features */
	// controller->vbt.int_tv_support = 1;
	// controller->vbt.int_crt_support = 1;

	// /* driver features */
	// controller->vbt.int_lvds_support = 1;

	// /* Default to using SSC */
	// controller->vbt.lvds_use_ssc = 1;
	// /*
	//  * Core/SandyBridge/IvyBridge use alternative (120MHz) reference
	//  * clock for LVDS.
	//  */
	// controller->vbt.lvds_ssc_freq = intel_bios_ssc_frequency(controller,
	// 														 !HAS_PCH_SPLIT(controller));
	// PRINT_DEBUG(EFI_D_ERROR,"Set default to SSC at %d kHz\n", controller->vbt.lvds_ssc_freq);

	// for (port = PORT_A; port < I915_MAX_PORTS; port++)
	// {
	// 	struct ddi_vbt_port_info *info =
	// 		&controller->vbt.ddi_port_info[port];

	// 	info->hdmi_level_shift = HDMI_LEVEL_SHIFT_UNKNOWN;
	// }
}
static const struct bdb_header *get_bdb_header(const struct vbt_header *vbt)
{
	const void *_vbt = vbt;

	return _vbt + vbt->bdb_offset;
}

/**
 * intel_bios_init - find VBT and initialize settings from the BIOS
 * @controller: i915 device instance
 *
 * Parse and initialize settings from the Video BIOS Tables (VBT). If the VBT
 * was not found in ACPI OpRegion, try to find it in PCI ROM first. Also
 * initialize some defaults if the VBT is not present at all.
 */
void intel_bios_init(i915_CONTROLLER *controller)
{
	//struct pci_dev *pdev = controller->drm.pdev;
	const struct vbt_header *vbt = controller->opRegion->vbt;
	const struct bdb_header *bdb;
	//	u8 *bios = NULL;

	// if (!HAS_DISPLAY(controller))
	// {
	//     PRINT_DEBUG(EFI_D_ERROR,"Skipping VBT init due to disabled display.\n");
	//     return;
	// }

	init_vbt_defaults(controller);

	// /* If the OpRegion does not have VBT, look in PCI ROM. */
	// if (!vbt)
	// {
	//     size_t size;

	//     bios = pci_map_rom(pdev, &size);
	//     if (!bios)
	//         goto out;

	//     vbt = find_vbt(bios, size);
	//     if (!vbt)
	//         goto out;

	//     PRINT_DEBUG(EFI_D_ERROR,"Found valid VBT in PCI ROM\n");
	// }

	bdb = get_bdb_header(vbt);

	PRINT_DEBUG(EFI_D_ERROR, "VBT signature \"%.*s\", BDB version %d\n",
				(int)sizeof(vbt->signature), vbt->signature, bdb->version);

	/* Grab useful general definitions */
	// parse_general_features(controller, bdb);
	//  parse_general_definitions(controller, bdb);
	//  parse_lfp_panel_data(controller, bdb);
	//   parse_lfp_backlight(controller, bdb);
	//   parse_sdvo_panel_data(controller, bdb);
	//  parse_driver_features(controller, bdb);
	parse_edp(controller, bdb);
	//parse_psr(controller, bdb);
	// parse_mipi_config(controller, bdb);
	// parse_mipi_sequence(controller, bdb);

	/* Further processing on pre-parsed data */
	// parse_sdvo_device_mapping(controller, bdb->version);
	parse_ddi_ports(controller, bdb->version);

	if (!vbt)
	{
		PRINT_DEBUG(EFI_D_ERROR, "Failed to find VBIOS tables");
	}
	// if (!vbt)
	// {
	//     DRM_INFO("Failed to find VBIOS tables (VBT)\n");
	//     init_vbt_missing_defaults(controller);
	// }

	// if (bios)
	//     pci_unmap_rom(pdev, bios);
}
static EFI_STATUS decode_vbt_child_blocks(struct context *context,
										  const struct bdb_block *block)
{
	EFI_STATUS status = EFI_SUCCESS;
	const struct bdb_general_definitions *defs = block->data;
	int child_dev_num;

	child_dev_num = (block->size - sizeof(*defs)) / defs->child_dev_size;
	/* 
	PRINT_DEBUG(EFI_D_ERROR,"CRT DDC GMBUS addr: 0x%02x\n", defs->crt_ddc_gmbus_pin);
	PRINT_DEBUG(EFI_D_ERROR,"Use ACPI DPMS CRT power states: %s\n",
			   YESNO(defs->dpms_acpi));
	PRINT_DEBUG(EFI_D_ERROR,"Skip CRT detect at boot: %s\n",
			   YESNO(defs->skip_boot_crt_detect));
	PRINT_DEBUG(EFI_D_ERROR,"Use DPMS on AIM devices: %s\n", YESNO(defs->dpms_aim)); */
	if (block->id == BDB_GENERAL_DEFINITIONS)
	{
		PRINT_DEBUG(EFI_D_ERROR, "Boot display type: 0x%02x%02x\n", defs->boot_display[1],
					defs->boot_display[0]);
	}
	PRINT_DEBUG(EFI_D_ERROR, "Child device size: %d\n", defs->child_dev_size);
	PRINT_DEBUG(EFI_D_ERROR, "Child device count: %d\n", child_dev_num);

	status = get_child_devices(context, defs->devices,
							   child_dev_num, defs->child_dev_size);
	int i;

	for (i = 0; i < child_dev_num; i++)
	{

		//children[i] = *child;
		dump_child_device(context, &(context->children[i]));
	}
	return status;
}

EFI_STATUS decodeVBT(struct intel_opregion *opRegion, int vbt_off)
{
	struct vbt_header *vbt = opRegion->vbt;
	UINT8 *VBIOS = (UINT8 *)opRegion->header;
	EFI_STATUS status = EFI_SUCCESS;
	//UINT8 *VBIOS;
	//	int index;
	//	int fd;
	//struct vbt_header *vbt = NULL;
	int bdb_off;
	//	const char *filename = NULL;
	//int size = 8192;
	struct context context = {
		.panel_type = -1,
	};

	context.vbt = vbt;
	bdb_off = vbt_off + vbt->bdb_offset;

	context.bdb = (const struct bdb_header *)(VBIOS + bdb_off);
	context.size = 8192;
	PRINT_DEBUG(EFI_D_ERROR, "vbt: 0x%04x, bdb: 0x%04x, sig: %s, bsig: %s \n", context.vbt, context.bdb, context.vbt->signature, context.bdb->signature);

	struct bdb_block *block = (struct bdb_block *)AllocatePool(sizeof(block));

	status = find_section(&context, BDB_GENERAL_DEFINITIONS, block);
	if (!EFI_ERROR(status))
	{
		status = decode_vbt_child_blocks(&context, block);
	}
	else
	{
		status = find_section(&context, BDB_CHILD_DEVICE_TABLE, block);
		if (!EFI_ERROR(status))
		{

			status = decode_vbt_child_blocks(&context, block);
		}
	}
	if (!EFI_ERROR(status))
	{
		opRegion->children = context.children;
		opRegion->numChildren = context.numChildren;
	}
	return status;
}

enum aux_ch intel_bios_port_aux_ch(i915_CONTROLLER *dev_priv,
								   enum port port)
{
	const struct ddi_vbt_port_info *info =
		&dev_priv->vbt.ddi_port_info[port];
	enum aux_ch aux_ch;

	if (!info->alternate_aux_channel)
	{
		aux_ch = (enum aux_ch)port;

		PRINT_DEBUG(EFI_D_ERROR,
					"using AUX %c for port %c (platform default)\n",
					aux_ch_name(aux_ch), port_name(port));
		return aux_ch;
	}

	switch (info->alternate_aux_channel)
	{
	case DP_AUX_A:
		aux_ch = AUX_CH_A;
		break;
	case DP_AUX_B:
		aux_ch = AUX_CH_B;
		break;
	case DP_AUX_C:
		aux_ch = AUX_CH_C;
		break;
	case DP_AUX_D:
		aux_ch = AUX_CH_D;
		break;
	case DP_AUX_E:
		aux_ch = AUX_CH_E;
		break;
	case DP_AUX_F:
		aux_ch = AUX_CH_F;
		break;
	case DP_AUX_G:
		aux_ch = AUX_CH_G;
		break;
	// case DP_AUX_H:
	// 	aux_ch = AUX_CH_H;
	// 	break;
	// case DP_AUX_I:
	// 	aux_ch = AUX_CH_I;
	// 	break;
	default:
		aux_ch = AUX_CH_A;
		break;
	}

	PRINT_DEBUG(EFI_D_ERROR, "using AUX %c for port %c (VBT)\n",
				aux_ch_name(aux_ch), port_name(port));

	return aux_ch;
}