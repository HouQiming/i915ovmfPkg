#include <Uefi.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/DevicePath.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

//registers are in bar 0
//frame buffer is in bar 2
#define PCH_DISPLAY_BASE	0xc0000u

#define gmbusSelect (PCH_DISPLAY_BASE+0x5100)
#define   GMBUS_AKSV_SELECT	(1 << 11)
#define   GMBUS_RATE_100KHZ	(0 << 8)
#define   GMBUS_RATE_50KHZ	(1 << 8)
#define   GMBUS_RATE_400KHZ	(2 << 8) /* reserved on Pineview */
#define   GMBUS_RATE_1MHZ	(3 << 8) /* reserved on Pineview */
#define   GMBUS_HOLD_EXT	(1 << 7) /* 300ns hold time, rsvd on Pineview */
#define   GMBUS_BYTE_CNT_OVERRIDE (1 << 6)
#define   GMBUS_PIN_DISABLED	0
#define   GMBUS_PIN_SSC		1
#define   GMBUS_PIN_VGADDC	2
#define   GMBUS_PIN_PANEL	3
#define   GMBUS_PIN_DPD_CHV	3 /* HDMID_CHV */
#define   GMBUS_PIN_DPC		4 /* HDMIC */
#define   GMBUS_PIN_DPB		5 /* SDVO, HDMIB */
#define   GMBUS_PIN_DPD		6 /* HDMID */
#define   GMBUS_PIN_RESERVED	7 /* 7 reserved */
#define   GMBUS_PIN_1_BXT	1 /* BXT+ (atom) and CNP+ (big core) */
#define   GMBUS_PIN_2_BXT	2
#define   GMBUS_PIN_3_BXT	3
#define   GMBUS_PIN_4_CNP	4
#define   GMBUS_PIN_9_TC1_ICP	9
#define   GMBUS_PIN_10_TC2_ICP	10
#define   GMBUS_PIN_11_TC3_ICP	11
#define   GMBUS_PIN_12_TC4_ICP	12

#define gmbusCommand (PCH_DISPLAY_BASE+0x5104)
#define   GMBUS_SW_CLR_INT	(1 << 31)
#define   GMBUS_SW_RDY		(1 << 30)
#define   GMBUS_ENT		(1 << 29) /* enable timeout */
#define   GMBUS_CYCLE_NONE	(0 << 25)
#define   GMBUS_CYCLE_WAIT	(1 << 25)
#define   GMBUS_CYCLE_INDEX	(2 << 25)
#define   GMBUS_CYCLE_STOP	(4 << 25)
#define   GMBUS_BYTE_COUNT_SHIFT 16
#define   GMBUS_BYTE_COUNT_MAX   256U
#define   GEN9_GMBUS_BYTE_COUNT_MAX 511U
#define   GMBUS_SLAVE_INDEX_SHIFT 8
#define   GMBUS_SLAVE_ADDR_SHIFT 1
#define   GMBUS_SLAVE_READ	(1 << 0)
#define   GMBUS_SLAVE_WRITE	(0 << 0)

#define gmbusStatus (PCH_DISPLAY_BASE+0x5108)
#define   GMBUS_INUSE		(1 << 15)
#define   GMBUS_HW_WAIT_PHASE	(1 << 14)
#define   GMBUS_STALL_TIMEOUT	(1 << 13)
#define   GMBUS_INT		(1 << 12)
#define   GMBUS_HW_RDY		(1 << 11)
#define   GMBUS_SATOER		(1 << 10)
#define   GMBUS_ACTIVE		(1 << 9)

#define gmbusData (PCH_DISPLAY_BASE+0x510C)

#define _PCH_DP_B		(0xe4100)
#define _PCH_DPB_AUX_CH_CTL	(0xe4110)
#define _PCH_DPB_AUX_CH_DATA1	(0xe4114)
#define _PCH_DPB_AUX_CH_DATA2	(0xe4118)
#define _PCH_DPB_AUX_CH_DATA3	(0xe411c)
#define _PCH_DPB_AUX_CH_DATA4	(0xe4120)
#define _PCH_DPB_AUX_CH_DATA5	(0xe4124)

#define _DPA_AUX_CH_CTL		(0x64010)
#define _DPA_AUX_CH_DATA1	(0x64014)
#define _DPA_AUX_CH_DATA2	(0x64018)
#define _DPA_AUX_CH_DATA3	(0x6401c)
#define _DPA_AUX_CH_DATA4	(0x64020)
#define _DPA_AUX_CH_DATA5	(0x64024)

#define   DP_AUX_CH_CTL_SEND_BUSY	    (1 << 31)
#define   DP_AUX_CH_CTL_DONE		    (1 << 30)
#define   DP_AUX_CH_CTL_INTERRUPT	    (1 << 29)
#define   DP_AUX_CH_CTL_TIME_OUT_ERROR	    (1 << 28)
#define   DP_AUX_CH_CTL_TIME_OUT_400us	    (0 << 26)
#define   DP_AUX_CH_CTL_TIME_OUT_600us	    (1 << 26)
#define   DP_AUX_CH_CTL_TIME_OUT_800us	    (2 << 26)
#define   DP_AUX_CH_CTL_TIME_OUT_MAX	    (3 << 26) /* Varies per platform */
#define   DP_AUX_CH_CTL_TIME_OUT_MASK	    (3 << 26)
#define   DP_AUX_CH_CTL_RECEIVE_ERROR	    (1 << 25)
#define   DP_AUX_CH_CTL_MESSAGE_SIZE_MASK    (0x1f << 20)
#define   DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT   20
#define   DP_AUX_CH_CTL_PRECHARGE_2US_MASK   (0xf << 16)
#define   DP_AUX_CH_CTL_PRECHARGE_2US_SHIFT  16
#define   DP_AUX_CH_CTL_AUX_AKSV_SELECT	    (1 << 15)
#define   DP_AUX_CH_CTL_MANCHESTER_TEST	    (1 << 14)
#define   DP_AUX_CH_CTL_SYNC_TEST	    (1 << 13)
#define   DP_AUX_CH_CTL_DEGLITCH_TEST	    (1 << 12)
#define   DP_AUX_CH_CTL_PRECHARGE_TEST	    (1 << 11)
#define   DP_AUX_CH_CTL_BIT_CLOCK_2X_MASK    (0x7ff)
#define   DP_AUX_CH_CTL_BIT_CLOCK_2X_SHIFT   0
#define   DP_AUX_CH_CTL_PSR_DATA_AUX_REG_SKL	(1 << 14)
#define   DP_AUX_CH_CTL_FS_DATA_AUX_REG_SKL	(1 << 13)
#define   DP_AUX_CH_CTL_GTC_DATA_AUX_REG_SKL	(1 << 12)
#define   DP_AUX_CH_CTL_TBT_IO			(1 << 11)
#define   DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL_MASK (0x1f << 5)
#define   DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(c) (((c) - 1) << 5)
#define   DP_AUX_CH_CTL_SYNC_PULSE_SKL(c)   ((c) - 1)

#define AUX_NATIVE_WRITE			0x8
#define AUX_NATIVE_READ			0x9
#define AUX_I2C_WRITE			0x0
#define AUX_I2C_READ			0x1
#define AUX_I2C_STATUS			0x2
#define AUX_I2C_MOT				0x4
#define AUX_I2C_REPLY_ACK			0x0

#define VGACNTRL		(0x71400)
#define VGA_DISP_DISABLE			(1 << 31)
#define VGA_2X_MODE				(1 << 30)

/* Pipe A timing regs */
#define HTOTAL_A	0x60000
#define HBLANK_A	0x60004
#define HSYNC_A	0x60008
#define VTOTAL_A	0x6000c
#define VBLANK_A	0x60010
#define VSYNC_A	0x60014
#define PIPEASRC	0x6001c
#define BCLRPAT_A	0x60020
#define VSYNCSHIFT_A	0x60028
#define PIPE_MULT_A	0x6002c

#define _PIPEACONF		0x70008
#define   PIPECONF_ENABLE	(1 << 31)
#define   PIPECONF_DISABLE	0
#define   I965_PIPECONF_ACTIVE	(1 << 30)

#define _DSPACNTR				0x70180
#define   DISPLAY_PLANE_ENABLE			(1 << 31)
#define   DISPLAY_PLANE_DISABLE			0
#define   PLANE_CTL_FORMAT_MASK			(0xf << 24)
#define   PLANE_CTL_FORMAT_YUV422		(0 << 24)
#define   PLANE_CTL_FORMAT_NV12			(1 << 24)
#define   PLANE_CTL_FORMAT_XRGB_2101010		(2 << 24)
#define   PLANE_CTL_FORMAT_XRGB_8888		(4 << 24)
#define   PLANE_CTL_ORDER_BGRX			(0 << 20)
#define   PLANE_CTL_ORDER_RGBX			(1 << 20)
#define   PLANE_CTL_ALPHA_MASK			(0x3 << 4) /* Pre-GLK */
#define   PLANE_CTL_ALPHA_DISABLE		(0 << 4)
#define   PLANE_CTL_ALPHA_SW_PREMULTIPLY	(2 << 4)
#define   PLANE_CTL_ALPHA_HW_PREMULTIPLY	(3 << 4)
#define   PLANE_CTL_TRICKLE_FEED_DISABLE	(1 << 14)
#define   PLANE_CTL_PLANE_GAMMA_DISABLE		(1 << 13) /* Pre-GLK */

#define   DISPPLANE_PIXFORMAT_MASK		(0xf << 26)
#define   DISPPLANE_YUV422			(0x0 << 26)
#define   DISPPLANE_8BPP			(0x2 << 26)
#define   DISPPLANE_BGRA555			(0x3 << 26)
#define   DISPPLANE_BGRX555			(0x4 << 26)
#define   DISPPLANE_BGRX565			(0x5 << 26)
#define   DISPPLANE_BGRX888			(0x6 << 26)
#define   DISPPLANE_BGRA888			(0x7 << 26)
#define   DISPPLANE_RGBX101010			(0x8 << 26)
#define   DISPPLANE_RGBA101010			(0x9 << 26)
#define   DISPPLANE_BGRX101010			(0xa << 26)
#define   DISPPLANE_RGBX161616			(0xc << 26)
#define   DISPPLANE_RGBX888			(0xe << 26)
#define   DISPPLANE_RGBA888			(0xf << 26)

#define _DSPAADDR				0x70184
#define _DSPASTRIDE				0x70188
#define _DSPAPOS				0x7018C /* reserved */
#define _DSPASIZE				0x70190
#define _DSPASURF				0x7019C /* 965+ only */
#define _DSPATILEOFF				0x701A4 /* 965+ only */
#define _DSPAOFFSET				0x701A4 /* HSW */
#define _DSPASURFLIVE				0x701AC

#define _TRANSA_MSA_MISC		0x60410
#define _TRANSB_MSA_MISC		0x61410
#define _TRANSC_MSA_MISC		0x62410
#define _TRANS_EDP_MSA_MISC		0x6f410

#define  TRANS_MSA_SYNC_CLK		(1 << 0)
#define  TRANS_MSA_SAMPLING_444		(2 << 1)
#define  TRANS_MSA_CLRSP_YCBCR		(2 << 3)
#define  TRANS_MSA_6_BPC		(0 << 5)
#define  TRANS_MSA_8_BPC		(1 << 5)
#define  TRANS_MSA_10_BPC		(2 << 5)
#define  TRANS_MSA_12_BPC		(3 << 5)
#define  TRANS_MSA_16_BPC		(4 << 5)
#define  TRANS_MSA_CEA_RANGE		(1 << 3)

#define _TRANS_DDI_FUNC_CTL_A		0x60400
#define _TRANS_DDI_FUNC_CTL_B		0x61400
#define _TRANS_DDI_FUNC_CTL_C		0x62400
#define _TRANS_DDI_FUNC_CTL_EDP		0x6F400
#define _TRANS_DDI_FUNC_CTL_DSI0	0x6b400
#define _TRANS_DDI_FUNC_CTL_DSI1	0x6bc00

#define  TRANS_DDI_FUNC_ENABLE		(1 << 31)
/* Those bits are ignored by pipe EDP since it can only connect to DDI A */
#define  TRANS_DDI_PORT_MASK		(7 << 28)
#define  TRANS_DDI_PORT_SHIFT		28
#define  TRANS_DDI_SELECT_PORT(x)	((x) << 28)
#define  TRANS_DDI_PORT_NONE		(0 << 28)
#define  TRANS_DDI_MODE_SELECT_MASK	(7 << 24)
#define  TRANS_DDI_MODE_SELECT_HDMI	(0 << 24)
#define  TRANS_DDI_MODE_SELECT_DVI	(1 << 24)
#define  TRANS_DDI_MODE_SELECT_DP_SST	(2 << 24)
#define  TRANS_DDI_MODE_SELECT_DP_MST	(3 << 24)
#define  TRANS_DDI_MODE_SELECT_FDI	(4 << 24)
#define  TRANS_DDI_BPC_MASK		(7 << 20)
#define  TRANS_DDI_BPC_8		(0 << 20)
#define  TRANS_DDI_BPC_10		(1 << 20)
#define  TRANS_DDI_BPC_6		(2 << 20)
#define  TRANS_DDI_BPC_12		(3 << 20)
#define  TRANS_DDI_PVSYNC		(1 << 17)
#define  TRANS_DDI_PHSYNC		(1 << 16)
#define  TRANS_DDI_EDP_INPUT_MASK	(7 << 12)
#define  TRANS_DDI_EDP_INPUT_A_ON	(0 << 12)
#define  TRANS_DDI_EDP_INPUT_A_ONOFF	(4 << 12)
#define  TRANS_DDI_EDP_INPUT_B_ONOFF	(5 << 12)
#define  TRANS_DDI_EDP_INPUT_C_ONOFF	(6 << 12)
#define  TRANS_DDI_HDCP_SIGNALLING	(1 << 9)
#define  TRANS_DDI_DP_VC_PAYLOAD_ALLOC	(1 << 8)
#define  TRANS_DDI_HDMI_SCRAMBLER_CTS_ENABLE (1 << 7)
#define  TRANS_DDI_HDMI_SCRAMBLER_RESET_FREQ (1 << 6)
#define  TRANS_DDI_BFI_ENABLE		(1 << 4)
#define  TRANS_DDI_HIGH_TMDS_CHAR_RATE	(1 << 4)
#define  TRANS_DDI_HDMI_SCRAMBLING	(1 << 0)
#define  TRANS_DDI_HDMI_SCRAMBLING_MASK (TRANS_DDI_HDMI_SCRAMBLER_CTS_ENABLE \
					| TRANS_DDI_HDMI_SCRAMBLER_RESET_FREQ \
					| TRANS_DDI_HDMI_SCRAMBLING)

#define _TRANS_DDI_FUNC_CTL2_A		0x60404
#define _TRANS_DDI_FUNC_CTL2_B		0x61404
#define _TRANS_DDI_FUNC_CTL2_C		0x62404
#define _TRANS_DDI_FUNC_CTL2_EDP	0x6f404
#define _TRANS_DDI_FUNC_CTL2_DSI0	0x6b404
#define _TRANS_DDI_FUNC_CTL2_DSI1	0x6bc04
#define  PORT_SYNC_MODE_ENABLE			(1 << 4)
#define  PORT_SYNC_MODE_MASTER_SELECT(x)	((x) < 0)
#define  PORT_SYNC_MODE_MASTER_SELECT_MASK	(0x7 << 0)
#define  PORT_SYNC_MODE_MASTER_SELECT_SHIFT	0

#define PORT_A 0

#define _FPA0	(PCH_DISPLAY_BASE+0x6040)
#define _FPA1	(PCH_DISPLAY_BASE+0x6044)
#define _FPB0	(PCH_DISPLAY_BASE+0x6048)
#define _FPB1	(PCH_DISPLAY_BASE+0x604c)
#define   FP_N_DIV_MASK		0x003f0000
#define   FP_N_PINEVIEW_DIV_MASK	0x00ff0000
#define   FP_N_DIV_SHIFT		16
#define   FP_M1_DIV_MASK	0x00003f00
#define   FP_M1_DIV_SHIFT		 8
#define   FP_M2_DIV_MASK	0x0000003f
#define   FP_M2_PINEVIEW_DIV_MASK	0x000000ff
#define   FP_M2_DIV_SHIFT		 0

#define _DPLL_A (PCH_DISPLAY_BASE + 0x6014)
#define _DPLL_B (PCH_DISPLAY_BASE + 0x6018)
#define   DPLL_VCO_ENABLE		(1 << 31)
#define   DPLL_SDVO_HIGH_SPEED		(1 << 30)
#define   DPLL_DVO_2X_MODE		(1 << 30)
#define   DPLL_EXT_BUFFER_ENABLE_VLV	(1 << 30)
#define   DPLL_SYNCLOCK_ENABLE		(1 << 29)
#define   DPLL_REF_CLK_ENABLE_VLV	(1 << 29)
#define   DPLL_VGA_MODE_DIS		(1 << 28)
#define   DPLLB_MODE_DAC_SERIAL		(1 << 26) /* i915 */
#define   DPLLB_MODE_LVDS		(2 << 26) /* i915 */
#define   DPLL_MODE_MASK		(3 << 26)
#define   DPLL_DAC_SERIAL_P2_CLOCK_DIV_10 (0 << 24) /* i915 */
#define   DPLL_DAC_SERIAL_P2_CLOCK_DIV_5 (1 << 24) /* i915 */
#define   DPLLB_LVDS_P2_CLOCK_DIV_14	(0 << 24) /* i915 */
#define   DPLLB_LVDS_P2_CLOCK_DIV_7	(1 << 24) /* i915 */
#define   DPLL_P2_CLOCK_DIV_MASK	0x03000000 /* i915 */
#define   DPLL_FPA01_P1_POST_DIV_MASK	0x00ff0000 /* i915 */
#define   DPLL_FPA01_P1_POST_DIV_MASK_PINEVIEW	0x00ff8000 /* Pineview */
#define   DPLL_LOCK_VLV			(1 << 15)
#define   DPLL_INTEGRATED_CRI_CLK_VLV	(1 << 14)
#define   DPLL_INTEGRATED_REF_CLK_VLV	(1 << 13)
#define   DPLL_SSC_REF_CLK_CHV		(1 << 13)
#define   DPLL_PORTC_READY_MASK		(0xf << 4)
#define   DPLL_PORTB_READY_MASK		(0xf)

#define   DPLL_FPA01_P1_POST_DIV_SHIFT	16
#define   DPLL_FPA01_P1_POST_DIV_SHIFT_PINEVIEW 15

#define   PLL_P2_DIVIDE_BY_4		(1 << 23)
#define   PLL_P1_DIVIDE_BY_TWO		(1 << 21) /* i830 */
#define   PLL_REF_INPUT_DREFCLK		(0 << 13)
#define   PLL_REF_INPUT_TVCLKINA	(1 << 13) /* i830 */
#define   PLL_REF_INPUT_TVCLKINBC	(2 << 13) /* SDVO TVCLKIN */
#define   PLLB_REF_INPUT_SPREADSPECTRUMIN (3 << 13)
#define   PLL_REF_INPUT_MASK		(3 << 13)
#define   PLL_LOAD_PULSE_PHASE_SHIFT		9
/* Ironlake */
#define PLL_REF_SDVO_HDMI_MULTIPLIER_SHIFT     9
#define PLL_REF_SDVO_HDMI_MULTIPLIER_MASK      (7 << 9)
#define PLL_REF_SDVO_HDMI_MULTIPLIER(x)	(((x) - 1) << 9)
#define DPLL_FPA1_P1_POST_DIV_SHIFT            0
#define DPLL_FPA1_P1_POST_DIV_MASK             0xff

#define _DPLL_A_MD (PCH_DISPLAY_BASE + 0x601c)
#define _DPLL_B_MD (PCH_DISPLAY_BASE + 0x6020)
#define   DPLL_MD_UDI_MULTIPLIER_MASK		0x00003f00
#define   DPLL_MD_UDI_MULTIPLIER_SHIFT		8
/*
 * SDVO/UDI pixel multiplier for VGA, same as DPLL_MD_UDI_MULTIPLIER_MASK.
 * This best be set to the default value (3) or the CRT won't work. No,
 * I don't entirely understand what this does...
 */
#define   DPLL_MD_VGA_UDI_MULTIPLIER_MASK	0x0000003f
#define   DPLL_MD_VGA_UDI_MULTIPLIER_SHIFT	0

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION g_mode_info[] = {
  {
    0,    // Version
    1024,  // HorizontalResolution
    768,  // VerticalResolution
  }
};

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE g_mode = {
  ARRAY_SIZE (g_mode_info),                // MaxMode
  0,                                              // Mode
  g_mode_info,                             // Info
  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),  // SizeOfInfo
};

#pragma pack(1)
typedef struct {
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
	struct {
		UINT8 resolution;
		UINT8 frequency;
	} standardTimings[8];
	struct {
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

typedef struct {
  UINT64                                Signature;
  EFI_HANDLE                            Handle;
  EFI_PCI_IO_PROTOCOL                   *PciIo;
  EFI_GRAPHICS_OUTPUT_PROTOCOL          GraphicsOutput;
  EFI_DEVICE_PATH_PROTOCOL              *GopDevicePath;
  EDID edid;
  EFI_PHYSICAL_ADDRESS FbBase;
  UINT32 stride;
  UINT32 gmadr;
} I915_VIDEO_PRIVATE_DATA;

I915_VIDEO_PRIVATE_DATA g_private={SIGNATURE_32('i','9','1','5')};

static void write32(UINT64 reg, UINT32 data){
	g_private.PciIo->Mem.Write (
		g_private.PciIo,
		EfiPciIoWidthFillUint32,
		PCI_BAR_IDX0,
		reg,
		1,
		&data
	);
}

static UINT32 read32(UINT64 reg){
	UINT32 data=0;
	g_private.PciIo->Mem.Read (
		g_private.PciIo,
		EfiPciIoWidthFillUint32,
		PCI_BAR_IDX0,
		reg,
		1,
		&data
	);
	return data;
}

static UINT64 read64(UINT64 reg){
	UINT64 data=0;
	g_private.PciIo->Mem.Read (
		g_private.PciIo,
		EfiPciIoWidthFillUint64,
		PCI_BAR_IDX0,
		reg,
		1,
		&data
	);
	return data;
}

static EFI_STATUS gmbusWait(UINT32 wanted){
	UINTN counter=0;
	for(;;){
		UINT32 status=read32(gmbusStatus);
		counter+=1;
		if(counter>=1024){
			//failed
			DebugPrint(EFI_D_ERROR,"i915: gmbus timeout");
			return EFI_DEVICE_ERROR;
		}
		if(status&GMBUS_SATOER){
			//failed
			DebugPrint(EFI_D_ERROR,"i915: gmbus error");
			return EFI_DEVICE_ERROR;
		}
		if(status&wanted){
			//worked
			return EFI_SUCCESS;
		}
	}
}

static EFI_STATUS ReadEDID(EDID* result){
	UINT32 pin=0;
	//it's an INTEL GPU, there's no way we could be big endian
	UINT32* p=(UINT32*)result;
	//try all the pins on GMBUS
	for(pin=2;pin<=6;pin++){
		DebugPrint(EFI_D_ERROR,"i915: trying pin %d\n",pin);
		write32(gmbusSelect, pin);
		if(EFI_ERROR(gmbusWait(GMBUS_HW_RDY))){
			//it's DP, need to hack AUX_CHAN
			continue;
		}
		//set read offset: i2cWrite(0x50, &offset, 1);
		write32(gmbusData, 0);
		write32(gmbusCommand, (0x50<<GMBUS_SLAVE_ADDR_SHIFT)|(1<<GMBUS_BYTE_COUNT_SHIFT)|GMBUS_SLAVE_WRITE|GMBUS_CYCLE_WAIT|GMBUS_SW_RDY);
		//gmbusWait(GMBUS_HW_WAIT_PHASE);
		gmbusWait(GMBUS_HW_RDY);
		//read the edid: i2cRead(0x50, &edid, 128);
		//note that we could fail here!
		write32(gmbusCommand, (0x50<<GMBUS_SLAVE_ADDR_SHIFT)|(128<<GMBUS_BYTE_COUNT_SHIFT)|GMBUS_SLAVE_READ|GMBUS_CYCLE_WAIT|GMBUS_SW_RDY);
		UINT32 i=0;
		for(i=0;i<128;i+=4){
			if(EFI_ERROR(gmbusWait(GMBUS_HW_RDY))){break;}
			p[i>>2]=read32(gmbusData);
		}
		//gmbusWait(GMBUS_HW_WAIT_PHASE);
		gmbusWait(GMBUS_HW_RDY);
		for(UINT32 i=0;i<16;i++){
			for(UINT32 j=0;j<8;j++){
				DebugPrint(EFI_D_ERROR,"%02x ",((UINT8*)(p))[i*8+j]);
			}
			DebugPrint(EFI_D_ERROR,"\n");
		}
		if(i>=128&&*(UINT64*)result->magic==0x00FFFFFFFFFFFF00uLL){return EFI_SUCCESS;}
	}
	//try DP AUX CHAN - Skylake
	//write32(_DPA_AUX_CH_CTL+(1<<8),0x1234)
	//write32(_DPA_AUX_CH_CTL+(0x600),0x1234);
	//write32(_DPA_AUX_CH_CTL+(0<<8),0x1234);
	//write32(_DPA_AUX_CH_DATA1+(0<<8),0xabcd);
	//write32(_DPA_AUX_CH_DATA2+(0<<8),0xabcd);
	//write32(_DPA_AUX_CH_DATA3+(0<<8),0xabcd);
	//DebugPrint(EFI_D_ERROR,"i915: SKL CTL %08x\n",read32(_DPA_AUX_CH_CTL+(0<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",read32(_DPA_AUX_CH_DATA1+(0<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",read32(_DPA_AUX_CH_DATA2+(0<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",read32(_DPA_AUX_CH_DATA3+(0<<8)));
	//write32(_PCH_DP_B+(1<<8),0x1234);
	//DebugPrint(EFI_D_ERROR,"i915: SKL %08x\n",read32(_DPA_AUX_CH_CTL+(1<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: PCH %08x\n",read32(_PCH_DP_B+(1<<8)));
	for(pin=0;pin<=5;pin++){
		DebugPrint(EFI_D_ERROR,"i915: trying DP aux %d\n",pin);
		//aux message header is 3-4 bytes: ctrl8 addr16 len8
		//the data is big endian
		//len is receive buffer size-1
		//i2c init
		UINT32 send_ctl=(
	      DP_AUX_CH_CTL_SEND_BUSY |
	      DP_AUX_CH_CTL_DONE |
	      DP_AUX_CH_CTL_TIME_OUT_ERROR |
	      DP_AUX_CH_CTL_TIME_OUT_MAX |
	      DP_AUX_CH_CTL_RECEIVE_ERROR |
	      (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
	      DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
	      DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
		);
		/* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
		write32(_DPA_AUX_CH_DATA1+(pin<<8), ((AUX_I2C_MOT|AUX_I2C_WRITE)<<28)|(0x50<<8)|0);
		write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
		UINT32 aux_status;
		UINT32 counter=0;
		for(;;){
			aux_status=read32(_DPA_AUX_CH_CTL+(pin<<8));
			if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
			counter+=1;
			if(counter>=16384){
				DebugPrint(EFI_D_ERROR,"i915:DP AUX channel timeout");
				break;
			}
		}
		write32(_DPA_AUX_CH_CTL+(pin<<8), 
			aux_status |
			DP_AUX_CH_CTL_DONE |
			DP_AUX_CH_CTL_TIME_OUT_ERROR |
			DP_AUX_CH_CTL_RECEIVE_ERROR
		);
		//i2c send 1 byte
		send_ctl=(
		     DP_AUX_CH_CTL_SEND_BUSY |
		     DP_AUX_CH_CTL_DONE |
		     DP_AUX_CH_CTL_TIME_OUT_ERROR |
		     DP_AUX_CH_CTL_TIME_OUT_MAX |
		     DP_AUX_CH_CTL_RECEIVE_ERROR |
		     (5 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		     DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		     DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
		);
		write32(_DPA_AUX_CH_DATA1+(pin<<8), (AUX_I2C_WRITE<<28)|(0x50<<8)|0);
		write32(_DPA_AUX_CH_DATA2+(pin<<8), 0);
		write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
		counter=0;
		for(;;){
			aux_status=read32(_DPA_AUX_CH_CTL+(pin<<8));
			if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
			counter+=1;
			if(counter>=16384){
				DebugPrint(EFI_D_ERROR,"i915:DP AUX channel timeout");
				break;
			}
		}
		write32(_DPA_AUX_CH_CTL+(pin<<8), 
			aux_status |
			DP_AUX_CH_CTL_DONE |
			DP_AUX_CH_CTL_TIME_OUT_ERROR |
			DP_AUX_CH_CTL_RECEIVE_ERROR
		);
		if (aux_status & (DP_AUX_CH_CTL_TIME_OUT_ERROR|DP_AUX_CH_CTL_RECEIVE_ERROR)){
			continue;
		}
		//i2c read 1 byte * 128
		DebugPrint(EFI_D_ERROR,"i915: reading DP aux %d\n",pin);
		//aux message header is 3-4 bytes: ctrl8 addr16 len8
		//the data is big endian
		//len is receive buffer size-1
		//i2c init
		send_ctl=(
		     DP_AUX_CH_CTL_SEND_BUSY |
		     DP_AUX_CH_CTL_DONE |
		     DP_AUX_CH_CTL_TIME_OUT_ERROR |
		     DP_AUX_CH_CTL_TIME_OUT_MAX |
		     DP_AUX_CH_CTL_RECEIVE_ERROR |
		     (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		     DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		     DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
		);
		/* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
		write32(_DPA_AUX_CH_DATA1+(pin<<8), ((AUX_I2C_MOT|AUX_I2C_READ)<<28)|(0x50<<8)|0);
		write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
		counter=0;
		for(;;){
			aux_status=read32(_DPA_AUX_CH_CTL+(pin<<8));
			if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
			counter+=1;
			if(counter>=16384){
				DebugPrint(EFI_D_ERROR,"i915: DP AUX channel timeout");
				break;
			}
		}
		write32(_DPA_AUX_CH_CTL+(pin<<8), 
			aux_status |
			DP_AUX_CH_CTL_DONE |
			DP_AUX_CH_CTL_TIME_OUT_ERROR |
			DP_AUX_CH_CTL_RECEIVE_ERROR
		);
		UINT32 i=0;
		for(i=0;i<128;i++){
			send_ctl=(
			     DP_AUX_CH_CTL_SEND_BUSY |
			     DP_AUX_CH_CTL_DONE |
			     DP_AUX_CH_CTL_TIME_OUT_ERROR |
			     DP_AUX_CH_CTL_TIME_OUT_MAX |
			     DP_AUX_CH_CTL_RECEIVE_ERROR |
			     (4 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
			     DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
			     DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
			);
			write32(_DPA_AUX_CH_DATA1+(pin<<8), (AUX_I2C_READ<<28)|(0x50<<8)|0);
			write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
			counter=0;
			for(;;){
				aux_status=read32(_DPA_AUX_CH_CTL+(pin<<8));
				if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
				counter+=1;
				if(counter>=16384){
					DebugPrint(EFI_D_ERROR,"i915: DP AUX channel timeout");
					break;
				}
			}
			write32(_DPA_AUX_CH_CTL+(pin<<8), 
				aux_status |
				DP_AUX_CH_CTL_DONE |
				DP_AUX_CH_CTL_TIME_OUT_ERROR |
				DP_AUX_CH_CTL_RECEIVE_ERROR
			);
			UINT32 word=read32(_DPA_AUX_CH_DATA1+(pin<<8));
			((UINT8*)p)[i]=(word>>16)&0xff;
		}
		for(UINT32 i=0;i<16;i++){
			for(UINT32 j=0;j<8;j++){
				DebugPrint(EFI_D_ERROR,"%02x ",((UINT8*)(p))[i*8+j]);
			}
			DebugPrint(EFI_D_ERROR,"\n");
		}
		if(i>=128&&*(UINT64*)result->magic==0x00FFFFFFFFFFFF00uLL){return EFI_SUCCESS;}
	}
	return EFI_NOT_FOUND;
}

STATIC EFI_STATUS EFIAPI i915GraphicsOutputQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *ModeInfo;
  DebugPrint(EFI_D_ERROR,"i915: query mode\n");

  if (Info == NULL || SizeOfInfo == NULL ||
      ModeNumber >= g_mode.MaxMode) {
    return EFI_INVALID_PARAMETER;
  }
  ModeInfo = &g_mode_info[ModeNumber];

  *Info = AllocateCopyPool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION), ModeInfo);
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  return EFI_SUCCESS;
}

STATIC FRAME_BUFFER_CONFIGURE        *g_i915FrameBufferBltConfigure=NULL;
STATIC UINTN                         g_i915FrameBufferBltConfigureSize=0;
STATIC INTN g_already_set=0;

struct dpll {
	/* given values */
	int n;
	int m1, m2;
	int p1, p2;
	/* derived values */
	int	dot;
	int	vco;
	int	m;
	int	p;
};

struct intel_limit {
	struct {
		int min, max;
	} dot, vco, n, m, m1, m2, p, p1;

	struct {
		int dot_limit;
		int p2_slow, p2_fast;
	} p2;
};

//intel_limits_i9xx_sdvo
static const struct intel_limit g_limits = {
	.dot = { .min = 20000, .max = 400000 },
	.vco = { .min = 1400000, .max = 2800000 },
	.n = { .min = 1, .max = 6 },
	.m = { .min = 70, .max = 120 },
	.m1 = { .min = 8, .max = 18 },
	.m2 = { .min = 3, .max = 7 },
	.p = { .min = 5, .max = 80 },
	.p1 = { .min = 1, .max = 8 },
	.p2 = { .dot_limit = 200000,
		.p2_slow = 10, .p2_fast = 5 },
};

STATIC EFI_STATUS EFIAPI i915GraphicsOutputSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN  UINT32                       ModeNumber
  )
{
	DebugPrint(EFI_D_ERROR,"i915: set mode %u\n",ModeNumber);
	if(g_already_set){
		DebugPrint(EFI_D_ERROR,"i915: mode already set\n");
		return EFI_SUCCESS;
	}
	g_already_set=1;
	
	//setup DPLL
	UINT32 refclock = 96000;
	UINT32 pixel_clock = (UINT32)(g_private.edid.detailTimings[0].pixelClock) * 10;
	UINT32 multiplier = 1;
	if(pixel_clock >= 100000) {
		multiplier = 1;
	}else if(pixel_clock >= 50000) {
		multiplier = 2;
	}else{
		//assert(pixel_clock >= 25000);
		multiplier = 4;
	}
	struct dpll final_params,params;
	INT32 target=(INT32)(pixel_clock * multiplier);
	INT32 best_err=target;
	for(params.n=g_limits.n.min;params.n<=g_limits.n.max;params.n++)
	for(params.m1=g_limits.m1.max;params.m1>=g_limits.m1.max;params.m1--)
	for(params.m2=g_limits.m2.max;params.m2>=g_limits.m2.max&&params.m2>=params.m1;params.m2--)
	for(params.p1=g_limits.p1.max;params.p1>=g_limits.p1.max;params.p1--)
	for(params.p2=g_limits.p2.p2_slow;params.p2>=g_limits.p2.p2_fast;params.p2--){
		params.m = 5 * (params.m1 + 2) + (params.m2 + 2);
		params.p = params.p1*params.p2;
		if(params.m < g_limits.m.min || params.m > g_limits.m.max){continue;}
		if(params.p < g_limits.p.min || params.p > g_limits.p.max){continue;}
		params.vco = (refclock * params.m + (params.n + 2) / 2) / (params.n + 2);
		params.dot = (params.vco + params.p / 2) / params.p;
		if(params.dot < g_limits.dot.min || params.dot > g_limits.dot.max){continue;}
		if(params.vco < g_limits.vco.min || params.vco > g_limits.vco.max){continue;}
		INT32 err=(INT32)params.dot-target;
		if(err<0){err=-err;}
		if(best_err>err){
			best_err=err;
			final_params=params;
		}
	}
	
	params=final_params;
	
	write32(_FPA0, params.n << 16 | params.m1 << 8 | params.m2);
	write32(_FPA1, params.n << 16 | params.m1 << 8 | params.m2);
	
	write32(_DPLL_A, 0);
	
	UINT32 dplla=DPLLB_MODE_DAC_SERIAL | DPLL_VGA_MODE_DIS | DPLL_SDVO_HIGH_SPEED | DPLL_VCO_ENABLE;
	dplla |= (1 << (params.p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT;
	switch (params.p2) {
	case 5:
		dplla |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_5;
		break;
	case 7:
		dplla |= DPLLB_LVDS_P2_CLOCK_DIV_7;
		break;
	case 10:
		dplla |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_10;
		break;
	case 14:
		dplla |= DPLLB_LVDS_P2_CLOCK_DIV_14;
		break;
	}
	dplla |= (6 << PLL_LOAD_PULSE_PHASE_SHIFT);
	dplla |= PLL_REF_INPUT_DREFCLK;
	
	write32(_DPLL_A, dplla);
	read32(_DPLL_A);
	
	//it's pointless to wait in GVT-g
	if(!g_private.gmadr){
		MicroSecondDelay(150);
	}
	
	write32(_DPLL_A_MD, (multiplier-1)<<DPLL_MD_UDI_MULTIPLIER_SHIFT);
	
	for(int i = 0; i < 3; i++) {
		write32(_DPLL_A, dplla);
		read32(_DPLL_A);
	
		if(!g_private.gmadr){
			MicroSecondDelay(150);
		}
	}
	
	//program PIPE_A
	UINT32 horz_active = g_private.edid.detailTimings[0].horzActive
			| ((UINT32)(g_private.edid.detailTimings[0].horzActiveBlankMsb >> 4) << 8);
	UINT32 horz_blank = g_private.edid.detailTimings[0].horzBlank
			| ((UINT32)(g_private.edid.detailTimings[0].horzActiveBlankMsb & 0xF) << 8);
	UINT32 horz_sync_offset = g_private.edid.detailTimings[0].horzSyncOffset
			| ((UINT32)(g_private.edid.detailTimings[0].syncMsb >> 6) << 8);
	UINT32 horz_sync_pulse = g_private.edid.detailTimings[0].horzSyncPulse
			| (((UINT32)(g_private.edid.detailTimings[0].syncMsb >> 4) & 0x3) << 8);
	
	UINT32 horizontal_active = horz_active;
	UINT32 horizontal_syncStart = horz_active + horz_sync_offset;
	UINT32 horizontal_syncEnd = horz_active + horz_sync_offset + horz_sync_pulse;
	UINT32 horizontal_total = horz_active + horz_blank;
	
	UINT32 vert_active =  g_private.edid.detailTimings[0].vertActive
			| ((UINT32)(g_private.edid.detailTimings[0].vertActiveBlankMsb >> 4) << 8);
	UINT32 vert_blank = g_private.edid.detailTimings[0].vertBlank
			| ((UINT32)(g_private.edid.detailTimings[0].vertActiveBlankMsb & 0xF) << 8);
	UINT32 vert_sync_offset = (g_private.edid.detailTimings[0].vertSync >> 4)
			| (((UINT32)(g_private.edid.detailTimings[0].syncMsb >> 2) & 0x3) << 4);
	UINT32 vert_sync_pulse = (g_private.edid.detailTimings[0].vertSync & 0xF)
			| ((UINT32)(g_private.edid.detailTimings[0].syncMsb & 0x3) << 4);
	
	UINT32 vertical_active = vert_active;
	UINT32 vertical_syncStart = vert_active + vert_sync_offset;
	UINT32 vertical_syncEnd = vert_active + vert_sync_offset + vert_sync_pulse;
	UINT32 vertical_total = vert_active + vert_blank;
	
	write32(VSYNCSHIFT_A, 0);
	
	write32(HTOTAL_A,
		   (horizontal_active - 1) |
		   ((horizontal_total - 1) << 16));
	write32(HBLANK_A,
		   (horizontal_active - 1) |
		   ((horizontal_total - 1) << 16));
	write32(HSYNC_A,
		   (horizontal_syncStart - 1) |
		   ((horizontal_syncEnd - 1) << 16));
	
	write32(VTOTAL_A,
		   (vertical_active - 1) |
		   ((vertical_total - 1) << 16));
	write32(VBLANK_A,
		   (vertical_active - 1) |
		   ((vertical_total - 1) << 16));
	write32(VSYNC_A,
		   (vertical_syncStart - 1) |
		   ((vertical_syncEnd - 1) << 16));
	
	write32(PIPEASRC,((horizontal_active-1)<<16)|(vertical_active-1));
	write32(PIPE_MULT_A, multiplier - 1);
	
	//ddi
	write32(_TRANSA_MSA_MISC, TRANS_MSA_SYNC_CLK|TRANS_MSA_8_BPC);
	write32(_TRANS_DDI_FUNC_CTL_A, (
		TRANS_DDI_FUNC_ENABLE|TRANS_DDI_SELECT_PORT(PORT_A)|TRANS_DDI_BPC_8|TRANS_DDI_MODE_SELECT_HDMI
	));
	
	UINT32 word=read32(_PIPEACONF);
	write32(_PIPEACONF,word|PIPECONF_ENABLE);
	UINT32 counter=0;
	for(;;){
		counter+=1;
		if(counter>=16384){
			DebugPrint(EFI_D_ERROR,"i915: pipe enabled\n");
			break;
		}
		if(read32(_PIPEACONF)&I965_PIPECONF_ACTIVE){
			DebugPrint(EFI_D_ERROR,"i915: failed to enable PIPE");
			break;
		}
	}
	//plane
	UINT32 stride=(horizontal_active*4+63)&-64;
	g_private.stride=stride;
	write32(_DSPAOFFSET,0);
	write32(_DSPAPOS,0);
	write32(_DSPASTRIDE,stride>>6);
	write32(_DSPASIZE,(horizontal_active - 1) | ((vertical_active-1)<<16));
	write32(_DSPACNTR,DISPLAY_PLANE_ENABLE|PLANE_CTL_FORMAT_XRGB_8888|PLANE_CTL_PLANE_GAMMA_DISABLE);
	write32(_DSPASURF,g_private.gmadr);
	//write32(_DSPAADDR,0);
	//word=read32(_DSPACNTR);
	//write32(_DSPACNTR,(word&~PLANE_CTL_FORMAT_MASK)|DISPLAY_PLANE_ENABLE|PLANE_CTL_FORMAT_XRGB_8888);
	//|PLANE_CTL_ORDER_RGBX
	g_mode.FrameBufferBase=g_private.FbBase;
	g_mode.FrameBufferSize=stride*vertical_active;
		
	//test pattern
	//there is just one page wrapping around... why?
	//we have intel_vgpu_mmap in effect so the correct range is mmaped host vmem
	//and the host vmem is actually one-page!
	//((UINT32*)g_private.FbBase)[-1]=0x00010203;
	//there is a mechanism called `get_pages` that seems to put main memory behind the aperture or sth
	//the page is the scratch page that unmapped GTT entries point to
	//we need to set up a GTT for our framebuffer: https://bwidawsk.net/blog/index.php/2014/06/the-global-gtt-part-1/
	//UINT32 cnt=0;
	//for(cnt=0;cnt<256*16;cnt++){
	//	((UINT32*)g_private.FbBase)[cnt]=0x00010203;
	//}
	//for(cnt=0;cnt<256*4;cnt++){
	//	UINT32 c=cnt&255;
	//	((UINT32*)g_private.FbBase)[cnt]=((cnt+256)&256?c:0)+((cnt+256)&512?c<<8:0)+((cnt+256)&1024?c<<16:0);
	//}
	//DebugPrint(EFI_D_ERROR,"i915: wrap test %08x %08x %08x %08x\n",((UINT32*)g_private.FbBase)[1024],((UINT32*)g_private.FbBase)[1025],((UINT32*)g_private.FbBase)[1026],((UINT32*)g_private.FbBase)[1027]);
	////
	//UEFI thinks it's BAR1
	//UINT32 cnt=0;
	//for(UINT32 y=0;y<vertical_active;y+=1){
	//	for(UINT32 x=0;x<horizontal_active;x+=1){
	//		UINT32 data=(((x<<8)/horizontal_active)<<16)|(((y<<8)/vertical_active)<<8);
	//		((UINT32*)g_private.FbBase)[cnt]=(data&0xffff00);
	//		cnt++;
	//	}
	//}
	//write32(_DSPACNTR,DISPLAY_PLANE_ENABLE|DISPPLANE_BGRX888);
	DebugPrint(EFI_D_ERROR,"i915: plane enabled, dspcntr: %08x, FbBase: %p\n",read32(_DSPACNTR),g_private.FbBase);
	
	//blt stuff
	EFI_STATUS Status;
	Status = FrameBufferBltConfigure (
	           (VOID*)g_private.FbBase,
	           g_mode_info,
	           g_i915FrameBufferBltConfigure,
	           &g_i915FrameBufferBltConfigureSize
	           );

	if (Status == RETURN_BUFFER_TOO_SMALL) {
	  if (g_i915FrameBufferBltConfigure != NULL) {
	    FreePool (g_i915FrameBufferBltConfigure);
	  }
	  g_i915FrameBufferBltConfigure = AllocatePool (g_i915FrameBufferBltConfigureSize);
	  if (g_i915FrameBufferBltConfigure == NULL) {
	    g_i915FrameBufferBltConfigureSize = 0;
	    return EFI_OUT_OF_RESOURCES;
	  }

	  Status = FrameBufferBltConfigure (
	             (VOID*)g_private.FbBase,
	             g_mode_info,
	             g_i915FrameBufferBltConfigure,
	             &g_i915FrameBufferBltConfigureSize
	             );
	}
	if( EFI_ERROR(Status) ){
		DebugPrint(EFI_D_ERROR,"i915: failed to setup blt\n");
	}
	
	return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI i915GraphicsOutputBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer, OPTIONAL
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION     BltOperation,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
{
	//return EFI_SUCCESS;
	EFI_STATUS Status=FrameBufferBlt (
	         g_i915FrameBufferBltConfigure,
	         BltBuffer,
	         BltOperation,
	         SourceX,
	         SourceY,
	         DestinationX,
	         DestinationY,
	         Width,
	         Height,
	         Delta
	         );
	DebugPrint(EFI_D_ERROR,"i915: blt %d %d,%d %dx%d\n",Status,DestinationX,DestinationY,Width,Height);
	return Status;
}

STATIC UINT8 edid_fallback[]={
	0,255,255,255,255,255,255,0,34,240,84,41,1,0,0,0,4,23,1,4,165,52,32,120,35,252,129,164,85,77,157,37,18,80,84,33,8,0,209,192,129,192,129,64,129,128,149,0,169,64,179,0,1,1,26,29,0,128,81,208,28,32,64,128,53,0,77,187,16,0,0,30,0,0,0,254,0,55,50,48,112,32,32,32,32,32,32,32,32,10,0,0,0,253,0,24,60,24,80,17,0,10,32,32,32,32,32,32,0,0,0,252,0,72,80,32,90,82,95,55,50,48,112,10,32,32,0,161
};


EFI_STATUS EFIAPI i915ControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
	EFI_TPL                           OldTpl;
	EFI_STATUS                        Status;
	I915_VIDEO_PRIVATE_DATA           *Private;
	PCI_TYPE00          Pci;
	
	OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
	DebugPrint(EFI_D_ERROR,"i915: start\n");
	
	Private = &g_private;
	
	Private->Signature  = SIGNATURE_32('i','9','1','5');
	
	Status = gBS->OpenProtocol (
	                Controller,
	                &gEfiPciIoProtocolGuid,
	                (VOID **) &Private->PciIo,
	                This->DriverBindingHandle,
	                Controller,
	                EFI_OPEN_PROTOCOL_BY_DRIVER
	                );
	if (EFI_ERROR (Status)) {
	  goto RestoreTpl;
	}
	
	Status = Private->PciIo->Pci.Read (
	                      Private->PciIo,
	                      EfiPciIoWidthUint32,
	                      0,
	                      sizeof (Pci) / sizeof (UINT32),
	                      &Pci
	                      );
	if (EFI_ERROR (Status)) {
	  goto ClosePciIo;
	}
	
	Status = Private->PciIo->Attributes (
	                          Private->PciIo,
	                          EfiPciIoAttributeOperationEnable,
	                          EFI_PCI_DEVICE_ENABLE,// | EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY,
	                          NULL
	                          );
	if (EFI_ERROR (Status)) {
	  goto ClosePciIo;
	}
	
	DebugPrint(EFI_D_ERROR,"i915: set pci attrs\n");
	
	//
	// Get ParentDevicePath
	//
	EFI_DEVICE_PATH_PROTOCOL          *ParentDevicePath;
	Status = gBS->HandleProtocol (
	                Controller,
	                &gEfiDevicePathProtocolGuid,
	                (VOID **) &ParentDevicePath
	                );
	if (EFI_ERROR (Status)) {
	  goto ClosePciIo;
	}

	//
	// Set Gop Device Path
	//
	ACPI_ADR_DEVICE_PATH              AcpiDeviceNode;
	ZeroMem (&AcpiDeviceNode, sizeof (ACPI_ADR_DEVICE_PATH));
	AcpiDeviceNode.Header.Type = ACPI_DEVICE_PATH;
	AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
	AcpiDeviceNode.ADR = ACPI_DISPLAY_ADR (1, 0, 0, 1, 0, ACPI_ADR_DISPLAY_TYPE_VGA, 0, 0);
	SetDevicePathNodeLength (&AcpiDeviceNode.Header, sizeof (ACPI_ADR_DEVICE_PATH));

	Private->GopDevicePath = AppendDevicePathNode (
	                                    ParentDevicePath,
	                                    (EFI_DEVICE_PATH_PROTOCOL *) &AcpiDeviceNode
	                                    );
	if (Private->GopDevicePath == NULL) {
	  Status = EFI_OUT_OF_RESOURCES;
	  goto ClosePciIo;
	}
	DebugPrint(EFI_D_ERROR,"i915: made gop path\n");
	
	//
	// Create new child handle and install the device path protocol on it.
	//
	Status = gBS->InstallMultipleProtocolInterfaces (
	                &Private->Handle,
	                &gEfiDevicePathProtocolGuid,
	                Private->GopDevicePath,
	                NULL
	                );
	if (EFI_ERROR (Status)) {
	  goto FreeGopDevicePath;
	}
	DebugPrint(EFI_D_ERROR,"i915: installed child handle\n");
	
	// query EDID and initialize the mode
	Status = ReadEDID(&g_private.edid);
	if (EFI_ERROR (Status)) {
		DebugPrint(EFI_D_ERROR,"i915: failed to read EDID\n");
		for(UINT32 i=0;i<128;i++){
			((UINT8*)&g_private.edid)[i]=edid_fallback[i];
		}
	}
	DebugPrint(EFI_D_ERROR,"i915: got EDID:\n");
	for(UINT32 i=0;i<16;i++){
		for(UINT32 j=0;j<8;j++){
			DebugPrint(EFI_D_ERROR,"%02x ",((UINT8*)(&g_private.edid))[i*8+j]);
		}
		DebugPrint(EFI_D_ERROR,"\n");
	}
	UINT32 pixel_clock = (UINT32)(g_private.edid.detailTimings[0].pixelClock) * 10;
	UINT32 x_active = g_private.edid.detailTimings[0].horzActive | ((UINT32)(g_private.edid.detailTimings[0].horzActiveBlankMsb >> 4) << 8);
	UINT32 y_active =  g_private.edid.detailTimings[0].vertActive | ((UINT32)(g_private.edid.detailTimings[0].vertActiveBlankMsb >> 4) << 8);
	DebugPrint(EFI_D_ERROR,"i915: %ux%u clock=%u\n",x_active,y_active,pixel_clock);
	g_mode_info[0].HorizontalResolution=x_active;
	g_mode_info[0].VerticalResolution=y_active;
	g_mode_info[0].PixelsPerScanLine = ((x_active*4+63)&-64)>>2;
	g_mode_info[0].PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
	//disable VGA
	UINT32 vgaword=read32(VGACNTRL);
	write32(VGACNTRL,(vgaword&~VGA_2X_MODE)|VGA_DISP_DISABLE);
	DebugPrint(EFI_D_ERROR,"i915: bars %08x %08x %08x %08x\n",Pci.Device.Bar[0],Pci.Device.Bar[1],Pci.Device.Bar[2],Pci.Device.Bar[3]);
	//get BAR 0 address and size
	EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *bar0Desc;
	Private->PciIo->GetBarAttributes (
		Private->PciIo,
		PCI_BAR_IDX0,
		NULL,
		(VOID**) &bar0Desc
	);
	EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *bar2Desc;
	Private->PciIo->GetBarAttributes (
		Private->PciIo,
		PCI_BAR_IDX1,
		NULL,
		(VOID**) &bar2Desc
	);
	DebugPrint(EFI_D_ERROR,"i915: bar ranges - %llx %llx, %llx %llx\n",
		bar0Desc->AddrRangeMin,bar0Desc->AddrLen,
		bar2Desc->AddrRangeMin,bar2Desc->AddrLen);
	UINT32 bar0Size=bar0Desc->AddrLen;
	EFI_PHYSICAL_ADDRESS mmio_base = bar0Desc->AddrRangeMin;
	
	//get BAR 2 address
	EFI_PHYSICAL_ADDRESS aperture_base = bar2Desc->AddrRangeMin;
	DebugPrint(EFI_D_ERROR,"i915: aperture at %p\n",aperture_base);
	//Private->PciIo->Pci.Write (Private->PciIo,EfiPciIoWidthUint32,0x18,1,&aperture_base);
	//Private->PciIo->Pci.Read (Private->PciIo,EfiPciIoWidthUint32,0x18,1,&bar_work);
	//DebugPrint(EFI_D_ERROR,"i915: aperture confirmed at %016x\n",bar_work);
	//GVT-g gmadr issue
	g_private.gmadr=0;
	if(read64(0x78000)==0x4776544776544776ULL){
		g_private.gmadr=read32(0x78040);
		//apertureSize=read32(0x78044);
	}
	DebugPrint(EFI_D_ERROR,"i915: gmadr = %08x, size = %08x, hgmadr = %08x, hsize = %08x\n",
		g_private.gmadr,read32(0x78044),read32(0x78048),read32(0x7804c));

	//create Global GTT entries to actually back the framebuffer
	g_private.FbBase=aperture_base+(UINT64)(g_private.gmadr);
	UINTN MaxFbSize=((x_active*4+64)&-64)*y_active;
	UINTN Pages = EFI_SIZE_TO_PAGES ((MaxFbSize+65535)&-65536);
	EFI_PHYSICAL_ADDRESS fb_backing=(EFI_PHYSICAL_ADDRESS)AllocateReservedPages(Pages);
	if(!fb_backing){
		DebugPrint(EFI_D_ERROR,"i915: failed to allocate framebuffer\n");
		Status=EFI_OUT_OF_RESOURCES;
		goto FreeGopDevicePath;
	}
	EFI_PHYSICAL_ADDRESS ggtt_base=mmio_base+(bar0Size>>1);
	UINT64* ggtt=(UINT64*)ggtt_base;
	DebugPrint(EFI_D_ERROR,"i915: ggtt_base at %p, entries: %08x %08x, backing fb: %p, %x bytes\n",ggtt_base,ggtt[0],ggtt[g_private.gmadr>>12],fb_backing,MaxFbSize);
	for(UINTN i=0;i<MaxFbSize;i+=4096){
		//create one PTE entry for each page
		//cache is whatever cache used by the linux driver on my host
		EFI_PHYSICAL_ADDRESS addr=fb_backing+i;
		ggtt[(g_private.gmadr+i)>>12]=((UINT32)(addr>>32)&0x7F0u)|((UINT32)addr&0xFFFFF000u)|11;
	}

	//TODO: setup OpRegion from fw_cfg (IgdAssignmentDxe), turn on backlight, after DPLL
	
	//
	// Start the GOP software stack.
	//
	EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;
	GraphicsOutput            = &Private->GraphicsOutput;
	GraphicsOutput->QueryMode = i915GraphicsOutputQueryMode;
	GraphicsOutput->SetMode   = i915GraphicsOutputSetMode;
	GraphicsOutput->Blt       = i915GraphicsOutputBlt;
	GraphicsOutput->Mode = &g_mode;
	Status = GraphicsOutput->SetMode (GraphicsOutput, 0);
	if (EFI_ERROR (Status)) {
		goto FreeGopDevicePath;
	}
	
	
	Status = gBS->InstallMultipleProtocolInterfaces (
	                &Private->Handle,
	                &gEfiGraphicsOutputProtocolGuid,
	                &Private->GraphicsOutput,
	                NULL
	                );
	if (EFI_ERROR (Status)) {
	  goto Destructi915Graphics;
	}
	
	//
	// Reference parent handle from child handle.
	//
	EFI_PCI_IO_PROTOCOL               *ChildPciIo;
	Status = gBS->OpenProtocol (
	              Controller,
	              &gEfiPciIoProtocolGuid,
	              (VOID **) &ChildPciIo,
	              This->DriverBindingHandle,
	              Private->Handle,
	              EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
	              );
	if (EFI_ERROR (Status)) {
	  goto UninstallGop;
	}
	
	DebugPrint(EFI_D_ERROR,"i915: gop ready\n");
	
	gBS->RestoreTPL (OldTpl);
	return EFI_SUCCESS;

UninstallGop:
	gBS->UninstallProtocolInterface (Private->Handle,
           &gEfiGraphicsOutputProtocolGuid, &Private->GraphicsOutput);

Destructi915Graphics:

ClosePciIo:
	gBS->CloseProtocol (Controller, &gEfiPciIoProtocolGuid,
           This->DriverBindingHandle, Controller);

FreeGopDevicePath:
	FreePool (Private->GopDevicePath);
	
RestoreTpl:
	gBS->RestoreTPL (OldTpl);
	return Status;
}

EFI_STATUS EFIAPI i915ControllerDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN UINTN                          NumberOfChildren,
  IN EFI_HANDLE                     *ChildHandleBuffer
  )
{
	DebugPrint(EFI_D_ERROR,"i915ControllerDriverStop\n");
	//TODO
	return EFI_UNSUPPORTED;
}


EFI_STATUS EFIAPI i915ControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
	EFI_STATUS          Status;
	EFI_PCI_IO_PROTOCOL *PciIo;
	PCI_TYPE00          Pci;
	EFI_DEV_PATH        *Node;

	//
	// Open the PCI I/O Protocol
	//
	Status = gBS->OpenProtocol (
	                Controller,
	                &gEfiPciIoProtocolGuid,
	                (VOID **) &PciIo,
	                This->DriverBindingHandle,
	                Controller,
	                EFI_OPEN_PROTOCOL_BY_DRIVER
	                );
	if (EFI_ERROR (Status)) {
	  return Status;
	}

	//
	// Read the PCI Configuration Header from the PCI Device
	//
	Status = PciIo->Pci.Read (
	                      PciIo,
	                      EfiPciIoWidthUint32,
	                      0,
	                      sizeof (Pci) / sizeof (UINT32),
	                      &Pci
	                      );
	if (EFI_ERROR (Status)) {
	  goto Done;
	}

	Status = EFI_UNSUPPORTED;
	if (Pci.Hdr.VendorId == 0x8086&&IS_PCI_DISPLAY(&Pci)){
		Status = EFI_SUCCESS;
		//
		// If this is an Intel graphics controller,
		// go further check RemainingDevicePath validation
		//
		if (RemainingDevicePath != NULL) {
		  Node = (EFI_DEV_PATH *) RemainingDevicePath;
		  //
		  // Check if RemainingDevicePath is the End of Device Path Node, 
		  // if yes, return EFI_SUCCESS
		  //
		  if (!IsDevicePathEnd (Node)) {
		    //
		    // If RemainingDevicePath isn't the End of Device Path Node,
		    // check its validation
		    //
		    if (Node->DevPath.Type != ACPI_DEVICE_PATH ||
		        Node->DevPath.SubType != ACPI_ADR_DP ||
		        DevicePathNodeLength(&Node->DevPath) != sizeof(ACPI_ADR_DEVICE_PATH)) {
		      Status = EFI_UNSUPPORTED;
		    }
		  }
		}
		if(Status==EFI_SUCCESS){
			DebugPrint(EFI_D_ERROR,"i915: found device %04x-%04x %p\n",Pci.Hdr.VendorId,Pci.Hdr.DeviceId,RemainingDevicePath);
			//DebugPrint(EFI_D_ERROR,"i915: bars %08x %08x %08x %08x\n",Pci.Device.Bar[0],Pci.Device.Bar[1],Pci.Device.Bar[2],Pci.Device.Bar[3]);
			//Status=EFI_UNSUPPORTED;
		}
	}
	
Done:
	gBS->CloseProtocol (
	      Controller,
	      &gEfiPciIoProtocolGuid,
	      This->DriverBindingHandle,
	      Controller
	      );
	return Status;
}

EFI_DRIVER_BINDING_PROTOCOL gi915DriverBinding = {
  i915ControllerDriverSupported,
  i915ControllerDriverStart,
  i915ControllerDriverStop,
  0x10,
  NULL,
  NULL
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mi915DriverNameTable[] = {
  { "eng;en", L"i915 Driver" },
  { NULL , NULL }
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mi915ControllerNameTable[] = {
  { "eng;en", L"i915 PCI Thing" },
  { NULL , NULL }
};

GLOBAL_REMOVE_IF_UNREFERENCED extern EFI_COMPONENT_NAME_PROTOCOL  gi915ComponentName;

EFI_STATUS
EFIAPI
i915ComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mi915DriverNameTable,
           DriverName,
           (BOOLEAN)(This == &gi915ComponentName)
           );
}

EFI_STATUS
EFIAPI
i915ComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL                     *This,
  IN  EFI_HANDLE                                      ControllerHandle,
  IN  EFI_HANDLE                                      ChildHandle        OPTIONAL,
  IN  CHAR8                                           *Language,
  OUT CHAR16                                          **ControllerName
  )
{
  EFI_STATUS                      Status;

  //
  // This is a device driver, so ChildHandle must be NULL.
  //
  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  //
  // Make sure this driver is currently managing ControllHandle
  //
  Status = EfiTestManagedDevice (
             ControllerHandle,
             gi915DriverBinding.DriverBindingHandle,
             &gEfiPciIoProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mi915ControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &gi915ComponentName)
           );
}

GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL  gi915ComponentName = {
  i915ComponentNameGetDriverName,
  i915ComponentNameGetControllerName,
  "eng"
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gi915ComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) i915ComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) i915ComponentNameGetControllerName,
  "en"
};

EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL gi915SupportedEfiVersion = {
  sizeof (EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL), // Size of Protocol structure.
  0                                                   // Version number to be filled at start up.
};

EFI_STATUS EFIAPI efi_main (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
	////////////
	DebugPrint (EFI_D_ERROR, "Driver starts!\n");
	EFI_STATUS Status;
	Status = EfiLibInstallDriverBindingComponentName2 (
	           ImageHandle,
	           SystemTable,
	           &gi915DriverBinding,
	           ImageHandle,
	           &gi915ComponentName,
	           &gi915ComponentName2
	           );
	ASSERT_EFI_ERROR (Status);
	
	gi915SupportedEfiVersion.FirmwareVersion = PcdGet32 (PcdDriverSupportedEfiVersion);
	Status = gBS->InstallMultipleProtocolInterfaces (
	                &ImageHandle,
	                &gEfiDriverSupportedEfiVersionProtocolGuid,
	                &gi915SupportedEfiVersion,
	                NULL
	                );
	ASSERT_EFI_ERROR (Status);
	
	return EFI_SUCCESS;
}
