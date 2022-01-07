#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>
#include "i915_reg.h"
#ifndef INTEL_CONTROLLERH
#define INTEL_CONTROLLERH

struct opregion_header
{
	UINT8 signature[16];
	UINT32 size;
	struct
	{
		UINT8 rsvd;
		UINT8 revision;
		UINT8 minor;
		UINT8 major;
	} __packed over;
	UINT8 bios_ver[32];
	UINT8 vbios_ver[16];
	UINT8 driver_ver[16];
	UINT32 mboxes;
	UINT32 driver_model;
	UINT32 pcon;
	UINT8 dver[32];
	UINT8 rsvd[124];
} __packed;

/* OpRegion mailbox #1: public ACPI methods */
struct opregion_acpi
{
	UINT32 drdy; /* driver readiness */
	UINT32 csts; /* notification status */
	UINT32 cevt; /* current event */
	UINT8 rsvd1[20];
	UINT32 didl[8]; /* supported display devices ID list */
	UINT32 cpdl[8]; /* currently presented display list */
	UINT32 cadl[8]; /* currently active display list */
	UINT32 nadl[8]; /* next active devices list */
	UINT32 aslp;	/* ASL sleep time-out */
	UINT32 tidx;	/* toggle table index */
	UINT32 chpd;	/* current hotplug enable indicator */
	UINT32 clid;	/* current lid state*/
	UINT32 cdck;	/* current docking state */
	UINT32 sxsw;	/* Sx state resume */
	UINT32 evts;	/* ASL supported events */
	UINT32 cnot;	/* current OS notification */
	UINT32 nrdy;	/* driver status */
	UINT32 did2[7]; /* extended supported display devices ID list */
	UINT32 cpd2[7]; /* extended attached display devices list */
	UINT8 rsvd2[4];
} __packed;

/* OpRegion mailbox #2: SWSCI */
struct opregion_swsci
{
	UINT32 scic; /* SWSCI command|status|data */
	UINT32 parm; /* command parameters */
	UINT32 dslp; /* driver sleep time-out */
	UINT8 rsvd[244];
} __packed;

/* OpRegion mailbox #3: ASLE */
struct opregion_asle
{
	UINT32 ardy;	 /* driver readiness */
	UINT32 aslc;	 /* ASLE interrupt command */
	UINT32 tche;	 /* technology enabled indicator */
	UINT32 alsi;	 /* current ALS illuminance reading */
	UINT32 bclp;	 /* backlight brightness to set */
	UINT32 pfit;	 /* panel fitting state */
	UINT32 cblv;	 /* current brightness level */
	UINT16 bclm[20]; /* backlight level duty cycle mapping table */
	UINT32 cpfm;	 /* current panel fitting mode */
	UINT32 epfm;	 /* enabled panel fitting modes */
	UINT8 plut[74];	 /* panel LUT and identifier */
	UINT32 pfmb;	 /* PWM freq and min brightness */
	UINT32 cddv;	 /* color correction default values */
	UINT32 pcft;	 /* power conservation features */
	UINT32 srot;	 /* supported rotation angles */
	UINT32 iuer;	 /* IUER events */
	UINT64 fdss;
	UINT32 fdsp;
	UINT32 stat;
	UINT64 rvda; /* Physical (2.0) or relative from opregion (2.1+)
			 * address of raw VBT data. */
	UINT32 rvds; /* Size of raw vbt data */
	UINT8 rsvd[58];
} __packed;

/* OpRegion mailbox #5: ASLE ext */
struct opregion_asle_ext
{
	UINT32 phed;	 /* Panel Header */
	UINT8 bddc[256]; /* Panel EDID */
	UINT8 rsvd[764];
} __packed;
/**
 * struct vbt_header - VBT Header structure
 * @signature:		VBT signature, always starts with "$VBT"
 * @version:		Version of this structure
 * @header_size:	Size of this structure
 * @vbt_size:		Size of VBT (VBT Header, BDB Header and data blocks)
 * @vbt_checksum:	Checksum
 * @reserved0:		Reserved
 * @bdb_offset:		Offset of &struct bdb_header from beginning of VBT
 * @aim_offset:		Offsets of add-in data blocks from beginning of VBT
 */
struct vbt_header
{
	UINT8 signature[20];
	UINT16 version;
	UINT16 header_size;
	UINT16 vbt_size;
	UINT8 vbt_checksum;
	UINT8 reserved0;
	UINT32 bdb_offset;
	UINT32 aim_offset[4];
} __packed;

/**
 * struct bdb_header - BDB Header structure
 * @signature:		BDB signature "BIOS_DATA_BLOCK"
 * @version:		Version of the data block definitions
 * @header_size:	Size of this structure
 * @bdb_size:		Size of BDB (BDB Header and data blocks)
 */
struct bdb_header
{
	UINT8 signature[16];
	UINT16 version;
	UINT16 header_size;
	UINT16 bdb_size;
} __packed;
struct intel_opregion
{
	struct opregion_header *header;
	struct opregion_acpi *acpi;
	struct opregion_swsci *swsci;
	struct opregion_asle *asle;
	struct vbt_header *vbt;
	struct bdb_header *bdb;
	struct opregion_asle_ext *asle_ext;
	struct child_device_config *children;
	UINT8 numChildren;
};

#pragma pack(1)
typedef struct
{
	UINT8 magic[8];
	UINT16 vendorId;
	UINT16 productId;
	UINT32 serialNumber;
	UINT8 manufactureWeek;
	UINT8 manufactureYear;
	UINT8 structVersion;
	UINT8 structRevision;
	UINT8 inputParameters;
	UINT8 screenWidth;
	UINT8 screenHeight;
	UINT8 gamma;
	UINT8 features;
	UINT8 colorCoordinates[10];
	UINT8 estTimings1;
	UINT8 estTimings2;
	UINT8 vendorTimings;
	struct
	{
		UINT8 resolution;
		UINT8 frequency;
	} standardTimings[8];
	struct
	{
		UINT16 pixelClock;
		UINT8 horzActive;
		UINT8 horzBlank;
		UINT8 horzActiveBlankMsb;
		UINT8 vertActive;
		UINT8 vertBlank;
		UINT8 vertActiveBlankMsb;
		UINT8 horzSyncOffset;
		UINT8 horzSyncPulse;
		UINT8 vertSync;
		UINT8 syncMsb;
		UINT8 dimensionWidth;
		UINT8 dimensionHeight;
		UINT8 dimensionMsb;
		UINT8 horzBorder;
		UINT8 vertBorder;
		UINT8 features;
	} detailTimings[4];
	UINT8 numExtensions;
	UINT8 checksum;
} EDID;
#pragma pack()
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

struct ddi_vbt_port_info
{
	/* Non-NULL if port present. */
	const struct child_device_config *child;

	int max_tmds_clock;

	/* This is an index in the HDMI/DVI DDI buffer translation table. */
	u8 hdmi_level_shift;
	u8 hdmi_level_shift_set : 1;

	u8 supports_dvi : 1;
	u8 supports_hdmi : 1;
	u8 supports_dp : 1;
	u8 supports_edp : 1;
	u8 supports_typec_usb : 1;
	u8 supports_tbt : 1;

	u8 alternate_aux_channel;
	u8 alternate_ddc_pin;

	u8 dp_boost_level;
	u8 hdmi_boost_level;
	int dp_max_link_rate; /* 0 for not limited by VBT */
	enum port port;
};
struct edp_power_seq
{
	u16 t1_t3;
	u16 t8;
	u16 t9;
	u16 t10;
	u16 t11_t12;
} __packed;
struct intel_vbt_data
{
	//struct drm_display_mode *lfp_lvds_vbt_mode; /* if any */
	//struct drm_display_mode *sdvo_lvds_vbt_mode; /* if any */

	/* Feature bits */
	unsigned int int_tv_support : 1;
	unsigned int lvds_dither : 1;
	unsigned int int_crt_support : 1;
	unsigned int lvds_use_ssc : 1;
	unsigned int int_lvds_support : 1;
	unsigned int display_clock_mode : 1;
	unsigned int fdi_rx_polarity_inverted : 1;
	unsigned int panel_type : 4;
	int lvds_ssc_freq;
	unsigned int bios_lvds_val; /* initial [PCH_]LVDS reg val in VBIOS */
	//enum drm_panel_orientation orientation;

	//	enum drrs_support_type drrs_type;

	struct
	{
		int rate;
		int lanes;
		int preemphasis;
		int vswing;
		bool low_vswing;
		bool initialized;
		int bpp;
		struct edp_power_seq pps;
		bool hobl;
	} edp;

	// struct {
	// 	bool enable;
	// 	bool full_link;
	// 	bool require_aux_wakeup;
	// 	int idle_frames;
	// 	enum psr_lines_to_wait lines_to_wait;
	// 	int tp1_wakeup_time_us;
	// 	int tp2_tp3_wakeup_time_us;
	// 	int psr2_tp2_tp3_wakeup_time_us;
	// } psr;

	// struct {
	// 	u16 pwm_freq_hz;
	// 	bool present;
	// 	bool active_low_pwm;
	// 	u8 min_brightness;	/* min_brightness/255 of max */
	// 	u8 controller;		/* brightness controller number */
	// 	enum intel_backlight_type type;
	// } backlight;

	// /* MIPI DSI */
	// struct {
	// 	u16 panel_id;
	// 	struct mipi_config *config;
	// 	struct mipi_pps_data *pps;
	// 	u16 bl_ports;
	// 	u16 cabc_ports;
	// 	u8 seq_version;
	// 	u32 size;
	// 	u8 *data;
	// 	const u8 *sequence[MIPI_SEQ_MAX];
	// 	u8 *deassert_seq; /* Used by fixup_mipi_sequences() */
	// 	enum drm_panel_orientation orientation;
	// } dsi;

	// int crt_ddc_pin;

	//struct list_head display_devices;

	struct ddi_vbt_port_info ddi_port_info[I915_MAX_PORTS];
	//struct sdvo_device_mapping sdvo_mappings[2];
};
typedef enum ConnectorTypes
{
	HDMI,
	DVI,
	VGA,
	eDP,
	DPSST,
	DPMST
} ConnectorType;
typedef struct
{
	UINT64 Signature;
	EFI_HANDLE Handle;
	EFI_PCI_IO_PROTOCOL *PciIo;
	EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOutput;
	EFI_DEVICE_PATH_PROTOCOL *GopDevicePath;
	EDID edid;
	EFI_PHYSICAL_ADDRESS FbBase;
	UINT32 stride;
	UINT32 gmadr;
	UINT32 is_gvt;
	UINT8 generation;
	UINTN fbsize;
	void (*write32)(UINT64 reg, UINT32 data);
	UINT32 rawclk_freq;
	UINT32(*read32)
	(UINT64 reg);

	UINT64(*read64)
	(UINT64 reg);
	struct
	{
		UINT32 Port;
		UINT32 AuxCh;
		ConnectorType ConType;
		UINT8 DPLL;
		UINT32 LinkRate;
		UINT8 LaneCount;
	} OutputPath;
	struct intel_opregion *opRegion;
	struct intel_vbt_data vbt;
	struct intel_dp *intel_dp;
} i915_CONTROLLER;
#endif