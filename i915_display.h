#pragma once
#ifndef i915_DISPLAYH
#define i915_DISPLAYH
#include <Uefi.h>
#include "i915_controller.h"
#include "i915_debug.h"
#include "i915_gmbus.h"
#include "i915_ddi.h"
#include "i915_dp.h"
#include "i915_hdmi.h"
#include "i915_reg.h"
#include "i915_gop.h"

#define VGACNTRL (0x71400)
#define VGA_DISP_DISABLE (1 << 31)
#define VGA_2X_MODE (1 << 30)

#define _PIPEACONF 0x70008
#define _PIPEBCONF 0x71008
#define _PIPEEDPCONF 0x7f008
#define PIPECONF_ENABLE (1 << 31)
#define PIPECONF_DISABLE 0
#define PIPECONF_DOUBLE_WIDE (1 << 30)
#define I965_PIPECONF_ACTIVE (1 << 30)
#define PIPECONF_DSI_PLL_LOCKED (1 << 29) /* vlv & pipe A only */
#define PIPECONF_FRAME_START_DELAY_MASK (3 << 27)
#define PIPECONF_SINGLE_WIDE 0
#define PIPECONF_PIPE_UNLOCKED 0
#define PIPECONF_PIPE_LOCKED (1 << 25)
#define PIPECONF_FORCE_BORDER (1 << 25)
#define PIPECONF_GAMMA_MODE_MASK_I9XX (1 << 24) /* gmch */
#define PIPECONF_GAMMA_MODE_MASK_ILK (3 << 24)  /* ilk-ivb */
#define PIPECONF_GAMMA_MODE_8BIT (0 << 24)      /* gmch,ilk-ivb */
#define PIPECONF_GAMMA_MODE_10BIT (1 << 24)     /* gmch,ilk-ivb */
#define PIPECONF_GAMMA_MODE_12BIT (2 << 24)     /* ilk-ivb */
#define PIPECONF_GAMMA_MODE_SPLIT (3 << 24)     /* ivb */
#define PIPECONF_GAMMA_MODE(x) ((x) << 24)      /* pass in GAMMA_MODE_MODE_* */
#define PIPECONF_GAMMA_MODE_SHIFT 24
#define PIPECONF_INTERLACE_MASK (7 << 21)
#define PIPECONF_INTERLACE_MASK_HSW (3 << 21)
/* Note that pre-gen3 does not support interlaced display directly. Panel
 * fitting must be disabled on pre-ilk for interlaced. */
#define PIPECONF_PROGRESSIVE (0 << 21)
#define PIPECONF_INTERLACE_W_SYNC_SHIFT_PANEL (4 << 21) /* gen4 only */
#define PIPECONF_INTERLACE_W_SYNC_SHIFT (5 << 21)       /* gen4 only */
#define PIPECONF_INTERLACE_W_FIELD_INDICATION (6 << 21)
#define PIPECONF_INTERLACE_FIELD_0_ONLY (7 << 21) /* gen3 only */
/* Ironlake and later have a complete new set of values for interlaced. PFIT
 * means panel fitter required, PF means progressive fetch, DBL means power
 * saving pixel doubling. */
#define PIPECONF_PFIT_PF_INTERLACED_ILK (1 << 21)
#define PIPECONF_INTERLACED_ILK (3 << 21)
#define PIPECONF_INTERLACED_DBL_ILK (4 << 21)         /* ilk/snb only */
#define PIPECONF_PFIT_PF_INTERLACED_DBL_ILK (5 << 21) /* ilk/snb only */
#define PIPECONF_INTERLACE_MODE_MASK (7 << 21)
#define PIPECONF_EDP_RR_MODE_SWITCH (1 << 20)
#define PIPECONF_CXSR_DOWNCLOCK (1 << 16)
#define PIPECONF_EDP_RR_MODE_SWITCH_VLV (1 << 14)
#define PIPECONF_COLOR_RANGE_SELECT (1 << 13)
#define PIPECONF_BPC_MASK (0x7 << 5)
#define PIPECONF_8BPC (0 << 5)
#define PIPECONF_10BPC (1 << 5)
#define PIPECONF_6BPC (2 << 5)
#define PIPECONF_12BPC (3 << 5)
#define PIPECONF_DITHER_EN (1 << 4)
#define PIPECONF_DITHER_TYPE_MASK (0x0000000c)
#define PIPECONF_DITHER_TYPE_SP (0 << 2)
#define PIPECONF_DITHER_TYPE_ST1 (1 << 2)
#define PIPECONF_DITHER_TYPE_ST2 (2 << 2)
#define PIPECONF_DITHER_TYPE_TEMP (3 << 2)
#define _PIPEASTAT 0x70024
#define _PIPEBSTAT 0x71024
#define PIPE_FIFO_UNDERRUN_STATUS (1UL << 31)
#define SPRITE1_FLIP_DONE_INT_EN_VLV (1UL << 30)
#define PIPE_CRC_ERROR_ENABLE (1UL << 29)
#define PIPE_CRC_DONE_ENABLE (1UL << 28)
#define PERF_COUNTER2_INTERRUPT_EN (1UL << 27)
#define PIPE_GMBUS_EVENT_ENABLE (1UL << 27)
#define PLANE_FLIP_DONE_INT_EN_VLV (1UL << 26)
#define PIPE_HOTPLUG_INTERRUPT_ENABLE (1UL << 26)
#define PIPE_VSYNC_INTERRUPT_ENABLE (1UL << 25)
#define PIPE_DISPLAY_LINE_COMPARE_ENABLE (1UL << 24)
#define PIPE_DPST_EVENT_ENABLE (1UL << 23)
#define SPRITE0_FLIP_DONE_INT_EN_VLV (1UL << 22)
#define PIPE_LEGACY_BLC_EVENT_ENABLE (1UL << 22)
#define PIPE_ODD_FIELD_INTERRUPT_ENABLE (1UL << 21)
#define PIPE_EVEN_FIELD_INTERRUPT_ENABLE (1UL << 20)
#define PIPE_B_PSR_INTERRUPT_ENABLE_VLV (1UL << 19)
#define PERF_COUNTER_INTERRUPT_EN (1UL << 19)
#define PIPE_HOTPLUG_TV_INTERRUPT_ENABLE (1UL << 18)   /* pre-965 */
#define PIPE_START_VBLANK_INTERRUPT_ENABLE (1UL << 18) /* 965 or later */
#define PIPE_FRAMESTART_INTERRUPT_ENABLE (1UL << 17)
#define PIPE_VBLANK_INTERRUPT_ENABLE (1UL << 17)
#define PIPEA_HBLANK_INT_EN_VLV (1UL << 16)
#define PIPE_OVERLAY_UPDATED_ENABLE (1UL << 16)
#define SPRITE1_FLIP_DONE_INT_STATUS_VLV (1UL << 15)
#define SPRITE0_FLIP_DONE_INT_STATUS_VLV (1UL << 14)
#define PIPE_CRC_ERROR_INTERRUPT_STATUS (1UL << 13)
#define PIPE_CRC_DONE_INTERRUPT_STATUS (1UL << 12)
#define PERF_COUNTER2_INTERRUPT_STATUS (1UL << 11)
#define PIPE_GMBUS_INTERRUPT_STATUS (1UL << 11)
#define PLANE_FLIP_DONE_INT_STATUS_VLV (1UL << 10)
#define PIPE_HOTPLUG_INTERRUPT_STATUS (1UL << 10)
#define PIPE_VSYNC_INTERRUPT_STATUS (1UL << 9)
#define PIPE_DISPLAY_LINE_COMPARE_STATUS (1UL << 8)
#define PIPE_DPST_EVENT_STATUS (1UL << 7)
#define PIPE_A_PSR_STATUS_VLV (1UL << 6)
#define PIPE_LEGACY_BLC_EVENT_STATUS (1UL << 6)
#define PIPE_ODD_FIELD_INTERRUPT_STATUS (1UL << 5)
#define PIPE_EVEN_FIELD_INTERRUPT_STATUS (1UL << 4)
#define PIPE_B_PSR_STATUS_VLV (1UL << 3)
#define PERF_COUNTER_INTERRUPT_STATUS (1UL << 3)
#define PIPE_HOTPLUG_TV_INTERRUPT_STATUS (1UL << 2)   /* pre-965 */
#define PIPE_START_VBLANK_INTERRUPT_STATUS (1UL << 2) /* 965 or later */
#define PIPE_FRAMESTART_INTERRUPT_STATUS (1UL << 1)
#define PIPE_VBLANK_INTERRUPT_STATUS (1UL << 1)
#define PIPE_HBLANK_INT_STATUS (1UL << 0)
#define PIPE_OVERLAY_UPDATED_STATUS (1UL << 0)

#define _DSPACNTR 0x70180
#define DISPLAY_PLANE_ENABLE (1 << 31)
#define DISPLAY_PLANE_DISABLE 0
#define PLANE_CTL_FORMAT_MASK (0xf << 24)
#define PLANE_CTL_FORMAT_YUV422 (0 << 24)
#define PLANE_CTL_FORMAT_NV12 (1 << 24)
#define PLANE_CTL_FORMAT_XRGB_2101010 (2 << 24)
#define PLANE_CTL_FORMAT_XRGB_8888 (4 << 24)
#define PLANE_CTL_ORDER_BGRX (0 << 20)
#define PLANE_CTL_ORDER_RGBX (1 << 20)
#define PLANE_CTL_ALPHA_MASK (0x3 << 4) /* Pre-GLK */
#define PLANE_CTL_ALPHA_DISABLE (0 << 4)
#define PLANE_CTL_ALPHA_SW_PREMULTIPLY (2 << 4)
#define PLANE_CTL_ALPHA_HW_PREMULTIPLY (3 << 4)
#define PLANE_CTL_TRICKLE_FEED_DISABLE (1 << 14)
#define PLANE_CTL_PLANE_GAMMA_DISABLE (1 << 13) /* Pre-GLK */

#define DISPPLANE_PIXFORMAT_MASK (0xf << 26)
#define DISPPLANE_YUV422 (0x0 << 26)
#define DISPPLANE_8BPP (0x2 << 26)
#define DISPPLANE_BGRA555 (0x3 << 26)
#define DISPPLANE_BGRX555 (0x4 << 26)
#define DISPPLANE_BGRX565 (0x5 << 26)
#define DISPPLANE_BGRX888 (0x6 << 26)
#define DISPPLANE_BGRA888 (0x7 << 26)
#define DISPPLANE_RGBX101010 (0x8 << 26)
#define DISPPLANE_RGBA101010 (0x9 << 26)
#define DISPPLANE_BGRX101010 (0xa << 26)
#define DISPPLANE_RGBX161616 (0xc << 26)
#define DISPPLANE_RGBX888 (0xe << 26)
#define DISPPLANE_RGBA888 (0xf << 26)

#define _DSPAADDR 0x70184
#define _DSPASTRIDE 0x70188
#define _DSPAPOS 0x7018C /* reserved */
#define _DSPASIZE 0x70190
#define _DSPASURF 0x7019C    /* 965+ only */
#define _DSPATILEOFF 0x701A4 /* 965+ only */
#define _DSPAOFFSET 0x701A4  /* HSW */
#define _DSPASURFLIVE 0x701AC

#define _TRANSA_MSA_MISC 0x60410
#define _TRANSB_MSA_MISC 0x61410
#define _TRANSC_MSA_MISC 0x62410
#define _TRANS_EDP_MSA_MISC 0x6f410

#define TRANS_MSA_SYNC_CLK (1 << 0)
#define TRANS_MSA_SAMPLING_444 (2 << 1)
#define TRANS_MSA_CLRSP_YCBCR (2 << 3)
#define TRANS_MSA_6_BPC (0 << 5)
#define TRANS_MSA_8_BPC (1 << 5)
#define TRANS_MSA_10_BPC (2 << 5)
#define TRANS_MSA_12_BPC (3 << 5)
#define TRANS_MSA_16_BPC (4 << 5)
#define TRANS_MSA_CEA_RANGE (1 << 3)

#define _TRANS_DDI_FUNC_CTL_A 0x60400
#define _TRANS_DDI_FUNC_CTL_B 0x61400
#define _TRANS_DDI_FUNC_CTL_C 0x62400
#define _TRANS_DDI_FUNC_CTL_EDP 0x6F400
#define _TRANS_DDI_FUNC_CTL_DSI0 0x6b400
#define _TRANS_DDI_FUNC_CTL_DSI1 0x6bc00

#define TRANS_DDI_FUNC_ENABLE (1 << 31)
/* Those bits are ignored by pipe EDP since it can only connect to DDI A */
#define TRANS_DDI_PORT_MASK (7 << 28)
#define TRANS_DDI_PORT_SHIFT 28
#define TRANS_DDI_SELECT_PORT(x) ((x) << 28)
#define TRANS_DDI_PORT_NONE (0 << 28)
#define TRANS_DDI_MODE_SELECT_MASK (7 << 24)
#define TRANS_DDI_MODE_SELECT_HDMI (0 << 24)
#define TRANS_DDI_MODE_SELECT_DVI (1 << 24)
#define TRANS_DDI_MODE_SELECT_DP_SST (2 << 24)
#define TRANS_DDI_MODE_SELECT_DP_MST (3 << 24)
#define TRANS_DDI_MODE_SELECT_FDI (4 << 24)
#define TRANS_DDI_BPC_MASK (7 << 20)
#define TRANS_DDI_BPC_8 (0 << 20)
#define TRANS_DDI_BPC_10 (1 << 20)
#define TRANS_DDI_BPC_6 (2 << 20)
#define TRANS_DDI_BPC_12 (3 << 20)
#define TRANS_DDI_PVSYNC (1 << 17)
#define TRANS_DDI_PHSYNC (1 << 16)
#define TRANS_DDI_EDP_INPUT_MASK (7 << 12)
#define TRANS_DDI_EDP_INPUT_A_ON (0 << 12)
#define TRANS_DDI_EDP_INPUT_A_ONOFF (4 << 12)
#define TRANS_DDI_EDP_INPUT_B_ONOFF (5 << 12)
#define TRANS_DDI_EDP_INPUT_C_ONOFF (6 << 12)
#define TRANS_DDI_HDCP_SIGNALLING (1 << 9)
#define TRANS_DDI_DP_VC_PAYLOAD_ALLOC (1 << 8)
#define TRANS_DDI_HDMI_SCRAMBLER_CTS_ENABLE (1 << 7)
#define TRANS_DDI_HDMI_SCRAMBLER_RESET_FREQ (1 << 6)
#define TRANS_DDI_BFI_ENABLE (1 << 4)
#define TRANS_DDI_HIGH_TMDS_CHAR_RATE (1 << 4)
#define TRANS_DDI_HDMI_SCRAMBLING (1 << 0)
#define TRANS_DDI_HDMI_SCRAMBLING_MASK (TRANS_DDI_HDMI_SCRAMBLER_CTS_ENABLE | TRANS_DDI_HDMI_SCRAMBLER_RESET_FREQ | TRANS_DDI_HDMI_SCRAMBLING)

#define _TRANS_DDI_FUNC_CTL2_A 0x60404
#define _TRANS_DDI_FUNC_CTL2_B 0x61404
#define _TRANS_DDI_FUNC_CTL2_C 0x62404
#define _TRANS_DDI_FUNC_CTL2_EDP 0x6f404
#define _TRANS_DDI_FUNC_CTL2_DSI0 0x6b404
#define _TRANS_DDI_FUNC_CTL2_DSI1 0x6bc04
#define PORT_SYNC_MODE_ENABLE (1 << 4)
#define PORT_SYNC_MODE_MASTER_SELECT(x) ((x) < 0)
#define PORT_SYNC_MODE_MASTER_SELECT_MASK (0x7 << 0)
#define PORT_SYNC_MODE_MASTER_SELECT_SHIFT 0

#define PORT_A 0
#define PORT_B 1
#define PORT_C 2
#define PORT_D 3
#define PORT_E 4

#define _FPA0 (PCH_DISPLAY_BASE + 0x6040)
#define _FPA1 (PCH_DISPLAY_BASE + 0x6044)
#define _FPB0 (PCH_DISPLAY_BASE + 0x6048)
#define _FPB1 (PCH_DISPLAY_BASE + 0x604c)
#define FP_N_DIV_MASK 0x003f0000
#define FP_N_PINEVIEW_DIV_MASK 0x00ff0000
#define FP_N_DIV_SHIFT 16
#define FP_M1_DIV_MASK 0x00003f00
#define FP_M1_DIV_SHIFT 8
#define FP_M2_DIV_MASK 0x0000003f
#define FP_M2_PINEVIEW_DIV_MASK 0x000000ff
#define FP_M2_DIV_SHIFT 0

#define _DPLL_A (PCH_DISPLAY_BASE + 0x6014)
#define _DPLL_B (PCH_DISPLAY_BASE + 0x6018)
#define DPLL_VCO_ENABLE (1 << 31)
#define DPLL_SDVO_HIGH_SPEED (1 << 30)
#define DPLL_DVO_2X_MODE (1 << 30)
#define DPLL_EXT_BUFFER_ENABLE_VLV (1 << 30)
#define DPLL_SYNCLOCK_ENABLE (1 << 29)
#define DPLL_REF_CLK_ENABLE_VLV (1 << 29)
#define DPLL_VGA_MODE_DIS (1 << 28)
#define DPLLB_MODE_DAC_SERIAL (1 << 26) /* i915 */
#define DPLLB_MODE_LVDS (2 << 26)       /* i915 */
#define DPLL_MODE_MASK (3 << 26)
#define DPLL_DAC_SERIAL_P2_CLOCK_DIV_10 (0 << 24)       /* i915 */
#define DPLL_DAC_SERIAL_P2_CLOCK_DIV_5 (1 << 24)        /* i915 */
#define DPLLB_LVDS_P2_CLOCK_DIV_14 (0 << 24)            /* i915 */
#define DPLLB_LVDS_P2_CLOCK_DIV_7 (1 << 24)             /* i915 */
#define DPLL_P2_CLOCK_DIV_MASK 0x03000000               /* i915 */
#define DPLL_FPA01_P1_POST_DIV_MASK 0x00ff0000          /* i915 */
#define DPLL_FPA01_P1_POST_DIV_MASK_PINEVIEW 0x00ff8000 /* Pineview */
#define DPLL_LOCK_VLV (1 << 15)
#define DPLL_INTEGRATED_CRI_CLK_VLV (1 << 14)
#define DPLL_INTEGRATED_REF_CLK_VLV (1 << 13)
#define DPLL_SSC_REF_CLK_CHV (1 << 13)
#define DPLL_PORTC_READY_MASK (0xf << 4)
#define DPLL_PORTB_READY_MASK (0xf)

#define DPLL_FPA01_P1_POST_DIV_SHIFT 16
#define DPLL_FPA01_P1_POST_DIV_SHIFT_PINEVIEW 15

#define PLL_P2_DIVIDE_BY_4 (1 << 23)
#define PLL_P1_DIVIDE_BY_TWO (1 << 21) /* i830 */
#define PLL_REF_INPUT_DREFCLK (0 << 13)
#define PLL_REF_INPUT_TVCLKINA (1 << 13)  /* i830 */
#define PLL_REF_INPUT_TVCLKINBC (2 << 13) /* SDVO TVCLKIN */
#define PLLB_REF_INPUT_SPREADSPECTRUMIN (3 << 13)
#define PLL_REF_INPUT_MASK (3 << 13)
#define PLL_LOAD_PULSE_PHASE_SHIFT 9
/* Ironlake */

#define DISPIO_CR_TX_BMU_CR0 (0x6C00C)
/* I_boost values */
#define BALANCE_LEG_SHIFT(port) (8 + 3 * (port))
#define BALANCE_LEG_MASK(port) (7 << (8 + 3 * (port)))
/* Balance leg disable bits */
#define BALANCE_LEG_DISABLE_SHIFT 23
#define BALANCE_LEG_DISABLE(port) (1 << (23 + (port)))

#define _TRANS_CLK_SEL_A 0x46140
#define _TRANS_CLK_SEL_B 0x46144
/* For each transcoder, we need to select the corresponding port clock */
#define TRANS_CLK_SEL_DISABLED (0x0 << 29)
#define TRANS_CLK_SEL_PORT(x) (((x) + 1) << 29)

#define _LGC_PALETTE_A 0x4a000
#define _LGC_PALETTE_B 0x4a800

#define _SKL_BOTTOM_COLOR_A 0x70034
#define SKL_BOTTOM_COLOR_GAMMA_ENABLE (1 << 31)
#define SKL_BOTTOM_COLOR_CSC_ENABLE (1 << 30)

#define _GAMMA_MODE_A 0x4a480
#define _GAMMA_MODE_B 0x4ac80
#define PRE_CSC_GAMMA_ENABLE (1 << 31)
#define POST_CSC_GAMMA_ENABLE (1 << 30)
#define GAMMA_MODE_MODE_8BIT (0 << 0)
#define GAMMA_MODE_MODE_10BIT (1 << 0)
#define GAMMA_MODE_MODE_12BIT (2 << 0)
#define GAMMA_MODE_MODE_SPLIT (3 << 0)

#define SFUSE_STRAP_FUSE_LOCK (1 << 13)
#define SFUSE_STRAP_RAW_FREQUENCY (1 << 8)
#define SFUSE_STRAP_DISPLAY_DISABLED (1 << 7)
#define SFUSE_STRAP_CRT_DISABLED (1 << 6)
#define SFUSE_STRAP_DDIF_DETECTED (1 << 3)
#define SFUSE_STRAP_DDIB_DETECTED (1 << 2)
#define SFUSE_STRAP_DDIC_DETECTED (1 << 1)
#define SFUSE_STRAP_DDID_DETECTED (1 << 0)

#define CHICKEN_TRANS_A (0x420c0)
#define CHICKEN_TRANS_B (0x420c4)
#define CHICKEN_TRANS_C (0x420c8)
#define CHICKEN_TRANS_EDP (0x420cc)
#define VSC_DATA_SEL_SOFTWARE_CONTROL (1 << 25) /* GLK and CNL+ */
#define DDI_TRAINING_OVERRIDE_ENABLE (1 << 19)
#define DDI_TRAINING_OVERRIDE_VALUE (1 << 18)
#define DDIE_TRAINING_OVERRIDE_ENABLE (1 << 17) /* CHICKEN_TRANS_A only */
#define DDIE_TRAINING_OVERRIDE_VALUE (1 << 16)  /* CHICKEN_TRANS_A only */
#define PSR2_ADD_VERTICAL_LINE_COUNT (1 << 15)
#define PSR2_VSC_ENABLE_PROG_HEADER (1 << 12)

#define HSW_NDE_RSTWRN_OPT (0x46408)
#define RESET_PCH_HANDSHAKE_ENABLE (1 << 4)

#define CDCLK_CTL (0x46000)
#define CDCLK_FREQ_SEL_MASK (3 << 26)
#define CDCLK_FREQ_450_432 (0 << 26)
#define CDCLK_FREQ_540 (1 << 26)
#define CDCLK_FREQ_337_308 (2 << 26)
#define CDCLK_FREQ_675_617 (3 << 26)
#define BXT_CDCLK_CD2X_DIV_SEL_MASK (3 << 22)
#define BXT_CDCLK_CD2X_DIV_SEL_1 (0 << 22)
#define BXT_CDCLK_CD2X_DIV_SEL_1_5 (1 << 22)
#define BXT_CDCLK_CD2X_DIV_SEL_2 (2 << 22)
#define BXT_CDCLK_CD2X_DIV_SEL_4 (3 << 22)
#define BXT_CDCLK_CD2X_PIPE(pipe) ((pipe) << 20)
#define CDCLK_DIVMUX_CD_OVERRIDE (1 << 19)
#define BXT_CDCLK_CD2X_PIPE_NONE BXT_CDCLK_CD2X_PIPE(3)
#define ICL_CDCLK_CD2X_PIPE_NONE (7 << 19)
#define BXT_CDCLK_SSA_PRECHARGE_ENABLE (1 << 16)
#define CDCLK_FREQ_DECIMAL_MASK (0x7ff)

#define DBUF_CTL (0x45008)
#define DBUF_CTL_S1 (0x45008)
#define DBUF_CTL_S2 (0x44FE8)
#define DBUF_POWER_REQUEST (1 << 31)
#define DBUF_POWER_STATE (1 << 30)

#define MBUS_ABOX_CTL (0x45038)
#define MBUS_ABOX_BW_CREDIT_MASK (3 << 20)
#define MBUS_ABOX_BW_CREDIT(x) ((x) << 20)
#define MBUS_ABOX_B_CREDIT_MASK (0xF << 16)
#define MBUS_ABOX_B_CREDIT(x) ((x) << 16)
#define MBUS_ABOX_BT_CREDIT_POOL2_MASK (0x1F << 8)
#define MBUS_ABOX_BT_CREDIT_POOL2(x) ((x) << 8)
#define MBUS_ABOX_BT_CREDIT_POOL1_MASK (0x1F << 0)
#define MBUS_ABOX_BT_CREDIT_POOL1(x) ((x) << 0)

#define _PLANE_BUF_CFG_1_A 0x7027c
#define HSW_PWR_WELL_CTL1 (0x45400)
#define HSW_PWR_WELL_CTL2 (0x45404)
#define HSW_PWR_WELL_CTL3 (0x45408)
#define HSW_PWR_WELL_CTL4 (0x4540C)

#define CHECK_STATUS_ERROR(status) \
        if (status != EFI_SUCCESS) \
                goto error;

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

/* DPLL cfg */

EFI_STATUS DisplayInit(i915_CONTROLLER *iController);

EFI_STATUS setDisplayGraphicsMode(
    UINT32 ModeNumber);
EFI_STATUS TrainDisplayPort(i915_CONTROLLER *controller);
#endif