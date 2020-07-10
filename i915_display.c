#include <Uefi.h>

#include "i915_display.h"

static i915_CONTROLLER *controller;

static EFI_STATUS ReadEDID(EDID *result)
{
    UINT32 pin = 0;
    //it's an INTEL GPU, there's no way we could be big endian
    UINT32 *p = (UINT32 *)result;
    //try all the pins on GMBUS
    for (pin = 1; pin <= 6; pin++)
    {
        DebugPrint(EFI_D_ERROR, "i915: trying pin %d\n", pin);
        controller->write32(gmbusSelect, pin);
        if (EFI_ERROR(gmbusWait(controller, GMBUS_HW_RDY)))
        {
            //it's DP, need to hack AUX_CHAN
            continue;
        }
        //set read offset: i2cWrite(0x50, &offset, 1);
        controller->write32(gmbusData, 0);
        controller->write32(gmbusCommand,
                            (0x50 << GMBUS_SLAVE_ADDR_SHIFT) | (1 << GMBUS_BYTE_COUNT_SHIFT) | GMBUS_SLAVE_WRITE |
                                GMBUS_CYCLE_WAIT | GMBUS_SW_RDY);
        //gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
        gmbusWait(controller, GMBUS_HW_RDY);
        //read the edid: i2cRead(0x50, &edid, 128);
        //note that we could fail here!
        controller->write32(gmbusCommand,
                            (0x50 << GMBUS_SLAVE_ADDR_SHIFT) | (128 << GMBUS_BYTE_COUNT_SHIFT) | GMBUS_SLAVE_READ |
                                GMBUS_CYCLE_WAIT | GMBUS_SW_RDY);
        UINT32 i = 0;
        for (i = 0; i < 128; i += 4)
        {
            if (EFI_ERROR(gmbusWait(controller, GMBUS_HW_RDY)))
            {
                break;
            }
            p[i >> 2] = controller->read32(gmbusData);
        }
        //gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
        gmbusWait(controller, GMBUS_HW_RDY);
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
            return EFI_SUCCESS;
        }
    }
    //try DP AUX CHAN - Skylake
    //controller->write32(_DPA_AUX_CH_CTL+(1<<8),0x1234)
    //controller->write32(_DPA_AUX_CH_CTL+(0x600),0x1234);
    //controller->write32(_DPA_AUX_CH_CTL+(0<<8),0x1234);
    //controller->write32(_DPA_AUX_CH_DATA1+(0<<8),0xabcd);
    //controller->write32(_DPA_AUX_CH_DATA2+(0<<8),0xabcd);
    //controller->write32(_DPA_AUX_CH_DATA3+(0<<8),0xabcd);
    //DebugPrint(EFI_D_ERROR,"i915: SKL CTL %08x\n",controller->read32(_DPA_AUX_CH_CTL+(0<<8)));
    //DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",controller->read32(_DPA_AUX_CH_DATA1+(0<<8)));
    //DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",controller->read32(_DPA_AUX_CH_DATA2+(0<<8)));
    //DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",controller->read32(_DPA_AUX_CH_DATA3+(0<<8)));
    //controller->write32(_PCH_DP_B+(1<<8),0x1234);
    //DebugPrint(EFI_D_ERROR,"i915: SKL %08x\n",controller->read32(_DPA_AUX_CH_CTL+(1<<8)));
    //DebugPrint(EFI_D_ERROR,"i915: PCH %08x\n",controller->read32(_PCH_DP_B+(1<<8)));
    for (pin = 0; pin <= 5; pin++)
    {
        DebugPrint(EFI_D_ERROR, "i915: trying DP aux %d\n", pin);
        //aux message header is 3-4 bytes: ctrl8 addr16 len8
        //the data is big endian
        //len is receive buffer size-1
        //i2c init
        UINT32 send_ctl = (DP_AUX_CH_CTL_SEND_BUSY |
                           DP_AUX_CH_CTL_DONE |
                           DP_AUX_CH_CTL_TIME_OUT_ERROR |
                           DP_AUX_CH_CTL_TIME_OUT_MAX |
                           DP_AUX_CH_CTL_RECEIVE_ERROR |
                           (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
                           DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
                           DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
        /* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
        controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8), ((AUX_I2C_MOT | AUX_I2C_WRITE) << 28) | (0x50 << 8) | 0);
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
            if (counter >= 16384)
            {
                DebugPrint(EFI_D_ERROR, "i915:DP AUX channel timeout");
                break;
            }
        }
        controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
                            aux_status |
                                DP_AUX_CH_CTL_DONE |
                                DP_AUX_CH_CTL_TIME_OUT_ERROR |
                                DP_AUX_CH_CTL_RECEIVE_ERROR);
        //i2c send 1 byte
        send_ctl = (DP_AUX_CH_CTL_SEND_BUSY |
                    DP_AUX_CH_CTL_DONE |
                    DP_AUX_CH_CTL_TIME_OUT_ERROR |
                    DP_AUX_CH_CTL_TIME_OUT_MAX |
                    DP_AUX_CH_CTL_RECEIVE_ERROR |
                    (5 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
                    DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
                    DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
        controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8), (AUX_I2C_WRITE << 28) | (0x50 << 8) | 0);
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
            if (counter >= 16384)
            {
                DebugPrint(EFI_D_ERROR, "i915:DP AUX channel timeout");
                break;
            }
        }
        controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
                            aux_status |
                                DP_AUX_CH_CTL_DONE |
                                DP_AUX_CH_CTL_TIME_OUT_ERROR |
                                DP_AUX_CH_CTL_RECEIVE_ERROR);
        if (aux_status & (DP_AUX_CH_CTL_TIME_OUT_ERROR | DP_AUX_CH_CTL_RECEIVE_ERROR))
        {
            continue;
        }
        //i2c read 1 byte * 128
        DebugPrint(EFI_D_ERROR, "i915: reading DP aux %d\n", pin);
        //aux message header is 3-4 bytes: ctrl8 addr16 len8
        //the data is big endian
        //len is receive buffer size-1
        //i2c init
        send_ctl = (DP_AUX_CH_CTL_SEND_BUSY |
                    DP_AUX_CH_CTL_DONE |
                    DP_AUX_CH_CTL_TIME_OUT_ERROR |
                    DP_AUX_CH_CTL_TIME_OUT_MAX |
                    DP_AUX_CH_CTL_RECEIVE_ERROR |
                    (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
                    DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
                    DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
        /* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
        controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8), ((AUX_I2C_MOT | AUX_I2C_READ) << 28) | (0x50 << 8) | 0);
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
            if (counter >= 16384)
            {
                DebugPrint(EFI_D_ERROR, "i915: DP AUX channel timeout");
                break;
            }
        }
        controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
                            aux_status |
                                DP_AUX_CH_CTL_DONE |
                                DP_AUX_CH_CTL_TIME_OUT_ERROR |
                                DP_AUX_CH_CTL_RECEIVE_ERROR);
        UINT32 i = 0;
        for (i = 0; i < 128; i++)
        {
            send_ctl = (DP_AUX_CH_CTL_SEND_BUSY |
                        DP_AUX_CH_CTL_DONE |
                        DP_AUX_CH_CTL_TIME_OUT_ERROR |
                        DP_AUX_CH_CTL_TIME_OUT_MAX |
                        DP_AUX_CH_CTL_RECEIVE_ERROR |
                        (4 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
                        DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
                        DP_AUX_CH_CTL_SYNC_PULSE_SKL(32));
            controller->write32(_DPA_AUX_CH_DATA1 + (pin << 8), (AUX_I2C_READ << 28) | (0x50 << 8) | 0);
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
                if (counter >= 16384)
                {
                    DebugPrint(EFI_D_ERROR, "i915: DP AUX channel timeout");
                    break;
                }
            }
            controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
                                aux_status |
                                    DP_AUX_CH_CTL_DONE |
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
            return EFI_SUCCESS;
        }
    }
    return EFI_NOT_FOUND;
}

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

static UINT32 port = PORT_B;
STATIC INTN g_already_set = 0;

EFI_STATUS SetupClocks()
{
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
     * as the DPLL id in this function.
     */
    ctrl1 = DPLL_CTRL1_OVERRIDE(0);
    ctrl1 |= DPLL_CTRL1_HDMI_MODE(0);

    {
        //clock in Hz
        UINT64 clock = (UINT64)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock) * 10000;
        UINT64 afe_clock = clock * 5; /* AFE Clock is 5x Pixel clock */
        UINT64 dco_central_freq[3] = {8400000000ULL, 9000000000ULL, 9600000000ULL};

        struct skl_wrpll_context ctx = {0};
        UINT64 dco, d, i;
        UINT64 p0, p1, p2;

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

EFI_STATUS SetupDDIBuffer()
{
    //intel_prepare_hdmi_ddi_buffers(encoder, level);
    //the driver doesn't seem to do this for port A
    controller->write32(DDI_BUF_TRANS_LO(port, 9), 0x80003015u);
    controller->write32(DDI_BUF_TRANS_HI(port, 9), 0xcdu);
    return EFI_SUCCESS;
}
EFI_STATUS SetupIBoost()
{
    //if (IS_GEN9_BC(dev_priv))
    //	skl_ddi_set_iboost(encoder, level, INTEL_OUTPUT_HDMI);

    UINT32 tmp;

    tmp = controller->read32(DISPIO_CR_TX_BMU_CR0);
    tmp &= ~(BALANCE_LEG_MASK(port) | BALANCE_LEG_DISABLE(port));
    tmp |= 1 << BALANCE_LEG_SHIFT(port);
    controller->write32(DISPIO_CR_TX_BMU_CR0, tmp);
    return EFI_SUCCESS;
}
EFI_STATUS MapTranscoderDDI()
{
    //intel_ddi_enable_pipe_clock(crtc_state);
    controller->write32(_TRANS_CLK_SEL_A, TRANS_CLK_SEL_PORT(port));
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, TRANS_CLK_SEL_PORT(port) is %08x\n", __LINE__,
               TRANS_CLK_SEL_PORT(port));
    return EFI_SUCCESS;
}
EFI_STATUS SetupTranscoderAndPipe()
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
    UINT32 multiplier = 1;
    controller->write32(PIPE_MULT_A, multiplier - 1);

    DebugPrint(EFI_D_ERROR, "i915: HTOTAL_A (%x) = %08x\n", HTOTAL_A, controller->read32(HTOTAL_A));
    DebugPrint(EFI_D_ERROR, "i915: HBLANK_A (%x) = %08x\n", HBLANK_A, controller->read32(HBLANK_A));
    DebugPrint(EFI_D_ERROR, "i915: HSYNC_A (%x) = %08x\n", HSYNC_A, controller->read32(HSYNC_A));
    DebugPrint(EFI_D_ERROR, "i915: VTOTAL_A (%x) = %08x\n", VTOTAL_A, controller->read32(VTOTAL_A));
    DebugPrint(EFI_D_ERROR, "i915: VBLANK_A (%x) = %08x\n", VBLANK_A, controller->read32(VBLANK_A));
    DebugPrint(EFI_D_ERROR, "i915: VSYNC_A (%x) = %08x\n", VSYNC_A, controller->read32(VSYNC_A));
    DebugPrint(EFI_D_ERROR, "i915: PIPEASRC (%x) = %08x\n", PIPEASRC, controller->read32(PIPEASRC));
    DebugPrint(EFI_D_ERROR, "i915: BCLRPAT_A (%x) = %08x\n", BCLRPAT_A, controller->read32(BCLRPAT_A));
    DebugPrint(EFI_D_ERROR, "i915: VSYNCSHIFT_A (%x) = %08x\n", VSYNCSHIFT_A, controller->read32(VSYNCSHIFT_A));
    DebugPrint(EFI_D_ERROR, "i915: PIPE_MULT_A (%x) = %08x\n", PIPE_MULT_A, controller->read32(PIPE_MULT_A));

    DebugPrint(EFI_D_ERROR, "i915: before pipe gamma\n");
    return EFI_SUCCESS;
}
EFI_STATUS ConfigurePipeGamma()
{
    DebugPrint(EFI_D_ERROR, "i915: before gamma\n");
    for (UINT32 i = 0; i < 256; i++)
    {
        UINT32 word = (i << 16) | (i << 8) | i;
        controller->write32(_LGC_PALETTE_A + i * 4, word);
    }
    DebugPrint(EFI_D_ERROR, "i915: before pipe gamma\n");
    //DebugPrint(EFI_D_ERROR,"i915: _PIPEACONF: %08x\n",controller->read32(_PIPEACONF));
    //g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
    //return EFI_UNSUPPORTED;
    controller->write32(_PIPEACONF, PIPECONF_PROGRESSIVE | PIPECONF_GAMMA_MODE_8BIT);
    //controller->write32(_SKL_BOTTOM_COLOR_A,SKL_BOTTOM_COLOR_GAMMA_ENABLE);
    //controller->write32(_SKL_BOTTOM_COLOR_A,0);
    //controller->write32(_SKL_BOTTOM_COLOR_A,0x335577);
    controller->write32(_SKL_BOTTOM_COLOR_A, 0);
    controller->write32(_GAMMA_MODE_A, GAMMA_MODE_MODE_8BIT);
    return EFI_SUCCESS;
}
EFI_STATUS ConfigureTransMSAMISC()
{
    controller->write32(_TRANSA_MSA_MISC, TRANS_MSA_SYNC_CLK | TRANS_MSA_8_BPC); //Sets MSA MISC FIelds for DP
    return EFI_SUCCESS;
}
EFI_STATUS ConfigureTransDDI()
{
    controller->write32(_TRANS_DDI_FUNC_CTL_A, (
                                                   TRANS_DDI_FUNC_ENABLE | TRANS_DDI_SELECT_PORT(port) | TRANS_DDI_PHSYNC | TRANS_DDI_PVSYNC |
                                                   TRANS_DDI_BPC_8 | TRANS_DDI_MODE_SELECT_HDMI));
    return EFI_SUCCESS;
}
EFI_STATUS EnablePipe()
{
    controller->write32(_PIPEACONF, PIPECONF_ENABLE | PIPECONF_PROGRESSIVE | PIPECONF_GAMMA_MODE_8BIT);
    return EFI_SUCCESS;
}
EFI_STATUS EnableDDI()
{
    /* Display WA #1143: skl,kbl,cfl */
    UINT32 saved_port_bits = controller->read32(DDI_BUF_CTL(port)) & (DDI_BUF_PORT_REVERSAL | DDI_A_4_LANES); //FOR HDMI, only port reversal and Lane count matter

    //if (IS_GEN9_BC(dev_priv))
    {
        /*
         * For some reason these chicken bits have been
         * stuffed into a transcoder register, event though
         * the bits affect a specific DDI port rather than
         * a specific transcoder.
         */

        //Workaround to get the HSWING to take effect on HDMI Ports. See https://patchwork.freedesktop.org/patch/199817/
        UINT32 reg = CHICKEN_TRANS_A;
        if (port == PORT_B)
        {
            reg = CHICKEN_TRANS_A;
        }
        if (port == PORT_C)
        {
            reg = CHICKEN_TRANS_B;
        }
        if (port == PORT_D)
        {
            reg = CHICKEN_TRANS_C;
        }
        //if(port==PORT_E){reg = CHICKEN_TRANS_A;}
        UINT32 val;

        val = controller->read32(reg);

        if (port == PORT_E)
            val |= DDIE_TRAINING_OVERRIDE_ENABLE |
                   DDIE_TRAINING_OVERRIDE_VALUE;
        else
            val |= DDI_TRAINING_OVERRIDE_ENABLE |
                   DDI_TRAINING_OVERRIDE_VALUE;

        controller->write32(reg, val);
        controller->read32(reg);

        //... don't have timer
        for (UINT32 counter = 0;;)
        {
            //controller->read32(reg);
            counter += 1;
            if (counter >= 16384)
            {
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

        controller->write32(reg, val);
    }

    /* In HDMI/DVI mode, the port width, and swing/emphasis values
     * are ignored so nothing special needs to be done besides
     * enabling the port.
     */
    controller->write32(DDI_BUF_CTL(port), saved_port_bits | DDI_BUF_CTL_ENABLE);
    DebugPrint(EFI_D_ERROR, "DDI_BUF_CTL(port) = %08x\n", controller->read32(DDI_BUF_CTL(port)));

    return EFI_SUCCESS;
}
EFI_STATUS SetupAndEnablePlane()
{
    UINT32 horz_active = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActive |
                         ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb >> 4) << 8);
    UINT32 vert_active = controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActive |
                         ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb >> 4) << 8);
    //plane
    UINT32 stride = (horz_active * 4 + 63) & -64;
    controller->stride = stride;
    controller->write32(_DSPAOFFSET, 0);
    controller->write32(_DSPAPOS, 0);
    controller->write32(_DSPASTRIDE, stride >> 6);
    controller->write32(_DSPASIZE, (horz_active - 1) | ((vert_active - 1) << 16));
    controller->write32(_DSPACNTR, DISPLAY_PLANE_ENABLE | PLANE_CTL_FORMAT_XRGB_8888 | PLANE_CTL_PLANE_GAMMA_DISABLE);
    controller->write32(_DSPASURF, controller->gmadr);
    controller->fbsize = stride * vert_active;
    //controller->write32(_DSPAADDR,0);
    //word=controller->read32(_DSPACNTR);
    //controller->write32(_DSPACNTR,(word&~PLANE_CTL_FORMAT_MASK)|DISPLAY_PLANE_ENABLE|PLANE_CTL_FORMAT_XRGB_8888);
    //|PLANE_CTL_ORDER_RGBX

    DebugPrint(EFI_D_ERROR, "i915: plane enabled, dspcntr: %08x, FbBase: %p\n", controller->read32(_DSPACNTR),
               controller->FbBase);
    return EFI_SUCCESS;
}
EFI_STATUS setOutputPath() {
    controller->OutputPath.ConType = HDMI;
    controller->OutputPath.DDI=0;
    controller->OutputPath.isEDP=FALSE;
    controller->OutputPath.Plane=0;
    controller->OutputPath.Port=0;
    controller->OutputPath.Transcoder=0;
    return EFI_SUCCESS;
}
EFI_STATUS setDisplayGraphicsMode(UINT32 ModeNumber)
{

    DebugPrint(EFI_D_ERROR, "i915: set mode %u\n", ModeNumber);
    if (g_already_set)
    {
        DebugPrint(EFI_D_ERROR, "i915: mode already set\n");
        return EFI_SUCCESS;
    }
    g_already_set = 1;

    controller->write32(_PIPEACONF, 0);

    SetupClocks();
    SetupDDIBuffer();

    //intel_hdmi_prepare(encoder, pipe_config);
    //hdmi_reg=DDI_BUF_CTL(port)

    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);

    //it's Type C
    //icl_enable_phy_clock_gating(dig_port);

    SetupIBoost();

    MapTranscoderDDI();

    //we got here

    //intel_dig_port->set_infoframes(encoder,
    //			       crtc_state->has_infoframe,
    //			       crtc_state, conn_state);

    //if (intel_crtc_has_dp_encoder(pipe_config))
    //	intel_dp_set_m_n(pipe_config, M1_N1);

    //program PIPE_A
    SetupTranscoderAndPipe();
    ConfigurePipeGamma();

    //bad setup causes hanging when enabling trans / pipe, but what is it?
    //we got here
    //ddi
    DebugPrint(EFI_D_ERROR, "i915: before DDI\n");
    ConfigureTransMSAMISC();
    ConfigureTransDDI();
    DebugPrint(EFI_D_ERROR, "i915: after DDI\n");
    //g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
    //return EFI_UNSUPPORTED;

    //test: could be Windows hanging, it's not
    //g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
    //we failed here
    //return EFI_UNSUPPORTED;

    EnablePipe();
    UINT32 counter = 0;
    for (;;)
    {
        counter += 1;
        if (counter >= 16384)
        {
            DebugPrint(EFI_D_ERROR, "i915: failed to enable PIPE\n");
            break;
        }
        if (controller->read32(_PIPEACONF) & I965_PIPECONF_ACTIVE)
        {
            DebugPrint(EFI_D_ERROR, "i915: pipe enabled\n");
            break;
        }
    }

    EnableDDI();
    SetupAndEnablePlane();
    i915GraphicsFramebufferConfigure(controller);

    return EFI_SUCCESS;
}

STATIC UINT8
    edid_fallback[] = {
        //generic 1280x720
        0, 255, 255, 255, 255, 255, 255, 0, 34, 240, 84, 41, 1, 0, 0, 0, 4, 23, 1, 4, 165, 52, 32, 120, 35, 252, 129, 164, 85, 77, 157, 37, 18, 80, 84, 33, 8, 0, 209, 192, 129, 192, 129, 64, 129, 128, 149, 0, 169, 64, 179, 0, 1, 1, 26, 29, 0, 128, 81, 208, 28, 32, 64, 128, 53, 0, 77, 187, 16, 0, 0, 30, 0, 0, 0, 254, 0, 55, 50, 48, 112, 32, 32, 32, 32, 32, 32, 32, 32, 10, 0, 0, 0, 253, 0, 24, 60, 24, 80, 17, 0, 10, 32, 32, 32, 32, 32, 32, 0, 0, 0, 252, 0, 72, 80, 32, 90, 82, 95, 55, 50, 48, 112, 10, 32, 32, 0, 161
        //the test monitor
        //0,255,255,255,255,255,255,0,6,179,192,39,141,30,0,0,49,26,1,3,128,60,34,120,42,83,165,167,86,82,156,38,17,80,84,191,239,0,209,192,179,0,149,0,129,128,129,64,129,192,113,79,1,1,2,58,128,24,113,56,45,64,88,44,69,0,86,80,33,0,0,30,0,0,0,255,0,71,67,76,77,84,74,48,48,55,56,50,49,10,0,0,0,253,0,50,75,24,83,17,0,10,32,32,32,32,32,32,0,0,0,252,0,65,83,85,83,32,86,90,50,55,57,10,32,32,1,153,2,3,34,113,79,1,2,3,17,18,19,4,20,5,14,15,29,30,31,144,35,9,23,7,131,1,0,0,101,3,12,0,32,0,140,10,208,138,32,224,45,16,16,62,150,0,86,80,33,0,0,24,1,29,0,114,81,208,30,32,110,40,85,0,86,80,33,0,0,30,1,29,0,188,82,208,30,32,184,40,85,64,86,80,33,0,0,30,140,10,208,144,32,64,49,32,12,64,85,0,86,80,33,0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,237
};

EFI_STATUS DisplayInit(i915_CONTROLLER *iController)
{
    EFI_STATUS Status;

    controller = iController;
    /* 1. Enable PCH reset handshake. */
    //intel_pch_reset_handshake(dev_priv, !HAS_PCH_NOP(dev_priv));
    controller->write32(HSW_NDE_RSTWRN_OPT, controller->read32(HSW_NDE_RSTWRN_OPT) | RESET_PCH_HANDSHAKE_ENABLE);

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
    controller->write32(HSW_PWR_WELL_CTL1, controller->read32(HSW_PWR_WELL_CTL1) | 0xA00002AAu);
    for (UINT32 counter = 0;; counter++)
    {
        UINT32 stat = controller->read32(HSW_PWR_WELL_CTL1);
        if (counter > 16384)
        {
            DebugPrint(EFI_D_ERROR, "i915: power well enabling timed out %08x\n", stat);
            break;
        }
        if (stat & 0x50000155u)
        {
            DebugPrint(EFI_D_ERROR, "i915: power well enabled %08x\n", stat);
            break;
        }
    }
    //disable VGA
    UINT32 vgaword = controller->read32(VGACNTRL);
    controller->write32(VGACNTRL, (vgaword & ~VGA_2X_MODE) | VGA_DISP_DISABLE);
    //DebugPrint(EFI_D_ERROR,"i915: bars %08x %08x %08x %08x\n",Pci.Device.Bar[0],Pci.Device.Bar[1],Pci.Device.Bar[2],Pci.Device.Bar[3]);

    ///* 5. Enable CDCLK. */
    //icl_init_cdclk(dev_priv);
    //080002a1 on test machine
    //DebugPrint(EFI_D_ERROR,"i915: CDCLK = %08x\n",controller->read32(CDCLK_CTL));
    //there seems no need to do so

    ///* 6. Enable DBUF. */
    //icl_dbuf_enable(dev_priv);
    controller->write32(DBUF_CTL_S1, controller->read32(DBUF_CTL_S1) | DBUF_POWER_REQUEST);
    controller->write32(DBUF_CTL_S2, controller->read32(DBUF_CTL_S2) | DBUF_POWER_REQUEST);
    controller->read32(DBUF_CTL_S2);
    for (UINT32 counter = 0;; counter++)
    {
        if (counter > 16384)
        {
            DebugPrint(EFI_D_ERROR, "i915: DBUF timeout\n");
            break;
        }
        if (controller->read32(DBUF_CTL_S1) & controller->read32(DBUF_CTL_S2) & DBUF_POWER_STATE)
        {
            DebugPrint(EFI_D_ERROR, "i915: DBUF good\n");
            break;
        }
    }

    ///* 7. Setup MBUS. */
    //icl_mbus_init(dev_priv);
    controller->write32(MBUS_ABOX_CTL,
                        MBUS_ABOX_BT_CREDIT_POOL1(16) |
                            MBUS_ABOX_BT_CREDIT_POOL2(16) |
                            MBUS_ABOX_B_CREDIT(1) |
                            MBUS_ABOX_BW_CREDIT(1));

    //set up display buffer
    //the value is from host
    DebugPrint(EFI_D_ERROR, "i915: _PLANE_BUF_CFG_1_A = %08x\n", controller->read32(_PLANE_BUF_CFG_1_A));
    controller->write32(_PLANE_BUF_CFG_1_A, 0x035b0000);
    DebugPrint(EFI_D_ERROR, "i915: _PLANE_BUF_CFG_1_A = %08x (after)\n", controller->read32(_PLANE_BUF_CFG_1_A));

    //initialize output
    //need workaround: always initialize DDI
    //intel_dig_port->hdmi.hdmi_reg = DDI_BUF_CTL(port);
    //intel_ddi_init(PORT_A);
    UINT32 found = controller->read32(SFUSE_STRAP);
    DebugPrint(EFI_D_ERROR, "i915: SFUSE_STRAP = %08x\n", found);
    port = PORT_A;
    if (found & SFUSE_STRAP_DDIB_DETECTED)
    {
        port = PORT_B; //intel_ddi_init(PORT_B);
    }
    else if (found & SFUSE_STRAP_DDIC_DETECTED)
    {
        port = PORT_C; //intel_ddi_init(PORT_C);
    }
    else if (found & SFUSE_STRAP_DDID_DETECTED)
    {
        port = PORT_D; //intel_ddi_init(PORT_D);
    }
    //if (found & SFUSE_STRAP_DDIF_DETECTED)
    //	intel_ddi_init(dev_priv, PORT_F);

    //reset GMBUS
    //intel_i2c_reset(dev_priv);
    controller->write32(GMBUS0, 0);
    controller->write32(GMBUS4, 0);

    // query EDID and initialize the mode
    // it somehow fails on real hardware
    Status = ReadEDID(&controller->edid);
    if (EFI_ERROR(Status))
    {
        DebugPrint(EFI_D_ERROR, "i915: failed to read EDID\n");
        for (UINT32 i = 0; i < 128; i++)
        {
            ((UINT8 *)&controller->edid)[i] = edid_fallback[i];
        }
    }
    DebugPrint(EFI_D_ERROR, "i915: got EDID:\n");
    for (UINT32 i = 0; i < 16; i++)
    {
        for (UINT32 j = 0; j < 8; j++)
        {
            DebugPrint(EFI_D_ERROR, "%02x ", ((UINT8 *)(&controller->edid))[i * 8 + j]);
        }
        DebugPrint(EFI_D_ERROR, "\n");
    }
    return EFI_SUCCESS;
}
