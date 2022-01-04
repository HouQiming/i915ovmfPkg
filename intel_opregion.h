/*
 * Copyright 2008 Intel Corporation <hong.liu@intel.com>
 * Copyright 2008 Red Hat <mjg@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT.  IN NO EVENT SHALL INTEL AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <Protocol/DevicePath.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Uefi.h>
#include <Library/MemoryAllocationLib.h>

#include "QemuFwCfgLib.h"
#include "i915_display.h"
#include "i915ovmf.h"
#include <IndustryStandard/Acpi.h>
#include <IndustryStandard/Pci.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include "i915_debug.h"
#include <Library/DevicePathLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiLib.h>
#include "i915_controller.h"

#ifndef INTEL_OPREGION
#define INTEL_OPREGION

#define OPREGION_HEADER_OFFSET 0
#define OPREGION_ACPI_OFFSET 0x100
#define ACPI_CLID 0x01ac /* current lid state indicator */
#define ACPI_CDCK 0x01b0 /* current docking state indicator */
#define OPREGION_SWSCI_OFFSET 0x200
#define OPREGION_ASLE_OFFSET 0x300
#define OPREGION_VBT_OFFSET 0x400
#define OPREGION_ASLE_EXT_OFFSET 0x1C00

#define OPREGION_SIGNATURE "IntelGraphicsMem"
#define MBOX_ACPI (1 << 0)
#define MBOX_SWSCI (1 << 1)
#define MBOX_ASLE (1 << 2)
#define MBOX_ASLE_EXT (1 << 4)

struct context
{
	const struct vbt_header *vbt;
	const struct bdb_header *bdb;
	int size;
	struct child_device_config *children;
	UINT8 numChildren;
	UINT32 devid;
	int panel_type;
	BOOLEAN dump_all_panel_types;
	BOOLEAN hexdump;
};

struct bdb_block
{
	UINT8 id;
	UINT32 size;
	const void *data;
};

struct dumper
{
	UINT8 id;
	const char *name;
	void (*dump)(struct context *context,
				 const struct bdb_block *block);
};

enum bdb_block_id
{
	BDB_GENERAL_FEATURES = 1,
	BDB_GENERAL_DEFINITIONS = 2,
	BDB_OLD_TOGGLE_LIST = 3,
	BDB_MODE_SUPPORT_LIST = 4,
	BDB_GENERIC_MODE_TABLE = 5,
	BDB_EXT_MMIO_REGS = 6,
	BDB_SWF_IO = 7,
	BDB_SWF_MMIO = 8,
	BDB_PSR = 9,
	BDB_MODE_REMOVAL_TABLE = 10,
	BDB_CHILD_DEVICE_TABLE = 11,
	BDB_DRIVER_FEATURES = 12,
	BDB_DRIVER_PERSISTENCE = 13,
	BDB_EXT_TABLE_PTRS = 14,
	BDB_DOT_CLOCK_OVERRIDE = 15,
	BDB_DISPLAY_SELECT = 16,
	BDB_DRIVER_ROTATION = 18,
	BDB_DISPLAY_REMOVE = 19,
	BDB_OEM_CUSTOM = 20,
	BDB_EFP_LIST = 21, /* workarounds for VGA hsync/vsync */
	BDB_SDVO_LVDS_OPTIONS = 22,
	BDB_SDVO_PANEL_DTDS = 23,
	BDB_SDVO_LVDS_PNP_IDS = 24,
	BDB_SDVO_LVDS_POWER_SEQ = 25,
	BDB_TV_OPTIONS = 26,
	BDB_EDP = 27,
	BDB_LVDS_OPTIONS = 40,
	BDB_LVDS_LFP_DATA_PTRS = 41,
	BDB_LVDS_LFP_DATA = 42,
	BDB_LVDS_BACKLIGHT = 43,
	BDB_LVDS_POWER = 44,
	BDB_MIPI_CONFIG = 52,
	BDB_MIPI_SEQUENCE = 53,
	BDB_COMPRESSION_PARAMETERS = 56,
	BDB_SKIP = 254, /* VBIOS private block, ignore */
};
#define port_name(p) ((p) + 'A')

#define for_each_port(__port) \
	for ((__port) = PORT_A; (__port) < I915_MAX_PORTS; (__port)++)

/* Get to bdb section of vbt. THen Scan through to read off the ids of the blocks until we find general definitions or legacy child devices. THen read them
*
*/
struct bdb_legacy_child_devices
{
	UINT8 child_dev_size;
	UINT8 devices[0]; /* presumably 7 * 33 */
} __attribute__((packed));
/* Driver readiness indicator */
#define ASLE_ARDY_READY (1 << 0)
#define ASLE_ARDY_NOT_READY (0 << 0)

/* ASLE Interrupt Command (ASLC) bits */
#define ASLC_SET_ALS_ILLUM (1 << 0)
#define ASLC_SET_BACKLIGHT (1 << 1)
#define ASLC_SET_PFIT (1 << 2)
#define ASLC_SET_PWM_FREQ (1 << 3)
#define ASLC_SUPPORTED_ROTATION_ANGLES (1 << 4)
#define ASLC_BUTTON_ARRAY (1 << 5)
#define ASLC_CONVERTIBLE_INDICATOR (1 << 6)
#define ASLC_DOCKING_INDICATOR (1 << 7)
#define ASLC_ISCT_STATE_CHANGE (1 << 8)
#define ASLC_REQ_MSK 0x1ff
/* response bits */
#define ASLC_ALS_ILLUM_FAILED (1 << 10)
#define ASLC_BACKLIGHT_FAILED (1 << 12)
#define ASLC_PFIT_FAILED (1 << 14)
#define ASLC_PWM_FREQ_FAILED (1 << 16)
#define ASLC_ROTATION_ANGLES_FAILED (1 << 18)
#define ASLC_BUTTON_ARRAY_FAILED (1 << 20)
#define ASLC_CONVERTIBLE_FAILED (1 << 22)
#define ASLC_DOCKING_FAILED (1 << 24)
#define ASLC_ISCT_STATE_FAILED (1 << 26)

/* Technology enabled indicator */
#define ASLE_TCHE_ALS_EN (1 << 0)
#define ASLE_TCHE_BLC_EN (1 << 1)
#define ASLE_TCHE_PFIT_EN (1 << 2)
#define ASLE_TCHE_PFMB_EN (1 << 3)

/* ASLE backlight brightness to set */
#define ASLE_BCLP_VALID (1 << 31)
#define ASLE_BCLP_MSK (~(1 << 31))

/* ASLE panel fitting request */
#define ASLE_PFIT_VALID (1 << 31)
#define ASLE_PFIT_CENTER (1 << 0)
#define ASLE_PFIT_STRETCH_TEXT (1 << 1)
#define ASLE_PFIT_STRETCH_GFX (1 << 2)

/* PWM frequency and minimum brightness */
#define ASLE_PFMB_BRIGHTNESS_MASK (0xff)
#define ASLE_PFMB_BRIGHTNESS_VALID (1 << 8)
#define ASLE_PFMB_PWM_MASK (0x7ffffe00)
#define ASLE_PFMB_PWM_VALID (1 << 31)

#define ASLE_CBLV_VALID (1 << 31)

/* IUER */
#define ASLE_IUER_DOCKING (1 << 7)
#define ASLE_IUER_CONVERTIBLE (1 << 6)
#define ASLE_IUER_ROTATION_LOCK_BTN (1 << 4)
#define ASLE_IUER_VOLUME_DOWN_BTN (1 << 3)
#define ASLE_IUER_VOLUME_UP_BTN (1 << 2)
#define ASLE_IUER_WINDOWS_BTN (1 << 1)
#define ASLE_IUER_POWER_BTN (1 << 0)

/* Software System Control Interrupt (SWSCI) */
#define SWSCI_SCIC_INDICATOR (1 << 0)
#define SWSCI_SCIC_MAIN_FUNCTION_SHIFT 1
#define SWSCI_SCIC_MAIN_FUNCTION_MASK (0xf << 1)
#define SWSCI_SCIC_SUB_FUNCTION_SHIFT 8
#define SWSCI_SCIC_SUB_FUNCTION_MASK (0xff << 8)
#define SWSCI_SCIC_EXIT_PARAMETER_SHIFT 8
#define SWSCI_SCIC_EXIT_PARAMETER_MASK (0xff << 8)
#define SWSCI_SCIC_EXIT_STATUS_SHIFT 5
#define SWSCI_SCIC_EXIT_STATUS_MASK (7 << 5)
#define SWSCI_SCIC_EXIT_STATUS_SUCCESS 1

#define SWSCI_FUNCTION_CODE(main, sub)          \
	((main) << SWSCI_SCIC_MAIN_FUNCTION_SHIFT | \
	 (sub) << SWSCI_SCIC_SUB_FUNCTION_SHIFT)

/* SWSCI: Get BIOS Data (GBDA) */
#define SWSCI_GBDA 4
#define SWSCI_GBDA_SUPPORTED_CALLS SWSCI_FUNCTION_CODE(SWSCI_GBDA, 0)
#define SWSCI_GBDA_REQUESTED_CALLBACKS SWSCI_FUNCTION_CODE(SWSCI_GBDA, 1)
#define SWSCI_GBDA_BOOT_DISPLAY_PREF SWSCI_FUNCTION_CODE(SWSCI_GBDA, 4)
#define SWSCI_GBDA_PANEL_DETAILS SWSCI_FUNCTION_CODE(SWSCI_GBDA, 5)
#define SWSCI_GBDA_TV_STANDARD SWSCI_FUNCTION_CODE(SWSCI_GBDA, 6)
#define SWSCI_GBDA_INTERNAL_GRAPHICS SWSCI_FUNCTION_CODE(SWSCI_GBDA, 7)
#define SWSCI_GBDA_SPREAD_SPECTRUM SWSCI_FUNCTION_CODE(SWSCI_GBDA, 10)

/* SWSCI: System BIOS Callbacks (SBCB) */
#define SWSCI_SBCB 6
#define SWSCI_SBCB_SUPPORTED_CALLBACKS SWSCI_FUNCTION_CODE(SWSCI_SBCB, 0)
#define SWSCI_SBCB_INIT_COMPLETION SWSCI_FUNCTION_CODE(SWSCI_SBCB, 1)
#define SWSCI_SBCB_PRE_HIRES_SET_MODE SWSCI_FUNCTION_CODE(SWSCI_SBCB, 3)
#define SWSCI_SBCB_POST_HIRES_SET_MODE SWSCI_FUNCTION_CODE(SWSCI_SBCB, 4)
#define SWSCI_SBCB_DISPLAY_SWITCH SWSCI_FUNCTION_CODE(SWSCI_SBCB, 5)
#define SWSCI_SBCB_SET_TV_FORMAT SWSCI_FUNCTION_CODE(SWSCI_SBCB, 6)
#define SWSCI_SBCB_ADAPTER_POWER_STATE SWSCI_FUNCTION_CODE(SWSCI_SBCB, 7)
#define SWSCI_SBCB_DISPLAY_POWER_STATE SWSCI_FUNCTION_CODE(SWSCI_SBCB, 8)
#define SWSCI_SBCB_SET_BOOT_DISPLAY SWSCI_FUNCTION_CODE(SWSCI_SBCB, 9)
#define SWSCI_SBCB_SET_PANEL_DETAILS SWSCI_FUNCTION_CODE(SWSCI_SBCB, 10)
#define SWSCI_SBCB_SET_INTERNAL_GFX SWSCI_FUNCTION_CODE(SWSCI_SBCB, 11)
#define SWSCI_SBCB_POST_HIRES_TO_DOS_FS SWSCI_FUNCTION_CODE(SWSCI_SBCB, 16)
#define SWSCI_SBCB_SUSPEND_RESUME SWSCI_FUNCTION_CODE(SWSCI_SBCB, 17)
#define SWSCI_SBCB_SET_SPREAD_SPECTRUM SWSCI_FUNCTION_CODE(SWSCI_SBCB, 18)
#define SWSCI_SBCB_POST_VBE_PM SWSCI_FUNCTION_CODE(SWSCI_SBCB, 19)
#define SWSCI_SBCB_ENABLE_DISABLE_AUDIO SWSCI_FUNCTION_CODE(SWSCI_SBCB, 21)

#define MAX_DSLP 1500

#define OPREGION_SIZE (8 * 1024)

/*
 * Copyright © 2006-2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

/*
 * This information is private to VBT parsing in intel_bios.c.
 *
 * Please do NOT include anywhere else.
 */

/*
 * There are several types of BIOS data blocks (BDBs), each block has
 * an ID and size in the first 3 bytes (ID in first, size in next 2).
 * Known types are listed below.
 */

/*
 * Block 1 - General Bit Definitions
 */

struct bdb_general_features
{
	/* bits 1 */
	UINT8 panel_fitting : 2;
	UINT8 flexaim : 1;
	UINT8 msg_enable : 1;
	UINT8 clear_screen : 3;
	UINT8 color_flip : 1;

	/* bits 2 */
	UINT8 download_ext_vbt : 1;
	UINT8 enable_ssc : 1;
	UINT8 ssc_freq : 1;
	UINT8 enable_lfp_on_override : 1;
	UINT8 disable_ssc_ddt : 1;
	UINT8 underscan_vga_timings : 1;
	UINT8 display_clock_mode : 1;
	UINT8 vbios_hotplug_support : 1;

	/* bits 3 */
	UINT8 disable_smooth_vision : 1;
	UINT8 single_dvi : 1;
	UINT8 rotate_180 : 1; /* 181 */
	UINT8 fdi_rx_polarity_inverted : 1;
	UINT8 vbios_extended_mode : 1;			  /* 160 */
	UINT8 copy_ilfp_dtd_to_sdvo_lvds_dtd : 1; /* 160 */
	UINT8 panel_best_fit_timing : 1;		  /* 160 */
	UINT8 ignore_strap_state : 1;			  /* 160 */

	/* bits 4 */
	UINT8 legacy_monitor_detect;

	/* bits 5 */
	UINT8 int_crt_support : 1;
	UINT8 int_tv_support : 1;
	UINT8 int_efp_support : 1;
	UINT8 dp_ssc_enable : 1; /* PCH attached eDP supports SSC */
	UINT8 dp_ssc_freq : 1;	 /* SSC freq for PCH attached eDP */
	UINT8 dp_ssc_dongle_supported : 1;
	UINT8 rsvd11 : 2; /* finish byte */
} __packed;

/*
 * Block 2 - General Bytes Definition
 */

/* pre-915 */
#define GPIO_PIN_DVI_LVDS 0x03	  /* "DVI/LVDS DDC GPIO pins" */
#define GPIO_PIN_ADD_I2C 0x05	  /* "ADDCARD I2C GPIO pins" */
#define GPIO_PIN_ADD_DDC 0x04	  /* "ADDCARD DDC GPIO pins" */
#define GPIO_PIN_ADD_DDC_I2C 0x06 /* "ADDCARD DDC/I2C GPIO pins" */

/* Pre 915 */
#define DEVICE_TYPE_NONE 0x00
#define DEVICE_TYPE_CRT 0x01
#define DEVICE_TYPE_TV 0x09
#define DEVICE_TYPE_EFP 0x12
#define DEVICE_TYPE_LFP 0x22
/* On 915+ */
#define DEVICE_TYPE_CRT_DPMS 0x6001
#define DEVICE_TYPE_CRT_DPMS_HOTPLUG 0x4001
#define DEVICE_TYPE_TV_COMPOSITE 0x0209
#define DEVICE_TYPE_TV_MACROVISION 0x0289
#define DEVICE_TYPE_TV_RF_COMPOSITE 0x020c
#define DEVICE_TYPE_TV_SVIDEO_COMPOSITE 0x0609
#define DEVICE_TYPE_TV_SCART 0x0209
#define DEVICE_TYPE_TV_CODEC_HOTPLUG_PWR 0x6009
#define DEVICE_TYPE_EFP_HOTPLUG_PWR 0x6012
#define DEVICE_TYPE_EFP_DVI_HOTPLUG_PWR 0x6052
#define DEVICE_TYPE_EFP_DVI_I 0x6053
#define DEVICE_TYPE_EFP_DVI_D_DUAL 0x6152
#define DEVICE_TYPE_EFP_DVI_D_HDCP 0x60d2
#define DEVICE_TYPE_OPENLDI_HOTPLUG_PWR 0x6062
#define DEVICE_TYPE_OPENLDI_DUALPIX 0x6162
#define DEVICE_TYPE_LFP_PANELLINK 0x5012
#define DEVICE_TYPE_LFP_CMOS_PWR 0x5042
#define DEVICE_TYPE_LFP_LVDS_PWR 0x5062
#define DEVICE_TYPE_LFP_LVDS_DUAL 0x5162
#define DEVICE_TYPE_LFP_LVDS_DUAL_HDCP 0x51e2

/* Add the device class for LFP, TV, HDMI */
#define DEVICE_TYPE_INT_LFP 0x1022
#define DEVICE_TYPE_INT_TV 0x1009
#define DEVICE_TYPE_HDMI 0x60D2
#define DEVICE_TYPE_DP 0x68C6
#define DEVICE_TYPE_DP_DUAL_MODE 0x60D6
#define DEVICE_TYPE_eDP 0x78C6

#define DEVICE_TYPE_CLASS_EXTENSION (1 << 15)
#define DEVICE_TYPE_POWER_MANAGEMENT (1 << 14)
#define DEVICE_TYPE_HOTPLUG_SIGNALING (1 << 13)
#define DEVICE_TYPE_INTERNAL_CONNECTOR (1 << 12)
#define DEVICE_TYPE_NOT_HDMI_OUTPUT (1 << 11)
#define DEVICE_TYPE_MIPI_OUTPUT (1 << 10)
#define DEVICE_TYPE_COMPOSITE_OUTPUT (1 << 9)
#define DEVICE_TYPE_DUAL_CHANNEL (1 << 8)
#define DEVICE_TYPE_HIGH_SPEED_LINK (1 << 6)
#define DEVICE_TYPE_LVDS_SIGNALING (1 << 5)
#define DEVICE_TYPE_TMDS_DVI_SIGNALING (1 << 4)
#define DEVICE_TYPE_VIDEO_SIGNALING (1 << 3)
#define DEVICE_TYPE_DISPLAYPORT_OUTPUT (1 << 2)
#define DEVICE_TYPE_DIGITAL_OUTPUT (1 << 1)
#define DEVICE_TYPE_ANALOG_OUTPUT (1 << 0)

/*
 * Bits we care about when checking for DEVICE_TYPE_eDP. Depending on the
 * system, the other bits may or may not be set for eDP outputs.
 */
#define DEVICE_TYPE_eDP_BITS          \
	(DEVICE_TYPE_INTERNAL_CONNECTOR | \
	 DEVICE_TYPE_MIPI_OUTPUT |        \
	 DEVICE_TYPE_COMPOSITE_OUTPUT |   \
	 DEVICE_TYPE_DUAL_CHANNEL |       \
	 DEVICE_TYPE_LVDS_SIGNALING |     \
	 DEVICE_TYPE_TMDS_DVI_SIGNALING | \
	 DEVICE_TYPE_VIDEO_SIGNALING |    \
	 DEVICE_TYPE_DISPLAYPORT_OUTPUT | \
	 DEVICE_TYPE_ANALOG_OUTPUT)

#define DEVICE_TYPE_DP_DUAL_MODE_BITS \
	(DEVICE_TYPE_INTERNAL_CONNECTOR | \
	 DEVICE_TYPE_MIPI_OUTPUT |        \
	 DEVICE_TYPE_COMPOSITE_OUTPUT |   \
	 DEVICE_TYPE_LVDS_SIGNALING |     \
	 DEVICE_TYPE_TMDS_DVI_SIGNALING | \
	 DEVICE_TYPE_VIDEO_SIGNALING |    \
	 DEVICE_TYPE_DISPLAYPORT_OUTPUT | \
	 DEVICE_TYPE_DIGITAL_OUTPUT |     \
	 DEVICE_TYPE_ANALOG_OUTPUT)

#define DEVICE_CFG_NONE 0x00
#define DEVICE_CFG_12BIT_DVOB 0x01
#define DEVICE_CFG_12BIT_DVOC 0x02
#define DEVICE_CFG_24BIT_DVOBC 0x09
#define DEVICE_CFG_24BIT_DVOCB 0x0a
#define DEVICE_CFG_DUAL_DVOB 0x11
#define DEVICE_CFG_DUAL_DVOC 0x12
#define DEVICE_CFG_DUAL_DVOBC 0x13
#define DEVICE_CFG_DUAL_LINK_DVOBC 0x19
#define DEVICE_CFG_DUAL_LINK_DVOCB 0x1a

#define DEVICE_WIRE_NONE 0x00
#define DEVICE_WIRE_DVOB 0x01
#define DEVICE_WIRE_DVOC 0x02
#define DEVICE_WIRE_DVOBC 0x03
#define DEVICE_WIRE_DVOBB 0x05
#define DEVICE_WIRE_DVOCC 0x06
#define DEVICE_WIRE_DVOB_MASTER 0x0d
#define DEVICE_WIRE_DVOC_MASTER 0x0e

/* dvo_port pre BDB 155 */
#define DEVICE_PORT_DVOA 0x00 /* none on 845+ */
#define DEVICE_PORT_DVOB 0x01
#define DEVICE_PORT_DVOC 0x02

/* dvo_port BDB 155+ */
/* dvo_port BDB 155+ */
#define DVO_PORT_HDMIA 0
#define DVO_PORT_HDMIB 1
#define DVO_PORT_HDMIC 2
#define DVO_PORT_HDMID 3
#define DVO_PORT_LVDS 4
#define DVO_PORT_TV 5
#define DVO_PORT_CRT 6
#define DVO_PORT_DPB 7
#define DVO_PORT_DPC 8
#define DVO_PORT_DPD 9
#define DVO_PORT_DPA 10
#define DVO_PORT_DPE 11	  /* 193 */
#define DVO_PORT_HDMIE 12 /* 193 */
#define DVO_PORT_DPF 13	  /* N/A */
#define DVO_PORT_HDMIF 14 /* N/A */
#define DVO_PORT_DPG 15	  /* 217 */
#define DVO_PORT_HDMIG 16 /* 217 */
#define DVO_PORT_DPH 17	  /* 217 */
#define DVO_PORT_HDMIH 18 /* 217 */
#define DVO_PORT_DPI 19	  /* 217 */
#define DVO_PORT_HDMII 20 /* 217 */
#define DVO_PORT_MIPIA 21 /* 171 */
#define DVO_PORT_MIPIB 22 /* 171 */
#define DVO_PORT_MIPIC 23 /* 171 */
#define DVO_PORT_MIPID 24 /* 171 */

#define HDMI_MAX_DATA_RATE_PLATFORM 0 /* 204 */
#define HDMI_MAX_DATA_RATE_297 1	  /* 204 */
#define HDMI_MAX_DATA_RATE_165 2	  /* 204 */

#define LEGACY_CHILD_DEVICE_CONFIG_SIZE 33

/* DDC Bus DDI Type 155+ */
enum vbt_gmbus_ddi
{
	DDC_BUS_DDI_B = 0x1,
	DDC_BUS_DDI_C,
	DDC_BUS_DDI_D,
	DDC_BUS_DDI_F,
	ICL_DDC_BUS_DDI_A = 0x1,
	ICL_DDC_BUS_DDI_B,
	TGL_DDC_BUS_DDI_C,
	ICL_DDC_BUS_PORT_1 = 0x4,
	ICL_DDC_BUS_PORT_2,
	ICL_DDC_BUS_PORT_3,
	ICL_DDC_BUS_PORT_4,
	TGL_DDC_BUS_PORT_5,
	TGL_DDC_BUS_PORT_6,
};

#define DP_AUX_A 0x40
#define DP_AUX_B 0x10
#define DP_AUX_C 0x20
#define DP_AUX_D 0x30
#define DP_AUX_E 0x50
#define DP_AUX_F 0x60
#define DP_AUX_G 0x70

#define VBT_DP_MAX_LINK_RATE_HBR3 0
#define VBT_DP_MAX_LINK_RATE_HBR2 1
#define VBT_DP_MAX_LINK_RATE_HBR 2
#define VBT_DP_MAX_LINK_RATE_LBR 3
#define _H_ACTIVE(x) (x[2] + ((x[4] & 0xF0) << 4))
#define _H_SYNC_OFF(x) (x[8] + ((x[11] & 0xC0) << 2))
#define _H_SYNC_WIDTH(x) (x[9] + ((x[11] & 0x30) << 4))
#define _H_BLANK(x) (x[3] + ((x[4] & 0x0F) << 8))
#define _V_ACTIVE(x) (x[5] + ((x[7] & 0xF0) << 4))
#define _V_SYNC_OFF(x) ((x[10] >> 4) + ((x[11] & 0x0C) << 2))
#define _V_SYNC_WIDTH(x) ((x[10] & 0x0F) + ((x[11] & 0x03) << 4))
#define _V_BLANK(x) (x[6] + ((x[7] & 0x0F) << 8))
#define _PIXEL_CLOCK(x) (x[0] + (x[1] << 8)) * 10000

#define YESNO(val) ((val) ? "yes" : "no")

struct bdb_general_definitions
{
	/* DDC GPIO */
	UINT8 crt_ddc_gmbus_pin;

	/* DPMS bits */
	UINT8 dpms_acpi : 1;
	UINT8 skip_boot_crt_detect : 1;
	UINT8 dpms_aim : 1;
	UINT8 rsvd1 : 5; /* finish byte */

	/* boot device bits */
	UINT8 boot_display[2];
	UINT8 child_dev_size;

	/*
	 * Device info:
	 * If TV is present, it'll be at devices[0].
	 * LVDS will be next, either devices[0] or [1], if present.
	 * On some platforms the number of device is 6. But could be as few as
	 * 4 if both TV and LVDS are missing.
	 * And the device num is related with the size of general definition
	 * block. It is obtained by using the following formula:
	 * number = (block_size - sizeof(bdb_general_definitions))/
	 *	     defs->child_dev_size;
	 */
	UINT8 devices[0];
} __packed;

/*
 * Block 9 - SRD Feature Block
 */

struct psr_table
{
	/* Feature bits */
	UINT8 full_link : 1;
	UINT8 require_aux_to_wakeup : 1;
	UINT8 feature_bits_rsvd : 6;

	/* Wait times */
	UINT8 idle_frames : 4;
	UINT8 lines_to_wait : 3;
	UINT8 wait_times_rsvd : 1;

	/* TP wake up time in multiple of 100 */
	UINT16 tp1_wakeup_time;
	UINT16 tp2_tp3_wakeup_time;
} __packed;

struct bdb_psr
{
	struct psr_table psr_table[16];

	/* PSR2 TP2/TP3 wakeup time for 16 panels */
	UINT32 psr2_tp2_tp3_wakeup_time;
} __packed;

/*
 * Block 12 - Driver Features Data Block
 */

#define BDB_DRIVER_FEATURE_NO_LVDS 0
#define BDB_DRIVER_FEATURE_INT_LVDS 1
#define BDB_DRIVER_FEATURE_SDVO_LVDS 2
#define BDB_DRIVER_FEATURE_INT_SDVO_LVDS 3

struct bdb_driver_features
{
	UINT8 boot_dev_algorithm : 1;
	UINT8 block_display_switch : 1;
	UINT8 allow_display_switch : 1;
	UINT8 hotplug_dvo : 1;
	UINT8 dual_view_zoom : 1;
	UINT8 int15h_hook : 1;
	UINT8 sprite_in_clone : 1;
	UINT8 primary_lfp_id : 1;

	UINT16 boot_mode_x;
	UINT16 boot_mode_y;
	UINT8 boot_mode_bpp;
	UINT8 boot_mode_refresh;

	UINT16 enable_lfp_primary : 1;
	UINT16 selective_mode_pruning : 1;
	UINT16 dual_frequency : 1;
	UINT16 render_clock_freq : 1; /* 0: high freq; 1: low freq */
	UINT16 nt_clone_support : 1;
	UINT16 power_scheme_ui : 1;		  /* 0: CUI; 1: 3rd party */
	UINT16 sprite_display_assign : 1; /* 0: secondary; 1: primary */
	UINT16 cui_aspect_scaling : 1;
	UINT16 preserve_aspect_ratio : 1;
	UINT16 sdvo_device_power_down : 1;
	UINT16 crt_hotplug : 1;
	UINT16 lvds_config : 2;
	UINT16 tv_hotplug : 1;
	UINT16 hdmi_config : 2;

	UINT8 static_display : 1;
	UINT8 reserved2 : 7;
	UINT16 legacy_crt_max_x;
	UINT16 legacy_crt_max_y;
	UINT8 legacy_crt_max_refresh;

	UINT8 hdmi_termination;
	UINT8 custom_vbt_version;
	/* Driver features data block */
	UINT16 rmpm_enabled : 1;
	UINT16 s2ddt_enabled : 1;
	UINT16 dpst_enabled : 1;
	UINT16 bltclt_enabled : 1;
	UINT16 adb_enabled : 1;
	UINT16 drrs_enabled : 1;
	UINT16 grs_enabled : 1;
	UINT16 gpmt_enabled : 1;
	UINT16 tbt_enabled : 1;
	UINT16 psr_enabled : 1;
	UINT16 ips_enabled : 1;
	UINT16 reserved3 : 4;
	UINT16 pc_feature_valid : 1;
} __packed;

/*
 * Block 22 - SDVO LVDS General Options
 */

struct bdb_sdvo_lvds_options
{
	UINT8 panel_backlight;
	UINT8 h40_set_panel_type;
	UINT8 panel_type;
	UINT8 ssc_clk_freq;
	UINT16 als_low_trip;
	UINT16 als_high_trip;
	UINT8 sclalarcoeff_tab_row_num;
	UINT8 sclalarcoeff_tab_row_size;
	UINT8 coefficient[8];
	UINT8 panel_misc_bits_1;
	UINT8 panel_misc_bits_2;
	UINT8 panel_misc_bits_3;
	UINT8 panel_misc_bits_4;
} __packed;

/*
 * Block 23 - SDVO LVDS Panel DTDs
 */

struct lvds_dvo_timing
{
	UINT16 clock; /**< In 10khz */
	UINT8 hactive_lo;
	UINT8 hblank_lo;
	UINT8 hblank_hi : 4;
	UINT8 hactive_hi : 4;
	UINT8 vactive_lo;
	UINT8 vblank_lo;
	UINT8 vblank_hi : 4;
	UINT8 vactive_hi : 4;
	UINT8 hsync_off_lo;
	UINT8 hsync_pulse_width_lo;
	UINT8 vsync_pulse_width_lo : 4;
	UINT8 vsync_off_lo : 4;
	UINT8 vsync_pulse_width_hi : 2;
	UINT8 vsync_off_hi : 2;
	UINT8 hsync_pulse_width_hi : 2;
	UINT8 hsync_off_hi : 2;
	UINT8 himage_lo;
	UINT8 vimage_lo;
	UINT8 vimage_hi : 4;
	UINT8 himage_hi : 4;
	UINT8 h_border;
	UINT8 v_border;
	UINT8 rsvd1 : 3;
	UINT8 digital : 2;
	UINT8 vsync_positive : 1;
	UINT8 hsync_positive : 1;
	UINT8 non_interlaced : 1;
} __packed;

struct bdb_sdvo_panel_dtds
{
	struct lvds_dvo_timing dtds[4];
} __packed;

/*
 * Block 27 - eDP VBT Block
 */
enum aux_ch
{
	AUX_CH_A,
	AUX_CH_B,
	AUX_CH_C,
	AUX_CH_D,
	AUX_CH_E, /* ICL+ */
	AUX_CH_F,
	AUX_CH_G,
	AUX_CH_H,
	AUX_CH_I,
};
#define aux_ch_name(a) ((a) + 'A')

#define EDP_18BPP 0
#define EDP_24BPP 1
#define EDP_30BPP 2
#define EDP_RATE_1_62 0
#define EDP_RATE_2_7 1
#define EDP_LANE_1 0
#define EDP_LANE_2 1
#define EDP_LANE_4 3
#define EDP_PREEMPHASIS_NONE 0
#define EDP_PREEMPHASIS_3_5dB 1
#define EDP_PREEMPHASIS_6dB 2
#define EDP_PREEMPHASIS_9_5dB 3
#define EDP_VSWING_0_4V 0
#define EDP_VSWING_0_6V 1
#define EDP_VSWING_0_8V 2
#define EDP_VSWING_1_2V 3

struct edp_fast_link_params
{
	UINT8 rate : 4;
	UINT8 lanes : 4;
	UINT8 preemphasis : 4;
	UINT8 vswing : 4;
} __packed;

struct edp_pwm_delays
{
	UINT16 pwm_on_to_backlight_enable;
	UINT16 backlight_disable_to_pwm_off;
} __packed;

struct edp_full_link_params
{
	UINT8 preemphasis : 4;
	UINT8 vswing : 4;
} __packed;
struct edp_power_seq
{
	UINT16 t3;
	UINT16 t7;
	UINT16 t9;
	UINT16 t10;
	UINT16 t12;
} __attribute__((packed));

struct bdb_edp
{
	struct edp_power_seq power_seqs[16];
	UINT32 color_depth;
	struct edp_fast_link_params fast_link_params[16];
	UINT32 sdrrs_msa_timing_delay;

	/* ith bit indicates enabled/disabled for (i+1)th panel */
	UINT16 edp_s3d_feature;							  /* 162 */
	UINT16 edp_t3_optimization;						  /* 165 */
	UINT64 edp_vswing_preemph;						  /* 173 */
	UINT16 fast_link_training;						  /* 182 */
	UINT16 dpcd_600h_write_required;				  /* 185 */
	struct edp_pwm_delays pwm_delays[16];			  /* 186 */
	UINT16 full_link_params_provided;				  /* 199 */
	struct edp_full_link_params full_link_params[16]; /* 199 */
} __packed;

/*
 * Block 40 - LFP Data Block
 */

/* Mask for DRRS / Panel Channel / SSC / BLT control bits extraction */
#define MODE_MASK 0x3

struct bdb_lvds_options
{
	UINT8 panel_type;
	UINT8 panel_type2; /* 212 */
	/* LVDS capabilities, stored in a dword */
	UINT8 pfit_mode : 2;
	UINT8 pfit_text_mode_enhanced : 1;
	UINT8 pfit_gfx_mode_enhanced : 1;
	UINT8 pfit_ratio_auto : 1;
	UINT8 pixel_dither : 1;
	UINT8 lvds_edid : 1;
	UINT8 rsvd2 : 1;
	UINT8 rsvd4;
	/* LVDS Panel channel bits stored here */
	UINT32 lvds_panel_channel_bits;
	/* LVDS SSC (Spread Spectrum Clock) bits stored here. */
	UINT16 ssc_bits;
	UINT16 ssc_freq;
	UINT16 ssc_ddt;
	/* Panel color depth defined here */
	UINT16 panel_color_depth;
	/* LVDS panel type bits stored here */
	UINT32 dps_panel_type_bits;
	/* LVDS backlight control type bits stored here */
	UINT32 blt_control_type_bits;

	UINT16 lcdvcc_s0_enable; /* 200 */
	UINT32 rotation;		 /* 228 */
} __packed;

/*
 * Block 41 - LFP Data Table Pointers
 */

/* LFP pointer table contains entries to the struct below */
struct lvds_lfp_data_ptr
{
	UINT16 fp_timing_offset; /* offsets are from start of bdb */
	UINT8 fp_table_size;
	UINT16 dvo_timing_offset;
	UINT8 dvo_table_size;
	UINT16 panel_pnp_id_offset;
	UINT8 pnp_table_size;
} __packed;

struct bdb_lvds_lfp_data_ptrs
{
	UINT8 lvds_entries; /* followed by one or more lvds_data_ptr structs */
	struct lvds_lfp_data_ptr ptr[16];
} __packed;

/*
 * Block 42 - LFP Data Tables
 */

/* LFP data has 3 blocks per entry */
struct lvds_fp_timing
{
	UINT16 x_res;
	UINT16 y_res;
	UINT32 lvds_reg;
	UINT32 lvds_reg_val;
	UINT32 pp_on_reg;
	UINT32 pp_on_reg_val;
	UINT32 pp_off_reg;
	UINT32 pp_off_reg_val;
	UINT32 pp_cycle_reg;
	UINT32 pp_cycle_reg_val;
	UINT32 pfit_reg;
	UINT32 pfit_reg_val;
	UINT16 terminator;
} __packed;

struct lvds_pnp_id
{
	UINT16 mfg_name;
	UINT16 product_code;
	UINT32 serial;
	UINT8 mfg_week;
	UINT8 mfg_year;
} __packed;

struct lvds_lfp_data_entry
{
	struct lvds_fp_timing fp_timing;
	struct lvds_dvo_timing dvo_timing;
	struct lvds_pnp_id pnp_id;
} __packed;

struct bdb_lvds_lfp_data
{
	struct lvds_lfp_data_entry data[16];
} __packed;

/*
 * Block 43 - LFP Backlight Control Data Block
 */

#define BDB_BACKLIGHT_TYPE_NONE 0
#define BDB_BACKLIGHT_TYPE_PWM 2

struct lfp_backlight_data_entry
{
	UINT8 type : 2;
	UINT8 active_low_pwm : 1;
	UINT8 obsolete1 : 5;
	UINT16 pwm_freq_hz;
	UINT8 min_brightness;
	UINT8 obsolete2;
	UINT8 obsolete3;
} __packed;

struct lfp_backlight_control_method
{
	UINT8 type : 4;
	UINT8 controller : 4;
} __packed;

struct bdb_lfp_backlight_data
{
	UINT8 entry_size;
	struct lfp_backlight_data_entry data[16];
	UINT8 level[16];
	struct lfp_backlight_control_method backlight_control[16];
} __packed;

/* Block 52 contains MiPi Panel info
 * 6 such enteries will there. Index into correct
 * entery is based on the panel_index in #40 LFP
 */
#define MAX_MIPI_CONFIGURATIONS 6
struct mipi_config
{
	UINT16 panel_id;

	/* General Params */
	UINT32 dithering : 1;
	UINT32 rsvd1 : 1;
	UINT32 panel_type : 1;
	UINT32 panel_arch_type : 2;
	UINT32 cmd_mode : 1;
	UINT32 vtm : 2;
	UINT32 cabc : 1;
	UINT32 pwm_blc : 1;

	/* Bit 13:10
	 * 000 - Reserved, 001 - RGB565, 002 - RGB666,
	 * 011 - RGB666Loosely packed, 100 - RGB888,
	 * others - rsvd
	 */
	UINT32 videomode_color_format : 4;

	/* Bit 15:14
	 * 0 - No rotation, 1 - 90 degree
	 * 2 - 180 degree, 3 - 270 degree
	 */
	UINT32 rotation : 2;
	UINT32 bta : 1;
	UINT32 rsvd2 : 15;

	/* 2 byte Port Description */
	UINT16 dual_link : 2;
	UINT16 lane_cnt : 2;
	UINT16 pixel_overlap : 3;
	UINT16 rsvd3 : 9;

	/* 2 byte DSI COntroller params */
	/* 0 - Using DSI PHY, 1 - TE usage */
	UINT16 dsi_usage : 1;
	UINT16 rsvd4 : 15;

	UINT8 rsvd5[5];
	UINT32 dsi_ddr_clk;
	UINT32 bridge_ref_clk;

	UINT8 byte_clk_sel : 2;
	UINT8 rsvd6 : 6;

	/* DPHY Flags */
	UINT16 dphy_param_valid : 1;
	UINT16 eot_disabled : 1;
	UINT16 clk_stop : 1;
	UINT16 rsvd7 : 13;

	UINT32 hs_tx_timeout;
	UINT32 lp_rx_timeout;
	UINT32 turn_around_timeout;
	UINT32 device_reset_timer;
	UINT32 master_init_timer;
	UINT32 dbi_bw_timer;
	UINT32 lp_byte_clk_val;

	/*  4 byte Dphy Params */
	UINT32 prepare_cnt : 6;
	UINT32 rsvd8 : 2;
	UINT32 clk_zero_cnt : 8;
	UINT32 trail_cnt : 5;
	UINT32 rsvd9 : 3;
	UINT32 exit_zero_cnt : 6;
	UINT32 rsvd10 : 2;

	UINT32 clk_lane_switch_cnt;
	UINT32 hl_switch_cnt;

	UINT32 rsvd11[6];

	/* timings based on dphy spec */
	UINT8 tclk_miss;
	UINT8 tclk_post;
	UINT8 rsvd12;
	UINT8 tclk_pre;
	UINT8 tclk_prepare;
	UINT8 tclk_settle;
	UINT8 tclk_term_enable;
	UINT8 tclk_trail;
	UINT16 tclk_prepare_clkzero;
	UINT8 rsvd13;
	UINT8 td_term_enable;
	UINT8 teot;
	UINT8 ths_exit;
	UINT8 ths_prepare;
	UINT16 ths_prepare_hszero;
	UINT8 rsvd14;
	UINT8 ths_settle;
	UINT8 ths_skip;
	UINT8 ths_trail;
	UINT8 tinit;
	UINT8 tlpx;
	UINT8 rsvd15[3];

	/* GPIOs */
	UINT8 panel_enable;
	UINT8 bl_enable;
	UINT8 pwm_enable;
	UINT8 reset_r_n;
	UINT8 pwr_down_r;
	UINT8 stdby_r_n;

} __attribute__((packed));

/* Block 52 contains MiPi configuration block
 * 6 * bdb_mipi_config, followed by 6 pps data
 * block below
 */
struct mipi_pps_data
{
	UINT16 panel_on_delay;
	UINT16 bl_enable_delay;
	UINT16 bl_disable_delay;
	UINT16 panel_off_delay;
	UINT16 panel_power_cycle_delay;
} __attribute__((packed));

struct bdb_mipi_config
{
	struct mipi_config config[MAX_MIPI_CONFIGURATIONS];
	struct mipi_pps_data pps[MAX_MIPI_CONFIGURATIONS];
} __packed;

/*
 * Block 53 - MIPI Sequence Block
 */

struct bdb_mipi_sequence
{
	UINT8 version;
	UINT8 data[0]; /* up to 6 variable length blocks */
} __packed;

/*
 * Block 56 - Compression Parameters
 */

#define VBT_RC_BUFFER_BLOCK_SIZE_1KB 0
#define VBT_RC_BUFFER_BLOCK_SIZE_4KB 1
#define VBT_RC_BUFFER_BLOCK_SIZE_16KB 2
#define VBT_RC_BUFFER_BLOCK_SIZE_64KB 3

#define VBT_DSC_LINE_BUFFER_DEPTH(vbt_value) ((vbt_value) + 8) /* bits */
#define VBT_DSC_MAX_BPP(vbt_value) (6 + (vbt_value)*2)

struct dsc_compression_parameters_entry
{
	UINT8 version_major : 4;
	UINT8 version_minor : 4;

	UINT8 rc_buffer_block_size : 2;
	UINT8 reserved1 : 6;

	/*
	 * Buffer size in bytes:
	 *
	 * 4 ^ rc_buffer_block_size * 1024 * (rc_buffer_size + 1) bytes
	 */
	UINT8 rc_buffer_size;
	UINT32 slices_per_line;

	UINT8 line_buffer_depth : 4;
	UINT8 reserved2 : 4;

	/* Flag Bits 1 */
	UINT8 block_prediction_enable : 1;
	UINT8 reserved3 : 7;

	UINT8 max_bpp; /* mapping */

	/* Color depth capabilities */
	UINT8 reserved4 : 1;
	UINT8 support_8bpc : 1;
	UINT8 support_10bpc : 1;
	UINT8 support_12bpc : 1;
	UINT8 reserved5 : 4;

	UINT16 slice_height;
} __packed;

struct bdb_compression_parameters
{
	UINT16 entry_size;
	struct dsc_compression_parameters_entry data[16];
} __packed;

EFI_STATUS decodeVBT(struct intel_opregion *opRegion, int vbt_off);
void parse_ddi_ports(i915_CONTROLLER *dev_priv, UINT8 bdb_version);
enum aux_ch intel_bios_port_aux_ch(i915_CONTROLLER *dev_priv,
								   enum port port);
#endif
