#include "i915_controller.h"
#include <Library/DebugLib.h>
#include "i915_gmbus.h"
#include "i915_ddi.h"
#include "i915_dp.h"
#include "i915_hdmi.h"
#include "i915_reg.h"
#include <Uefi.h>

EFI_STATUS SetupClockeDP(i915_CONTROLLER* controller) {
    
    UINT32 ctrl1, cfgcr1, cfgcr2;
    struct skl_wrpll_params wrpll_params = {
        0,
    };
    UINT8 id = controller->OutputPath.DPLL;
    /*
     * See comment in intel_dpll_hw_state to understand why we always use 0
     * as the DPLL id in this function. Basically, we put them in the first 6 bits then shift them into place for easier comparison
     */
    ctrl1 = DPLL_CTRL1_OVERRIDE(id); //Enable Programming
    ctrl1 |= DPLL_CTRL1_SSC(id);
    // ctrl1 |= DPLL_CTRL1_HDMI_MODE(0); //Set Mode to HDMI

    

    UINT32 val = controller->read32(DPLL_CTRL1);

    //it's clock id!
    //how's port clock comptued?
    UINT64 clock_khz=(UINT64)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock)*10;
    UINT32 linkrate=DPLL_CTRL1_LINK_RATE_810;
    if(clock_khz>>1 >=135000){
    	linkrate=DPLL_CTRL1_LINK_RATE_1350;
    }else if(clock_khz>>1 >=270000){
    	linkrate=DPLL_CTRL1_LINK_RATE_2700;
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
    DebugPrint(EFI_D_ERROR, "i915: DPLL_CTRL1 = %08x\n", controller->read32(DPLL_CTRL1));
    DebugPrint(EFI_D_ERROR, "i915: _DPLL1_CFGCR1 = %08x\n", controller->read32(_DPLL1_CFGCR1));
    DebugPrint(EFI_D_ERROR, "i915: _DPLL1_CFGCR2 = %08x\n", controller->read32(_DPLL1_CFGCR2));

    /* the enable bit is always bit 31 */
    controller->write32(LCPLL2_CTL, controller->read32(LCPLL2_CTL) | LCPLL_PLL_ENABLE);

    for (UINT32 counter = 0;; counter++)
    {
        if (controller->read32(DPLL_STATUS) & DPLL_LOCK(id))
        {
            DebugPrint(EFI_D_ERROR, "i915: DPLL %d locked\n", id);
            break;
        }
        if (counter > 16384)
        {
            DebugPrint(EFI_D_ERROR, "i915: DPLL %d not locked\n", id);
            break;
        }
    }

    //intel_encoders_pre_enable(crtc, pipe_config, old_state);
    //could be intel_ddi_pre_enable_hdmi
    //intel_ddi_clk_select(encoder, crtc_state);
    UINT32 port = controller->OutputPath.Port;
    DebugPrint(EFI_D_ERROR, "i915: port is %d\n", port);
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
    DebugPrint(EFI_D_ERROR, "i915: DPLL_CTRL2 = %08x\n", controller->read32(DPLL_CTRL2));
    return EFI_SUCCESS;
}
struct ddi_buf_trans {
	UINT32 trans1;	/* balance leg enable, de-emph level */
	UINT32 trans2;	/* vref sel, vswing */
	UINT8 i_boost;	/* SKL: I_boost; valid: 0x0, 0x1, 0x3, 0x7 */
};
/* Skylake H and S */
static const struct ddi_buf_trans skl_ddi_translations_dp[] = {
	{ 0x00002016, 0x000000A0, 0x0 },
	{ 0x00005012, 0x0000009B, 0x0 },
	{ 0x00007011, 0x00000088, 0x0 },
	{ 0x80009010, 0x000000C0, 0x1 },
	{ 0x00002016, 0x0000009B, 0x0 },
	{ 0x00005012, 0x00000088, 0x0 },
	{ 0x80007011, 0x000000C0, 0x1 },
	{ 0x00002016, 0x000000DF, 0x0 },
	{ 0x80005012, 0x000000C0, 0x1 },
};

/* Skylake U */
static const struct ddi_buf_trans skl_u_ddi_translations_dp[] = {
	{ 0x0000201B, 0x000000A2, 0x0 },
	{ 0x00005012, 0x00000088, 0x0 },
	{ 0x80007011, 0x000000CD, 0x1 },
	{ 0x80009010, 0x000000C0, 0x1 },
	{ 0x0000201B, 0x0000009D, 0x0 },
	{ 0x80005012, 0x000000C0, 0x1 },
	{ 0x80007011, 0x000000C0, 0x1 },
	{ 0x00002016, 0x00000088, 0x0 },
	{ 0x80005012, 0x000000C0, 0x1 },
};

/* Skylake Y */
static const struct ddi_buf_trans skl_y_ddi_translations_dp[] = {
	{ 0x00000018, 0x000000A2, 0x0 },
	{ 0x00005012, 0x00000088, 0x0 },
	{ 0x80007011, 0x000000CD, 0x3 },
	{ 0x80009010, 0x000000C0, 0x3 },
	{ 0x00000018, 0x0000009D, 0x0 },
	{ 0x80005012, 0x000000C0, 0x3 },
	{ 0x80007011, 0x000000C0, 0x3 },
	{ 0x00000018, 0x00000088, 0x0 },
	{ 0x80005012, 0x000000C0, 0x3 },
};
#define IS_SKL_ULX(cont) (0)
#define IS_SKL_ULT(cont) (1)
static const struct ddi_buf_trans *
skl_get_buf_trans_dp(i915_CONTROLLER* controller, int *n_entries)
{
	if (IS_SKL_ULX(controller)) {
		*n_entries = ARRAY_SIZE(skl_y_ddi_translations_dp);
		return skl_y_ddi_translations_dp;
	} else if (IS_SKL_ULT(controller)) {
		*n_entries = ARRAY_SIZE(skl_u_ddi_translations_dp);
		return skl_u_ddi_translations_dp;
	} else {
		*n_entries = ARRAY_SIZE(skl_ddi_translations_dp);
		return skl_ddi_translations_dp;
	}
}

EFI_STATUS SetupDDIBufferDP(i915_CONTROLLER* controller) {
    	const struct ddi_buf_trans *ddi_translations;

	int i, n_entries;

		ddi_translations = skl_get_buf_trans_dp(controller,
							      &n_entries);
/* If we're boosting the current, set bit 31 of trans1 */
//	if (IS_GEN9_BC(dev_priv) && intel_bios_dp_boost_level(encoder))
//		iboost_bit = DDI_BUF_BALANCE_LEG_ENABLE;
    UINT32 port = controller->OutputPath.Port;
	for (i = 0; i < n_entries; i++) {
		controller->write32(DDI_BUF_TRANS_LO(port, i),
			       ddi_translations[i].trans1);
		controller->write32( DDI_BUF_TRANS_HI(port, i),
			       ddi_translations[i].trans2);
	}
    return EFI_SUCCESS;
}