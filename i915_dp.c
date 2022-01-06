#include "i915_controller.h"
#include "i915_debug.h"
#include "i915_gmbus.h"
#include "i915_ddi.h"
#include "i915_dp.h"
#include "i915_hdmi.h"
#include "i915_reg.h"
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

/* Cedartrail */
#define PP_ON_DELAYS 0x61208 /* Cedartrail */
#define PANEL_PORT_SELECT_MASK (3 << 30)
#define PANEL_PORT_SELECT_LVDS (0 << 30)
#define PANEL_PORT_SELECT_EDP (1 << 30)
#define PANEL_POWER_UP_DELAY_MASK (0x1fff0000)
#define PANEL_POWER_UP_DELAY_SHIFT 16
#define PANEL_LIGHT_ON_DELAY_MASK (0x1fff)
#define PANEL_LIGHT_ON_DELAY_SHIFT 0

#define PP_OFF_DELAYS 0x6120c /* Cedartrail */
#define PANEL_POWER_DOWN_DELAY_MASK (0x1fff0000)
#define PANEL_POWER_DOWN_DELAY_SHIFT 16
#define PANEL_LIGHT_OFF_DELAY_MASK (0x1fff)
#define PANEL_LIGHT_OFF_DELAY_SHIFT 0

//#define PP_DIVISOR		0x61210		/* Cedartrail */
#define PP_REFERENCE_DIVIDER_MASK (0xffffff00)
#define PP_REFERENCE_DIVIDER_SHIFT 8
#define PANEL_POWER_CYCLE_DELAY_MASK (0x1f)
#define PANEL_POWER_CYCLE_DELAY_SHIFT 0
EFI_STATUS ReadEDIDDP(EDID *result, i915_CONTROLLER *controller, UINT8 pin)
{

	UINT32 *p = (UINT32 *)result;

	PRINT_DEBUG(EFI_D_ERROR, "trying DP aux %d\n", pin);
	// aux message header is 3-4 bytes: ctrl8 addr16 len8
	// the data is big endian
	// len is receive buffer size-1
	// i2c init
	UINT32 send_ctl =
		(DP_AUX_CH_CTL_SEND_BUSY | DP_AUX_CH_CTL_DONE |
		 DP_AUX_CH_CTL_TIME_OUT_ERROR | DP_AUX_CH_CTL_TIME_OUT_MAX |
		 DP_AUX_CH_CTL_RECEIVE_ERROR | (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		 DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		 DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
	/* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
	controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8),
						((AUX_I2C_MOT | AUX_I2C_WRITE) << 28) | (0x50 << 8) |
							0);
	controller->write32(_DPA_AUX_CH_CTL + (pin << 8), send_ctl);
	UINT32 aux_status;
	UINT32 counter = 0;
	for (;;)
	{
		aux_status = controller->read32(_DPA_AUX_CH_CTL + (pin << 8));
		if (!(aux_status & DP_AUX_CH_CTL_SEND_BUSY))
		{
			break;
		}
		counter += 1;
		if (counter >= 1500)
		{
			PRINT_DEBUG(EFI_D_ERROR, "DP AUX channel timeout");
			break;
		}
		gBS->Stall(10);
	}
	controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
						aux_status | DP_AUX_CH_CTL_DONE |
							DP_AUX_CH_CTL_TIME_OUT_ERROR |
							DP_AUX_CH_CTL_RECEIVE_ERROR);
	// i2c send 1 byte
	send_ctl =
		(DP_AUX_CH_CTL_SEND_BUSY | DP_AUX_CH_CTL_DONE |
		 DP_AUX_CH_CTL_TIME_OUT_ERROR | DP_AUX_CH_CTL_TIME_OUT_MAX |
		 DP_AUX_CH_CTL_RECEIVE_ERROR | (5 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		 DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		 DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
	controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8),
						(AUX_I2C_WRITE << 28) | (0x50 << 8) | 0);
	controller->write32(_DPA_AUX_CH_DATA2 + (pin << 8), 0);
	controller->write32(_DPA_AUX_CH_CTL + (pin << 8), send_ctl);
	counter = 0;
	for (;;)
	{
		aux_status = controller->read32(_DPA_AUX_CH_CTL + (pin << 8));
		if (!(aux_status & DP_AUX_CH_CTL_SEND_BUSY))
		{
			break;
		}
		counter += 1;
		if (counter >= 1500)
		{
			PRINT_DEBUG(EFI_D_ERROR, "DP AUX channel timeout");
			break;
		}
		gBS->Stall(10);
	}
	controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
						aux_status | DP_AUX_CH_CTL_DONE |
							DP_AUX_CH_CTL_TIME_OUT_ERROR |
							DP_AUX_CH_CTL_RECEIVE_ERROR);
	if (aux_status &
		(DP_AUX_CH_CTL_TIME_OUT_ERROR | DP_AUX_CH_CTL_RECEIVE_ERROR))
	{
		return EFI_NOT_FOUND;
	}
	// i2c read 1 byte * 128
	PRINT_DEBUG(EFI_D_ERROR, "reading DP aux %d\n", pin);
	// aux message header is 3-4 bytes: ctrl8 addr16 len8
	// the data is big endian
	// len is receive buffer size-1
	// i2c init
	send_ctl =
		(DP_AUX_CH_CTL_SEND_BUSY | DP_AUX_CH_CTL_DONE |
		 DP_AUX_CH_CTL_TIME_OUT_ERROR | DP_AUX_CH_CTL_TIME_OUT_MAX |
		 DP_AUX_CH_CTL_RECEIVE_ERROR | (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		 DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		 DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
	/* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
	controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8),
						((AUX_I2C_MOT | AUX_I2C_READ) << 28) | (0x50 << 8) | 0);
	controller->write32(_DPA_AUX_CH_CTL + (pin << 8), send_ctl);
	counter = 0;
	for (;;)
	{
		aux_status = controller->read32(_DPA_AUX_CH_CTL + (pin << 8));
		if (!(aux_status & DP_AUX_CH_CTL_SEND_BUSY))
		{
			break;
		}
		counter += 1;
		if (counter >= 1500)
		{
			PRINT_DEBUG(EFI_D_ERROR, "DP AUX channel timeout");
			break;
		}
		gBS->Stall(10);
	}
	controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
						aux_status | DP_AUX_CH_CTL_DONE |
							DP_AUX_CH_CTL_TIME_OUT_ERROR |
							DP_AUX_CH_CTL_RECEIVE_ERROR);
	UINT32 i = 0;
	for (i = 0; i < 128; i++)
	{
		send_ctl = (DP_AUX_CH_CTL_SEND_BUSY | DP_AUX_CH_CTL_DONE |
					DP_AUX_CH_CTL_TIME_OUT_ERROR | DP_AUX_CH_CTL_TIME_OUT_MAX |
					DP_AUX_CH_CTL_RECEIVE_ERROR |
					(4 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
					DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
					DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
		controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8),
							(AUX_I2C_READ << 28) | (0x50 << 8) | 0);
		controller->write32(_DPA_AUX_CH_CTL + (pin << 8), send_ctl);
		counter = 0;
		for (;;)
		{
			aux_status = controller->read32(_DPA_AUX_CH_CTL + (pin << 8));
			if (!(aux_status & DP_AUX_CH_CTL_SEND_BUSY))
			{
				break;
			}
			counter += 1;
			if (counter >= 1500)
			{
				PRINT_DEBUG(EFI_D_ERROR, "DP AUX channel timeout");
				break;
			}
			gBS->Stall(10);
		}
		controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
							aux_status | DP_AUX_CH_CTL_DONE |
								DP_AUX_CH_CTL_TIME_OUT_ERROR |
								DP_AUX_CH_CTL_RECEIVE_ERROR);
		UINT32 word = controller->read32(_DPA_AUX_CH_DATA1 + (pin << 8));
		((UINT8 *)p)[i] = (word >> 16) & 0xff;
	}
	for (UINT32 i = 0; i < 16; i++)
	{
		for (UINT32 j = 0; j < 8; j++)
		{
			DebugPrint(EFI_D_ERROR, "%02x ", ((UINT8 *)(p))[i * 8 + j]);
		}
		DebugPrint(EFI_D_ERROR, "\n");
	}
	if (i >= 128 && *(UINT64 *)result->magic == 0x00FFFFFFFFFFFF00uLL)
	{
		controller->OutputPath.AuxCh = pin;
		return EFI_SUCCESS;
	}

	return EFI_NOT_FOUND;
}

static int cnp_rawclk(i915_CONTROLLER *controller)
{
	int divider, fraction;

	if (controller->read32(SFUSE_STRAP) & SFUSE_STRAP_RAW_FREQUENCY)
	{
		/* 24 MHz */
		divider = 24000;
		fraction = 0;
	}
	else
	{
		/* 19.2 MHz */
		divider = 19000;
		fraction = 200;
	}

	/* 	rawclk = CNP_RAWCLK_DIV(divider / 1000);
	if (fraction) {
		int numerator = 1;

		rawclk |= CNP_RAWCLK_DEN(DIV_ROUND_CLOSEST(numerator * 1000,
							   fraction) - 1);
		if (INTEL_PCH_TYPE(dev_priv) >= PCH_ICP)
			rawclk |= ICP_RAWCLK_NUM(numerator);
	}

	controller->write32(PCH_RAWCLK_FREQ, rawclk); */
	return divider + fraction;
}

//WE CAN USE THIS TO GET BETTER DELAY VALUES
// static void
// intel_dp_init_panel_power_sequencer(struct intel_dp *intel_dp)
// {
// 	//struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
// 	//struct edp_power_seq cur, vbt, spec;
// 		//*final = &intel_dp->pps_delays;

// 	//lockdep_assert_held(&dev_priv->pps_mutex);

// 	/* already initialized? */
// 	if (final->t11_t12 != 0)
// 		return;

// 	intel_pps_readout_hw_state(intel_dp, &cur);

// 	intel_pps_dump_state("cur", &cur);

// 	vbt = dev_priv->vbt.edp.pps;
// 	/* On Toshiba Satellite P50-C-18C system the VBT T12 delay
// 	 * of 500ms appears to be too short. Ocassionally the panel
// 	 * just fails to power back on. Increasing the delay to 800ms
// 	 * seems sufficient to avoid this problem.
// 	 */
// 	if (dev_priv->quirks & QUIRK_INCREASE_T12_DELAY) {
// 		vbt.t11_t12 = max_t(UINT16, vbt.t11_t12, 1300 * 10);
// 		drm_dbg_kms(&dev_priv->drm,
// 			    "Increasing T12 panel delay as per the quirk to %d\n",
// 			    vbt.t11_t12);
// 	}
// 	/* T11_T12 delay is special and actually in units of 100ms, but zero
// 	 * based in the hw (so we need to add 100 ms). But the sw vbt
// 	 * table multiplies it with 1000 to make it in units of 100usec,
// 	 * too. */
// 	vbt.t11_t12 += 100 * 10;

// 	/* Upper limits from eDP 1.3 spec. Note that we use the clunky units of
// 	 * our hw here, which are all in 100usec. */
// 	spec.t1_t3 = 210 * 10;
// 	spec.t8 = 50 * 10; /* no limit for t8, use t7 instead */
// 	spec.t9 = 50 * 10; /* no limit for t9, make it symmetric with t8 */
// 	spec.t10 = 500 * 10;
// 	/* This one is special and actually in units of 100ms, but zero
// 	 * based in the hw (so we need to add 100 ms). But the sw vbt
// 	 * table multiplies it with 1000 to make it in units of 100usec,
// 	 * too. */
// 	spec.t11_t12 = (510 + 100) * 10;

// 	intel_pps_dump_state("vbt", &vbt);

// 	/* Use the max of the register settings and vbt. If both are
// 	 * unset, fall back to the spec limits. */
// #define assign_final(field)	final->field = (max(cur.field, vbt.field) == 0 ? spec.field : max(cur.field, vbt.field))
// 	assign_final(t1_t3);
// 	assign_final(t8);
// 	assign_final(t9);
// 	assign_final(t10);
// 	assign_final(t11_t12);
// #undef assign_final

// #define get_delay(field)	(DIV_ROUND_UP(final->field, 10))
// 	intel_dp->panel_power_up_delay = get_delay(t1_t3);
// 	intel_dp->backlight_on_delay = get_delay(t8);
// 	intel_dp->backlight_off_delay = get_delay(t9);
// 	intel_dp->panel_power_down_delay = get_delay(t10);
// 	intel_dp->panel_power_cycle_delay = get_delay(t11_t12);
// #undef get_delay

// 	drm_dbg_kms(&dev_priv->drm,
// 		    "panel power up delay %d, power down delay %d, power cycle delay %d\n",
// 		    intel_dp->panel_power_up_delay,
// 		    intel_dp->panel_power_down_delay,
// 		    intel_dp->panel_power_cycle_delay);

// 	drm_dbg_kms(&dev_priv->drm, "backlight on delay %d, off delay %d\n",
// 		    intel_dp->backlight_on_delay,
// 		    intel_dp->backlight_off_delay);

// 	/*
// 	 * We override the HW backlight delays to 1 because we do manual waits
// 	 * on them. For T8, even BSpec recommends doing it. For T9, if we
// 	 * don't do this, we'll end up waiting for the backlight off delay
// 	 * twice: once when we do the manual sleep, and once when we disable
// 	 * the panel and wait for the PP_STATUS bit to become zero.
// 	 */
// 	final->t8 = 1;
// 	final->t9 = 1;

// 	/*
// 	 * HW has only a 100msec granularity for t11_t12 so round it up
// 	 * accordingly.
// 	 */
// 	final->t11_t12 = roundup(final->t11_t12, 100 * 10);
// }
static void
intel_dp_init_panel_power_sequencer_registers(i915_CONTROLLER *controller)
{
	//struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	UINT32 pp_on, pp_off, port_sel = 0;
	//int div = RUNTIME_INFO(dev_priv)->rawclk_freq / 1000;
	int div = cnp_rawclk(controller); //Varies by generation
	//struct pps_registers regs;
	//UINT32 port = controller->OutputPath.Port;
	//const struct edp_power_seq *seq = &intel_dp->pps_delays;

	//	lockdep_assert_held(&dev_priv->pps_mutex);

	//	intel_pps_get_registers(intel_dp, &regs);

	//units are 100us
	pp_on = (2100 << 15) |
			(500);
	pp_off = (5000 << 15) |
			 (500);
	/* Haswell doesn't have any port selection bits for the panel
	 * power sequencer any more. */
	/* 
	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
		port_sel = PANEL_PORT_SELECT_VLV(port);
	} else if (HAS_PCH_IBX(dev_priv) || HAS_PCH_CPT(dev_priv)) {
		switch (port) {
		case PORT_A:
			port_sel = PANEL_PORT_SELECT_DPA;
			break;
		case PORT_C:
			port_sel = PANEL_PORT_SELECT_DPC;
			break;
		case PORT_D:
			port_sel = PANEL_PORT_SELECT_DPD;
			break;
		default:
			MISSING_CASE(port);
			break;
		}
	}
 */
	pp_on |= port_sel;

	controller->write32(PP_ON, pp_on);
	controller->write32(PP_OFF, pp_off);

	/*
	 * Compute the divisor for the pp clock, simply match the Bspec formula.
	 */
	//if (i915_mmio_reg_valid(PP_DIVISOR)) {
	controller->write32(PP_DIVISOR,
						(((100 * div) / 2 - 1) << 7) | 11);
	//   controller->write32(PP_DIVISOR,
	//  0xffffffff);
	/* 	} else {  USED FOR Gens where divisor is in cntrl var
		UINT32 pp_ctl;

		pp_ctl = controller->read32( regs.pp_ctrl);
		pp_ctl &= ~BXT_POWER_CYCLE_DELAY_MASK;
		pp_ctl |= REG_FIELD_PREP(BXT_POWER_CYCLE_DELAY_MASK, DIV_ROUND_UP(seq->t11_t12, 1000));
		controller->write32(regs.pp_ctrl, pp_ctl);
	} */

	PRINT_DEBUG(EFI_D_ERROR,
				"panel power sequencer register settings: PP_ON %#x, PP_OFF %#x, PP_DIV %#x\n",
				controller->read32(PP_ON),
				controller->read32(PP_OFF),
				controller->read32(PP_DIVISOR));
}

void intel_dp_pps_init(i915_CONTROLLER *controller)
{
	//struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);

	// if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
	// 	vlv_initial_power_sequencer_setup(intel_dp);
	// } else {
	//	intel_dp_init_panel_power_sequencer(intel_dp);
	intel_dp_init_panel_power_sequencer_registers(controller);
	//}
}

EFI_STATUS SetupClockeDP(i915_CONTROLLER *controller)
{

	UINT32 ctrl1;
	UINT32 port = controller->OutputPath.Port;

	UINT8 id = controller->OutputPath.DPLL;
	/*
     * See comment in intel_dpll_hw_state to understand why we always use 0
     * as the DPLL id in this function. Basically, we put them in the first 6 bits then shift them into place for easier comparison
     */
	ctrl1 = DPLL_CTRL1_OVERRIDE(id); //Enable Programming
	ctrl1 |= DPLL_CTRL1_SSC(id);
	// ctrl1 |= DPLL_CTRL1_HDMI_MODE(0); //Set Mode to HDMI

	UINT32 val = controller->read32(DPLL_CTRL2);

	//val &= ~(DPLL_CTRL2_DDI_CLK_OFF(PORT_A) |
	//	 DPLL_CTRL2_DDI_CLK_SEL_MASK(PORT_A));
	//val |= (DPLL_CTRL2_DDI_CLK_SEL(id, PORT_A) |
	//	DPLL_CTRL2_DDI_SEL_OVERRIDE(PORT_A));

	val &= ~(DPLL_CTRL2_DDI_CLK_OFF(port));
	val |= (DPLL_CTRL2_DDI_CLK_OFF(port));

	controller->write32(DPLL_CTRL2, val);
	controller->write32(LCPLL2_CTL, controller->read32(LCPLL2_CTL) & ~(LCPLL_PLL_ENABLE));
	controller->write32(LCPLL1_CTL, controller->read32(LCPLL1_CTL) & ~(LCPLL_PLL_ENABLE));
	val = controller->read32(DPLL_CTRL1);
	for (UINT32 counter = 0;; counter++)
	{
		if (controller->read32(DPLL_STATUS) & DPLL_LOCK(id))
		{
			PRINT_DEBUG(EFI_D_ERROR, "DPLL %d locked\n", id);
			break;
		}
		if (counter > 500)
		{
			PRINT_DEBUG(EFI_D_ERROR, "DPLL %d not locked\n", id);
			break;
		}
		gBS->Stall(10);
	}
	//it's clock id!
	//how's port clock comptued?
	UINT64 clock_khz = controller->OutputPath.LinkRate;
	PRINT_DEBUG(EFI_D_ERROR, "Link Rate: %u\n", clock_khz);
	UINT32 linkrate = DPLL_CTRL1_LINK_RATE_810;
	if (clock_khz >> 1 >= 135000)
	{
		linkrate = DPLL_CTRL1_LINK_RATE_1350;
	}
	else if (clock_khz >> 1 >= 270000)
	{
		linkrate = DPLL_CTRL1_LINK_RATE_2700;
	}
	//hack: anything else hangs
	// UINT32 id = DPLL_CTRL1_LINK_RATE_1350;
	ctrl1 |= DPLL_CTRL1_LINK_RATE(linkrate, id);

	val &= ~(DPLL_CTRL1_HDMI_MODE(id) |
			 DPLL_CTRL1_SSC(id) |
			 DPLL_CTRL1_LINK_RATE_MASK(id));
	val |= ctrl1;

	//DPLL 1
	controller->write32(DPLL_CTRL1, val);
	controller->read32(DPLL_CTRL1);

	//845 80400173 3a5
	PRINT_DEBUG(EFI_D_ERROR, "DPLL_CTRL1 = %08x\n", controller->read32(DPLL_CTRL1));
	PRINT_DEBUG(EFI_D_ERROR, "_DPLL1_CFGCR1 = %08x\n", controller->read32(_DPLL1_CFGCR1));
	PRINT_DEBUG(EFI_D_ERROR, "_DPLL1_CFGCR2 = %08x\n", controller->read32(_DPLL1_CFGCR2));

	/* the enable bit is always bit 31 */
	controller->write32(LCPLL2_CTL, controller->read32(LCPLL2_CTL) | LCPLL_PLL_ENABLE);
	controller->write32(LCPLL1_CTL, controller->read32(LCPLL1_CTL) | LCPLL_PLL_ENABLE);

	for (UINT32 counter = 0;; counter++)
	{
		if (controller->read32(DPLL_STATUS) & DPLL_LOCK(id))
		{
			PRINT_DEBUG(EFI_D_ERROR, "DPLL %d locked\n", id);
			break;
		}
		if (counter > 500)
		{
			PRINT_DEBUG(EFI_D_ERROR, "DPLL %d not locked\n", id);
			break;
		}
		gBS->Stall(10);
	}

	//intel_encoders_pre_enable(crtc, pipe_config, old_state);
	//could be intel_ddi_pre_enable_hdmi
	//intel_ddi_clk_select(encoder, crtc_state);
	PRINT_DEBUG(EFI_D_ERROR, "port is %d\n", port);
	{
		UINT32 val = controller->read32(DPLL_CTRL2);

		//val &= ~(DPLL_CTRL2_DDI_CLK_OFF(PORT_A) |
		//	 DPLL_CTRL2_DDI_CLK_SEL_MASK(PORT_A));
		//val |= (DPLL_CTRL2_DDI_CLK_SEL(id, PORT_A) |
		//	DPLL_CTRL2_DDI_SEL_OVERRIDE(PORT_A));

		val &= ~(DPLL_CTRL2_DDI_CLK_OFF(port) |
				 DPLL_CTRL2_DDI_CLK_SEL_MASK(port));
		val |= (DPLL_CTRL2_DDI_CLK_SEL(id, port) |
				DPLL_CTRL2_DDI_SEL_OVERRIDE(port));

		controller->write32(DPLL_CTRL2, val);
	}
	PRINT_DEBUG(EFI_D_ERROR, "DPLL_CTRL2 = %08x\n", controller->read32(DPLL_CTRL2));
	return EFI_SUCCESS;
}
struct ddi_buf_trans
{
	UINT32 trans1; /* balance leg enable, de-emph level */
	UINT32 trans2; /* vref sel, vswing */
	UINT8 i_boost; /* SKL: I_boost; valid: 0x0, 0x1, 0x3, 0x7 */
};
/* Skylake H and S */
static const struct ddi_buf_trans skl_ddi_translations_dp[] = {
	{0x00002016, 0x000000A0, 0x0},
	{0x00005012, 0x0000009B, 0x0},
	{0x00007011, 0x00000088, 0x0},
	{0x80009010, 0x000000C0, 0x1},
	{0x00002016, 0x0000009B, 0x0},
	{0x00005012, 0x00000088, 0x0},
	{0x80007011, 0x000000C0, 0x1},
	{0x00002016, 0x000000DF, 0x0},
	{0x80005012, 0x000000C0, 0x1},
};

/* Skylake U */
static const struct ddi_buf_trans skl_u_ddi_translations_dp[] = {
	{0x0000201B, 0x000000A2, 0x0},
	{0x00005012, 0x00000088, 0x0},
	{0x80007011, 0x000000CD, 0x1},
	{0x80009010, 0x000000C0, 0x1},
	{0x0000201B, 0x0000009D, 0x0},
	{0x80005012, 0x000000C0, 0x1},
	{0x80007011, 0x000000C0, 0x1},
	{0x00002016, 0x00000088, 0x0},
	{0x80005012, 0x000000C0, 0x1},
};

/* Skylake Y */
static const struct ddi_buf_trans skl_y_ddi_translations_dp[] = {
	{0x00000018, 0x000000A2, 0x0},
	{0x00005012, 0x00000088, 0x0},
	{0x80007011, 0x000000CD, 0x3},
	{0x80009010, 0x000000C0, 0x3},
	{0x00000018, 0x0000009D, 0x0},
	{0x80005012, 0x000000C0, 0x3},
	{0x80007011, 0x000000C0, 0x3},
	{0x00000018, 0x00000088, 0x0},
	{0x80005012, 0x000000C0, 0x3},
};
#define IS_SKL_ULX(cont) (0)
#define IS_SKL_ULT(cont) (1)
static const struct ddi_buf_trans *
skl_get_buf_trans_dp(i915_CONTROLLER *controller, int *n_entries)
{
	if (IS_SKL_ULX(controller))
	{
		*n_entries = ARRAY_SIZE(skl_y_ddi_translations_dp);
		return skl_y_ddi_translations_dp;
	}
	else if (IS_SKL_ULT(controller))
	{
		*n_entries = ARRAY_SIZE(skl_u_ddi_translations_dp);
		return skl_u_ddi_translations_dp;
	}
	else
	{
		*n_entries = ARRAY_SIZE(skl_ddi_translations_dp);
		return skl_ddi_translations_dp;
	}
}

/* Display Port */
#define DP_A _MMIO(0x64000) /* eDP */
#define DP_B _MMIO(0x64100)
#define DP_C _MMIO(0x64200)
#define DP_D _MMIO(0x64300)

EFI_STATUS SetupDDIBufferDP(i915_CONTROLLER *controller)
{
	const struct ddi_buf_trans *ddi_translations;

	int i, n_entries;

	ddi_translations = skl_get_buf_trans_dp(controller,
											&n_entries);
	/* If we're boosting the current, set bit 31 of trans1 */
	//	if (IS_GEN9_BC(dev_priv) && intel_bios_dp_boost_level(encoder))
	//		iboost_bit = DDI_BUF_BALANCE_LEG_ENABLE;
	UINT32 port = controller->OutputPath.Port;
	for (i = 0; i < n_entries; i++)
	{
		controller->write32(DDI_BUF_TRANS_LO(port, i),
							ddi_translations[i].trans1);
		controller->write32(DDI_BUF_TRANS_HI(port, i),
							ddi_translations[i].trans2);
	}
	return EFI_SUCCESS;
}
/**
 * struct drm_dp_aux_msg - DisplayPort AUX channel transaction
 * @address: address of the (first) register to access
 * @request: contains the type of transaction (see DP_AUX_* macros)
 * @reply: upon completion, contains the reply type of the transaction
 * @buffer: pointer to a transmission or reception buffer
 * @size: size of @buffer
 */
struct drm_dp_aux_msg
{
	unsigned int address;
	UINT8 request;
	UINT8 reply;
	void *buffer;
	UINT32 size;
};

struct cec_adapter;
struct edid;
struct drm_connector;

/**
 * struct drm_dp_aux_cec - DisplayPort CEC-Tunneling-over-AUX
 * @lock: mutex protecting this struct
 * @adap: the CEC adapter for CEC-Tunneling-over-AUX support.
 * @connector: the connector this CEC adapter is associated with
 * @unregister_work: unregister the CEC adapter
 */
/* struct drm_dp_aux_cec {
	struct mutex lock;
	struct cec_adapter *adap;
	struct drm_connector *connector;
	struct delayed_work unregister_work;
}; */

/**
 * struct drm_dp_aux - DisplayPort AUX channel
 * @name: user-visible name of this AUX channel and the I2C-over-AUX adapter
 * @ddc: I2C adapter that can be used for I2C-over-AUX communication
 * @dev: pointer to struct device that is the parent for this AUX channel
 * @crtc: backpointer to the crtc that is currently using this AUX channel
 * @hw_mutex: internal mutex used for locking transfers
 * @crc_work: worker that captures CRCs for each frame
 * @crc_count: counter of captured frame CRCs
 * @transfer: transfers a message representing a single AUX transaction
 *
 * The .dev field should be set to a pointer to the device that implements
 * the AUX channel.
 *
 * The .name field may be used to specify the name of the I2C adapter. If set to
 * NULL, dev_name() of .dev will be used.
 *
 * Drivers provide a hardware-specific implementation of how transactions
 * are executed via the .transfer() function. A pointer to a drm_dp_aux_msg
 * structure describing the transaction is passed into this function. Upon
 * success, the implementation should return the number of payload bytes
 * that were transferred, or a negative error-code on failure. Helpers
 * propagate errors from the .transfer() function, with the exception of
 * the -EBUSY error, which causes a transaction to be retried. On a short,
 * helpers will return -EPROTO to make it simpler to check for failure.
 *
 * An AUX channel can also be used to transport I2C messages to a sink. A
 * typical application of that is to access an EDID that's present in the
 * sink device. The .transfer() function can also be used to execute such
 * transactions. The drm_dp_aux_register() function registers an I2C
 * adapter that can be passed to drm_probe_ddc(). Upon removal, drivers
 * should call drm_dp_aux_unregister() to remove the I2C adapter.
 * The I2C adapter uses long transfers by default; if a partial response is
 * received, the adapter will drop down to the size given by the partial
 * response for this transaction only.
 *
 * Note that the aux helper code assumes that the .transfer() function
 * only modifies the reply field of the drm_dp_aux_msg structure.  The
 * retry logic and i2c helpers assume this is the case.
 */
#define BARE_ADDRESS_SIZE 3
#define HEADER_SIZE (BARE_ADDRESS_SIZE + 1)

static void
intel_dp_aux_header(UINT8 txbuf[HEADER_SIZE],
					const struct drm_dp_aux_msg *msg)
{
	txbuf[0] = (msg->request << 4) | ((msg->address >> 16) & 0xf);
	txbuf[1] = (msg->address >> 8) & 0xff;
	txbuf[2] = msg->address & 0xff;
	txbuf[3] = msg->size - 1;
}
#define EPERM 1	   /* Operation not permitted */
#define ENOENT 2   /* No such file or directory */
#define ESRCH 3	   /* No such process */
#define EINTR 4	   /* Interrupted system call */
#define EIO 5	   /* I/O error */
#define ENXIO 6	   /* No such device or address */
#define E2BIG 7	   /* Argument list too long */
#define ENOEXEC 8  /* Exec format error */
#define EBADF 9	   /* Bad file number */
#define ECHILD 10  /* No child processes */
#define EAGAIN 11  /* Try again */
#define ENOMEM 12  /* Out of memory */
#define EACCES 13  /* Permission denied */
#define EFAULT 14  /* Bad address */
#define ENOTBLK 15 /* Block device required */
#define EBUSY 16   /* Device or resource busy */
#define EEXIST 17  /* File exists */
#define EXDEV 18   /* Cross-device link */
#define ENODEV 19  /* No such device */
#define ENOTDIR 20 /* Not a directory */
#define EISDIR 21  /* Is a directory */
#define EINVAL 22  /* Invalid argument */
#define ENFILE 23  /* File table overflow */
#define EMFILE 24  /* Too many open files */
#define ENOTTY 25  /* Not a typewriter */
#define ETXTBSY 26 /* Text file busy */
#define EFBIG 27   /* File too large */
#define ENOSPC 28  /* No space left on device */
#define ESPIPE 29  /* Illegal seek */
#define EROFS 30   /* Read-only file system */
#define EMLINK 31  /* Too many links */
#define EPIPE 32   /* Broken pipe */
#define EDOM 33	   /* Math argument out of domain of func */
#define ERANGE 34  /* Math result not representable */
#define ETIMEDOUT 35
static UINT32
intel_dp_aux_wait_done(i915_CONTROLLER *controller)
{
	UINT32 pin = controller->OutputPath.AuxCh;
	UINT64 ch_ctl = _DPA_AUX_CH_CTL + (pin << 8);
	const unsigned int timeout_ms = 10;
	UINT32 status;
	BOOLEAN done;

#define C (((status = controller->read32(ch_ctl)) & DP_AUX_CH_CTL_SEND_BUSY) == 0)
	gBS->Stall(10 * 1000);
	done = C;
	/* 	done = wait_event_timeout(i915->gmbus_wait_queue, C,
				  msecs_to_jiffies_timeout(timeout_ms)); */

	/* just trace the final value */
	//trace_i915_reg_rw(FALSE, ch_ctl, status, sizeof(status), TRUE);

	if (!done)
		PRINT_DEBUG(EFI_D_ERROR,
					"%s: did not complete or timeout within %ums (status 0x%08x)\n",
					pin, timeout_ms, status);
#undef C

	return status;
}
static UINT32 skl_get_aux_clock_divider(int index)
{
	/*
	 * SKL doesn't need us to program the AUX clock divider (Hardware will
	 * derive the clock from CDCLK automatically). We still implement the
	 * get_aux_clock_divider vfunc to plug-in into the existing code.
	 */
	return index ? 0 : 1;
}
UINT32 intel_dp_pack_aux(const UINT8 *src, int src_bytes)
{
	int i;
	UINT32 v = 0;

	if (src_bytes > 4)
		src_bytes = 4;
	for (i = 0; i < src_bytes; i++)
		v |= ((UINT32)src[i]) << ((3 - i) * 8);
	return v;
}

static void intel_dp_unpack_aux(UINT32 src, UINT8 *dst, int dst_bytes)
{
	int i;
	if (dst_bytes > 4)
		dst_bytes = 4;
	for (i = 0; i < dst_bytes; i++)
		dst[i] = src >> ((3 - i) * 8);
}
static UINT32 skl_get_aux_send_ctl(
	int send_bytes,
	UINT32 unused)
{
	/* 	struct intel_digital_port *intel_dig_port = dp_to_dig_port(intel_dp);
	struct drm_i915_private *i915 =
			to_i915(intel_dig_port->base.base.dev);
	enum phy phy = intel_port_to_phy(i915, intel_dig_port->base.port); */
	UINT32 ret;

	ret = DP_AUX_CH_CTL_SEND_BUSY |
		  DP_AUX_CH_CTL_DONE |
		  DP_AUX_CH_CTL_INTERRUPT |
		  DP_AUX_CH_CTL_TIME_OUT_ERROR |
		  DP_AUX_CH_CTL_TIME_OUT_MAX |
		  DP_AUX_CH_CTL_RECEIVE_ERROR |
		  (send_bytes << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		  DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		  DP_AUX_CH_CTL_SYNC_PULSE_SKL(32);

	/* if (intel_phy_is_tc(i915, phy) &&
	    intel_dig_port->tc_mode == TC_PORT_TBT_ALT)
		ret |= DP_AUX_CH_CTL_TBT_IO; */

	return ret;
}
static int
intel_dp_aux_xfer(i915_CONTROLLER *controller,
				  const UINT8 *send, int send_bytes,
				  UINT8 *recv, int recv_size,
				  UINT32 aux_send_ctl_flags)
{
	//	struct intel_digital_port *intel_dig_port = dp_to_dig_port(intel_dp);
	//	struct drm_i915_private *i915 =
	//			to_i915(intel_dig_port->base.base.dev);
	//struct intel_uncore *uncore = &i915->uncore;
	//	enum phy phy = intel_port_to_phy(i915, intel_dig_port->base.port);
	//	BOOLEAN is_tc_port = intel_phy_is_tc(i915, phy);
	UINT64 ch_ctl, ch_data[5];
	UINT32 aux_clock_divider;
	//enum intel_display_power_domain aux_domain;
	//intel_wakeref_t aux_wakeref;
	//	intel_wakeref_t pps_wakeref;
	int i, ret, recv_bytes;
	int try, clock = 0;
	UINT32 val;
	UINT32 status;
	//BOOLEAN vdd;
	UINT32 pin = controller->OutputPath.AuxCh;
	ch_ctl = _DPA_AUX_CH_CTL + (pin << 8);
#define _PICK_EVEN(__index, __a, __b) ((__a) + (__index) * ((__b) - (__a)))

	for (i = 0; i < ARRAY_SIZE(ch_data); i++)
		ch_data[i] = (_DPA_AUX_CH_DATA1 + pin * (0x64114 - _DPA_AUX_CH_DATA1)) + (i)*4;

	/* 	if (is_tc_port)
		intel_tc_port_lock(intel_dig_port);

	aux_domain = intel_aux_power_domain(intel_dig_port);

	aux_wakeref = intel_display_power_get(i915, aux_domain);
	pps_wakeref = pps_lock(intel_dp); */

	/*
	 * We will be called with VDD already enabled for dpcd/edid/oui reads.
	 * In such cases we want to leave VDD enabled and it's up to upper layers
	 * to turn it off. But for eg. i2c-dev access we need to turn it on/off
	 * ourselves.
	 */
	controller->write32(PP_CONTROL, 15);
	//vdd = edp_panel_vdd_on(intel_dp);

	/* dp aux is extremely sensitive to irq latency, hence request the
	 * lowest possible wakeup latency and so prevent the cpu from going into
	 * deep sleep states.
	 */
	/* 	cpu_latency_qos_update_request(&i915->pm_qos, 0);

	intel_dp_check_edp(intel_dp); */

	/* Try to wait for any previous AUX channel activity */
	for (try = 0; try < 3; try++)
	{
		status = controller->read32(ch_ctl);
		if ((status & DP_AUX_CH_CTL_SEND_BUSY) == 0)
			break;
		gBS->Stall(1000);
	}
	/* just trace the final value */
	//trace_i915_reg_rw(FALSE, ch_ctl, status, sizeof(status), TRUE);

	if (try == 3)
	{
		/* 		const UINT32 status = controller->read32(_DPA_AUX_CH_CTL + (pin << 8));	 */

		/* 		if (status != intel_dp->aux_busy_last_status) {
			drm_WARN(&i915->drm, 1,
				 "%s: not started (status 0x%08x)\n",
				 pin, status);
			intel_dp->aux_busy_last_status = status;
		} */

		ret = -EBUSY;
		goto out;
	}

	/* Only 5 data registers! */
	if (send_bytes > 20 || recv_size > 20)
	{
		ret = -E2BIG;
		goto out;
	}

	while ((aux_clock_divider = skl_get_aux_clock_divider(clock++)))
	{
		UINT32 send_ctl = skl_get_aux_send_ctl(
			send_bytes,
			aux_clock_divider);

		send_ctl |= aux_send_ctl_flags;

		/* Must try at least 3 times according to DP spec */
		for (try = 0; try < 5; try++)
		{
			/* Load the send data into the aux channel data registers */
			for (i = 0; i < send_bytes; i += 4)
				controller->write32(
					ch_data[i >> 2],
					intel_dp_pack_aux(send + i,
									  send_bytes - i));

			/* Send the command and wait for it to complete */
			controller->write32(ch_ctl, send_ctl);

			status = intel_dp_aux_wait_done(controller);

			/* Clear done status and any errors */
			controller->write32(
				ch_ctl,
				status |
					DP_AUX_CH_CTL_DONE |
					DP_AUX_CH_CTL_TIME_OUT_ERROR |
					DP_AUX_CH_CTL_RECEIVE_ERROR);

			/* DP CTS 1.2 Core Rev 1.1, 4.2.1.1 & 4.2.1.2
			 *   400us delay required for errors and timeouts
			 *   Timeout errors from the HW already meet this
			 *   requirement so skip to next iteration
			 */
			if (status & DP_AUX_CH_CTL_TIME_OUT_ERROR)
				continue;

			if (status & DP_AUX_CH_CTL_RECEIVE_ERROR)
			{
				gBS->Stall(500);
				continue;
			}
			if (status & DP_AUX_CH_CTL_DONE)
				goto done;
		}
	}

	if ((status & DP_AUX_CH_CTL_DONE) == 0)
	{
		PRINT_DEBUG(EFI_D_ERROR, "%s: not done (status 0x%08x)\n",
					pin, status);
		ret = -EBUSY;
		goto out;
	}

done:
	/* Check for timeout or receive error.
	 * Timeouts occur when the sink is not connected
	 */
	if (status & DP_AUX_CH_CTL_RECEIVE_ERROR)
	{
		PRINT_DEBUG(EFI_D_ERROR, "%s: receive error (status 0x%08x)\n",
					pin, status);
		ret = -EIO;
		goto out;
	}

	/* Timeouts occur when the device isn't connected, so they're
	 * "normal" -- don't fill the kernel log with these */
	if (status & DP_AUX_CH_CTL_TIME_OUT_ERROR)
	{
		PRINT_DEBUG(EFI_D_ERROR, "%s: timeout (status 0x%08x)\n",
					pin, status);
		ret = -ETIMEDOUT;
		goto out;
	}

	/* Unload any bytes sent back from the other side */
	recv_bytes = ((status & DP_AUX_CH_CTL_MESSAGE_SIZE_MASK) >>
				  DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT);

	/*
	 * By BSpec: "Message sizes of 0 or >20 are not allowed."
	 * We have no idea of what happened so we return -EBUSY so
	 * drm layer takes care for the necessary retries.
	 */
	if (recv_bytes == 0 || recv_bytes > 20)
	{
		PRINT_DEBUG(EFI_D_ERROR,
					"%s: Forbidden recv_bytes = %d on aux transaction\n",
					pin, recv_bytes);
		ret = -EBUSY;
		goto out;
	}

	if (recv_bytes > recv_size)
		recv_bytes = recv_size;

	for (i = 0; i < recv_bytes; i += 4)
		intel_dp_unpack_aux(controller->read32(ch_data[i >> 2]),
							recv + i, recv_bytes - i);

	ret = recv_bytes;
out:

	val = controller->read32(PP_CONTROL);
	val &= ~(1 << 3);
	controller->write32(PP_CONTROL, val);
	//	pps_unlock(intel_dp, pps_wakeref);
	//	intel_display_power_put_async(i915, aux_domain, aux_wakeref);

	//	if (is_tc_port)
	//		intel_tc_port_unlock(intel_dig_port);

	return ret;
}
void memcpy(void *dest, void *src, UINTN n)
{
	// Typecast src and dest addresses to (char *)
	char *csrc = (char *)src;
	char *cdest = (char *)dest;

	// Copy contents of src[] to dest[]
	for (int i = 0; i < n; i++)
		cdest[i] = csrc[i];
}
/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use min/max/clamp at all, of course.
 */
#define min_t(type, x, y) (                \
	{                                      \
		type __min1 = (x);                 \
		type __min2 = (y);                 \
		__min1 < __min2 ? __min1 : __min2; \
	})

#define max_t(type, x, y) (                \
	{                                      \
		type __max1 = (x);                 \
		type __max2 = (y);                 \
		__max1 > __max2 ? __max1 : __max2; \
	})

/**
 * clamp_t - return a value clamped to a given range using a given type
 * @type: the type of variable to use
 * @val: current value
 * @lo: minimum allowable value
 * @hi: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of type
 * 'type' to make all the comparisons.
 */
#define clamp_t(type, val, lo, hi) min_t(type, max_t(type, val, lo), hi)
static INT32 intel_dp_aux_transfer(i915_CONTROLLER *controller, struct drm_dp_aux_msg *msg)
{

	//struct intel_dp *intel_dp = container_of(aux, struct intel_dp, aux);
	UINT8 txbuf[20], rxbuf[20];
	UINTN txsize, rxsize;
	int ret;

	intel_dp_aux_header(txbuf, msg);

	switch (msg->request & ~DP_AUX_I2C_MOT)
	{
	case DP_AUX_NATIVE_WRITE:
	case DP_AUX_I2C_WRITE:
	case DP_AUX_I2C_WRITE_STATUS_UPDATE:
		txsize = msg->size ? HEADER_SIZE + msg->size : BARE_ADDRESS_SIZE;
		rxsize = 2; /* 0 or 1 data bytes */

		if (txsize > 20)
			return -E2BIG;

		//WARN_ON(!msg->buffer != !msg->size);

		if (msg->buffer)
			memcpy(txbuf + HEADER_SIZE, msg->buffer, msg->size);

		ret = intel_dp_aux_xfer(controller, txbuf, txsize,
								rxbuf, rxsize, 0);
		if (ret > 0)
		{
			msg->reply = rxbuf[0] >> 4;

			if (ret > 1)
			{
				/* Number of bytes written in a short write. */
				ret = clamp_t(int, rxbuf[1], 0, msg->size);
			}
			else
			{
				/* Return payload size. */
				ret = msg->size;
			}
		}
		break;

	case DP_AUX_NATIVE_READ:
	case DP_AUX_I2C_READ:
		txsize = msg->size ? HEADER_SIZE : BARE_ADDRESS_SIZE;
		rxsize = msg->size + 1;

		if ((rxsize > 20))
			return -E2BIG;

		ret = intel_dp_aux_xfer(controller, txbuf, txsize,
								rxbuf, rxsize, 0);
		if (ret > 0)
		{
			msg->reply = rxbuf[0] >> 4;
			/*
			 * Assume happy day, and copy the data. The caller is
			 * expected to check msg->reply before touching it.
			 *
			 * Return payload size.
			 */
			ret--;
			memcpy(msg->buffer, rxbuf + 1, ret);
		}
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * DOC: dp helpers
 *
 * The DisplayPort AUX channel is an abstraction to allow generic, driver-
 * independent access to AUX functionality. Drivers can take advantage of
 * this by filling in the fields of the drm_dp_aux structure.
 *
 * Transactions are described using a hardware-independent drm_dp_aux_msg
 * structure, which is passed into a driver's .transfer() implementation.
 * Both native and I2C-over-AUX transactions are supported.
 */
#define AUX_RETRY_INTERVAL 500 /* us */

static RETURN_STATUS drm_dp_dpcd_access(UINT8 request,
										unsigned int offset, void *buffer, UINT32 size, i915_CONTROLLER *controller)
{
	struct drm_dp_aux_msg msg;
	unsigned int retry, native_reply;
	RETURN_STATUS err = 0, ret = 0;

	// memset(&msg, 0, sizeof(msg)); //Not Defined
	msg.address = offset;
	msg.request = request;
	msg.buffer = buffer;
	msg.size = size;

	/*
	 * The specification doesn't give any recommendation on how often to
	 * retry native transactions. We used to retry 7 times like for
	 * aux i2c transactions but real world devices this wasn't
	 * sufficient, bump to 32 which makes Dell 4k monitors happier.
	 */
	for (retry = 0; retry < 32; retry++)
	{
		if (ret != 0 && ret != -RETURN_TIMEOUT)
		{
			gBS->Stall(AUX_RETRY_INTERVAL);
		}

		ret = intel_dp_aux_transfer(controller, &msg);
		if (ret >= 0)
		{
			native_reply = msg.reply & DP_AUX_NATIVE_REPLY_MASK;
			if (native_reply == DP_AUX_NATIVE_REPLY_ACK)
			{
				if (ret == size)
					goto unlock;

				ret = -RETURN_PROTOCOL_ERROR;
			}
			else
				ret = -RETURN_NOT_FOUND;
		}

		/*
		 * We want the error we return to be the error we received on
		 * the first transaction, since we may get a different error the
		 * next time we retry
		 */
		if (!err)
			err = ret;
	}

	PRINT_DEBUG(EFI_D_ERROR, "Too many retries, giving up. First error: %d\n", err);
	ret = err;

unlock:
	return ret;
}
/**
 * drm_dp_dpcd_read() - read a series of bytes from the DPCD
 * @aux: DisplayPort AUX channel (SST or MST)
 * @offset: address of the (first) register to read
 * @buffer: buffer to store the register values
 * @size: number of bytes in @buffer
 *
 * Returns the number of bytes transferred on success, or a negative error
 * code on failure. -EIO is returned if the request was NAKed by the sink or
 * if the retry count was exceeded. If not all bytes were transferred, this
 * function returns -EPROTO. Errors from the underlying AUX channel transfer
 * function, with the exception of -EBUSY (which causes the transaction to
 * be retried), are propagated to the caller.
 */
INT32 drm_dp_dpcd_read(unsigned int offset,
					   void *buffer, UINT32 size, i915_CONTROLLER *controller)
{
	int ret;

	/*
	 * HP ZR24w corrupts the first DPCD access after entering power save
	 * mode. Eg. on a read, the entire buffer will be filled with the same
	 * byte. Do a throw away read to avoid corrupting anything we care
	 * about. Afterwards things will work correctly until the monitor
	 * gets woken up and subsequently re-enters power save mode.
	 *
	 * The user pressing any button on the monitor is enough to wake it
	 * up, so there is no particularly good place to do the workaround.
	 * We just have to do it before any DPCD access and hope that the
	 * monitor doesn't power down exactly after the throw away read.
	 */
	//if (!aux->is_remote) {
	ret = drm_dp_dpcd_access(DP_AUX_NATIVE_READ, DP_DPCD_REV,
							 buffer, 1, controller);
	if (ret != 1)
		goto out;
	//	}

	//	if (aux->is_remote)
	//		ret = drm_dp_mst_dpcd_read(aux, offset, buffer, size);
	//	else
	ret = drm_dp_dpcd_access(DP_AUX_NATIVE_READ, offset,
							 buffer, size, controller);

out:
	return ret;
}
BOOLEAN
intel_dp_get_link_status(UINT8 link_status[DP_LINK_STATUS_SIZE], i915_CONTROLLER *controller)
{
	return drm_dp_dpcd_read(DP_LANE0_1_STATUS, link_status,
							DP_LINK_STATUS_SIZE, controller) == DP_LINK_STATUS_SIZE;
}
/* Helpers for DP link training */
static UINT8 dp_link_status(const UINT8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
}
static UINT8 dp_get_lane_status(const UINT8 link_status[DP_LINK_STATUS_SIZE],
								int lane)
{
	int i = DP_LANE0_1_STATUS + (lane >> 1);
	int s = (lane & 1) * 4;
	UINT8 l = dp_link_status(link_status, i);
	return (l >> s) & 0xf;
}
BOOLEAN drm_dp_clock_recovery_ok(const UINT8 link_status[DP_LINK_STATUS_SIZE],
								 int lane_count)
{
	int lane;
	UINT8 lane_status;

	for (lane = 0; lane < lane_count; lane++)
	{
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_LANE_CR_DONE) == 0)
			return FALSE;
	}
	return TRUE;
}

UINT8 drm_dp_get_adjust_request_voltage(const UINT8 link_status[DP_LINK_STATUS_SIZE],
										int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ? DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT : DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT);
	UINT8 l = dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_VOLTAGE_SWING_SHIFT;
}

UINT8 drm_dp_get_adjust_request_pre_emphasis(const UINT8 link_status[DP_LINK_STATUS_SIZE],
											 int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ? DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT : DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT);
	UINT8 l = dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_PRE_EMPHASIS_SHIFT;
}
/* These are source-specific values. */
UINT8
intel_dp_voltage_max()
{
	/* 	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	enum port port = encoder->port; */

	/* 	if (HAS_DDI(dev_priv))
		return intel_ddi_dp_voltage_max(encoder);
	else if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv))
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_3;
	else if (IS_IVYBRIDGE(dev_priv) && port == PORT_A)
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_2;
	else if (HAS_PCH_CPT(dev_priv) && port != PORT_A)
		return DP_TRAIN_VOLTAGE_SWING_LEVEL_3;
	else */
	return DP_TRAIN_VOLTAGE_SWING_LEVEL_2;
}

UINT8
intel_dp_pre_emphasis_max(UINT8 voltage_swing)
{
	/* 	struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	struct intel_encoder *encoder = &dp_to_dig_port(intel_dp)->base;
	enum port port = encoder->port; */

	/* 	if (HAS_DDI(dev_priv)) {
		return intel_ddi_dp_pre_emphasis_max(encoder, voltage_swing);
	} else if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
		switch (voltage_swing & DP_TRAIN_VOLTAGE_SWING_MASK) {
		case DP_TRAIN_VOLTAGE_SWING_LEVEL_0:
			return DP_TRAIN_PRE_EMPH_LEVEL_3;
		case DP_TRAIN_VOLTAGE_SWING_LEVEL_1:
			return DP_TRAIN_PRE_EMPH_LEVEL_2;
		case DP_TRAIN_VOLTAGE_SWING_LEVEL_2:
			return DP_TRAIN_PRE_EMPH_LEVEL_1;
		case DP_TRAIN_VOLTAGE_SWING_LEVEL_3:
		default:
			return DP_TRAIN_PRE_EMPH_LEVEL_0;
		}
	} else if (IS_IVYBRIDGE(dev_priv) && port == PORT_A) {
		switch (voltage_swing & DP_TRAIN_VOLTAGE_SWING_MASK) {
		case DP_TRAIN_VOLTAGE_SWING_LEVEL_0:
			return DP_TRAIN_PRE_EMPH_LEVEL_2;
		case DP_TRAIN_VOLTAGE_SWING_LEVEL_1:
		case DP_TRAIN_VOLTAGE_SWING_LEVEL_2:
			return DP_TRAIN_PRE_EMPH_LEVEL_1;
		default:
			return DP_TRAIN_PRE_EMPH_LEVEL_0;
		}
	} else { */
	switch (voltage_swing & DP_TRAIN_VOLTAGE_SWING_MASK)
	{
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_0:
		return DP_TRAIN_PRE_EMPH_LEVEL_2;
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_1:
		return DP_TRAIN_PRE_EMPH_LEVEL_2;
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_2:
		return DP_TRAIN_PRE_EMPH_LEVEL_1;
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_3:
	default:
		return DP_TRAIN_PRE_EMPH_LEVEL_0;
	}
	//}
}
struct intel_dp
{
	UINT8 lane_count;
	UINT8 train_set[4];
	UINT8 pipe_bpp;
	int link_rate;
	i915_CONTROLLER *controller;
	bool use_max_rate;
	/* source rates */
	int num_source_rates;
	const int *source_rates;
	/* sink rates as reported by DP_MAX_LINK_RATE/DP_SUPPORTED_LINK_RATES */
	int num_sink_rates;
	int sink_rates[DP_MAX_SUPPORTED_RATES];
	//BOOLEAN use_rate_select;
	/* intersection of source and sink rates */
	int num_common_rates;
	int common_rates[DP_MAX_SUPPORTED_RATES];
	/* Max lane count for the current link */
	int max_link_lane_count;
	/* Max rate for the current link */
	int max_link_rate;
};

void intel_dp_get_adjust_train(struct intel_dp *intel_dp,
							   const UINT8 link_status[DP_LINK_STATUS_SIZE])
{
	UINT8 v = 0;
	UINT8 p = 0;
	int lane;
	UINT8 voltage_max;
	UINT8 preemph_max;

	for (lane = 0; lane < intel_dp->lane_count; lane++)
	{
		UINT8 this_v = drm_dp_get_adjust_request_voltage(link_status, lane);
		UINT8 this_p = drm_dp_get_adjust_request_pre_emphasis(link_status, lane);
		PRINT_DEBUG(EFI_D_ERROR, "this_v:%u v:%u  this_p:%u  p:%u \n", this_v, v, this_p, p);
		if (this_v > v)
			v = this_v;
		if (this_p > p)
			p = this_p;
	}

	voltage_max = intel_dp_voltage_max();
	if (v >= voltage_max)
		v = voltage_max | DP_TRAIN_MAX_SWING_REACHED;

	preemph_max = intel_dp_pre_emphasis_max(v);
	if (p >= preemph_max)
		p = preemph_max | DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

	PRINT_DEBUG(EFI_D_ERROR, "v:%u  p:%u \n", v, p);

	UINT8 val = intel_dp->train_set[0];
	if (val < 8)
	{
		val++;
	}
	for (lane = 0; lane < 4; lane++)
	{
		intel_dp->train_set[lane] = v | p;
		PRINT_DEBUG(EFI_D_ERROR, "TrainSet[%u]: %u \n", lane, intel_dp->train_set[lane]);
	}
}
/**
 * drm_dp_dpcd_write() - write a series of bytes to the DPCD
 * @aux: DisplayPort AUX channel (SST or MST)
 * @offset: address of the (first) register to write
 * @buffer: buffer containing the values to write
 * @size: number of bytes in @buffer
 *
 * Returns the number of bytes transferred on success, or a negative error
 * code on failure. -EIO is returned if the request was NAKed by the sink or
 * if the retry count was exceeded. If not all bytes were transferred, this
 * function returns -EPROTO. Errors from the underlying AUX channel transfer
 * function, with the exception of -EBUSY (which causes the transaction to
 * be retried), are propagated to the caller.
 */
INT32 drm_dp_dpcd_write(unsigned int offset,
						void *buffer, UINT32 size, i915_CONTROLLER *controller)
{
	int ret;

	//if (aux->is_remote)
	//	ret = drm_dp_mst_dpcd_write(aux, offset, buffer, size);
	//else
	ret = drm_dp_dpcd_access(DP_AUX_NATIVE_WRITE, offset,
							 buffer, size, controller);

	return ret;
}
#define DP_VOLTAGE_0_4 (0 << 25)
#define DP_VOLTAGE_0_6 (1 << 25)
#define DP_VOLTAGE_0_8 (2 << 25)
#define DP_VOLTAGE_1_2 (3 << 25)
#define DP_VOLTAGE_MASK (7 << 25)
#define DP_VOLTAGE_SHIFT 25

/* Signal pre-emphasis levels, like voltages, the other end tells us what
 * they want
 */
#define DP_PRE_EMPHASIS_0 (0 << 22)
#define DP_PRE_EMPHASIS_3_5 (1 << 22)
#define DP_PRE_EMPHASIS_6 (2 << 22)
#define DP_PRE_EMPHASIS_9_5 (3 << 22)
#define DP_PRE_EMPHASIS_MASK (7 << 22)
#define DP_PRE_EMPHASIS_SHIFT 22
static UINT32 g4x_signal_levels(UINT8 train_set)
{
	UINT32 signal_levels = 0;

	switch (train_set & DP_TRAIN_VOLTAGE_SWING_MASK)
	{
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_0:
	default:
		signal_levels |= DP_VOLTAGE_0_4;
		break;
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_1:
		signal_levels |= DP_VOLTAGE_0_6;
		break;
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_2:
		signal_levels |= DP_VOLTAGE_0_8;
		break;
	case DP_TRAIN_VOLTAGE_SWING_LEVEL_3:
		signal_levels |= DP_VOLTAGE_1_2;
		break;
	}
	switch (train_set & DP_TRAIN_PRE_EMPHASIS_MASK)
	{
	case DP_TRAIN_PRE_EMPH_LEVEL_0:
	default:
		signal_levels |= DP_PRE_EMPHASIS_0;
		break;
	case DP_TRAIN_PRE_EMPH_LEVEL_1:
		signal_levels |= DP_PRE_EMPHASIS_3_5;
		break;
	case DP_TRAIN_PRE_EMPH_LEVEL_2:
		signal_levels |= DP_PRE_EMPHASIS_6;
		break;
	case DP_TRAIN_PRE_EMPH_LEVEL_3:
		signal_levels |= DP_PRE_EMPHASIS_9_5;
		break;
	}
	return signal_levels;
}
#define DP_PORT_EN (1 << 31)

static void
g4x_set_signal_levels(struct intel_dp *intel_dp)
{
	//struct drm_i915_private *dev_priv = dp_to_i915(intel_dp);
	UINT8 train_set = intel_dp->train_set[0];
	UINT32 signal_levels;

	signal_levels = g4x_signal_levels(train_set);

	PRINT_DEBUG(EFI_D_ERROR, "Using signal levels %08x\n",
				signal_levels);
	PRINT_DEBUG(EFI_D_ERROR, "Using vswing level %d%a, pre-emphasis level %d%a\n",
				train_set & DP_TRAIN_VOLTAGE_SWING_MASK,
				train_set & DP_TRAIN_MAX_SWING_REACHED ? " (max)" : "",
				(train_set & DP_TRAIN_PRE_EMPHASIS_MASK) >>
					DP_TRAIN_PRE_EMPHASIS_SHIFT,
				train_set & DP_TRAIN_MAX_PRE_EMPHASIS_REACHED ? " (max)" : "");
	UINT32 DP = intel_dp->controller->read32(DDI_BUF_CTL(intel_dp->controller->OutputPath.Port));
	DP |= DP_PORT_EN;
	DP &= ~(DP_VOLTAGE_MASK | DP_PRE_EMPHASIS_MASK);
	DP |= signal_levels;

	intel_dp->controller->write32(DDI_BUF_CTL(intel_dp->controller->OutputPath.Port), DP);
	//intel_de_posting_read(dev_priv, intel_dp->output_reg);
}
static void intel_dp_set_signal_levels(struct intel_dp *intel_dp)
{

	//Write to Appropraite DDI_BUF_CTL
	g4x_set_signal_levels(intel_dp);
}

static BOOLEAN
intel_dp_update_link_train(struct intel_dp *intel_dp)
{
	int ret;

	intel_dp_set_signal_levels(intel_dp);

	ret = drm_dp_dpcd_write(DP_TRAINING_LANE0_SET,
							intel_dp->train_set, intel_dp->lane_count, intel_dp->controller);

	return ret == intel_dp->lane_count;
}
static inline UINT8
drm_dp_training_pattern_mask()
{
	//return (dpcd[DP_DPCD_REV] >= 0x14) ? DP_TRAINING_PATTERN_MASK_1_4 :
	//	DP_TRAINING_PATTERN_MASK;
	return DP_TRAINING_PATTERN_MASK;
}
/* CPT Link training mode */
#define DP_LINK_TRAIN_PAT_1_CPT (0 << 8)
#define DP_LINK_TRAIN_PAT_2_CPT (1 << 8)
#define DP_LINK_TRAIN_PAT_IDLE_CPT (2 << 8)
#define DP_LINK_TRAIN_OFF_CPT (3 << 8)
#define DP_LINK_TRAIN_MASK_CPT (7 << 8)
#define DP_LINK_TRAIN_SHIFT_CPT 8
static void
g4x_set_link_train(struct intel_dp *intel_dp,
				   UINT8 dp_train_pat)
{
	UINT32 DP = intel_dp->controller->read32(DP_TP_CTL(intel_dp->controller->OutputPath.Port));

	DP &= ~DP_LINK_TRAIN_MASK_CPT;

	switch (dp_train_pat & DP_TRAINING_PATTERN_MASK)
	{
	case DP_TRAINING_PATTERN_DISABLE:
		DP |= DP_LINK_TRAIN_OFF_CPT;
		break;
	case DP_TRAINING_PATTERN_1:
		DP |= DP_LINK_TRAIN_PAT_1_CPT;
		break;
	case DP_TRAINING_PATTERN_2:
		DP |= DP_LINK_TRAIN_PAT_2_CPT;
		break;
	case DP_TRAINING_PATTERN_3:
		/* 		drm_dbg_kms(&dev_priv->drm,
			    "TPS3 not supported, using TPS2 instead\n"); */
		DP |= DP_LINK_TRAIN_PAT_2_CPT;
		break;
	}
	intel_dp->controller->write32(DP_TP_CTL(intel_dp->controller->OutputPath.Port), DP);
}

void intel_dp_program_link_training_pattern(struct intel_dp *intel_dp,
											UINT8 dp_train_pat)
{
	//	UINT8 train_pat_mask = drm_dp_training_pattern_mask();

	g4x_set_link_train(intel_dp, dp_train_pat);
}

static BOOLEAN
intel_dp_set_link_train(struct intel_dp *intel_dp,
						UINT8 dp_train_pat)
{
	UINT8 buf[sizeof(intel_dp->train_set) + 1];
	int ret, len;
	i915_CONTROLLER *controller = intel_dp->controller;
	intel_dp_program_link_training_pattern(intel_dp, dp_train_pat);

	buf[0] = dp_train_pat;
	if ((dp_train_pat & DP_TRAINING_PATTERN_MASK) ==
		DP_TRAINING_PATTERN_DISABLE)
	{
		/* don't write DP_TRAINING_LANEx_SET on disable */
		len = 1;
	}
	else
	{
		for (int i = 0; i < intel_dp->lane_count; i++)
		{
			buf[i + 1] = intel_dp->train_set[i];
		}
		/* DP_TRAINING_LANEx_SET follow DP_TRAINING_PATTERN_SET */
		//memcpy(buf + 1, intel_dp->train_set, intel_dp->lane_count);
		len = intel_dp->lane_count + 1;
	}

	ret = drm_dp_dpcd_write(DP_TRAINING_PATTERN_SET,
							buf, len, controller);

	return ret == len;
}

static BOOLEAN
intel_dp_reset_link_train(struct intel_dp *intel_dp,
						  UINT8 dp_train_pat, i915_CONTROLLER *controller)
{
	for (int i = 0; i < 4; i++)
	{
		intel_dp->train_set[i] = 0;
	}
	intel_dp_set_signal_levels(intel_dp);
	return intel_dp_set_link_train(intel_dp, dp_train_pat);
}

UINT8 drm_dp_link_rate_to_bw_code(int link_rate)
{
	/* Spec says link_bw = link_rate / 0.27Gbps */
	return link_rate / 27000;
}
void intel_dp_compute_rate(struct intel_dp *intel_dp, int port_clock,
						   UINT8 *link_bw, UINT8 *rate_select)
{
	/* eDP 1.4 rate select method. */
	/* 	if (intel_dp->use_rate_select) {
		*link_bw = 0;
		*rate_select =
			intel_dp_rate_select(intel_dp, port_clock);
	} else { */
	*link_bw = drm_dp_link_rate_to_bw_code(port_clock);
	*rate_select = 0;
	//	}
}
static BOOLEAN intel_dp_link_max_vswing_reached(struct intel_dp *intel_dp)
{
	int lane;

	for (lane = 0; lane < intel_dp->lane_count; lane++)
		if ((intel_dp->train_set[lane] &
			 DP_TRAIN_MAX_SWING_REACHED) == 0)
			return FALSE;

	return TRUE;
}
#define DP_PLL_FREQ_270MHZ (0 << 16)
#define DP_PLL_FREQ_162MHZ (1 << 16)
#define DP_PLL_FREQ_MASK (3 << 16)
/* Enable corresponding port and start training pattern 1 */
static BOOLEAN
intel_dp_link_training_clock_recovery(struct intel_dp *intel_dp)
{
	i915_CONTROLLER *controller = intel_dp->controller;
	//struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	UINT8 voltage;
	int voltage_tries, cr_tries, max_cr_tries;
	BOOLEAN max_vswing_reached = FALSE;
	UINT8 link_config[2];
	UINT8 link_bw, rate_select;
	intel_dp_compute_rate(intel_dp, intel_dp->link_rate,
						  &link_bw, &rate_select);
	/* if (intel_dp->prepare_link_retrain)
		intel_dp->prepare_link_retrain(intel_dp);
 */
	/* if ((controller->read32(0x64000) & DP_PLL_FREQ_MASK) == DP_PLL_FREQ_162MHZ)
			intel_dp_compute_rate(intel_dp, 162000,
			      &link_bw, &rate_select);
		else
			intel_dp_compute_rate(intel_dp, 270000,
			      &link_bw, &rate_select); */
	//WHAT RATE IS PLUGGED IN? Port Clock

	if (link_bw)
		PRINT_DEBUG(EFI_D_ERROR, "Using LINK_BW_SET value %u\n", link_bw);
	else
		PRINT_DEBUG(EFI_D_ERROR, "Using LINK_RATE_SET value %u\n", rate_select);

	/* Write the link configuration data */
	link_config[0] = link_bw;
	link_config[1] = intel_dp->lane_count;
	/* 	if (drm_dp_enhanced_frame_cap(intel_dp->dpcd))
		link_config[1] |= DP_LANE_COUNT_ENHANCED_FRAME_EN; */
	drm_dp_dpcd_write(DP_LINK_BW_SET, link_config, 2, controller);

	/* eDP 1.4 rate select method. */
	if (!link_bw)
		drm_dp_dpcd_write(DP_LINK_RATE_SET,
						  &rate_select, 1, controller);

	link_config[0] = 0;
	link_config[1] = DP_SET_ANSI_8B10B;
	drm_dp_dpcd_write(DP_DOWNSPREAD_CTRL, link_config, 2, controller);

	//intel_dp->DP |= DP_PORT_EN;

	/* clock recovery */
	if (!intel_dp_reset_link_train(intel_dp,
								   DP_TRAINING_PATTERN_1 |
									   DP_LINK_SCRAMBLING_DISABLE,
								   controller))
	{
		PRINT_DEBUG(EFI_D_ERROR, "failed to enable link training\n");
		return TRUE;
	}

	/*
	 * The DP 1.4 spec defines the max clock recovery retries value
	 * as 10 but for pre-DP 1.4 devices we set a very tolerant
	 * retry limit of 80 (4 voltage levels x 4 preemphasis levels x
	 * x 5 identical voltage retries). Since the previous specs didn't
	 * define a limit and created the possibility of an infinite loop
	 * we want to prevent any sync from triggering that corner case.
	 */
	//	if (intel_dp->dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14)
	//		max_cr_tries = 10;
	//	else
	max_cr_tries = 80;

	voltage_tries = 1;
	for (cr_tries = 0; cr_tries < max_cr_tries; ++cr_tries)
	{
		UINT8 link_status[DP_LINK_STATUS_SIZE];
		gBS->Stall(600);
		//drm_dp_link_train_clock_recovery_delay(intel_dp->dpcd);

		if (!intel_dp_get_link_status(link_status, controller))
		{
			PRINT_DEBUG(EFI_D_ERROR, "failed to get link status\n");
			return FALSE;
		}

		if (drm_dp_clock_recovery_ok(link_status, intel_dp->lane_count))
		{
			PRINT_DEBUG(EFI_D_ERROR, "clock recovery OK\n");
			return TRUE;
		}

		if (voltage_tries == 5)
		{
			PRINT_DEBUG(EFI_D_ERROR,
						"Same voltage tried 5 times\n");
			return FALSE;
		}

		if (max_vswing_reached)
		{
			PRINT_DEBUG(EFI_D_ERROR, "Max Voltage Swing reached\n");
			return FALSE;
		}

		voltage = intel_dp->train_set[0] & DP_TRAIN_VOLTAGE_SWING_MASK;
		PRINT_DEBUG(EFI_D_ERROR,
					"Voltage used: %08x\n", voltage);
		/* Update training set as requested by target */
		intel_dp_get_adjust_train(intel_dp, link_status);
		if (!intel_dp_update_link_train(intel_dp))
		{
			PRINT_DEBUG(EFI_D_ERROR,
						"failed to update link training\n");
			return FALSE;
		}

		if ((intel_dp->train_set[0] & DP_TRAIN_VOLTAGE_SWING_MASK) ==
			voltage)
			++voltage_tries;
		else
			voltage_tries = 1;

		if (intel_dp_link_max_vswing_reached(intel_dp))
			max_vswing_reached = TRUE;
	}
	PRINT_DEBUG(EFI_D_ERROR,
				"Failed clock recovery %d times, giving up!\n", max_cr_tries);
	return FALSE;
}
/*
 * Pick training pattern for channel equalization. Training pattern 4 for HBR3
 * or for 1.4 devices that support it, training Pattern 3 for HBR2
 * or 1.2 devices that support it, Training Pattern 2 otherwise.
 */
static UINT32 intel_dp_training_pattern(struct intel_dp *intel_dp)
{
	//BOOELAN source_tps3, sink_tps3, source_tps4, sink_tps4;

	/*
	 * Intel platforms that support HBR3 also support TPS4. It is mandatory
	 * for all downstream devices that support HBR3. There are no known eDP
	 * panels that support TPS4 as of Feb 2018 as per VESA eDP_v1.4b_E1
	 * specification.
	 */
	/* 	source_tps4 = intel_dp_source_supports_hbr3(intel_dp);
	sink_tps4 = drm_dp_tps4_supported(intel_dp->dpcd);
	if (source_tps4 && sink_tps4) {
		return DP_TRAINING_PATTERN_4;
	} else if (intel_dp->link_rate == 810000) {
 		if (!source_tps4)
			drm_dbg_kms(&dp_to_i915(intel_dp)->drm,
				    "8.1 Gbps link rate without source HBR3/TPS4 support\n");
		if (!sink_tps4)
			drm_dbg_kms(&dp_to_i915(intel_dp)->drm,
				    "8.1 Gbps link rate without sink TPS4 support\n"); */
	//} */
	/*
	 * Intel platforms that support HBR2 also support TPS3. TPS3 support is
	 * also mandatory for downstream devices that support HBR2. However, not
	 * all sinks follow the spec.
	 */
	/* 	source_tps3 = intel_dp_source_supports_hbr2(intel_dp);
	sink_tps3 = drm_dp_tps3_supported(intel_dp->dpcd);
	if (source_tps3 && sink_tps3) {
		return  DP_TRAINING_PATTERN_3;
	} else if (intel_dp->link_rate >= 540000) {
		 if (!source_tps3)
			drm_dbg_kms(&dp_to_i915(intel_dp)->drm,
				    ">=5.4/6.48 Gbps link rate without source HBR2/TPS3 support\n");
		if (!sink_tps3)
			drm_dbg_kms(&dp_to_i915(intel_dp)->drm,
				    ">=5.4/6.48 Gbps link rate without sink TPS3 support\n"); */
	//} */

	return DP_TRAINING_PATTERN_2;
}
BOOLEAN drm_dp_channel_eq_ok(const UINT8 link_status[DP_LINK_STATUS_SIZE],
							 int lane_count)
{
	UINT8 lane_align;
	UINT8 lane_status;
	int lane;

	lane_align = dp_link_status(link_status,
								DP_LANE_ALIGN_STATUS_UPDATED);
	if ((lane_align & DP_INTERLANE_ALIGN_DONE) == 0)
	{
		PRINT_DEBUG(EFI_D_ERROR, "NO Lane Align\n");
		return FALSE;
	}
	for (lane = 0; lane < lane_count; lane++)
	{
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS)
		{
			PRINT_DEBUG(EFI_D_ERROR, "NO EQ BITS\n");
			return FALSE;
		}
	}
	return TRUE;
}
static BOOLEAN
intel_dp_link_training_channel_equalization(struct intel_dp *intel_dp)
{
	//struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int tries;
	UINT32 training_pattern;
	UINT8 link_status[DP_LINK_STATUS_SIZE];
	BOOLEAN channel_eq = FALSE;

	training_pattern = intel_dp_training_pattern(intel_dp);
	/* Scrambling is disabled for TPS2/3 and enabled for TPS4 */
	if (training_pattern != DP_TRAINING_PATTERN_4)
		training_pattern |= DP_LINK_SCRAMBLING_DISABLE;

	/* channel equalization */
	if (!intel_dp_set_link_train(intel_dp,
								 training_pattern))
	{
		PRINT_DEBUG(EFI_D_ERROR, "failed to start channel equalization\n");
		return FALSE;
	}

	for (tries = 0; tries < 5; tries++)
	{
		gBS->Stall(600);
		//drm_dp_link_train_channel_eq_delay(intel_dp->dpcd);
		if (!intel_dp_get_link_status(link_status, intel_dp->controller))
		{
			PRINT_DEBUG(EFI_D_ERROR,
						"failed to get link status\n");
			break;
		}
		PRINT_DEBUG(EFI_D_ERROR, "Read Link Status 0: %x\n", link_status[0]);
		PRINT_DEBUG(EFI_D_ERROR, "Read Link Status 1: %x\n", link_status[1]);
		PRINT_DEBUG(EFI_D_ERROR, "Read Link Status 2: %x\n", link_status[2]);
		PRINT_DEBUG(EFI_D_ERROR, "Read Link Status 3: %x\n", link_status[3]);
		PRINT_DEBUG(EFI_D_ERROR, "Read Link Status 4: %x\n", link_status[4]);
		PRINT_DEBUG(EFI_D_ERROR, "Read Link Status 5: %x\n", link_status[5]);

		/* Make sure clock is still ok */
		if (!drm_dp_clock_recovery_ok(link_status,
									  intel_dp->lane_count))
		{
			//intel_dp_dump_link_status(link_status);
			PRINT_DEBUG(EFI_D_ERROR,
						"Clock recovery check failed, cannot continue channel equalization\n");
			break;
		}

		if (drm_dp_channel_eq_ok(link_status,
								 intel_dp->lane_count))
		{
			channel_eq = TRUE;
			PRINT_DEBUG(EFI_D_ERROR, "Channel EQ done. DP Training "
									 "successful\n");
			break;
		}

		/* Update training set as requested by target */
		intel_dp_get_adjust_train(intel_dp, link_status);
		if (!intel_dp_update_link_train(intel_dp))
		{
			PRINT_DEBUG(EFI_D_ERROR,
						"failed to update link training\n");
			break;
		}
	}

	/* Try 5 times, else fail and try at lower BW */
	if (tries == 5)
	{
		//intel_dp_dump_link_status(link_status);
		PRINT_DEBUG(EFI_D_ERROR,
					"Channel equalization failed 5 times\n");
	}

	UINT32 DP = intel_dp->controller->read32(DP_TP_CTL(intel_dp->controller->OutputPath.Port));

	DP &= ~DP_LINK_TRAIN_MASK_CPT;

	DP |= DP_TP_CTL_LINK_TRAIN_IDLE;

	intel_dp->controller->write32(DP_TP_CTL(intel_dp->controller->OutputPath.Port), DP);
	return channel_eq;
}
static int intersect_rates(const int *source_rates, int source_len,
						   const int *sink_rates, int sink_len,
						   int *common_rates)
{
	int i = 0, j = 0, k = 0;

	while (i < source_len && j < sink_len)
	{
		if (source_rates[i] == sink_rates[j])
		{
			if (k >= DP_MAX_SUPPORTED_RATES)
				return k;
			common_rates[k] = source_rates[i];
			++k;
			++i;
			++j;
		}
		else if (source_rates[i] < sink_rates[j])
		{
			++i;
		}
		else
		{
			++j;
		}
	}
	return k;
}
int intel_dp_max_data_rate(int max_link_clock, int max_lanes)
{
	/* max_link_clock is the link symbol clock (LS_Clk) in kHz and not the
	 * link rate that is generally expressed in Gbps. Since, 8 bits of data
	 * is transmitted every LS_Clk per lane, there is no need to account for
	 * the channel encoding that is done in the PHY layer here.
	 */

	return max_link_clock * max_lanes;
}
INT32 intel_dp_link_required(int pixel_clock, int bpp)
{
	/* pixel_clock is in kHz, divide bpp by 8 for bit to Byte conversion */
	return DIV_ROUND_UP(pixel_clock * bpp, 8);
}
static BOOLEAN intel_dp_can_link_train_fallback_for_edp(struct intel_dp *intel_dp,
														int link_rate,
														UINT8 lane_count)
{
	/* const struct drm_display_mode *fixed_mode =
		intel_dp->attached_connector->panel.fixed_mode; */
	int mode_rate, max_rate;

	mode_rate = intel_dp_link_required(intel_dp->controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock * 10, 24);
	max_rate = intel_dp_max_data_rate(link_rate, lane_count);
	PRINT_DEBUG(EFI_D_ERROR, "Mode: %u, Max:%u\n", mode_rate, max_rate);
	if (mode_rate > max_rate)
		return FALSE;

	return TRUE;
}
static void intel_dp_set_common_rates(struct intel_dp *intel_dp)
{
	//WARN_ON(!intel_dp->num_source_rates || !intel_dp->num_sink_rates);

	intel_dp->num_common_rates = intersect_rates(intel_dp->source_rates,
												 intel_dp->num_source_rates,
												 intel_dp->sink_rates,
												 intel_dp->num_sink_rates,
												 intel_dp->common_rates);

	/* Paranoia, there should always be something in common. */
	if (intel_dp->num_common_rates == 0)
	{
		intel_dp->common_rates[0] = 162000;
		intel_dp->num_common_rates = 1;
	}
	intel_dp->max_link_rate = intel_dp->common_rates[intel_dp->num_common_rates - 1];
}
static int intel_dp_rate_index(const int *rates, int len, int rate)
{
	int i;

	for (i = 0; i < len; i++)
		if (rate == rates[i])
			return i;

	return -1;
}
static int intel_dp_max_common_rate(struct intel_dp *intel_dp)
{
	return intel_dp->common_rates[intel_dp->num_common_rates - 1];
}

int intel_dp_get_link_train_fallback_values(struct intel_dp *intel_dp,
											int link_rate, UINT8 lane_count)
{
	//	struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	int index;
	if (intel_dp->controller->OutputPath.ConType == eDP && !intel_dp->use_max_rate)
	{
		intel_dp->use_max_rate = true;
		return 0;
	}
	index = intel_dp_rate_index(intel_dp->common_rates,
								intel_dp->num_common_rates,
								link_rate);
	if (index > 0)
	{
		if (intel_dp->controller->OutputPath.ConType == eDP &&
			!intel_dp_can_link_train_fallback_for_edp(intel_dp,
													  intel_dp->common_rates[index - 1],
													  lane_count))
		{
			PRINT_DEBUG(EFI_D_ERROR,
						"Retrying Link training for eDP with same parameters\n");
			return 0;
		}
		intel_dp->max_link_rate = intel_dp->common_rates[index - 1];
		intel_dp->max_link_lane_count = lane_count;
	}
	else if (lane_count > 1)
	{
		if (intel_dp->controller->OutputPath.ConType == eDP &&
			!intel_dp_can_link_train_fallback_for_edp(intel_dp,
													  intel_dp_max_common_rate(intel_dp),
													  lane_count >> 1))
		{
			PRINT_DEBUG(EFI_D_ERROR,
						"Retrying Link training for eDP with same parameters\n");
			return 0;
		}
		intel_dp->max_link_rate = intel_dp_max_common_rate(intel_dp);
		intel_dp->max_link_lane_count = lane_count >> 1;
	}
	else
	{
		PRINT_DEBUG(EFI_D_ERROR, "Link Training Unsuccessful\n");
		return -1;
	}
	intel_dp->lane_count = intel_dp->max_link_lane_count;
	intel_dp->link_rate = intel_dp->max_link_rate;

	return 0;
}
static void
intel_dp_set_source_rates(struct intel_dp *intel_dp)
{
	/* The values must be in increasing order */
	/* 	static const int cnl_rates[] = {
		162000, 216000, 270000, 324000, 432000, 540000, 648000, 810000
	};
	static const int bxt_rates[] = {
		162000, 216000, 243000, 270000, 324000, 432000, 540000
	};

	static const int hsw_rates[] = {
		162000, 270000, 540000
	};
	static const int g4x_rates[] = {
		162000, 270000
	}; 
*/
	static const int skl_rates[] = {
		162000, 216000, 270000, 324000, 432000, 540000};
/*			static const int skl_rates[] = {
		162000, 216000, 270000}; */
	/* 	struct intel_digital_port *dig_port = dp_to_dig_port(intel_dp);
	struct intel_encoder *encoder = &dig_port->base;
	struct drm_i915_private *dev_priv = to_i915(dig_port->base.base.dev); */
	const int *source_rates;
	int size;
	//int max_rate = 0, vbt_max_rate;

	/* This should only be done once */
	/* drm_WARN_ON(&dev_priv->drm,
		    intel_dp->source_rates || intel_dp->num_source_rates);
 */
	/* 	if (INTEL_GEN(dev_priv) >= 10) {
		source_rates = cnl_rates;
		size = ARRAY_SIZE(cnl_rates);
		if (IS_GEN(dev_priv, 10))
			max_rate = cnl_max_source_rate(intel_dp);
		else
			max_rate = icl_max_source_rate(intel_dp);
	} else if (IS_GEN9_LP(dev_priv)) {
		source_rates = bxt_rates;
		size = ARRAY_SIZE(bxt_rates);
	} else if (IS_GEN9_BC(dev_priv)) { */
	source_rates = skl_rates;
	size = ARRAY_SIZE(skl_rates);
	/* 	} else if ((IS_HASWELL(dev_priv) && !IS_HSW_ULX(dev_priv)) ||
		   IS_BROADWELL(dev_priv)) {
		source_rates = hsw_rates;
		size = ARRAY_SIZE(hsw_rates);
	} else {
		source_rates = g4x_rates;
		size = ARRAY_SIZE(g4x_rates);
	} */

	/* vbt_max_rate = intel_bios_dp_max_link_rate(encoder);
	if (max_rate && vbt_max_rate)
		max_rate = min(max_rate, vbt_max_rate);
	else if (vbt_max_rate)
		max_rate = vbt_max_rate; */

	/* 	if (max_rate)
		size = intel_dp_rate_limit_len(source_rates, size, max_rate); */

	intel_dp->source_rates = source_rates;
	intel_dp->num_source_rates = size;
}

/* update sink rates from dpcd. We don't read the dpcd(Should we?) So manually adding the 324000 sink rate */
static void intel_dp_set_sink_rates(struct intel_dp *intel_dp)
{
	static const int dp_rates[] = {
		162000, 270000, 540000, 810000};
	int i, max_rate;

	/* if (drm_dp_has_quirk(&intel_dp->desc, 0,
			     DP_DPCD_QUIRK_CAN_DO_MAX_LINK_RATE_3_24_GBPS)) { */
	/* Needed, e.g., for Apple MBP 2017, 15 inch eDP Retina panel
	int quirk_rates[] = {162000, 270000, 324000};

	memcpy(intel_dp->sink_rates, quirk_rates, sizeof(quirk_rates));
	intel_dp->num_sink_rates = ARRAY_SIZE(quirk_rates);

	return;
	} */

	max_rate = dp_rates[3];

	for (i = 0; i < ARRAY_SIZE(dp_rates); i++)
	{
		if (dp_rates[i] > max_rate)
			break;
		intel_dp->sink_rates[i] = dp_rates[i];
	}

	intel_dp->num_sink_rates = i;
}
/* Get length of rates array potentially limited by max_rate. */
static int intel_dp_rate_limit_len(const int *rates, int len, int max_rate)
{
	int i;

	/* Limit results by potentially reduced max rate */
	for (i = 0; i < len; i++)
	{
		if (rates[len - i - 1] <= max_rate)
			return len - i;
	}

	return 0;
}

/* Get length of common rates array potentially limited by max_rate. */
static int intel_dp_common_len_rate_limit(const struct intel_dp *intel_dp,
										  int max_rate)
{
	return intel_dp_rate_limit_len(intel_dp->common_rates,
								   intel_dp->num_common_rates, max_rate);
}
/* Optimize link config in order: max bpp, min clock, min lanes */
static EFI_STATUS
intel_dp_compute_link_config_wide(struct intel_dp *intel_dp,
								  const struct link_config_limits *limits)
{
	int bpp, clock, lane_count;
	int mode_rate, link_clock, link_avail;

	for (bpp = limits->max_bpp; bpp >= limits->min_bpp; bpp -= 2 * 3)
	{
		//int output_bpp = intel_dp_output_bpp(pipe_config->output_format, bpp);
		int output_bpp = bpp;

		mode_rate = intel_dp_link_required(intel_dp->controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock * 10,
										   output_bpp);

		for (clock = limits->min_clock; clock <= limits->max_clock; clock++)
		{
			for (lane_count = limits->min_lane_count;
				 lane_count <= limits->max_lane_count;
				 lane_count <<= 1)
			{
				link_clock = intel_dp->common_rates[clock];
				link_avail = intel_dp_max_data_rate(link_clock,
													lane_count);

				if (mode_rate <= link_avail)
				{
					intel_dp->lane_count = lane_count;
					intel_dp->pipe_bpp = bpp;
					intel_dp->link_rate = link_clock;

					return 0;
				}
			}
		}
	}

	return EFI_UNSUPPORTED;
}

static EFI_STATUS i915_dp_get_link_config(struct intel_dp *intel_dp)
{
	struct link_config_limits limits;
	int common_len;
	int ret;

	common_len = intel_dp_common_len_rate_limit(intel_dp,
												intel_dp->max_link_rate);

	limits.min_clock = 0;
	limits.max_clock = common_len - 1;

	limits.min_lane_count = 1;
	limits.max_lane_count = intel_dp->max_link_lane_count;

	limits.min_bpp = 18;
	//limits.max_bpp = intel_dp_max_bpp(intel_dp, pipe_config);
	limits.max_bpp = 24; //Setting a reasonable(hopefully) default
	PRINT_DEBUG(EFI_D_ERROR, "max_clock index: %d, common_len: %d, clock rate[0]: %d, max_link_rate: %d, clockrate[3]: %d \n", limits.max_clock, common_len, intel_dp->common_rates[0],intel_dp->max_link_rate,0);
	if (intel_dp->use_max_rate)
	{
		/*
		 * Use the maximum clock and number of lanes the eDP panel
		 * advertizes being capable of in case the initial fast
		 * optimal params failed us. The panels are generally
		 * designed to support only a single clock and lane
		 * configuration, and typically on older panels these
		 * values correspond to the native resolution of the panel.
		 */
		limits.min_lane_count = limits.max_lane_count;
		limits.min_clock = limits.max_clock;
	}

	//intel_dp_adjust_compliance_config(intel_dp, pipe_config, &limits); //Ignoring. Hopefully still works.

	PRINT_DEBUG(EFI_D_ERROR, "DP link computation with max lane count %d max rate %d max bpp %d pixel clock %dKHz\n",
				limits.max_lane_count,
				intel_dp->common_rates[limits.max_clock],
				limits.max_bpp, intel_dp->controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock * 10);

	/*
	 * Optimize for slow and wide for everything, because there are some
	 * eDP 1.3 and 1.4 panels don't work well with fast and narrow.
	 */
	ret = intel_dp_compute_link_config_wide(intel_dp, &limits);
	return ret;
}

EFI_STATUS _TrainDisplayPort(struct intel_dp *intel_dp)
{
	UINT32 port = intel_dp->controller->OutputPath.Port;
	UINT32 val = intel_dp->controller->read32(DP_TP_CTL(port));
	val &= ~(DP_TP_CTL_ENABLE);
	// val |= DP_TP_CTL_MODE_SST;
	// val |= DP_TP_CTL_LINK_TRAIN_PAT1;
	//val |= DP_TP_CTL_ENHANCED_FRAME_ENABLE;
	intel_dp->controller->write32(DP_TP_CTL(port), val);
	val = intel_dp->controller->read32(DDI_BUF_CTL(port));
	val &= ~(DDI_PORT_WIDTH_MASK | DDI_BUF_CTL_ENABLE);
	//val |= DDI_BUF_TRANS_SELECT(0);
	//val |= DDI_A_4_LANES;
	val |= DDI_PORT_WIDTH(intel_dp->lane_count);
	intel_dp->controller->write32(DDI_BUF_CTL(port), val);

	gBS->Stall(600);
	val = intel_dp->controller->read32(DP_TP_CTL(port));
	val |= DP_TP_CTL_ENABLE;
	intel_dp->controller->write32(DP_TP_CTL(port), val);

	val = intel_dp->controller->read32(DDI_BUF_CTL(port));
	val |= DDI_BUF_CTL_ENABLE;

	intel_dp->controller->write32(DDI_BUF_CTL(port), val);
	if (!intel_dp_link_training_clock_recovery(intel_dp))
		goto failure_handling;
	if (!intel_dp_link_training_channel_equalization(intel_dp))
		goto failure_handling;
	intel_dp_set_link_train(intel_dp,
							DP_TRAINING_PATTERN_DISABLE);
	UINT32 DP = intel_dp->controller->read32(DP_TP_CTL(port));

	DP &= ~DP_LINK_TRAIN_MASK_CPT;

	DP |= DP_TP_CTL_LINK_TRAIN_NORMAL;

	intel_dp->controller->write32(DP_TP_CTL(port), DP);
	PRINT_DEBUG(EFI_D_ERROR, "Link Rate: %d, lane count: %d\n",
				intel_dp->controller->OutputPath.LinkRate, intel_dp->lane_count);
	intel_dp->controller->OutputPath.LinkRate = intel_dp->link_rate;
	intel_dp->controller->OutputPath.LaneCount = intel_dp->lane_count;

	return EFI_SUCCESS;
failure_handling:
	PRINT_DEBUG(EFI_D_ERROR,
				" Link Training failed at link rate = %d, lane count = %d\n",
				intel_dp->link_rate, intel_dp->lane_count);
	if (!intel_dp_get_link_train_fallback_values(intel_dp,
												 intel_dp->link_rate,
												 intel_dp->lane_count))
	{
		intel_dp->controller->OutputPath.LinkRate = intel_dp->link_rate;
		intel_dp->controller->OutputPath.LaneCount = intel_dp->lane_count;
		SetupClockeDP(intel_dp->controller);

		/* Schedule a Hotplug Uevent to userspace to start modeset */
		return _TrainDisplayPort(intel_dp);
	}
	else if (intel_dp->use_max_rate)
	{
		i915_dp_get_link_config(intel_dp);
		return _TrainDisplayPort(intel_dp);
	}
	return EFI_ABORTED;
}

EFI_STATUS TrainDisplayPort(i915_CONTROLLER *controller)
{
	UINT32 port = controller->OutputPath.Port;
	UINT32 val = 0;
	EFI_STATUS status = EFI_SUCCESS;
	val |= DP_TP_CTL_ENABLE;
	val |= DP_TP_CTL_MODE_SST;
	val |= DP_TP_CTL_LINK_TRAIN_PAT1;
	val |= DP_TP_CTL_ENHANCED_FRAME_ENABLE;
	controller->write32(DP_TP_CTL(port), val);
	val = DDI_BUF_CTL_ENABLE;

	val |= DDI_BUF_TRANS_SELECT(0);
	val |= DDI_A_4_LANES;
	val |= DDI_PORT_WIDTH(4);
	controller->write32(DDI_BUF_CTL(port), val);
	gBS->Stall(500);

	struct intel_dp intel_dp;
	intel_dp.controller = controller;
	intel_dp.max_link_lane_count = 4;
	// intel_dp.lane_count = 2;
	// if ((controller->read32(0x64000) & DP_PLL_FREQ_MASK) == DP_PLL_FREQ_162MHZ)
	// 	intel_dp.link_rate = 162000;
	// else
	// 	intel_dp.link_rate = 270000;

	intel_dp_set_source_rates(&intel_dp);

	intel_dp_set_sink_rates(&intel_dp);

	intel_dp_set_common_rates(&intel_dp);
	i915_dp_get_link_config(&intel_dp);
	status = _TrainDisplayPort(&intel_dp);
	UINT8 count = 0;
	while (!intel_dp_can_link_train_fallback_for_edp(&intel_dp, intel_dp.link_rate, intel_dp.lane_count) && count < 4)
	{
		PRINT_DEBUG(EFI_D_ERROR, "Higher rate than configured, Trying Lower Pixel Clock\n");
		controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock >>= 1;
		count++;
	}
	if ((count == 4) && (!intel_dp_can_link_train_fallback_for_edp(&intel_dp, intel_dp.link_rate, intel_dp.lane_count)))
	{
		PRINT_DEBUG(EFI_D_ERROR, "Error: Higher rate than configured\n");

		status = EFI_UNSUPPORTED;
	}
	return status;
}
/* Transfer unit size for display port - 1, default is 0x3f (for TU size 64) */
#define TU_SIZE(x) (((x)-1) << 25) /* default size 64 */
#define TU_SIZE_SHIFT 25
#define TU_SIZE_MASK (0x3f << 25)

#define DATA_LINK_M_N_MASK (0xffffff)
#define DATA_LINK_N_MAX (0x800000)
UINT32 roundup_pow_of_two(UINT32 v)
{

	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	v++;
	return v;
}
static void
intel_reduce_m_n_ratio(UINT32 *num, UINT32 *den)
{
	while (*num > DATA_LINK_M_N_MASK ||
		   *den > DATA_LINK_M_N_MASK)
	{
		*num >>= 1;
		*den >>= 1;
	}
}
static inline UINT64 div_UINT64_rem(UINT64 dividend, UINT32 divisor, UINT32 *remainder)
{
	union
	{
		UINT64 v64;
		UINT32 v32[2];
	} d = {dividend};
	UINT32 upper;

	upper = d.v32[1];
	d.v32[1] = 0;
	if (upper >= divisor)
	{
		d.v32[1] = upper / divisor;
		upper %= divisor;
	}
	asm("divl %2"
		: "=a"(d.v32[0]), "=d"(*remainder)
		: "rm"(divisor), "0"(d.v32[0]), "1"(upper));
	return d.v64;
}
static inline UINT64 div_UINT64(UINT64 dividend, UINT32 divisor)
{
	UINT32 remainder;
	return div_UINT64_rem(dividend, divisor, &remainder);
}
static inline UINT64 mul_UINT32_UINT32(UINT32 a, UINT32 b)
{
	UINT32 high, low;

	asm("mull %[b]"
		: "=a"(low), "=d"(high)
		: [a] "a"(a), [b] "rm"(b));

	return low | ((UINT64)high) << 32;
}
static void compute_m_n(unsigned int m, unsigned int n,
						UINT32 *ret_m, UINT32 *ret_n,
						BOOLEAN constant_n)
{
	/*         controller->write32(0x6f030, 0x7e6cf53b);
        controller->write32(0x6f034, 0x00800000);
        controller->write32(0x6f040, 0x00048a37);
        controller->write32(0x6f044, 0x00080000); */
	/*
	 * Several DP dongles in particular seem to be fussy about
	 * too large link M/N values. Give N value as 0x8000 that
	 * should be acceptable by specific devices. 0x8000 is the
	 * specified fixed N value for asynchronous clock mode,
	 * which the devices expect also in synchronous clock mode.
	 */
	PRINT_DEBUG(EFI_D_ERROR, "progressed to dpline %d\n",
				__LINE__);
	if (constant_n)
		*ret_n = 0x8000;
	else
		*ret_n = min_t(unsigned int, roundup_pow_of_two(n), DATA_LINK_N_MAX);

	PRINT_DEBUG(EFI_D_ERROR, "progressed to dpline %d\n",
				__LINE__);
	PRINT_DEBUG(EFI_D_ERROR, "m: %u, n: %u, ret_n: %u\n", m, n, *ret_n);
	*ret_m = div_UINT64(mul_UINT32_UINT32(m, *ret_n), n);
	intel_reduce_m_n_ratio(ret_m, ret_n);
}
struct intel_link_m_n
{
	UINT32 tu;
	UINT32 gmch_m;
	UINT32 gmch_n;
	UINT32 link_m;
	UINT32 link_n;
};
void intel_link_compute_m_n(UINT16 bits_per_pixel, int nlanes,
							int pixel_clock, int link_clock,
							struct intel_link_m_n *m_n,
							BOOLEAN constant_n, BOOLEAN fec_enable)
{
	UINT32 data_clock = bits_per_pixel * pixel_clock;
	PRINT_DEBUG(EFI_D_ERROR, "intel_link_compute_m_n: bpp: %u, lanes: %u, pclock:%u, link_clock: %u\n",
				bits_per_pixel, nlanes, pixel_clock, link_clock);
	/* if (fec_enable)
		data_clock = intel_dp_mode_to_fec_clock(data_clock);
 */
	PRINT_DEBUG(EFI_D_ERROR, "progressed to dpline %d\n",
				__LINE__);
	m_n->tu = 64;
	PRINT_DEBUG(EFI_D_ERROR, "progressed to dpline %d\n",
				__LINE__);
	compute_m_n(data_clock,
				link_clock * nlanes * 8,
				&(m_n->gmch_m), &(m_n->gmch_n),
				constant_n);
	PRINT_DEBUG(EFI_D_ERROR, "progressed to dpline %d\n",
				__LINE__);
	compute_m_n(pixel_clock, link_clock,
				&m_n->link_m, &m_n->link_n,
				constant_n);
	PRINT_DEBUG(EFI_D_ERROR, "progressed to dpline %d\n",
				__LINE__);
}

EFI_STATUS SetupTranscoderAndPipeDP(i915_CONTROLLER *controller)
{
	UINT32 horz_active = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActive |
						 ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb >> 4) << 8);
	UINT32 horz_blank = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzBlank |
						((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb & 0xF) << 8);
	UINT32 horz_sync_offset = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzSyncOffset | ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 6) << 8);
	UINT32 horz_sync_pulse = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzSyncPulse |
							 (((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 4) & 0x3) << 8);

	UINT32 horizontal_active = horz_active;
	UINT32 horizontal_syncStart = horz_active + horz_sync_offset;
	UINT32 horizontal_syncEnd = horz_active + horz_sync_offset + horz_sync_pulse;
	UINT32 horizontal_total = horz_active + horz_blank;

	UINT32 vert_active = controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActive |
						 ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb >> 4) << 8);
	UINT32 vert_blank = controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertBlank |
						((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb & 0xF) << 8);
	UINT32 vert_sync_offset = (controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertSync >> 4) | (((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 2) & 0x3)
																									  << 4);
	UINT32 vert_sync_pulse = (controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertSync & 0xF) | ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb & 0x3) << 4);

	UINT32 vertical_active = vert_active;
	UINT32 vertical_syncStart = vert_active + vert_sync_offset;
	UINT32 vertical_syncEnd = vert_active + vert_sync_offset + vert_sync_pulse;
	UINT32 vertical_total = vert_active + vert_blank;

	controller->write32(VSYNCSHIFT_A, 0);

	controller->write32(HTOTAL_A,
						(horizontal_active - 1) |
							((horizontal_total - 1) << 16));
	controller->write32(HBLANK_A,
						(horizontal_active - 1) |
							((horizontal_total - 1) << 16));
	controller->write32(HSYNC_A,
						(horizontal_syncStart - 1) |
							((horizontal_syncEnd - 1) << 16));

	controller->write32(VTOTAL_A,
						(vertical_active - 1) |
							((vertical_total - 1) << 16));
	controller->write32(VBLANK_A,
						(vertical_active - 1) |
							((vertical_total - 1) << 16));
	controller->write32(VSYNC_A,
						(vertical_syncStart - 1) |
							((vertical_syncEnd - 1) << 16));

	controller->write32(PIPEASRC, ((horizontal_active - 1) << 16) | (vertical_active - 1));
	struct intel_link_m_n m_n = {0};

	intel_link_compute_m_n(24, controller->OutputPath.LaneCount, controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock * 10, controller->OutputPath.LinkRate, &m_n, FALSE, FALSE);
	controller->write32(PIPEA_DATA_M1,
						TU_SIZE(m_n.tu) | m_n.gmch_m);
	controller->write32(PIPEA_DATA_N1,
						m_n.gmch_n);
	controller->write32(PIPEA_LINK_M1,
						m_n.link_m);
	controller->write32(PIPEA_LINK_N1,
						m_n.link_n);
	PRINT_DEBUG(EFI_D_ERROR, "HTOTAL_A (%x) = %08x\n", HTOTAL_A, controller->read32(HTOTAL_A));
	PRINT_DEBUG(EFI_D_ERROR, "HBLANK_A (%x) = %08x\n", HBLANK_A, controller->read32(HBLANK_A));
	PRINT_DEBUG(EFI_D_ERROR, "HSYNC_A (%x) = %08x\n", HSYNC_A, controller->read32(HSYNC_A));
	PRINT_DEBUG(EFI_D_ERROR, "VTOTAL_A (%x) = %08x\n", VTOTAL_A, controller->read32(VTOTAL_A));
	PRINT_DEBUG(EFI_D_ERROR, "VBLANK_A (%x) = %08x\n", VBLANK_A, controller->read32(VBLANK_A));
	PRINT_DEBUG(EFI_D_ERROR, "VSYNC_A (%x) = %08x\n", VSYNC_A, controller->read32(VSYNC_A));
	PRINT_DEBUG(EFI_D_ERROR, "PIPEASRC (%x) = %08x\n", PIPEASRC, controller->read32(PIPEASRC));
	PRINT_DEBUG(EFI_D_ERROR, "BCLRPAT_A (%x) = %08x\n", BCLRPAT_A, controller->read32(BCLRPAT_A));
	PRINT_DEBUG(EFI_D_ERROR, "VSYNCSHIFT_A (%x) = %08x\n", VSYNCSHIFT_A, controller->read32(VSYNCSHIFT_A));

	PRINT_DEBUG(EFI_D_ERROR, "before pipe gamma\n");
	return EFI_SUCCESS;
}
EFI_STATUS SetupTranscoderAndPipeEDP(i915_CONTROLLER *controller)
{
	UINT32 horz_active = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActive |
						 ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb >> 4) << 8);
	UINT32 horz_blank = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzBlank |
						((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb & 0xF) << 8);
	UINT32 horz_sync_offset = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzSyncOffset | ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 6) << 8);
	UINT32 horz_sync_pulse = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzSyncPulse |
							 (((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 4) & 0x3) << 8);

	UINT32 horizontal_active = horz_active;
	UINT32 horizontal_syncStart = horz_active + horz_sync_offset;
	UINT32 horizontal_syncEnd = horz_active + horz_sync_offset + horz_sync_pulse;
	UINT32 horizontal_total = horz_active + horz_blank;

	UINT32 vert_active = controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActive |
						 ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb >> 4) << 8);
	UINT32 vert_blank = controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertBlank |
						((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb & 0xF) << 8);
	UINT32 vert_sync_offset = (controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertSync >> 4) | (((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 2) & 0x3)
																									  << 4);
	UINT32 vert_sync_pulse = (controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertSync & 0xF) | ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb & 0x3) << 4);

	UINT32 vertical_active = vert_active;
	UINT32 vertical_syncStart = vert_active + vert_sync_offset;
	UINT32 vertical_syncEnd = vert_active + vert_sync_offset + vert_sync_pulse;
	UINT32 vertical_total = vert_active + vert_blank;

	controller->write32(VSYNCSHIFT_EDP, 0);

	controller->write32(HTOTAL_EDP,
						(horizontal_active - 1) |
							((horizontal_total - 1) << 16));
	controller->write32(HBLANK_EDP,
						(horizontal_active - 1) |
							((horizontal_total - 1) << 16));
	controller->write32(HSYNC_EDP,
						(horizontal_syncStart - 1) |
							((horizontal_syncEnd - 1) << 16));

	controller->write32(VTOTAL_EDP,
						(vertical_active - 1) |
							((vertical_total - 1) << 16));
	controller->write32(VBLANK_EDP,
						(vertical_active - 1) |
							((vertical_total - 1) << 16));
	controller->write32(VSYNC_EDP,
						(vertical_syncStart - 1) |
							((vertical_syncEnd - 1) << 16));

	controller->write32(PIPEASRC, ((horizontal_active - 1) << 16) | (vertical_active - 1));
	/*         controller->write32(0x6f030, 0x7e6cf53b);
        controller->write32(0x6f034, 0x00800000);
        controller->write32(0x6f040, 0x00048a37);
        controller->write32(0x6f044, 0x00080000); */
	struct intel_link_m_n m_n = {0};
	//struct intel_link_m_n *m_n= &m_n
	intel_link_compute_m_n(24, controller->OutputPath.LaneCount, controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock * 10, controller->OutputPath.LinkRate, &m_n, FALSE, FALSE);
	PRINT_DEBUG(EFI_D_ERROR, "progressed to dpline %d\n",
				__LINE__);
	PRINT_DEBUG(EFI_D_ERROR, "PIPEEDP_DATA_M1 (%x) = %08x\n", PIPEEDP_DATA_M1, TU_SIZE(m_n.tu) | m_n.gmch_m);
	PRINT_DEBUG(EFI_D_ERROR, "PIPEEDP_DATA_N1 (%x) = %08x\n", PIPEEDP_DATA_N1, m_n.gmch_n);
	PRINT_DEBUG(EFI_D_ERROR, "PIPEEDP_LINK_M1 (%x) = %08x\n", PIPEEDP_LINK_M1, m_n.link_m);
	PRINT_DEBUG(EFI_D_ERROR, "PIPEEDP_LINK_N1 (%x) = %08x\n", PIPEEDP_LINK_N1, m_n.link_n);

	controller->write32(PIPEEDP_DATA_M1,
						TU_SIZE(m_n.tu) | m_n.gmch_m);
	controller->write32(PIPEEDP_DATA_N1,
						m_n.gmch_n);
	controller->write32(PIPEEDP_LINK_M1,
						m_n.link_m);
	controller->write32(PIPEEDP_LINK_N1,
						m_n.link_n);
	PRINT_DEBUG(EFI_D_ERROR, "HTOTAL_EDP (%x) = %08x\n", HTOTAL_EDP, controller->read32(HTOTAL_EDP));
	PRINT_DEBUG(EFI_D_ERROR, "HBLANK_EDP (%x) = %08x\n", HBLANK_EDP, controller->read32(HBLANK_EDP));
	PRINT_DEBUG(EFI_D_ERROR, "HSYNC_EDP (%x) = %08x\n", HSYNC_EDP, controller->read32(HSYNC_EDP));
	PRINT_DEBUG(EFI_D_ERROR, "VTOTAL_EDP (%x) = %08x\n", VTOTAL_EDP, controller->read32(VTOTAL_EDP));
	PRINT_DEBUG(EFI_D_ERROR, "VBLANK_EDP (%x) = %08x\n", VBLANK_EDP, controller->read32(VBLANK_EDP));
	PRINT_DEBUG(EFI_D_ERROR, "VSYNC_EDP (%x) = %08x\n", VSYNC_EDP, controller->read32(VSYNC_EDP));
	PRINT_DEBUG(EFI_D_ERROR, "PIPEASRC (%x) = %08x\n", PIPEASRC, controller->read32(PIPEASRC));
	PRINT_DEBUG(EFI_D_ERROR, "BCLRPAT_EDP (%x) = %08x\n", BCLRPAT_EDP, controller->read32(BCLRPAT_EDP));
	PRINT_DEBUG(EFI_D_ERROR, "VSYNCSHIFT_EDP (%x) = %08x\n", VSYNCSHIFT_EDP, controller->read32(VSYNCSHIFT_EDP));

	PRINT_DEBUG(EFI_D_ERROR, "before pipe gamma\n");
	return EFI_SUCCESS;
}
