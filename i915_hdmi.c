#include "i915_controller.h"
#include "i915_debug.h"
#include "i915_gmbus.h"
#include "i915_ddi.h"
#include "i915_dp.h"
#include "i915_hdmi.h"
#include "i915_reg.h"
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

static int intel_hdmi_source_max_tmds_clock()
{
    // struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
    int max_tmds_clock;
    //, vbt_max_tmds_clock;

    // if (INTEL_GEN(dev_priv) >= 10 || IS_GEMINILAKE(dev_priv))
    // 	max_tmds_clock = 594000;
    // else if (INTEL_GEN(dev_priv) >= 8 || IS_HASWELL(dev_priv))
    max_tmds_clock = 300000;
    // else if (INTEL_GEN(dev_priv) >= 5)
    // 	max_tmds_clock = 225000;
    // else
    // 	max_tmds_clock = 165000;

    // vbt_max_tmds_clock = intel_bios_max_tmds_clock(encoder);
    // if (vbt_max_tmds_clock)
    // 	max_tmds_clock = min(max_tmds_clock, vbt_max_tmds_clock);

    return max_tmds_clock;
}
static int hdmi_port_clock_limit()
{
    //struct intel_encoder *encoder = &hdmi_to_dig_port(hdmi)->base;
    int max_tmds_clock = intel_hdmi_source_max_tmds_clock();

    return max_tmds_clock;
}
INT32 intel_hdmi_link_required(int pixel_clock, int bpp)
{
    /* pixel_clock is in kHz, divide bpp by 8 for bit to Byte conversion */
    return DIV_ROUND_UP(pixel_clock * bpp, 8);
}
static BOOLEAN intel_hdmi_valid_link_rate(UINT32 pixelClock)
{
    /* const struct drm_display_mode *fixed_mode =
		intel_dp->attached_connector->panel.fixed_mode; */
    int mode_rate, max_rate;

    mode_rate = intel_hdmi_link_required(pixelClock * 10, 8);
    max_rate = hdmi_port_clock_limit();
    PRINT_DEBUG(EFI_D_ERROR, "Mode: %u, Max:%u\n", mode_rate, max_rate);
    if (mode_rate > max_rate)
        return FALSE;

    return TRUE;
}
EFI_STATUS ConvertFallbackEDIDToHDMIEDID(EDID *result, i915_CONTROLLER *controller, UINT8 *fallback)
{
    UINT32 i = 0;

    // it's an INTEL GPU, there's no way we could be big endian
    for (i = 0; i < 128; i++)
    {
        ((UINT8 *)result)[i] = fallback[i];
    }
    UINT32 *p = (UINT32 *)result;
    // try all the pins on GMBUS
    {
        // gmbusWait(controller,GMBUS_HW_WAIT_PHASE);

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
            if (!intel_hdmi_valid_link_rate(result->detailTimings[DETAIL_TIME_SELCTION].pixelClock))
            {
                for (int j = 0; j < 4; j++)
                {
                    if (result->detailTimings[j].pixelClock > 0 && intel_hdmi_valid_link_rate(result->detailTimings[j].pixelClock))
                    {
                        result->detailTimings[DETAIL_TIME_SELCTION] = result->detailTimings[j];
                        return EFI_SUCCESS;
                    }
                }
                PRINT_DEBUG(EFI_D_ERROR, "pixelClock: %d\n", result->detailTimings[DETAIL_TIME_SELCTION].pixelClock);

                for (int j = 0; j < 4; j++)
                {
                    if (result->detailTimings[j].pixelClock >> 1 > 0 && intel_hdmi_valid_link_rate(result->detailTimings[j].pixelClock >> 1))
                    {
                        result->detailTimings[j].pixelClock = result->detailTimings[j].pixelClock >> 1;
                        result->detailTimings[DETAIL_TIME_SELCTION] = result->detailTimings[j];
                        return EFI_SUCCESS;
                    }
                }
                PRINT_DEBUG(EFI_D_ERROR, "pixelClock: %d\n", result->detailTimings[DETAIL_TIME_SELCTION].pixelClock);
            }
            // if (result->detailTimings[DETAIL_TIME_SELCTION].pixelClock > 3) {
            //     result->detailTimings[DETAIL_TIME_SELCTION].pixelClock >> 1;
            // }
            return EFI_SUCCESS;
        }
    }
    return EFI_NOT_FOUND;
}
EFI_STATUS ReadEDIDHDMI(EDID *result, i915_CONTROLLER *controller, UINT8 pin)
{
    // it's an INTEL GPU, there's no way we could be big endian
    UINT32 *p = (UINT32 *)result;
    // try all the pins on GMBUS
    {
        PRINT_DEBUG(EFI_D_ERROR, "trying pin %d\n", pin);
        controller->write32(gmbusSelect, pin);
        if (EFI_ERROR(gmbusWait(controller, GMBUS_HW_RDY)))
        {

            return EFI_NOT_FOUND;
        }
        // set read offset: i2cWrite(0x50, &offset, 1);
        controller->write32(gmbusData, 0);
        controller->write32(gmbusCommand, (0x50 << GMBUS_SLAVE_ADDR_SHIFT) |
                                              (1 << GMBUS_BYTE_COUNT_SHIFT) |
                                              GMBUS_SLAVE_WRITE | GMBUS_CYCLE_WAIT |
                                              GMBUS_SW_RDY);
        // gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
        gmbusWait(controller, GMBUS_HW_RDY);
        PRINT_DEBUG(EFI_D_ERROR, "trying pin %d\n", pin);

        // read the edid: i2cRead(0x50, &edid, 128);
        // note that we could fail here!
        controller->write32(gmbusCommand, (0x50 << GMBUS_SLAVE_ADDR_SHIFT) |
                                              (128 << GMBUS_BYTE_COUNT_SHIFT) |
                                              GMBUS_SLAVE_READ | GMBUS_CYCLE_WAIT |
                                              GMBUS_SW_RDY);
        UINT32 i = 0;
        for (i = 0; i < 128; i += 4)
        {
            if (EFI_ERROR(gmbusWait(controller, GMBUS_HW_RDY)))
            {
                PRINT_DEBUG(EFI_D_ERROR, "trying pin %d\n", pin);

                break;
            }
            p[i >> 2] = controller->read32(gmbusData);
        }
        // gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
        gmbusWait(controller, GMBUS_HW_RDY);
        PRINT_DEBUG(EFI_D_ERROR, "trying pin %d\n", pin);

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
            if (!intel_hdmi_valid_link_rate(result->detailTimings[DETAIL_TIME_SELCTION].pixelClock))
            {
                for (int j = 0; j < 4; j++)
                {
                    if (result->detailTimings[j].pixelClock > 0 && intel_hdmi_valid_link_rate(result->detailTimings[j].pixelClock))
                    {
                        result->detailTimings[DETAIL_TIME_SELCTION] = result->detailTimings[j];
                        return EFI_SUCCESS;
                    }
                }
                PRINT_DEBUG(EFI_D_ERROR, "pixelClock: %d\n", result->detailTimings[DETAIL_TIME_SELCTION].pixelClock);

                for (int j = 0; j < 4; j++)
                {
                    if (result->detailTimings[j].pixelClock >> 1 > 0 && intel_hdmi_valid_link_rate(result->detailTimings[j].pixelClock >> 1))
                    {
                        result->detailTimings[j].pixelClock = result->detailTimings[j].pixelClock >> 1;
                        result->detailTimings[DETAIL_TIME_SELCTION] = result->detailTimings[j];
                        return EFI_SUCCESS;
                    }
                }
                PRINT_DEBUG(EFI_D_ERROR, "pixelClock: %d\n", result->detailTimings[DETAIL_TIME_SELCTION].pixelClock);
            }
            // if (result->detailTimings[DETAIL_TIME_SELCTION].pixelClock > 3) {
            //     result->detailTimings[DETAIL_TIME_SELCTION].pixelClock >> 1;
            // }
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
        PRINT_DEBUG(EFI_D_ERROR, "Incorrect PDiv\n");
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
        PRINT_DEBUG(EFI_D_ERROR, "Incorrect KDiv\n");
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

EFI_STATUS SetupClockHDMI(i915_CONTROLLER *controller)
{

    UINT32 ctrl1, cfgcr1, cfgcr2;
    struct skl_wrpll_params wrpll_params = {
        0,
    };

    /*
     * See comment in intel_dpll_hw_state to understand why we always use 0
     * as the DPLL id in this function. Basically, we put them in the first 6 bits then shift them into place for easier comparison
     */
    ctrl1 = DPLL_CTRL1_OVERRIDE(0);   //Enable Programming
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
            PRINT_DEBUG(EFI_D_ERROR, "No valid divider found for %dHz\n", clock);
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
    PRINT_DEBUG(EFI_D_ERROR, "DPLL_CTRL1 = %08x\n", controller->read32(DPLL_CTRL1));
    PRINT_DEBUG(EFI_D_ERROR, "_DPLL1_CFGCR1 = %08x\n", controller->read32(_DPLL1_CFGCR1));
    PRINT_DEBUG(EFI_D_ERROR, "_DPLL1_CFGCR2 = %08x\n", controller->read32(_DPLL1_CFGCR2));

    /* the enable bit is always bit 31 */
    controller->write32(LCPLL2_CTL, controller->read32(LCPLL2_CTL) | LCPLL_PLL_ENABLE);

    for (UINT32 counter = 0;; counter++)
    {
        if (controller->read32(DPLL_STATUS) & DPLL_LOCK(1))
        {
            PRINT_DEBUG(EFI_D_ERROR, "DPLL %d locked\n", 1);
            break;
        }
        if (counter > 500)
        {
            PRINT_DEBUG(EFI_D_ERROR, "DPLL %d not locked\n", 1);
            break;
        }
        gBS->Stall(10);
    }

    //intel_encoders_pre_enable(crtc, pipe_config, old_state);
    //could be intel_ddi_pre_enable_hdmi
    //intel_ddi_clk_select(encoder, crtc_state);
    UINT32 port = controller->OutputPath.Port;
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
EFI_STATUS SetupTranscoderAndPipeHDMI(i915_CONTROLLER *controller)
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

    PRINT_DEBUG(EFI_D_ERROR, "HTOTAL_A (%x) = %08x\n", HTOTAL_A, controller->read32(HTOTAL_A));
    PRINT_DEBUG(EFI_D_ERROR, "HBLANK_A (%x) = %08x\n", HBLANK_A, controller->read32(HBLANK_A));
    PRINT_DEBUG(EFI_D_ERROR, "HSYNC_A (%x) = %08x\n", HSYNC_A, controller->read32(HSYNC_A));
    PRINT_DEBUG(EFI_D_ERROR, "VTOTAL_A (%x) = %08x\n", VTOTAL_A, controller->read32(VTOTAL_A));
    PRINT_DEBUG(EFI_D_ERROR, "VBLANK_A (%x) = %08x\n", VBLANK_A, controller->read32(VBLANK_A));
    PRINT_DEBUG(EFI_D_ERROR, "VSYNC_A (%x) = %08x\n", VSYNC_A, controller->read32(VSYNC_A));
    PRINT_DEBUG(EFI_D_ERROR, "PIPEASRC (%x) = %08x\n", PIPEASRC, controller->read32(PIPEASRC));
    PRINT_DEBUG(EFI_D_ERROR, "BCLRPAT_A (%x) = %08x\n", BCLRPAT_A, controller->read32(BCLRPAT_A));
    PRINT_DEBUG(EFI_D_ERROR, "VSYNCSHIFT_A (%x) = %08x\n", VSYNCSHIFT_A, controller->read32(VSYNCSHIFT_A));
    PRINT_DEBUG(EFI_D_ERROR, "PIPE_MULT_A (%x) = %08x\n", PIPE_MULT_A, controller->read32(PIPE_MULT_A));

    PRINT_DEBUG(EFI_D_ERROR, "before pipe gamma\n");
    return EFI_SUCCESS;
}