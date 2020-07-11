#include "i915_controller.h"
#include <Library/DebugLib.h>
#include "i915_gmbus.h"
#include "i915_ddi.h"
#include "i915_dp.h"
#include "i915_hdmi.h"
#include "i915_reg.h"
#include <Uefi.h>
static void skl_wrpll_get_multipliers(UINT64 p,
                                      UINT64 *p0 /* out */,
                                      UINT64 *p1 /* out */,
                                      UINT64 *p2 /* out */)
{
    /* even dividers */
    if (p % 2 == 0)
    {
        UINT64 half = p / 2;

        if (half == 1 || half == 2 || half == 3 || half == 5)
        {
            *p0 = 2;
            *p1 = 1;
            *p2 = half;
        }
        else if (half % 2 == 0)
        {
            *p0 = 2;
            *p1 = half / 2;
            *p2 = 2;
        }
        else if (half % 3 == 0)
        {
            *p0 = 3;
            *p1 = half / 3;
            *p2 = 2;
        }
        else if (half % 7 == 0)
        {
            *p0 = 7;
            *p1 = half / 7;
            *p2 = 2;
        }
    }
    else if (p == 3 || p == 9)
    { /* 3, 5, 7, 9, 15, 21, 35 */
        *p0 = 3;
        *p1 = 1;
        *p2 = p / 3;
    }
    else if (p == 5 || p == 7)
    {
        *p0 = p;
        *p1 = 1;
        *p2 = 1;
    }
    else if (p == 15)
    {
        *p0 = 3;
        *p1 = 1;
        *p2 = 5;
    }
    else if (p == 21)
    {
        *p0 = 7;
        *p1 = 1;
        *p2 = 3;
    }
    else if (p == 35)
    {
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

    switch (central_freq)
    {
    case 9600000000ULL:
        params->central_freq = 0;
        break;
    case 9000000000ULL:
        params->central_freq = 1;
        break;
    case 8400000000ULL:
        params->central_freq = 3;
    }

    switch (p0)
    {
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
        DebugPrint(EFI_D_ERROR, "Incorrect PDiv\n");
    }

    switch (p2)
    {
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
        DebugPrint(EFI_D_ERROR, "Incorrect KDiv\n");
    }

    params->qdiv_ratio = p1;
    params->qdiv_mode = (params->qdiv_ratio == 1) ? 0 : 1;

    dco_freq = p0 * p1 * p2 * afe_clock;

    /*
     * Intermediate values are in Hz.
     * Divide by MHz to match bsepc
     */
    params->dco_integer = (dco_freq) / (24 * MHz(1));
    params->dco_fraction = (((dco_freq) / (24) - params->dco_integer * MHz(1)) * 0x8000) / (MHz(1));
}

static void skl_wrpll_try_divider(struct skl_wrpll_context *ctx,
                                  UINT64 central_freq,
                                  UINT64 dco_freq,
                                  UINT64 divider)
{
    UINT64 deviation;
    INT64 abs_diff = (INT64)dco_freq - (INT64)central_freq;
    if (abs_diff < 0)
    {
        abs_diff = -abs_diff;
    }

    deviation = (10000 * (UINT64)abs_diff) / (central_freq);

    /* positive deviation */
    if (dco_freq >= central_freq)
    {
        if (deviation < SKL_DCO_MAX_PDEVIATION &&
            deviation < ctx->min_deviation)
        {
            ctx->min_deviation = deviation;
            ctx->central_freq = central_freq;
            ctx->dco_freq = dco_freq;
            ctx->p = divider;
        }
        /* negative deviation */
    }
    else if (deviation < SKL_DCO_MAX_NDEVIATION &&
             deviation < ctx->min_deviation)
    {
        ctx->min_deviation = deviation;
        ctx->central_freq = central_freq;
        ctx->dco_freq = dco_freq;
        ctx->p = divider;
    }
}

EFI_STATUS SetupClockHDMI(i915_CONTROLLER* controller) {
     /*     //setup DPLL (old GPU, doesn't apply here)
    //UINT32 refclock = 96000;
    //UINT32 pixel_clock = (UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock) * 10;
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

    //controller->write32(_FPA0, params.n << 16 | params.m1 << 8 | params.m2);
    //controller->write32(_FPA1, params.n << 16 | params.m1 << 8 | params.m2);

    //controller->write32(_DPLL_A, 0);

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

    //controller->write32(_DPLL_A, dplla);
    //read32(_DPLL_A);
    //DebugPrint(EFI_D_ERROR,"i915: DPLL set %08x, read %08x\n",dplla,read32(_DPLL_A));

    ////it's pointless to wait in GVT-g
    //if(!controller->is_gvt){
    //	//MicroSecondDelay is unusable
    //	for(UINT32 counter=0;counter<16384;counter++){
    //		read32(_DPLL_A);
    //	}
    //}

    //controller->write32(_DPLL_A_MD, (multiplier-1)<<DPLL_MD_UDI_MULTIPLIER_SHIFT);
    //DebugPrint(EFI_D_ERROR,"i915: DPLL_MD set\n");

    //for(int i = 0; i < 3; i++) {
    //	controller->write32(_DPLL_A, dplla);
    //	controller->read32(_DPLL_A);

    //	if(!controller->is_gvt){
    //		for(UINT32 counter=0;counter<16384;counter++){
    //			controller->read32(_DPLL_A);
    //		}
    //	}
    //}
    //DebugPrint(EFI_D_ERROR,"i915: DPLL all set %08x, read %08x\n",dplla,controller->read32(_DPLL_A));

    //SkyLake shared DPLL sequence: it's completely different!
    // DPLL 1
    //.ctl = LCPLL2_CTL,
    //.cfgcr1 = _DPLL1_CFGCR1,
    //.cfgcr2 = _DPLL1_CFGCR2,
    //intel_encoders_pre_pll_enable(crtc, pipe_config, old_state);
 */
    UINT32 ctrl1, cfgcr1, cfgcr2;
    struct skl_wrpll_params wrpll_params = {
        0,
    };

    /*
     * See comment in intel_dpll_hw_state to understand why we always use 0
     * as the DPLL id in this function. Basically, we put them in the first 6 bits then shift them into place for easier comparison
     */
    ctrl1 = DPLL_CTRL1_OVERRIDE(0); //Enable Programming
    ctrl1 |= DPLL_CTRL1_HDMI_MODE(0); //Set Mode to HDMI

    {
        //clock in Hz
        UINT64 clock = (UINT64)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock) * 10000;
        UINT64 afe_clock = clock * 5; /* AFE Clock is 5x Pixel clock */
        UINT64 dco_central_freq[3] = {8400000000ULL, 9000000000ULL, 9600000000ULL};

        struct skl_wrpll_context ctx = {0};
        UINT64 dco, d, i;
        UINT64 p0, p1, p2;

        //Find the DCO, Dividers, and DCO central freq
        ctx.min_deviation = 1ULL << 62;

        for (d = 0; d < ARRAY_SIZE(dividers); d++)
        {
            for (dco = 0; dco < ARRAY_SIZE(dco_central_freq); dco++)
            {
                for (i = 0; i < dividers[d].n_dividers; i++)
                {
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

        if (!ctx.p)
        {
            DebugPrint(EFI_D_ERROR, "i915: No valid divider found for %dHz\n", clock);
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

    UINT32 val = controller->read32(DPLL_CTRL1);

    //it's clock id!
    //how's port clock comptued?
    //UINT64 clock_khz=(UINT64)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock)*10;
    //UINT32 id=DPLL_CTRL1_LINK_RATE_810;
    //if(clock_khz>>1 >=135000){
    //	id=DPLL_CTRL1_LINK_RATE_1350;
    //}else if(clock_khz>>1 >=270000){
    //	id=DPLL_CTRL1_LINK_RATE_2700;
    //}
    //hack: anything else hangs
    UINT32 id = DPLL_CTRL1_LINK_RATE_1350;

    val &= ~(DPLL_CTRL1_HDMI_MODE(id) |
             DPLL_CTRL1_SSC(id) |
             DPLL_CTRL1_LINK_RATE_MASK(id));
    val |= ctrl1 << (id * 6);

    //DPLL 1
    controller->write32(DPLL_CTRL1, val);
    controller->read32(DPLL_CTRL1);

    controller->write32(_DPLL1_CFGCR1, cfgcr1);
    controller->write32(_DPLL1_CFGCR2, cfgcr2);
    controller->read32(_DPLL1_CFGCR1);
    controller->read32(_DPLL1_CFGCR2);

    //845 80400173 3a5
    DebugPrint(EFI_D_ERROR, "i915: DPLL_CTRL1 = %08x\n", controller->read32(DPLL_CTRL1));
    DebugPrint(EFI_D_ERROR, "i915: _DPLL1_CFGCR1 = %08x\n", controller->read32(_DPLL1_CFGCR1));
    DebugPrint(EFI_D_ERROR, "i915: _DPLL1_CFGCR2 = %08x\n", controller->read32(_DPLL1_CFGCR2));

    /* the enable bit is always bit 31 */
    controller->write32(LCPLL2_CTL, controller->read32(LCPLL2_CTL) | LCPLL_PLL_ENABLE);

    for (UINT32 counter = 0;; counter++)
    {
        if (controller->read32(DPLL_STATUS) & DPLL_LOCK(1))
        {
            DebugPrint(EFI_D_ERROR, "i915: DPLL %d locked\n", 1);
            break;
        }
        if (counter > 16384)
        {
            DebugPrint(EFI_D_ERROR, "i915: DPLL %d not locked\n", 1);
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