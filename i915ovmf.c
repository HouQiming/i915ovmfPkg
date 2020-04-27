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
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include "QemuFwCfgLib.h"

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

//registers are in bar 0
//frame buffer is in bar 2
#define PCH_DISPLAY_BASE	0xc0000u

#define HSW_PWR_WELL_CTL1			(0x45400)
#define HSW_PWR_WELL_CTL2			(0x45404)
#define HSW_PWR_WELL_CTL3			(0x45408)
#define HSW_PWR_WELL_CTL4			(0x4540C)

#define GMBUS0 (PCH_DISPLAY_BASE+0x5100)
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
#define GMBUS4 (PCH_DISPLAY_BASE+0x5110)

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

/* Pipe B timing regs */
#define HTOTAL_B	0x61000
#define HBLANK_B	0x61004
#define HSYNC_B	0x61008
#define VTOTAL_B	0x6100c
#define VBLANK_B	0x61010
#define VSYNC_B	0x61014
#define PIPEBSRC	0x6101c
#define BCLRPAT_B	0x61020
#define VSYNCSHIFT_B	0x61028
#define PIPE_MULT_B	0x6102c

#define _PIPEACONF		0x70008
#define _PIPEBCONF		0x71008
#define   PIPECONF_ENABLE	(1 << 31)
#define   PIPECONF_DISABLE	0
#define   PIPECONF_DOUBLE_WIDE	(1 << 30)
#define   I965_PIPECONF_ACTIVE	(1 << 30)
#define   PIPECONF_DSI_PLL_LOCKED	(1 << 29) /* vlv & pipe A only */
#define   PIPECONF_FRAME_START_DELAY_MASK (3 << 27)
#define   PIPECONF_SINGLE_WIDE	0
#define   PIPECONF_PIPE_UNLOCKED 0
#define   PIPECONF_PIPE_LOCKED	(1 << 25)
#define   PIPECONF_FORCE_BORDER	(1 << 25)
#define   PIPECONF_GAMMA_MODE_MASK_I9XX	(1 << 24) /* gmch */
#define   PIPECONF_GAMMA_MODE_MASK_ILK	(3 << 24) /* ilk-ivb */
#define   PIPECONF_GAMMA_MODE_8BIT	(0 << 24) /* gmch,ilk-ivb */
#define   PIPECONF_GAMMA_MODE_10BIT	(1 << 24) /* gmch,ilk-ivb */
#define   PIPECONF_GAMMA_MODE_12BIT	(2 << 24) /* ilk-ivb */
#define   PIPECONF_GAMMA_MODE_SPLIT	(3 << 24) /* ivb */
#define   PIPECONF_GAMMA_MODE(x)	((x) << 24) /* pass in GAMMA_MODE_MODE_* */
#define   PIPECONF_GAMMA_MODE_SHIFT	24
#define   PIPECONF_INTERLACE_MASK	(7 << 21)
#define   PIPECONF_INTERLACE_MASK_HSW	(3 << 21)
/* Note that pre-gen3 does not support interlaced display directly. Panel
 * fitting must be disabled on pre-ilk for interlaced. */
#define   PIPECONF_PROGRESSIVE			(0 << 21)
#define   PIPECONF_INTERLACE_W_SYNC_SHIFT_PANEL	(4 << 21) /* gen4 only */
#define   PIPECONF_INTERLACE_W_SYNC_SHIFT	(5 << 21) /* gen4 only */
#define   PIPECONF_INTERLACE_W_FIELD_INDICATION	(6 << 21)
#define   PIPECONF_INTERLACE_FIELD_0_ONLY	(7 << 21) /* gen3 only */
/* Ironlake and later have a complete new set of values for interlaced. PFIT
 * means panel fitter required, PF means progressive fetch, DBL means power
 * saving pixel doubling. */
#define   PIPECONF_PFIT_PF_INTERLACED_ILK	(1 << 21)
#define   PIPECONF_INTERLACED_ILK		(3 << 21)
#define   PIPECONF_INTERLACED_DBL_ILK		(4 << 21) /* ilk/snb only */
#define   PIPECONF_PFIT_PF_INTERLACED_DBL_ILK	(5 << 21) /* ilk/snb only */
#define   PIPECONF_INTERLACE_MODE_MASK		(7 << 21)
#define   PIPECONF_EDP_RR_MODE_SWITCH		(1 << 20)
#define   PIPECONF_CXSR_DOWNCLOCK	(1 << 16)
#define   PIPECONF_EDP_RR_MODE_SWITCH_VLV	(1 << 14)
#define   PIPECONF_COLOR_RANGE_SELECT	(1 << 13)
#define   PIPECONF_BPC_MASK	(0x7 << 5)
#define   PIPECONF_8BPC		(0 << 5)
#define   PIPECONF_10BPC	(1 << 5)
#define   PIPECONF_6BPC		(2 << 5)
#define   PIPECONF_12BPC	(3 << 5)
#define   PIPECONF_DITHER_EN	(1 << 4)
#define   PIPECONF_DITHER_TYPE_MASK (0x0000000c)
#define   PIPECONF_DITHER_TYPE_SP (0 << 2)
#define   PIPECONF_DITHER_TYPE_ST1 (1 << 2)
#define   PIPECONF_DITHER_TYPE_ST2 (2 << 2)
#define   PIPECONF_DITHER_TYPE_TEMP (3 << 2)
#define _PIPEASTAT		0x70024
#define _PIPEBSTAT		0x71024
#define   PIPE_FIFO_UNDERRUN_STATUS		(1UL << 31)
#define   SPRITE1_FLIP_DONE_INT_EN_VLV		(1UL << 30)
#define   PIPE_CRC_ERROR_ENABLE			(1UL << 29)
#define   PIPE_CRC_DONE_ENABLE			(1UL << 28)
#define   PERF_COUNTER2_INTERRUPT_EN		(1UL << 27)
#define   PIPE_GMBUS_EVENT_ENABLE		(1UL << 27)
#define   PLANE_FLIP_DONE_INT_EN_VLV		(1UL << 26)
#define   PIPE_HOTPLUG_INTERRUPT_ENABLE		(1UL << 26)
#define   PIPE_VSYNC_INTERRUPT_ENABLE		(1UL << 25)
#define   PIPE_DISPLAY_LINE_COMPARE_ENABLE	(1UL << 24)
#define   PIPE_DPST_EVENT_ENABLE		(1UL << 23)
#define   SPRITE0_FLIP_DONE_INT_EN_VLV		(1UL << 22)
#define   PIPE_LEGACY_BLC_EVENT_ENABLE		(1UL << 22)
#define   PIPE_ODD_FIELD_INTERRUPT_ENABLE	(1UL << 21)
#define   PIPE_EVEN_FIELD_INTERRUPT_ENABLE	(1UL << 20)
#define   PIPE_B_PSR_INTERRUPT_ENABLE_VLV	(1UL << 19)
#define   PERF_COUNTER_INTERRUPT_EN		(1UL << 19)
#define   PIPE_HOTPLUG_TV_INTERRUPT_ENABLE	(1UL << 18) /* pre-965 */
#define   PIPE_START_VBLANK_INTERRUPT_ENABLE	(1UL << 18) /* 965 or later */
#define   PIPE_FRAMESTART_INTERRUPT_ENABLE	(1UL << 17)
#define   PIPE_VBLANK_INTERRUPT_ENABLE		(1UL << 17)
#define   PIPEA_HBLANK_INT_EN_VLV		(1UL << 16)
#define   PIPE_OVERLAY_UPDATED_ENABLE		(1UL << 16)
#define   SPRITE1_FLIP_DONE_INT_STATUS_VLV	(1UL << 15)
#define   SPRITE0_FLIP_DONE_INT_STATUS_VLV	(1UL << 14)
#define   PIPE_CRC_ERROR_INTERRUPT_STATUS	(1UL << 13)
#define   PIPE_CRC_DONE_INTERRUPT_STATUS	(1UL << 12)
#define   PERF_COUNTER2_INTERRUPT_STATUS	(1UL << 11)
#define   PIPE_GMBUS_INTERRUPT_STATUS		(1UL << 11)
#define   PLANE_FLIP_DONE_INT_STATUS_VLV	(1UL << 10)
#define   PIPE_HOTPLUG_INTERRUPT_STATUS		(1UL << 10)
#define   PIPE_VSYNC_INTERRUPT_STATUS		(1UL << 9)
#define   PIPE_DISPLAY_LINE_COMPARE_STATUS	(1UL << 8)
#define   PIPE_DPST_EVENT_STATUS		(1UL << 7)
#define   PIPE_A_PSR_STATUS_VLV			(1UL << 6)
#define   PIPE_LEGACY_BLC_EVENT_STATUS		(1UL << 6)
#define   PIPE_ODD_FIELD_INTERRUPT_STATUS	(1UL << 5)
#define   PIPE_EVEN_FIELD_INTERRUPT_STATUS	(1UL << 4)
#define   PIPE_B_PSR_STATUS_VLV			(1UL << 3)
#define   PERF_COUNTER_INTERRUPT_STATUS		(1UL << 3)
#define   PIPE_HOTPLUG_TV_INTERRUPT_STATUS	(1UL << 2) /* pre-965 */
#define   PIPE_START_VBLANK_INTERRUPT_STATUS	(1UL << 2) /* 965 or later */
#define   PIPE_FRAMESTART_INTERRUPT_STATUS	(1UL << 1)
#define   PIPE_VBLANK_INTERRUPT_STATUS		(1UL << 1)
#define   PIPE_HBLANK_INT_STATUS		(1UL << 0)
#define   PIPE_OVERLAY_UPDATED_STATUS		(1UL << 0)

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
#define PORT_B 1
#define PORT_C 2
#define PORT_D 3
#define PORT_E 4

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

#define DPLL_CTRL2				(0x6C05C)
#define  DPLL_CTRL2_DDI_CLK_OFF(port)		(1 << ((port) + 15))
#define  DPLL_CTRL2_DDI_CLK_SEL_MASK(port)	(3 << ((port) * 3 + 1))
#define  DPLL_CTRL2_DDI_CLK_SEL_SHIFT(port)    ((port) * 3 + 1)
#define  DPLL_CTRL2_DDI_CLK_SEL(clk, port)	((clk) << ((port) * 3 + 1))
#define  DPLL_CTRL2_DDI_SEL_OVERRIDE(port)     (1 << ((port) * 3))

#define _PICK_EVEN(__index, __a, __b) ((__a) + (__index) * ((__b) - (__a)))
#define _PORT(port, a, b)		_PICK_EVEN(port, a, b)

#define _DDI_BUF_TRANS_A		0x64E00
#define _DDI_BUF_TRANS_B		0x64E60
#define DDI_BUF_TRANS_LO(port, i)	(_PORT(port, _DDI_BUF_TRANS_A,_DDI_BUF_TRANS_B) + (i) * 8)
#define  DDI_BUF_BALANCE_LEG_ENABLE	(1 << 31)
#define DDI_BUF_TRANS_HI(port, i)	(_PORT(port, _DDI_BUF_TRANS_A,_DDI_BUF_TRANS_B) + (i) * 8 + 4)

#define DISPIO_CR_TX_BMU_CR0		(0x6C00C)
/* I_boost values */
#define BALANCE_LEG_SHIFT(port)		(8 + 3 * (port))
#define BALANCE_LEG_MASK(port)		(7 << (8 + 3 * (port)))
/* Balance leg disable bits */
#define BALANCE_LEG_DISABLE_SHIFT	23
#define BALANCE_LEG_DISABLE(port)	(1 << (23 + (port)))

#define _TRANS_CLK_SEL_A		0x46140
#define _TRANS_CLK_SEL_B		0x46144
/* For each transcoder, we need to select the corresponding port clock */
#define  TRANS_CLK_SEL_DISABLED		(0x0 << 29)
#define  TRANS_CLK_SEL_PORT(x)		(((x) + 1) << 29)

#define _LGC_PALETTE_A           0x4a000
#define _LGC_PALETTE_B           0x4a800

#define _SKL_BOTTOM_COLOR_A		0x70034
#define   SKL_BOTTOM_COLOR_GAMMA_ENABLE	(1 << 31)
#define   SKL_BOTTOM_COLOR_CSC_ENABLE	(1 << 30)

#define _GAMMA_MODE_A		0x4a480
#define _GAMMA_MODE_B		0x4ac80
#define  PRE_CSC_GAMMA_ENABLE	(1 << 31)
#define  POST_CSC_GAMMA_ENABLE	(1 << 30)
#define  GAMMA_MODE_MODE_8BIT	(0 << 0)
#define  GAMMA_MODE_MODE_10BIT	(1 << 0)
#define  GAMMA_MODE_MODE_12BIT	(2 << 0)
#define  GAMMA_MODE_MODE_SPLIT	(3 << 0)

#define SFUSE_STRAP			(0xc2014)
#define  SFUSE_STRAP_FUSE_LOCK		(1 << 13)
#define  SFUSE_STRAP_RAW_FREQUENCY	(1 << 8)
#define  SFUSE_STRAP_DISPLAY_DISABLED	(1 << 7)
#define  SFUSE_STRAP_CRT_DISABLED	(1 << 6)
#define  SFUSE_STRAP_DDIF_DETECTED	(1 << 3)
#define  SFUSE_STRAP_DDIB_DETECTED	(1 << 2)
#define  SFUSE_STRAP_DDIC_DETECTED	(1 << 1)
#define  SFUSE_STRAP_DDID_DETECTED	(1 << 0)

#define _DDI_BUF_CTL_A				0x64000
#define _DDI_BUF_CTL_B				0x64100
#define DDI_BUF_CTL(port) _PORT(port, _DDI_BUF_CTL_A, _DDI_BUF_CTL_B)
#define  DDI_BUF_CTL_ENABLE			(1 << 31)
#define  DDI_BUF_TRANS_SELECT(n)	((n) << 24)
#define  DDI_BUF_EMP_MASK			(0xf << 24)
#define  DDI_BUF_PORT_REVERSAL			(1 << 16)
#define  DDI_BUF_IS_IDLE			(1 << 7)
#define  DDI_A_4_LANES				(1 << 4)
#define  DDI_PORT_WIDTH(width)			(((width) - 1) << 1)
#define  DDI_PORT_WIDTH_MASK			(7 << 1)
#define  DDI_PORT_WIDTH_SHIFT			1
#define  DDI_INIT_DISPLAY_DETECTED		(1 << 0)

#define CHICKEN_TRANS_A		(0x420c0)
#define CHICKEN_TRANS_B		(0x420c4)
#define CHICKEN_TRANS_C		(0x420c8)
#define CHICKEN_TRANS_EDP	(0x420cc)
#define  VSC_DATA_SEL_SOFTWARE_CONTROL	(1 << 25) /* GLK and CNL+ */
#define  DDI_TRAINING_OVERRIDE_ENABLE	(1 << 19)
#define  DDI_TRAINING_OVERRIDE_VALUE	(1 << 18)
#define  DDIE_TRAINING_OVERRIDE_ENABLE	(1 << 17) /* CHICKEN_TRANS_A only */
#define  DDIE_TRAINING_OVERRIDE_VALUE	(1 << 16) /* CHICKEN_TRANS_A only */
#define  PSR2_ADD_VERTICAL_LINE_COUNT   (1 << 15)
#define  PSR2_VSC_ENABLE_PROG_HEADER    (1 << 12)

#define HSW_NDE_RSTWRN_OPT	(0x46408)
#define  RESET_PCH_HANDSHAKE_ENABLE	(1 << 4)

#define CDCLK_CTL			(0x46000)
#define  CDCLK_FREQ_SEL_MASK		(3 << 26)
#define  CDCLK_FREQ_450_432		(0 << 26)
#define  CDCLK_FREQ_540			(1 << 26)
#define  CDCLK_FREQ_337_308		(2 << 26)
#define  CDCLK_FREQ_675_617		(3 << 26)
#define  BXT_CDCLK_CD2X_DIV_SEL_MASK	(3 << 22)
#define  BXT_CDCLK_CD2X_DIV_SEL_1	(0 << 22)
#define  BXT_CDCLK_CD2X_DIV_SEL_1_5	(1 << 22)
#define  BXT_CDCLK_CD2X_DIV_SEL_2	(2 << 22)
#define  BXT_CDCLK_CD2X_DIV_SEL_4	(3 << 22)
#define  BXT_CDCLK_CD2X_PIPE(pipe)	((pipe) << 20)
#define  CDCLK_DIVMUX_CD_OVERRIDE	(1 << 19)
#define  BXT_CDCLK_CD2X_PIPE_NONE	BXT_CDCLK_CD2X_PIPE(3)
#define  ICL_CDCLK_CD2X_PIPE_NONE	(7 << 19)
#define  BXT_CDCLK_SSA_PRECHARGE_ENABLE	(1 << 16)
#define  CDCLK_FREQ_DECIMAL_MASK	(0x7ff)

#define DBUF_CTL	(0x45008)
#define DBUF_CTL_S1	(0x45008)
#define DBUF_CTL_S2	(0x44FE8)
#define  DBUF_POWER_REQUEST		(1 << 31)
#define  DBUF_POWER_STATE		(1 << 30)

#define MBUS_ABOX_CTL			(0x45038)
#define MBUS_ABOX_BW_CREDIT_MASK	(3 << 20)
#define MBUS_ABOX_BW_CREDIT(x)		((x) << 20)
#define MBUS_ABOX_B_CREDIT_MASK		(0xF << 16)
#define MBUS_ABOX_B_CREDIT(x)		((x) << 16)
#define MBUS_ABOX_BT_CREDIT_POOL2_MASK	(0x1F << 8)
#define MBUS_ABOX_BT_CREDIT_POOL2(x)	((x) << 8)
#define MBUS_ABOX_BT_CREDIT_POOL1_MASK	(0x1F << 0)
#define MBUS_ABOX_BT_CREDIT_POOL1(x)	((x) << 0)

#define _PLANE_BUF_CFG_1_A			0x7027c

#define I915_READ read32
#define I915_WRITE write32

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
  UINT32 is_gvt;
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
			DebugPrint(EFI_D_ERROR,"i915: gmbus timeout\n");
			return EFI_DEVICE_ERROR;
		}
		if(status&GMBUS_SATOER){
			//failed
			DebugPrint(EFI_D_ERROR,"i915: gmbus error\n");
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
	for(pin=1;pin<=6;pin++){
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
//static const struct intel_limit g_limits = {
//	.dot = { .min = 20000, .max = 400000 },
//	.vco = { .min = 1400000, .max = 2800000 },
//	.n = { .min = 1, .max = 6 },
//	.m = { .min = 70, .max = 120 },
//	.m1 = { .min = 8, .max = 18 },
//	.m2 = { .min = 3, .max = 7 },
//	.p = { .min = 5, .max = 80 },
//	.p1 = { .min = 1, .max = 8 },
//	.p2 = { .dot_limit = 200000,
//		.p2_slow = 10, .p2_fast = 5 },
//};

#define DPLL_CTRL1		(0x6C058)
#define  DPLL_CTRL1_HDMI_MODE(id)		(1 << ((id) * 6 + 5))
#define  DPLL_CTRL1_SSC(id)			(1 << ((id) * 6 + 4))
#define  DPLL_CTRL1_LINK_RATE_MASK(id)		(7 << ((id) * 6 + 1))
#define  DPLL_CTRL1_LINK_RATE_SHIFT(id)		((id) * 6 + 1)
#define  DPLL_CTRL1_LINK_RATE(linkrate, id)	((linkrate) << ((id) * 6 + 1))
#define  DPLL_CTRL1_OVERRIDE(id)		(1 << ((id) * 6))
#define  DPLL_CTRL1_LINK_RATE_2700		0
#define  DPLL_CTRL1_LINK_RATE_1350		1
#define  DPLL_CTRL1_LINK_RATE_810		2
#define  DPLL_CTRL1_LINK_RATE_1620		3
#define  DPLL_CTRL1_LINK_RATE_1080		4
#define  DPLL_CTRL1_LINK_RATE_2160		5

#define DPLL_STATUS	(0x6C060)
#define  DPLL_LOCK(id) (1 << ((id) * 8))

#define LCPLL1_CTL		(0x46010)
#define LCPLL2_CTL		(0x46014)
#define  LCPLL_PLL_ENABLE	(1 << 31)

/* DPLL cfg */
#define _DPLL1_CFGCR1	0x6C040
#define _DPLL2_CFGCR1	0x6C048
#define _DPLL3_CFGCR1	0x6C050
#define  DPLL_CFGCR1_FREQ_ENABLE	(1 << 31)
#define  DPLL_CFGCR1_DCO_FRACTION_MASK	(0x7fff << 9)
#define  DPLL_CFGCR1_DCO_FRACTION(x)	((x) << 9)
#define  DPLL_CFGCR1_DCO_INTEGER_MASK	(0x1ff)

#define _DPLL1_CFGCR2	0x6C044
#define _DPLL2_CFGCR2	0x6C04C
#define _DPLL3_CFGCR2	0x6C054
#define  DPLL_CFGCR2_QDIV_RATIO_MASK	(0xff << 8)
#define  DPLL_CFGCR2_QDIV_RATIO(x)	((x) << 8)
#define  DPLL_CFGCR2_QDIV_MODE(x)	((x) << 7)
#define  DPLL_CFGCR2_KDIV_MASK		(3 << 5)
#define  DPLL_CFGCR2_KDIV(x)		((x) << 5)
#define  DPLL_CFGCR2_KDIV_5 (0 << 5)
#define  DPLL_CFGCR2_KDIV_2 (1 << 5)
#define  DPLL_CFGCR2_KDIV_3 (2 << 5)
#define  DPLL_CFGCR2_KDIV_1 (3 << 5)
#define  DPLL_CFGCR2_PDIV_MASK		(7 << 2)
#define  DPLL_CFGCR2_PDIV(x)		((x) << 2)
#define  DPLL_CFGCR2_PDIV_1 (0 << 2)
#define  DPLL_CFGCR2_PDIV_2 (1 << 2)
#define  DPLL_CFGCR2_PDIV_3 (2 << 2)
#define  DPLL_CFGCR2_PDIV_7 (4 << 2)
#define  DPLL_CFGCR2_CENTRAL_FREQ_MASK	(3)

struct skl_wrpll_params {
	UINT32 dco_fraction;
	UINT32 dco_integer;
	UINT32 qdiv_ratio;
	UINT32 qdiv_mode;
	UINT32 kdiv;
	UINT32 pdiv;
	UINT32 central_freq;
};

static const int even_dividers[] = {  4,  6,  8, 10, 12, 14, 16, 18, 20,
				     24, 28, 30, 32, 36, 40, 42, 44,
				     48, 52, 54, 56, 60, 64, 66, 68,
				     70, 72, 76, 78, 80, 84, 88, 90,
				     92, 96, 98 };
static const int odd_dividers[] = { 3, 5, 7, 9, 15, 21, 35 };
static const struct {
	const int *list;
	int n_dividers;
} dividers[] = {
	{ even_dividers, ARRAY_SIZE(even_dividers) },
	{ odd_dividers, ARRAY_SIZE(odd_dividers) },
};

struct skl_wrpll_context {
	UINT64 min_deviation;		/* current minimal deviation */
	UINT64 central_freq;		/* chosen central freq */
	UINT64 dco_freq;			/* chosen dco freq */
	UINT64 p;			/* chosen divider */
};

static void skl_wrpll_get_multipliers(UINT64 p,
				      UINT64 *p0 /* out */,
				      UINT64 *p1 /* out */,
				      UINT64 *p2 /* out */)
{
	/* even dividers */
	if (p % 2 == 0) {
		UINT64 half = p / 2;

		if (half == 1 || half == 2 || half == 3 || half == 5) {
			*p0 = 2;
			*p1 = 1;
			*p2 = half;
		} else if (half % 2 == 0) {
			*p0 = 2;
			*p1 = half / 2;
			*p2 = 2;
		} else if (half % 3 == 0) {
			*p0 = 3;
			*p1 = half / 3;
			*p2 = 2;
		} else if (half % 7 == 0) {
			*p0 = 7;
			*p1 = half / 7;
			*p2 = 2;
		}
	} else if (p == 3 || p == 9) {  /* 3, 5, 7, 9, 15, 21, 35 */
		*p0 = 3;
		*p1 = 1;
		*p2 = p / 3;
	} else if (p == 5 || p == 7) {
		*p0 = p;
		*p1 = 1;
		*p2 = 1;
	} else if (p == 15) {
		*p0 = 3;
		*p1 = 1;
		*p2 = 5;
	} else if (p == 21) {
		*p0 = 7;
		*p1 = 1;
		*p2 = 3;
	} else if (p == 35) {
		*p0 = 7;
		*p1 = 1;
		*p2 = 5;
	}
}

#define KHz(x) (1000 * (x))
#define MHz(x) KHz(1000 * (x))

static void skl_wrpll_params_populate(struct skl_wrpll_params *params,
				      UINT64 afe_clock,
				      UINT64 central_freq,
				      UINT64 p0, UINT64 p1, UINT64 p2)
{
	UINT64 dco_freq;

	switch (central_freq) {
	case 9600000000ULL:
		params->central_freq = 0;
		break;
	case 9000000000ULL:
		params->central_freq = 1;
		break;
	case 8400000000ULL:
		params->central_freq = 3;
	}

	switch (p0) {
	case 1:
		params->pdiv = 0;
		break;
	case 2:
		params->pdiv = 1;
		break;
	case 3:
		params->pdiv = 2;
		break;
	case 7:
		params->pdiv = 4;
		break;
	default:
		DebugPrint(EFI_D_ERROR,"Incorrect PDiv\n");
	}

	switch (p2) {
	case 5:
		params->kdiv = 0;
		break;
	case 2:
		params->kdiv = 1;
		break;
	case 3:
		params->kdiv = 2;
		break;
	case 1:
		params->kdiv = 3;
		break;
	default:
		DebugPrint(EFI_D_ERROR,"Incorrect KDiv\n");
	}

	params->qdiv_ratio = p1;
	params->qdiv_mode = (params->qdiv_ratio == 1) ? 0 : 1;

	dco_freq = p0 * p1 * p2 * afe_clock;

	/*
	 * Intermediate values are in Hz.
	 * Divide by MHz to match bsepc
	 */
	params->dco_integer = (dco_freq)/(24 * MHz(1));
	params->dco_fraction = (((dco_freq)/(24) - params->dco_integer * MHz(1)) * 0x8000)/(MHz(1));
}

/* DCO freq must be within +1%/-6%  of the DCO central freq */
#define SKL_DCO_MAX_PDEVIATION	100
#define SKL_DCO_MAX_NDEVIATION	600

static void skl_wrpll_try_divider(struct skl_wrpll_context *ctx,
				  UINT64 central_freq,
				  UINT64 dco_freq,
				  UINT64 divider)
{
	UINT64 deviation;
	INT64 abs_diff=(INT64)dco_freq-(INT64)central_freq;
	if(abs_diff<0){abs_diff=-abs_diff;}

	deviation = (10000 * (UINT64)abs_diff)/(central_freq);

	/* positive deviation */
	if (dco_freq >= central_freq) {
		if (deviation < SKL_DCO_MAX_PDEVIATION &&
		    deviation < ctx->min_deviation) {
			ctx->min_deviation = deviation;
			ctx->central_freq = central_freq;
			ctx->dco_freq = dco_freq;
			ctx->p = divider;
		}
	/* negative deviation */
	} else if (deviation < SKL_DCO_MAX_NDEVIATION &&
		   deviation < ctx->min_deviation) {
		ctx->min_deviation = deviation;
		ctx->central_freq = central_freq;
		ctx->dco_freq = dco_freq;
		ctx->p = divider;
	}
}

static UINT32 port=PORT_B;
static EFI_SYSTEM_TABLE     *g_SystemTable=NULL;
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
	
	write32(_PIPEACONF,0);
	
	//setup DPLL (old GPU, doesn't apply here)
	//UINT32 refclock = 96000;
	//UINT32 pixel_clock = (UINT32)(g_private.edid.detailTimings[0].pixelClock) * 10;
	//UINT32 multiplier = 1;
	////if(pixel_clock >= 100000) {
	////	multiplier = 1;
	////}else if(pixel_clock >= 50000) {
	////	multiplier = 2;
	////}else{
	////	//assert(pixel_clock >= 25000);
	////	multiplier = 4;
	////}
	//struct dpll final_params,params;
	//INT32 target=(INT32)(pixel_clock * multiplier);
	//INT32 best_err=target;
	//DebugPrint(EFI_D_ERROR,"i915: before DPLL compute\n");
	//for(params.n=g_limits.n.min;params.n<=g_limits.n.max;params.n++)
	//for(params.m1=g_limits.m1.max;params.m1>=g_limits.m1.min;params.m1--)
	//for(params.m2=g_limits.m2.max;params.m2>=g_limits.m2.min;params.m2--)
	//for(params.p1=g_limits.p1.max;params.p1>=g_limits.p1.min;params.p1--)
	//for(params.p2=g_limits.p2.p2_slow;params.p2>=g_limits.p2.p2_fast;params.p2-=5){
	//	if(params.p2!=5&&params.p2!=7&&params.p2!=10&&params.p2!=14){continue;}
	//	params.m = 5 * (params.m1 + 2) + (params.m2 + 2);
	//	params.p = params.p1*params.p2;
	//	if(params.m < g_limits.m.min || params.m > g_limits.m.max){continue;}
	//	if(params.p < g_limits.p.min || params.p > g_limits.p.max){continue;}
	//	params.vco = (refclock * params.m + (params.n + 2) / 2) / (params.n + 2);
	//	params.dot = (params.vco + params.p / 2) / params.p;
	//	if(params.dot < g_limits.dot.min || params.dot > g_limits.dot.max){continue;}
	//	if(params.vco < g_limits.vco.min || params.vco > g_limits.vco.max){continue;}
	//	INT32 err=(INT32)params.dot-target;
	//	if(err<0){err=-err;}
	//	if(best_err>err){
	//		best_err=err;
	//		final_params=params;
	//	}
	//}
	
	//params=final_params;
	
	//DebugPrint(EFI_D_ERROR,"i915: DPLL params: n=%d m1=%d m2=%d p1=%d p2=%d\n",
	//	params.n,params.m1,params.m2,params.p1,params.p2);
	//DebugPrint(EFI_D_ERROR,"i915: DPLL params: m=%d p=%d vco=%d dot=%d, target=%d\n",
	//	params.m,params.p,params.vco,params.dot,target);
	
	//write32(_FPA0, params.n << 16 | params.m1 << 8 | params.m2);
	//write32(_FPA1, params.n << 16 | params.m1 << 8 | params.m2);
	
	//write32(_DPLL_A, 0);
	
	////UINT32 dplla=DPLLB_MODE_DAC_SERIAL | DPLL_VGA_MODE_DIS | DPLL_SDVO_HIGH_SPEED | DPLL_VCO_ENABLE;
	//UINT32 dplla=DPLLB_MODE_DAC_SERIAL | DPLL_VGA_MODE_DIS | DPLL_VCO_ENABLE;
	//dplla |= (1 << (params.p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT;
	//switch (params.p2) {
	//case 5:
	//	dplla |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_5;
	//	break;
	//case 7:
	//	dplla |= DPLLB_LVDS_P2_CLOCK_DIV_7;
	//	break;
	//case 10:
	//	dplla |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_10;
	//	break;
	//case 14:
	//	dplla |= DPLLB_LVDS_P2_CLOCK_DIV_14;
	//	break;
	//}
	//dplla |= (6 << PLL_LOAD_PULSE_PHASE_SHIFT);
	////this is 0 anyway
	//dplla |= PLL_REF_INPUT_DREFCLK;
	
	//write32(_DPLL_A, dplla);
	//read32(_DPLL_A);
	//DebugPrint(EFI_D_ERROR,"i915: DPLL set %08x, read %08x\n",dplla,read32(_DPLL_A));
	
	////it's pointless to wait in GVT-g
	//if(!g_private.is_gvt){
	//	//MicroSecondDelay is unusable
	//	for(UINT32 counter=0;counter<16384;counter++){
	//		read32(_DPLL_A);
	//	}
	//}
	
	//write32(_DPLL_A_MD, (multiplier-1)<<DPLL_MD_UDI_MULTIPLIER_SHIFT);
	//DebugPrint(EFI_D_ERROR,"i915: DPLL_MD set\n");
	
	//for(int i = 0; i < 3; i++) {
	//	write32(_DPLL_A, dplla);
	//	read32(_DPLL_A);
	
	//	if(!g_private.is_gvt){
	//		for(UINT32 counter=0;counter<16384;counter++){
	//			read32(_DPLL_A);
	//		}
	//	}
	//}
	//DebugPrint(EFI_D_ERROR,"i915: DPLL all set %08x, read %08x\n",dplla,read32(_DPLL_A));
	
	//SkyLake shared DPLL sequence: it's completely different!
	/* DPLL 1 */
	//.ctl = LCPLL2_CTL,
	//.cfgcr1 = _DPLL1_CFGCR1,
	//.cfgcr2 = _DPLL1_CFGCR2,
	
	//intel_encoders_pre_pll_enable(crtc, pipe_config, old_state);
	
	UINT32 ctrl1, cfgcr1, cfgcr2;
	struct skl_wrpll_params wrpll_params = { 0, };
	
	/*
	 * See comment in intel_dpll_hw_state to understand why we always use 0
	 * as the DPLL id in this function.
	 */
	ctrl1 = DPLL_CTRL1_OVERRIDE(0);
	ctrl1 |= DPLL_CTRL1_HDMI_MODE(0);
	
	{
		//clock in Hz
		UINT64 clock=(UINT64)(g_private.edid.detailTimings[0].pixelClock)*10000;
		UINT64 afe_clock = clock * 5; /* AFE Clock is 5x Pixel clock */
		UINT64 dco_central_freq[3] = { 8400000000ULL, 9000000000ULL, 9600000000ULL };
		
		struct skl_wrpll_context ctx={0};
		UINT64 dco, d, i;
		UINT64 p0, p1, p2;
	
		ctx.min_deviation = 1ULL<<62;
	
		for (d = 0; d < ARRAY_SIZE(dividers); d++) {
			for (dco = 0; dco < ARRAY_SIZE(dco_central_freq); dco++) {
				for (i = 0; i < dividers[d].n_dividers; i++) {
					UINT64 p = dividers[d].list[i];
					UINT64 dco_freq = p * afe_clock;
	
					skl_wrpll_try_divider(&ctx,
							      dco_central_freq[dco],
							      dco_freq,
							      p);
					/*
					 * Skip the remaining dividers if we're sure to
					 * have found the definitive divider, we can't
					 * improve a 0 deviation.
					 */
					if (ctx.min_deviation == 0)
						goto skip_remaining_dividers;
				}
			}

	skip_remaining_dividers:
			/*
			 * If a solution is found with an even divider, prefer
			 * this one.
			 */
			if (d == 0 && ctx.p)
				break;
		}
		
		if (!ctx.p) {
			DebugPrint(EFI_D_ERROR,"i915: No valid divider found for %dHz\n", clock);
			return EFI_UNSUPPORTED;
		}
		
		/*
		 * gcc incorrectly analyses that these can be used without being
		 * initialized. To be fair, it's hard to guess.
		 */
		p0 = p1 = p2 = 0;
		skl_wrpll_get_multipliers(ctx.p, &p0, &p1, &p2);
		skl_wrpll_params_populate(&wrpll_params, afe_clock, ctx.central_freq,
					  p0, p1, p2);
	}
	
	cfgcr1 = DPLL_CFGCR1_FREQ_ENABLE |
		DPLL_CFGCR1_DCO_FRACTION(wrpll_params.dco_fraction) |
		wrpll_params.dco_integer;
	
	cfgcr2 = DPLL_CFGCR2_QDIV_RATIO(wrpll_params.qdiv_ratio) |
		DPLL_CFGCR2_QDIV_MODE(wrpll_params.qdiv_mode) |
		DPLL_CFGCR2_KDIV(wrpll_params.kdiv) |
		DPLL_CFGCR2_PDIV(wrpll_params.pdiv) |
		wrpll_params.central_freq;
	
	UINT32 val = read32(DPLL_CTRL1);
	
	//it's clock id!
	//how's port clock comptued?
	//UINT64 clock_khz=(UINT64)(g_private.edid.detailTimings[0].pixelClock)*10;
	//UINT32 id=DPLL_CTRL1_LINK_RATE_810;
	//if(clock_khz>>1 >=135000){
	//	id=DPLL_CTRL1_LINK_RATE_1350;
	//}else if(clock_khz>>1 >=270000){
	//	id=DPLL_CTRL1_LINK_RATE_2700;
	//}
	//hack: anything else hangs
	UINT32 id=DPLL_CTRL1_LINK_RATE_1350;
	
	val &= ~(DPLL_CTRL1_HDMI_MODE(id) |
		 DPLL_CTRL1_SSC(id) |
		 DPLL_CTRL1_LINK_RATE_MASK(id));
	val |= ctrl1 << (id * 6);
	
	//DPLL 1
	write32(DPLL_CTRL1, val);
	read32(DPLL_CTRL1);
	
	write32(_DPLL1_CFGCR1, cfgcr1);
	write32(_DPLL1_CFGCR2, cfgcr2);
	read32(_DPLL1_CFGCR1);
	read32(_DPLL1_CFGCR2);
	
	//845 80400173 3a5
	DebugPrint(EFI_D_ERROR,"i915: DPLL_CTRL1 = %08x\n", read32(DPLL_CTRL1));
	DebugPrint(EFI_D_ERROR,"i915: _DPLL1_CFGCR1 = %08x\n", read32(_DPLL1_CFGCR1));
	DebugPrint(EFI_D_ERROR,"i915: _DPLL1_CFGCR2 = %08x\n", read32(_DPLL1_CFGCR2));
	
	/* the enable bit is always bit 31 */
	write32(LCPLL2_CTL, read32(LCPLL2_CTL) | LCPLL_PLL_ENABLE);
	
	for(UINT32 counter=0;;counter++){
		if(read32(DPLL_STATUS)&DPLL_LOCK(1)){
			DebugPrint(EFI_D_ERROR,"i915: DPLL %d locked\n", 1);
			break;
		}
		if(counter>16384){
			DebugPrint(EFI_D_ERROR,"i915: DPLL %d not locked\n", 1);
			break;
		}
	}
		
	//intel_encoders_pre_enable(crtc, pipe_config, old_state);
	//could be intel_ddi_pre_enable_hdmi
	//intel_ddi_clk_select(encoder, crtc_state);
	DebugPrint(EFI_D_ERROR,"i915: port is %d\n", port);
	{
		UINT32 val = read32(DPLL_CTRL2);
		
		//val &= ~(DPLL_CTRL2_DDI_CLK_OFF(PORT_A) |
		//	 DPLL_CTRL2_DDI_CLK_SEL_MASK(PORT_A));
		//val |= (DPLL_CTRL2_DDI_CLK_SEL(id, PORT_A) |
		//	DPLL_CTRL2_DDI_SEL_OVERRIDE(PORT_A));
		
		val &= ~(DPLL_CTRL2_DDI_CLK_OFF(port) |
			 DPLL_CTRL2_DDI_CLK_SEL_MASK(port));
		val |= (DPLL_CTRL2_DDI_CLK_SEL(id, port) |
			DPLL_CTRL2_DDI_SEL_OVERRIDE(port));
		
		write32(DPLL_CTRL2, val);
	}
	DebugPrint(EFI_D_ERROR,"i915: DPLL_CTRL2 = %08x\n", read32(DPLL_CTRL2));
	
	//intel_prepare_hdmi_ddi_buffers(encoder, level);
	//the driver doesn't seem to do this for port A
	write32(DDI_BUF_TRANS_LO(port, 9), 0x80003015u);
	write32(DDI_BUF_TRANS_HI(port, 9), 0xcdu);
	
	//intel_hdmi_prepare(encoder, pipe_config);
	//hdmi_reg=DDI_BUF_CTL(port)
	
	DebugPrint(EFI_D_ERROR,"i915: progressed to line %d\n", __LINE__);
	
	//it's Type C
	//icl_enable_phy_clock_gating(dig_port);
	
	//if (IS_GEN9_BC(dev_priv))
	//	skl_ddi_set_iboost(encoder, level, INTEL_OUTPUT_HDMI);
	{
		UINT32 tmp;
	
		tmp = read32(DISPIO_CR_TX_BMU_CR0);
		tmp &= ~(BALANCE_LEG_MASK(port) | BALANCE_LEG_DISABLE(port));
		tmp |= 1 << BALANCE_LEG_SHIFT(port);
		write32(DISPIO_CR_TX_BMU_CR0, tmp);
	}
	
	//intel_ddi_enable_pipe_clock(crtc_state);
	write32(_TRANS_CLK_SEL_A, TRANS_CLK_SEL_PORT(port));
	DebugPrint(EFI_D_ERROR,"i915: progressed to line %d, TRANS_CLK_SEL_PORT(port) is %08x\n", __LINE__, TRANS_CLK_SEL_PORT(port));
	
	//we got here	
	
	//intel_dig_port->set_infoframes(encoder,
	//			       crtc_state->has_infoframe,
	//			       crtc_state, conn_state);
	
	//if (intel_crtc_has_dp_encoder(pipe_config))
	//	intel_dp_set_m_n(pipe_config, M1_N1);
	
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
	UINT32 multiplier=1;
	write32(PIPE_MULT_A, multiplier - 1);
	
	DebugPrint(EFI_D_ERROR,"i915: HTOTAL_A (%x) = %08x\n",HTOTAL_A,read32(HTOTAL_A));
	DebugPrint(EFI_D_ERROR,"i915: HBLANK_A (%x) = %08x\n",HBLANK_A,read32(HBLANK_A));
	DebugPrint(EFI_D_ERROR,"i915: HSYNC_A (%x) = %08x\n",HSYNC_A,read32(HSYNC_A));
	DebugPrint(EFI_D_ERROR,"i915: VTOTAL_A (%x) = %08x\n",VTOTAL_A,read32(VTOTAL_A));
	DebugPrint(EFI_D_ERROR,"i915: VBLANK_A (%x) = %08x\n",VBLANK_A,read32(VBLANK_A));
	DebugPrint(EFI_D_ERROR,"i915: VSYNC_A (%x) = %08x\n",VSYNC_A,read32(VSYNC_A));
	DebugPrint(EFI_D_ERROR,"i915: PIPEASRC (%x) = %08x\n",PIPEASRC,read32(PIPEASRC));
	DebugPrint(EFI_D_ERROR,"i915: BCLRPAT_A (%x) = %08x\n",BCLRPAT_A,read32(BCLRPAT_A));
	DebugPrint(EFI_D_ERROR,"i915: VSYNCSHIFT_A (%x) = %08x\n",VSYNCSHIFT_A,read32(VSYNCSHIFT_A));
	DebugPrint(EFI_D_ERROR,"i915: PIPE_MULT_A (%x) = %08x\n",PIPE_MULT_A,read32(PIPE_MULT_A));
	
	DebugPrint(EFI_D_ERROR,"i915: before pipe gamma\n");
	
	//intel_color_load_luts(pipe_config);
	//intel_color_commit(pipe_config);
	DebugPrint(EFI_D_ERROR,"i915: before gamma\n");	
	for (UINT32 i = 0; i < 256; i++) {
		UINT32 word = (i << 16) | (i << 8) | i;
		write32(_LGC_PALETTE_A+i*4, word);
	}
	DebugPrint(EFI_D_ERROR,"i915: before pipe gamma\n");
	//DebugPrint(EFI_D_ERROR,"i915: _PIPEACONF: %08x\n",read32(_PIPEACONF));
	//g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);	
	//return EFI_UNSUPPORTED;
	write32(_PIPEACONF,PIPECONF_PROGRESSIVE|PIPECONF_GAMMA_MODE_8BIT);
	//write32(_SKL_BOTTOM_COLOR_A,SKL_BOTTOM_COLOR_GAMMA_ENABLE);
	//write32(_SKL_BOTTOM_COLOR_A,0);
	//write32(_SKL_BOTTOM_COLOR_A,0x335577);
	write32(_SKL_BOTTOM_COLOR_A,0);
	write32(_GAMMA_MODE_A,GAMMA_MODE_MODE_8BIT);
	
	//bad setup causes hanging when enabling trans / pipe, but what is it?
	//we got here
	//ddi
	DebugPrint(EFI_D_ERROR,"i915: before DDI\n");	
	write32(_TRANSA_MSA_MISC, TRANS_MSA_SYNC_CLK|TRANS_MSA_8_BPC);
	write32(_TRANS_DDI_FUNC_CTL_A, (
		TRANS_DDI_FUNC_ENABLE|TRANS_DDI_SELECT_PORT(port)|TRANS_DDI_PHSYNC|TRANS_DDI_PVSYNC|TRANS_DDI_BPC_8|TRANS_DDI_MODE_SELECT_HDMI
	));
	DebugPrint(EFI_D_ERROR,"i915: after DDI\n");
	//g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);	
	//return EFI_UNSUPPORTED;
	
	//test: could be Windows hanging, it's not
	//g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
	//we failed here
	//return EFI_UNSUPPORTED;
	
	write32(_PIPEACONF,PIPECONF_ENABLE|PIPECONF_PROGRESSIVE|PIPECONF_GAMMA_MODE_8BIT);
	UINT32 counter=0;
	for(;;){
		counter+=1;
		if(counter>=16384){
			DebugPrint(EFI_D_ERROR,"i915: failed to enable PIPE\n");
			break;
		}
		if(read32(_PIPEACONF)&I965_PIPECONF_ACTIVE){
			DebugPrint(EFI_D_ERROR,"i915: pipe enabled\n");
			break;
		}
	}
	
	//if (pipe_config->has_pch_encoder)
	//	lpt_pch_enable(old_intel_state, pipe_config);
	
	//if (intel_crtc_has_type(pipe_config, INTEL_OUTPUT_DP_MST))
	//	intel_ddi_set_vc_payload_alloc(pipe_config, true);
	
	//intel_encoders_enable(crtc, pipe_config, old_state);
	/* Display WA #1143: skl,kbl,cfl */
	UINT32 saved_port_bits = I915_READ(DDI_BUF_CTL(port)) & (DDI_BUF_PORT_REVERSAL | DDI_A_4_LANES);
	
	//if (IS_GEN9_BC(dev_priv)) 
	{
		/*
		 * For some reason these chicken bits have been
		 * stuffed into a transcoder register, event though
		 * the bits affect a specific DDI port rather than
		 * a specific transcoder.
		 */
		UINT32 reg = CHICKEN_TRANS_A;
		if(port==PORT_B){reg = CHICKEN_TRANS_A;}
		if(port==PORT_C){reg = CHICKEN_TRANS_B;}
		if(port==PORT_D){reg = CHICKEN_TRANS_C;}
		//if(port==PORT_E){reg = CHICKEN_TRANS_A;}
		UINT32 val;
	
		val = I915_READ(reg);
	
		if (port == PORT_E)
			val |= DDIE_TRAINING_OVERRIDE_ENABLE |
				DDIE_TRAINING_OVERRIDE_VALUE;
		else
			val |= DDI_TRAINING_OVERRIDE_ENABLE |
				DDI_TRAINING_OVERRIDE_VALUE;
	
		I915_WRITE(reg, val);
		read32(reg);
	
		//... don't have timer
		for(UINT32 counter=0;;){
			read32(reg);
			counter+=1;
			if(counter>=16384){
				break;
			}
		}
		//udelay(1);
	
		if (port == PORT_E)
			val &= ~(DDIE_TRAINING_OVERRIDE_ENABLE |
				 DDIE_TRAINING_OVERRIDE_VALUE);
		else
			val &= ~(DDI_TRAINING_OVERRIDE_ENABLE |
				 DDI_TRAINING_OVERRIDE_VALUE);
	
		I915_WRITE(reg, val);
	}
	
	/* In HDMI/DVI mode, the port width, and swing/emphasis values
	 * are ignored so nothing special needs to be done besides
	 * enabling the port.
	 */
	I915_WRITE(DDI_BUF_CTL(port), saved_port_bits | DDI_BUF_CTL_ENABLE);
	DebugPrint(EFI_D_ERROR,"DDI_BUF_CTL(port) = %08x\n",read32(DDI_BUF_CTL(port)));
	
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
	//UINT32 cnt=0;
	//for(UINT32 y=0;y<vertical_active;y+=1){
	//	for(UINT32 x=0;x<horizontal_active;x+=1){
	//		UINT32 data=(((x<<8)/horizontal_active)<<16)|(((y<<8)/vertical_active)<<8);
	//		((UINT32*)g_private.FbBase)[cnt]=(data&0xffff00)|0x80;
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

//
// selector and size of ASSIGNED_IGD_FW_CFG_OPREGION
//
STATIC FIRMWARE_CONFIG_ITEM mOpRegionItem;
STATIC UINTN                mOpRegionSize;
//
// value read from ASSIGNED_IGD_FW_CFG_BDSM_SIZE, converted to UINTN
//
STATIC UINTN                mBdsmSize;

/**
  Allocate memory in the 32-bit address space, with the requested UEFI memory
  type and the requested alignment.

  @param[in] MemoryType        Assign MemoryType to the allocated pages as
                               memory type.

  @param[in] NumberOfPages     The number of pages to allocate.

  @param[in] AlignmentInPages  On output, Address will be a whole multiple of
                               EFI_PAGES_TO_SIZE (AlignmentInPages).
                               AlignmentInPages must be a power of two.

  @param[out] Address          Base address of the allocated area.

  @retval EFI_SUCCESS            Allocation successful.

  @retval EFI_INVALID_PARAMETER  AlignmentInPages is not a power of two (a
                                 special case of which is when AlignmentInPages
                                 is zero).

  @retval EFI_OUT_OF_RESOURCES   Integer overflow detected.

  @return                        Error codes from gBS->AllocatePages().
**/
STATIC
EFI_STATUS
Allocate32BitAlignedPagesWithType (
  IN  EFI_MEMORY_TYPE      MemoryType,
  IN  UINTN                NumberOfPages,
  IN  UINTN                AlignmentInPages,
  OUT EFI_PHYSICAL_ADDRESS *Address
  )
{
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS PageAlignedAddress;
  EFI_PHYSICAL_ADDRESS FullyAlignedAddress;
  UINTN                BottomPages;
  UINTN                TopPages;

  //
  // AlignmentInPages must be a power of two.
  //
  if (AlignmentInPages == 0 ||
      (AlignmentInPages & (AlignmentInPages - 1)) != 0) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // (NumberOfPages + (AlignmentInPages - 1)) must not overflow UINTN.
  //
  if (AlignmentInPages - 1 > MAX_UINTN - NumberOfPages) {
    return EFI_OUT_OF_RESOURCES;
  }
  //
  // EFI_PAGES_TO_SIZE (AlignmentInPages) must not overflow UINTN.
  //
  if (AlignmentInPages > (MAX_UINTN >> EFI_PAGE_SHIFT)) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Allocate with sufficient padding for alignment.
  //
  PageAlignedAddress = BASE_4GB - 1;
  //PageAlignedAddress = BASE_2GB - 1;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  MemoryType,
                  NumberOfPages + (AlignmentInPages - 1),
                  &PageAlignedAddress
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  FullyAlignedAddress = ALIGN_VALUE (
                          PageAlignedAddress,
                          (UINT64)EFI_PAGES_TO_SIZE (AlignmentInPages)
                          );

  //
  // Release bottom and/or top padding.
  //
  BottomPages = EFI_SIZE_TO_PAGES (
                  (UINTN)(FullyAlignedAddress - PageAlignedAddress)
                  );
  TopPages = (AlignmentInPages - 1) - BottomPages;
  if (BottomPages > 0) {
    Status = gBS->FreePages (PageAlignedAddress, BottomPages);
    ASSERT_EFI_ERROR (Status);
  }
  if (TopPages > 0) {
    Status = gBS->FreePages (
                    FullyAlignedAddress + EFI_PAGES_TO_SIZE (NumberOfPages),
                    TopPages
                    );
    ASSERT_EFI_ERROR (Status);
  }

  *Address = FullyAlignedAddress;
  return EFI_SUCCESS;
}

//CHAR8 OPREGION_SIGNATURE[]="IntelGraphicsMem";

typedef struct {
  UINT16 VendorId;
  UINT8  ClassCode[3];
  UINTN  Segment;
  UINTN  Bus;
  UINTN  Device;
  UINTN  Function;
  CHAR8  Name[sizeof "0000:00:02.0"];
} CANDIDATE_PCI_INFO;


/**
  Populate the CANDIDATE_PCI_INFO structure for a PciIo protocol instance.

  @param[in] PciIo     EFI_PCI_IO_PROTOCOL instance to interrogate.

  @param[out] PciInfo  CANDIDATE_PCI_INFO structure to fill.

  @retval EFI_SUCCESS  PciInfo has been filled in. PciInfo->Name has been set
                       to the empty string.

  @return              Error codes from PciIo->Pci.Read() and
                       PciIo->GetLocation(). The contents of PciInfo are
                       indeterminate.
**/
STATIC
EFI_STATUS
InitPciInfo (
  IN  EFI_PCI_IO_PROTOCOL *PciIo,
  OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  EFI_STATUS Status;

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PCI_VENDOR_ID_OFFSET,
                        1,                    // Count
                        &PciInfo->VendorId
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_CLASSCODE_OFFSET,
                        sizeof PciInfo->ClassCode,
                        PciInfo->ClassCode
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->GetLocation (
                    PciIo,
                    &PciInfo->Segment,
                    &PciInfo->Bus,
                    &PciInfo->Device,
                    &PciInfo->Function
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PciInfo->Name[0] = '\0';
  return EFI_SUCCESS;
}

#define ASSIGNED_IGD_FW_CFG_OPREGION  "etc/igd-opregion"
#define ASSIGNED_IGD_FW_CFG_BDSM_SIZE "etc/igd-bdsm-size"

//
// Alignment constants. UEFI page allocation automatically satisfies the
// requirements for the OpRegion, thus we only need to define an alignment
// constant for IGD stolen memory.
//
#define ASSIGNED_IGD_BDSM_ALIGN SIZE_1MB

//
// PCI config space registers. The naming follows the PCI_*_OFFSET pattern seen
// in MdePkg/Include/IndustryStandard/Pci*.h.
//
#define ASSIGNED_IGD_PCI_BDSM_OFFSET 0x5C
#define ASSIGNED_IGD_PCI_ASLS_OFFSET 0xFC

//
// PCI location and vendor
//
#define ASSIGNED_IGD_PCI_BUS       0x00
#define ASSIGNED_IGD_PCI_DEVICE    0x02
#define ASSIGNED_IGD_PCI_FUNCTION  0x0
#define ASSIGNED_IGD_PCI_VENDOR_ID 0x8086

/**
  Set up the OpRegion for the device identified by PciIo.

  @param[in] PciIo        The device to set up the OpRegion for.

  @param[in,out] PciInfo  On input, PciInfo must have been initialized from
                          PciIo with InitPciInfo(). SetupOpRegion() may call
                          GetPciName() on PciInfo, possibly modifying it.

  @retval EFI_SUCCESS            OpRegion setup successful.

  @retval EFI_INVALID_PARAMETER  mOpRegionSize is zero.

  @return                        Error codes propagated from underlying
                                 functions.
**/
STATIC
EFI_STATUS
SetupOpRegion (
  IN     EFI_PCI_IO_PROTOCOL *PciIo,
  IN OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  UINTN                OpRegionPages;
  UINTN                OpRegionResidual;
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS Address;
  UINT8                *BytePointer;

  if (mOpRegionSize == 0) {
    return EFI_INVALID_PARAMETER;
  }
  OpRegionPages = EFI_SIZE_TO_PAGES (mOpRegionSize<8192?8192:mOpRegionSize);
  OpRegionResidual = EFI_PAGES_TO_SIZE (OpRegionPages) - mOpRegionSize;

  //
  // While QEMU's "docs/igd-assign.txt" specifies reserved memory, Intel's IGD
  // OpRegion spec refers to ACPI NVS.
  //
  Status = Allocate32BitAlignedPagesWithType (
             EfiACPIMemoryNVS,
             OpRegionPages,
             1,                // AlignmentInPages
             &Address
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to allocate OpRegion: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    return Status;
  }

  //
  // Download OpRegion contents from fw_cfg, zero out trailing portion.
  //
  BytePointer = (UINT8 *)(UINTN)Address;
  QemuFwCfgSelectItem (mOpRegionItem);
  QemuFwCfgReadBytes (mOpRegionSize, BytePointer);
  if(OpRegionResidual){
      ZeroMem (BytePointer + mOpRegionSize, OpRegionResidual);
  }
  
  //for(int i=0;i<sizeof(OPREGION_SIGNATURE);i++){
  //    BytePointer[i]=(UINT8)OPREGION_SIGNATURE[i];
  //}
  //BytePointer[0x43f]=0x20;
  
  //
  // Write address of OpRegion to PCI config space.
  //
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        ASSIGNED_IGD_PCI_ASLS_OFFSET,
                        1,                            // Count
                        &Address
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to write OpRegion address: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    goto FreeOpRegion;
  }

  DebugPrint(EFI_D_ERROR, "i915: %a: OpRegion @ 0x%Lx size 0x%Lx in %d pages\n", __FUNCTION__,
    Address, (UINT64)mOpRegionSize,(int)OpRegionPages);
  return EFI_SUCCESS;

FreeOpRegion:
  gBS->FreePages (Address, OpRegionPages);
  return Status;
}


/**
  Set up stolen memory for the device identified by PciIo.

  @param[in] PciIo        The device to set up stolen memory for.

  @param[in,out] PciInfo  On input, PciInfo must have been initialized from
                          PciIo with InitPciInfo(). SetupStolenMemory() may
                          call GetPciName() on PciInfo, possibly modifying it.

  @retval EFI_SUCCESS            Stolen memory setup successful.

  @retval EFI_INVALID_PARAMETER  mBdsmSize is zero.

  @return                        Error codes propagated from underlying
                                 functions.
**/
STATIC
EFI_STATUS
SetupStolenMemory (
  IN     EFI_PCI_IO_PROTOCOL *PciIo,
  IN OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  UINTN                BdsmPages;
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS Address;

  if (mBdsmSize == 0) {
    return EFI_INVALID_PARAMETER;
  }
  BdsmPages = EFI_SIZE_TO_PAGES (mBdsmSize);

  Status = Allocate32BitAlignedPagesWithType (
             EfiReservedMemoryType,//
             BdsmPages,
             EFI_SIZE_TO_PAGES ((UINTN)ASSIGNED_IGD_BDSM_ALIGN),
             &Address
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to allocate stolen memory: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    return Status;
  }

  //
  // Zero out stolen memory.
  //
  ZeroMem ((VOID *)(UINTN)Address, EFI_PAGES_TO_SIZE (BdsmPages));

  //
  // Write address of stolen memory to PCI config space.
  //
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        ASSIGNED_IGD_PCI_BDSM_OFFSET,
                        1,                            // Count
                        &Address
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to write stolen memory address: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    goto FreeStolenMemory;
  }

  DEBUG ((DEBUG_INFO, "%a: %a: stolen memory @ 0x%Lx size 0x%Lx\n",
    __FUNCTION__, GetPciName (PciInfo), Address, (UINT64)mBdsmSize));
  return EFI_SUCCESS;

FreeStolenMemory:
  gBS->FreePages (Address, BdsmPages);
  return Status;
}

STATIC UINT8 edid_fallback[]={
	//generic 1280x720
	0,255,255,255,255,255,255,0,34,240,84,41,1,0,0,0,4,23,1,4,165,52,32,120,35,252,129,164,85,77,157,37,18,80,84,33,8,0,209,192,129,192,129,64,129,128,149,0,169,64,179,0,1,1,26,29,0,128,81,208,28,32,64,128,53,0,77,187,16,0,0,30,0,0,0,254,0,55,50,48,112,32,32,32,32,32,32,32,32,10,0,0,0,253,0,24,60,24,80,17,0,10,32,32,32,32,32,32,0,0,0,252,0,72,80,32,90,82,95,55,50,48,112,10,32,32,0,161
	//the test monitor
	//0,255,255,255,255,255,255,0,6,179,192,39,141,30,0,0,49,26,1,3,128,60,34,120,42,83,165,167,86,82,156,38,17,80,84,191,239,0,209,192,179,0,149,0,129,128,129,64,129,192,113,79,1,1,2,58,128,24,113,56,45,64,88,44,69,0,86,80,33,0,0,30,0,0,0,255,0,71,67,76,77,84,74,48,48,55,56,50,49,10,0,0,0,253,0,50,75,24,83,17,0,10,32,32,32,32,32,32,0,0,0,252,0,65,83,85,83,32,86,90,50,55,57,10,32,32,1,153,2,3,34,113,79,1,2,3,17,18,19,4,20,5,14,15,29,30,31,144,35,9,23,7,131,1,0,0,101,3,12,0,32,0,140,10,208,138,32,224,45,16,16,62,150,0,86,80,33,0,0,24,1,29,0,114,81,208,30,32,110,40,85,0,86,80,33,0,0,30,1,29,0,188,82,208,30,32,184,40,85,64,86,80,33,0,0,30,140,10,208,144,32,64,49,32,12,64,85,0,86,80,33,0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,237
};

STATIC EFI_STATUS SetupFwcfgStuff(EFI_PCI_IO_PROTOCOL *PciIo){
	EFI_STATUS OpRegionStatus = QemuFwCfgFindFile (
	                   ASSIGNED_IGD_FW_CFG_OPREGION,
	                   &mOpRegionItem,
	                   &mOpRegionSize
	                   );
	FIRMWARE_CONFIG_ITEM BdsmItem;
	UINTN                BdsmItemSize;
	EFI_STATUS BdsmStatus = QemuFwCfgFindFile (
	               ASSIGNED_IGD_FW_CFG_BDSM_SIZE,
	               &BdsmItem,
	               &BdsmItemSize
	               );
	//
	// If neither fw_cfg file is available, assume no IGD is assigned.
	//
	if (EFI_ERROR (OpRegionStatus) && EFI_ERROR (BdsmStatus)) {
	  return EFI_UNSUPPORTED;
	}
	
	//
	// Require all fw_cfg files that are present to be well-formed.
	//
	if (!EFI_ERROR (OpRegionStatus) && mOpRegionSize == 0)  {
	  DEBUG ((DEBUG_ERROR, "%a: %a: zero size\n", __FUNCTION__,
	    ASSIGNED_IGD_FW_CFG_OPREGION));
	  return EFI_PROTOCOL_ERROR;
	}
	
	if (!EFI_ERROR (BdsmStatus)) {
	  UINT64 BdsmSize;
	
	  if (BdsmItemSize != sizeof BdsmSize) {
	    DEBUG ((DEBUG_ERROR, "%a: %a: invalid fw_cfg size: %Lu\n", __FUNCTION__,
	      ASSIGNED_IGD_FW_CFG_BDSM_SIZE, (UINT64)BdsmItemSize));
	    return EFI_PROTOCOL_ERROR;
	  }
	  QemuFwCfgSelectItem (BdsmItem);
	  QemuFwCfgReadBytes (BdsmItemSize, &BdsmSize);
	
	  if (BdsmSize == 0 || BdsmSize > MAX_UINTN) {
	    DEBUG ((DEBUG_ERROR, "%a: %a: invalid value: %Lu\n", __FUNCTION__,
	      ASSIGNED_IGD_FW_CFG_BDSM_SIZE, BdsmSize));
	    return EFI_PROTOCOL_ERROR;
	  }
	  DEBUG((DEBUG_INFO,"BdsmSize=%Lu\n",BdsmSize));
	  mBdsmSize = (UINTN)BdsmSize;
	}else{
	    //assume 64M
	    DEBUG((DEBUG_INFO,"BdsmSize not found\n"));
	    //mBdsmSize = (UINTN)(64<<20);
	}
	
	CANDIDATE_PCI_INFO PciInfo={};
	InitPciInfo (PciIo, &PciInfo);
	if (mOpRegionSize > 0) {
	  SetupOpRegion (PciIo, &PciInfo);
	}
	if (mBdsmSize > 0) {
	  SetupStolenMemory (PciIo, &PciInfo);
	}
	return EFI_SUCCESS;
}

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
	
	/* 1. Enable PCH reset handshake. */
	//intel_pch_reset_handshake(dev_priv, !HAS_PCH_NOP(dev_priv));
	write32(HSW_NDE_RSTWRN_OPT,read32(HSW_NDE_RSTWRN_OPT)|RESET_PCH_HANDSHAKE_ENABLE);
	
	//DOESN'T APPLY
	///* 2-3. */
	//icl_combo_phys_init(dev_priv);
		
	//if (resume && dev_priv->csr.dmc_payload)
	//	intel_csr_load_program(dev_priv);
	
	//power well enable, we are requesting these to be enabled
	//#define   SKL_PW_CTL_IDX_PW_2			15
	//#define   SKL_PW_CTL_IDX_PW_1			14
	//#define   SKL_PW_CTL_IDX_DDI_D			4
	//#define   SKL_PW_CTL_IDX_DDI_C			3
	//#define   SKL_PW_CTL_IDX_DDI_B			2
	//#define   SKL_PW_CTL_IDX_DDI_A_E		1
	//#define   SKL_PW_CTL_IDX_MISC_IO		0
	write32(HSW_PWR_WELL_CTL1,read32(HSW_PWR_WELL_CTL1)|0xA00002AAu);
	for(UINT32 counter=0;;counter++){
		UINT32 stat=read32(HSW_PWR_WELL_CTL1);
		if(counter>16384){
			DebugPrint(EFI_D_ERROR,"i915: power well enabling timed out %08x\n",stat);
			break;
		}
		if(stat&0x50000155u){
			DebugPrint(EFI_D_ERROR,"i915: power well enabled %08x\n",stat);
			break;
		}
	}
	//disable VGA
	UINT32 vgaword=read32(VGACNTRL);
	write32(VGACNTRL,(vgaword&~VGA_2X_MODE)|VGA_DISP_DISABLE);
	//DebugPrint(EFI_D_ERROR,"i915: bars %08x %08x %08x %08x\n",Pci.Device.Bar[0],Pci.Device.Bar[1],Pci.Device.Bar[2],Pci.Device.Bar[3]);
	
	///* 5. Enable CDCLK. */
	//icl_init_cdclk(dev_priv);
	//080002a1 on test machine
	//DebugPrint(EFI_D_ERROR,"i915: CDCLK = %08x\n",read32(CDCLK_CTL));
	//there seems no need to do so
	
	///* 6. Enable DBUF. */
	//icl_dbuf_enable(dev_priv);
	I915_WRITE(DBUF_CTL_S1, I915_READ(DBUF_CTL_S1) | DBUF_POWER_REQUEST);
	I915_WRITE(DBUF_CTL_S2, I915_READ(DBUF_CTL_S2) | DBUF_POWER_REQUEST);
	read32(DBUF_CTL_S2);
	for(UINT32 counter=0;;counter++){
		if(counter>16384){
			DebugPrint(EFI_D_ERROR,"i915: DBUF timeout\n");
			break;
		}
		if(read32(DBUF_CTL_S1)&read32(DBUF_CTL_S2)&DBUF_POWER_STATE){
			DebugPrint(EFI_D_ERROR,"i915: DBUF good\n");
			break;
		}
	}
	
	///* 7. Setup MBUS. */
	//icl_mbus_init(dev_priv);
	I915_WRITE(MBUS_ABOX_CTL, 
		MBUS_ABOX_BT_CREDIT_POOL1(16) |
		MBUS_ABOX_BT_CREDIT_POOL2(16) |
		MBUS_ABOX_B_CREDIT(1) |
		MBUS_ABOX_BW_CREDIT(1)
	);
	
	//set up display buffer
	//the value is from host
	DebugPrint(EFI_D_ERROR,"i915: _PLANE_BUF_CFG_1_A = %08x\n",read32(_PLANE_BUF_CFG_1_A));
	write32(_PLANE_BUF_CFG_1_A,0x035b0000);
	DebugPrint(EFI_D_ERROR,"i915: _PLANE_BUF_CFG_1_A = %08x (after)\n",read32(_PLANE_BUF_CFG_1_A));
	
	//initialize output
	//need workaround: always initialize DDI
	//intel_dig_port->hdmi.hdmi_reg = DDI_BUF_CTL(port);
	//intel_ddi_init(PORT_A);
	UINT32 found = I915_READ(SFUSE_STRAP);
	DebugPrint(EFI_D_ERROR,"i915: SFUSE_STRAP = %08x\n",found);
	port=PORT_A;
	if (found & SFUSE_STRAP_DDIB_DETECTED){
		port=PORT_B;//intel_ddi_init(PORT_B);
	}else if (found & SFUSE_STRAP_DDIC_DETECTED){
		port=PORT_C;//intel_ddi_init(PORT_C);
	}else if (found & SFUSE_STRAP_DDID_DETECTED){
		port=PORT_D;//intel_ddi_init(PORT_D);
	}
	//if (found & SFUSE_STRAP_DDIF_DETECTED)
	//	intel_ddi_init(dev_priv, PORT_F);
	
	//reset GMBUS
	//intel_i2c_reset(dev_priv);
	I915_WRITE(GMBUS0, 0);
	I915_WRITE(GMBUS4, 0);
	
	// query EDID and initialize the mode
	// it somehow fails on real hardware
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
	g_private.is_gvt=0;
	if(read64(0x78000)==0x4776544776544776ULL){
		g_private.gmadr=read32(0x78040);
		g_private.is_gvt=1;
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

	//setup OpRegion from fw_cfg (IgdAssignmentDxe)
	DebugPrint(EFI_D_ERROR,"i915: before QEMU shenanigans\n");
	QemuFwCfgInitialize();
	if(QemuFwCfgIsAvailable()){
		//setup opregion
		Status=SetupFwcfgStuff(Private->PciIo);
		DebugPrint(EFI_D_ERROR,"i915: SetupFwcfgStuff returns %d\n",Status);
	}
	DebugPrint(EFI_D_ERROR,"i915: after QEMU shenanigans\n");
	
	//TODO: turn on backlight if found in OpRegion, need eDP initialization first...
	
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
	//we don't support this, Windows can clean up our mess without this anyway
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
	g_SystemTable=SystemTable;
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
