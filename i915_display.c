#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

#include "i915_display.h"

static i915_CONTROLLER *controller;

static EFI_STATUS ReadEDID(EDID *result)
{
    UINT32 pin = 0;
    // it's an INTEL GPU, there's no way we could be big endian
    UINT32 *p = (UINT32 *)result;
    // try all the pins on GMBUS
    for (pin = 1; pin <= 6; pin++)
    {
        DebugPrint(EFI_D_ERROR, "i915: trying pin %d\n", pin);
        controller->write32(gmbusSelect, pin);
        if (EFI_ERROR(gmbusWait(controller, GMBUS_HW_RDY)))
        {
            // it's DP, need to hack AUX_CHAN
            continue;
        }
        // set read offset: i2cWrite(0x50, &offset, 1);
        controller->write32(gmbusData, 0);
        controller->write32(gmbusCommand, (0x50 << GMBUS_SLAVE_ADDR_SHIFT) |
                                              (1 << GMBUS_BYTE_COUNT_SHIFT) |
                                              GMBUS_SLAVE_WRITE | GMBUS_CYCLE_WAIT |
                                              GMBUS_SW_RDY);
        // gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
        gmbusWait(controller, GMBUS_HW_RDY);
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
                break;
            }
            p[i >> 2] = controller->read32(gmbusData);
        }
        // gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
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
    // try DP AUX CHAN - Skylake
    // controller->write32(_DPA_AUX_CH_CTL+(1<<8),0x1234)
    // controller->write32(_DPA_AUX_CH_CTL+(0x600),0x1234);
    // controller->write32(_DPA_AUX_CH_CTL+(0<<8),0x1234);
    // controller->write32(_DPA_AUX_CH_DATA1+(0<<8),0xabcd);
    // controller->write32(_DPA_AUX_CH_DATA2+(0<<8),0xabcd);
    // controller->write32(_DPA_AUX_CH_DATA3+(0<<8),0xabcd);
    // DebugPrint(EFI_D_ERROR,"i915: SKL CTL
    // %08x\n",controller->read32(_DPA_AUX_CH_CTL+(0<<8)));
    // DebugPrint(EFI_D_ERROR,"i915: SKL DATA
    // %08x\n",controller->read32(_DPA_AUX_CH_DATA1+(0<<8)));
    // DebugPrint(EFI_D_ERROR,"i915: SKL DATA
    // %08x\n",controller->read32(_DPA_AUX_CH_DATA2+(0<<8)));
    // DebugPrint(EFI_D_ERROR,"i915: SKL DATA
    // %08x\n",controller->read32(_DPA_AUX_CH_DATA3+(0<<8)));
    // controller->write32(_PCH_DP_B+(1<<8),0x1234);
    // DebugPrint(EFI_D_ERROR,"i915: SKL
    // %08x\n",controller->read32(_DPA_AUX_CH_CTL+(1<<8)));
    // DebugPrint(EFI_D_ERROR,"i915: PCH
    // %08x\n",controller->read32(_PCH_DP_B+(1<<8)));
    DebugPrint(EFI_D_ERROR, "Reading PP_STATUS: %u \n", controller->read32(PP_STATUS) );
    for (pin = 0; pin <= 5; pin++)
    {
        DebugPrint(EFI_D_ERROR, "i915: trying DP aux %d\n", pin);
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
            if (counter >= 16384)
            {
                DebugPrint(EFI_D_ERROR, "i915:DP AUX channel timeout");
                break;
            }
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
            if (counter >= 16384)
            {
                DebugPrint(EFI_D_ERROR, "i915:DP AUX channel timeout");
                break;
            }
        }
        controller->write32(_DPA_AUX_CH_CTL + (pin << 8),
                            aux_status | DP_AUX_CH_CTL_DONE |
                                DP_AUX_CH_CTL_TIME_OUT_ERROR |
                                DP_AUX_CH_CTL_RECEIVE_ERROR);
        if (aux_status &
            (DP_AUX_CH_CTL_TIME_OUT_ERROR | DP_AUX_CH_CTL_RECEIVE_ERROR))
        {
            continue;
        }
        // i2c read 1 byte * 128
        DebugPrint(EFI_D_ERROR, "i915: reading DP aux %d\n", pin);
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
            if (counter >= 16384)
            {
                DebugPrint(EFI_D_ERROR, "i915: DP AUX channel timeout");
                break;
            }
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
                if (counter >= 16384)
                {
                    DebugPrint(EFI_D_ERROR, "i915: DP AUX channel timeout");
                    break;
                }
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
    }
    return EFI_NOT_FOUND;
}

STATIC INTN g_already_set = 0;

EFI_STATUS SetupClocks()
{
    EFI_STATUS status;
    switch (controller->OutputPath.ConType)
    {
    case HDMI:
        status = SetupClockHDMI(controller);
        break;
    case eDP:
        status = SetupClockeDP(controller);

        break;
    case DPSST:
        status = SetupClockeDP(controller);

        break;
    default:
        status = EFI_NOT_FOUND;
        break;
    }
    return status;
}

EFI_STATUS SetupDDIBuffer()
{
    // intel_prepare_hdmi_ddi_buffers(encoder, level);
    // the driver doesn't seem to do this for port A
    UINT32 port = controller->OutputPath.Port;
    EFI_STATUS status = EFI_NOT_FOUND;
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);

    switch (controller->OutputPath.ConType)
    {
    case HDMI:
        DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
                   __LINE__, status);

        controller->write32(DDI_BUF_TRANS_LO(port, 9), 0x80003015u);
        controller->write32(DDI_BUF_TRANS_HI(port, 9), 0xcdu);
        DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
                   __LINE__, status);

        status = EFI_SUCCESS;
        DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
                   __LINE__, status);

        break;
    case eDP:
        status = SetupDDIBufferDP(controller);

        break;
    case DPSST:
        status = SetupDDIBufferDP(controller);

        break;
    default:
        status = EFI_NOT_FOUND;
        break;
    }
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);

    return EFI_SUCCESS;
}
EFI_STATUS SetupIBoost()
{
    UINT32 port = controller->OutputPath.Port;

    // if (IS_GEN9_BC(dev_priv))
    //	skl_ddi_set_iboost(encoder, level, INTEL_OUTPUT_HDMI);
   // if (controller->OutputPath.ConType == HDMI)
    {
        UINT32 tmp;

        tmp = controller->read32(DISPIO_CR_TX_BMU_CR0);
        tmp &= ~(BALANCE_LEG_MASK(port) | BALANCE_LEG_DISABLE(port));
        tmp |= 1 << 24; // temp
        tmp |= 1 << BALANCE_LEG_SHIFT(port);
        controller->write32(DISPIO_CR_TX_BMU_CR0, tmp);
    }

    return EFI_SUCCESS;
}
EFI_STATUS MapTranscoderDDI()
{
    UINT32 port = controller->OutputPath.Port;
    if (controller->OutputPath.ConType != eDP)
    {
        // intel_ddi_enable_pipe_clock(crtc_state);
        controller->write32(_TRANS_CLK_SEL_A, TRANS_CLK_SEL_PORT(port));
        DebugPrint(
            EFI_D_ERROR,
            "i915: progressed to line %d, TRANS_CLK_SEL_PORT(port) is %08x\n",
            __LINE__, TRANS_CLK_SEL_PORT(port));
    }
    return EFI_SUCCESS;
}
EFI_STATUS SetupTranscoderAndPipe()
{
    DebugPrint(EFI_D_ERROR, "i915: before TranscoderPipe  %u \n",
               controller->OutputPath.ConType);

    switch (controller->OutputPath.ConType)
    {
    case HDMI:
        SetupTranscoderAndPipeHDMI(controller);
        break;
    case DPSST:
        SetupTranscoderAndPipeDP(controller);
        break;
    case DPMST:
        SetupTranscoderAndPipeDP(controller);
        break;
    case eDP:
        SetupTranscoderAndPipeEDP(controller);
        break;

    default:
        break;
    }
    DebugPrint(EFI_D_ERROR, "i915: after TranscoderPipe\n");

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
    // DebugPrint(EFI_D_ERROR,"i915: _PIPEACONF:
    // %08x\n",controller->read32(_PIPEACONF));
    // g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
    // return EFI_UNSUPPORTED;
    UINT64 reg = _PIPEACONF;
    if (controller->OutputPath.ConType == eDP)
    {
        reg = _PIPEEDPCONF;
    }
    DebugPrint(EFI_D_ERROR, "REGISTER %x", reg);
        controller->write32(reg, PIPECONF_PROGRESSIVE |
   PIPECONF_GAMMA_MODE_8BIT);
    //controller->write32(_SKL_BOTTOM_COLOR_A,SKL_BOTTOM_COLOR_GAMMA_ENABLE);
    //controller->write32(_SKL_BOTTOM_COLOR_A,0);
    controller->write32(_SKL_BOTTOM_COLOR_A,0x335577);
    //controller->write32(_SKL_BOTTOM_COLOR_A, 0);
    controller->write32(_GAMMA_MODE_A, GAMMA_MODE_MODE_8BIT); 
    return EFI_SUCCESS;
}
EFI_STATUS ConfigureTransMSAMISC()
{
    UINT64 reg = _TRANSA_MSA_MISC;
    if (controller->OutputPath.ConType == eDP)
    {
        reg = _TRANS_EDP_MSA_MISC;
    }
    controller->write32(reg, TRANS_MSA_SYNC_CLK |
                                 TRANS_MSA_8_BPC); // Sets MSA MISC FIelds for DP
    return EFI_SUCCESS;
}
EFI_STATUS ConfigureTransDDI()
{
    UINT32 port = controller->OutputPath.Port;
    DebugPrint(EFI_D_ERROR, "DDI Port: %u \n", port);
    switch (controller->OutputPath.ConType)
    {
    case HDMI:
        controller->write32(_TRANS_DDI_FUNC_CTL_A,
                            (TRANS_DDI_FUNC_ENABLE | TRANS_DDI_SELECT_PORT(port) |
                             TRANS_DDI_PHSYNC | TRANS_DDI_PVSYNC | TRANS_DDI_BPC_8 |
                             TRANS_DDI_MODE_SELECT_HDMI));
        break;
    case eDP:
        controller->write32(_TRANS_DDI_FUNC_CTL_EDP,
                            (TRANS_DDI_FUNC_ENABLE | TRANS_DDI_SELECT_PORT(port) |
                              TRANS_DDI_BPC_8 |
                             TRANS_DDI_MODE_SELECT_DP_SST | ((controller->OutputPath.LaneCount - 1) << 1) ) );
        break;
    default:
        controller->write32(_TRANS_DDI_FUNC_CTL_A,
                            (TRANS_DDI_FUNC_ENABLE | TRANS_DDI_SELECT_PORT(port) |
                             TRANS_DDI_PHSYNC | TRANS_DDI_PVSYNC | TRANS_DDI_BPC_8 |
                             TRANS_DDI_MODE_SELECT_DP_SST));
        break;
    }
    DebugPrint(EFI_D_ERROR, "REG TransDDI: %08x\n", controller->read32(_TRANS_DDI_FUNC_CTL_EDP));
    return EFI_SUCCESS;
}
EFI_STATUS EnablePipe()
{
    UINT64 reg = _PIPEACONF;
    if (controller->OutputPath.ConType == eDP)
    {
        reg = _PIPEEDPCONF;
    }
    controller->write32(reg, PIPECONF_ENABLE | PIPECONF_PROGRESSIVE |
                                 PIPECONF_GAMMA_MODE_8BIT);
    return EFI_SUCCESS;
}
EFI_STATUS EnableDDI()
{
    UINT32 port = controller->OutputPath.Port;

    /* Display WA #1143: skl,kbl,cfl */
        DebugPrint(EFI_D_ERROR, "DDI_BUF_CTL(port) = %08x\n",
               controller->read32(DDI_BUF_CTL(port)));
    UINT32 saved_port_bits =
        controller->read32(DDI_BUF_CTL(port)) &
        (DDI_BUF_PORT_REVERSAL |
         DDI_A_4_LANES | (15 <<24) ); // FOR HDMI, only port reversal and Lane count matter
    if (controller->OutputPath.ConType == HDMI)
    {
        /*
     * For some reason these chicken bits have been
     * stuffed into a transcoder register, event though
     * the bits affect a specific DDI port rather than
     * a specific transcoder.
     */

        // Workaround to get the HSWING to take effect on HDMI Ports. See
        // https://patchwork.freedesktop.org/patch/199817/
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
        // if(port==PORT_E){reg = CHICKEN_TRANS_A;}
        UINT32 val;

        val = controller->read32(reg);

        if (port == PORT_E)
            val |= DDIE_TRAINING_OVERRIDE_ENABLE | DDIE_TRAINING_OVERRIDE_VALUE;
        else
            val |= DDI_TRAINING_OVERRIDE_ENABLE | DDI_TRAINING_OVERRIDE_VALUE;

        controller->write32(reg, val);
        controller->read32(reg);

        //... don't have timer
        for (UINT32 counter = 0;;)
        {
            // controller->read32(reg);
            counter += 1;
            if (counter >= 16384)
            {
                break;
            }
        }
        // udelay(1);

        if (port == PORT_E)
            val &= ~(DDIE_TRAINING_OVERRIDE_ENABLE | DDIE_TRAINING_OVERRIDE_VALUE);
        else
            val &= ~(DDI_TRAINING_OVERRIDE_ENABLE | DDI_TRAINING_OVERRIDE_VALUE);

        controller->write32(reg, val);
    }

    /* In HDMI/DVI mode, the port width, and swing/emphasis values
   * are ignored so nothing special needs to be done besides
   * enabling the port.
   */
  DebugPrint(EFI_D_ERROR, "SAVED BTIS %08x \n", saved_port_bits);
    if (controller->OutputPath.ConType == eDP) {
         saved_port_bits |= ((controller->OutputPath.LaneCount - 1) << 1);
    }
    controller->write32(DDI_BUF_CTL(port), saved_port_bits | DDI_BUF_CTL_ENABLE);
    DebugPrint(EFI_D_ERROR, "DDI_BUF_CTL(port) = %08x\n",
               controller->read32(DDI_BUF_CTL(port)));

    return EFI_SUCCESS;
}
EFI_STATUS SetupAndEnablePlane()
{
    UINT32 horz_active =
        controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActive |
        ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION]
                      .horzActiveBlankMsb >>
                  4)
         << 8);
    UINT32 vert_active =
        controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActive |
        ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION]
                      .vertActiveBlankMsb >>
                  4)
         << 8);
    // plane
    UINT32 stride = (horz_active * 4 + 63) & -64;
    controller->stride = stride;
    controller->write32(_DSPAOFFSET, 0);
    controller->write32(_DSPAPOS, 0);
    controller->write32(_DSPASTRIDE, stride >> 6);
    controller->write32(_DSPASIZE, (horz_active - 1) | ((vert_active - 1) << 16));
/*     controller->write32(_DSPACNTR, DISPLAY_PLANE_ENABLE |
                                       PLANE_CTL_FORMAT_XRGB_8888 |
                                       PLANE_CTL_PLANE_GAMMA_DISABLE); */
    controller->write32(_DSPACNTR, 0xC4042400);

    controller->write32(_DSPASURF, controller->gmadr);
    controller->fbsize = stride * vert_active;
    // controller->write32(_DSPAADDR,0);
    // word=controller->read32(_DSPACNTR);
    // controller->write32(_DSPACNTR,(word&~PLANE_CTL_FORMAT_MASK)|DISPLAY_PLANE_ENABLE|PLANE_CTL_FORMAT_XRGB_8888);
    //|PLANE_CTL_ORDER_RGBX

    DebugPrint(EFI_D_ERROR, "i915: plane enabled, dspcntr: %08x, FbBase: %p\n",
               controller->read32(_DSPACNTR), controller->FbBase);
    return EFI_SUCCESS;
}
EFI_STATUS setOutputPath()
{
    /*  controller->OutputPath.ConType = HDMI;
    controller->OutputPath.DPLL = 1;

    controller->OutputPath.Port = PORT_B;  */
          controller->OutputPath.ConType = eDP;
    controller->OutputPath.DPLL=1;

    controller->OutputPath.Port=PORT_A;  
    return EFI_SUCCESS;
}
#define _BXT_BLC_PWM_CTL1			0xC8250
#define   BXT_BLC_PWM_ENABLE			(1 << 31)
#define   BXT_BLC_PWM_POLARITY			(1 << 29)
#define _BXT_BLC_PWM_FREQ1			0xC8254
#define _BXT_BLC_PWM_DUTY1			0xC8258

#define _BXT_BLC_PWM_CTL2			0xC8350
#define _BXT_BLC_PWM_FREQ2			0xC8354
#define _BXT_BLC_PWM_DUTY2			0xC8358
static int cnp_rawclk(i915_CONTROLLER* controller)
{
	UINT32 rawclk;
	int divider, fraction;

	if (controller->read32(SFUSE_STRAP) & SFUSE_STRAP_RAW_FREQUENCY) {
		/* 24 MHz */
		divider = 24000;
		fraction = 0;
	} else {
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
#define KHz(x) (1000 * (x))
#define DIV_ROUND_CLOSEST(x, divisor)(			\
{							\
	typeof(x) __x = x;				\
	typeof(divisor) __d = divisor;			\
	(((typeof(x))-1) > 0 ||				\
	 ((typeof(divisor))-1) > 0 || (__x) > 0) ?	\
		(((__x) + ((__d) / 2)) / (__d)) :	\
		(((__x) - ((__d) / 2)) / (__d));	\
}							\
)
static void PrintReg(UINT64 reg, const char* name){
   // DebugPrint(EFI_D_ERROR, "%a\n", name);
    DebugPrint(EFI_D_ERROR, "i915: Reg %a(%08x), val: %08x\n", name, reg, controller->read32(reg));
}
EFI_STATUS setDisplayGraphicsMode(UINT32 ModeNumber)
{
    EFI_STATUS status;
    DebugPrint(EFI_D_ERROR, "i915: set mode %u\n", ModeNumber);
    if (g_already_set > 1)
    {
        DebugPrint(EFI_D_ERROR, "i915: mode already set\n");
        goto error;
    }
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);

    controller->write32(_PIPEACONF, 0);
        controller->write32(_PIPEEDPCONF, 0);

    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);

    status = SetupClocks();
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
        DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);

        goto error;
    }
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);

    status = RETURN_ABORTED;
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);

    // DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
    //   __LINE__, status);

    status = SetupDDIBuffer();
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
        goto error;
    }
    status = RETURN_ABORTED;

    // intel_hdmi_prepare(encoder, pipe_config);
    // hdmi_reg=DDI_BUF_CTL(port)

    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d\n", __LINE__);

    // it's Type C
    // icl_enable_phy_clock_gating(dig_port);
    // Train Displayport

    if (controller->OutputPath.ConType == eDP)
    {
                DebugPrint(EFI_D_ERROR, "PP_CTL:  %08x, PP_STAT  %08x \n", controller->read32(PP_CONTROL), controller->read32(PP_STATUS));

        status = TrainDisplayPort(controller);
        DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
                   __LINE__, status);
        if (status != EFI_SUCCESS)
        {
            goto error;
        }
    }
        status = SetupClocks();

    status = RETURN_ABORTED;

    status = SetupIBoost();
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
        goto error;
    }
    status = RETURN_ABORTED;

    status = MapTranscoderDDI();
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
        goto error;
    }
    status = RETURN_ABORTED;

    // we got here

    // intel_dig_port->set_infoframes(encoder,
    //			       crtc_state->has_infoframe,
    //			       crtc_state, conn_state);

    // if (intel_crtc_has_dp_encoder(pipe_config))
    //	intel_dp_set_m_n(pipe_config, M1_N1);

    // program PIPE_A
    status = SetupTranscoderAndPipe();
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
        goto error;
    }
    status = RETURN_ABORTED;

    status = ConfigurePipeGamma();
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
        goto error;
    }
    status = RETURN_ABORTED;

    // bad setup causes hanging when enabling trans / pipe, but what is it?
    // we got here
    // ddi
    DebugPrint(EFI_D_ERROR, "i915: before DDI\n");
    status = ConfigureTransMSAMISC();
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
        goto error;
    }
    status = RETURN_ABORTED;
        status = ConfigureTransDDI();
      DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
goto error;
    }
    status = RETURN_ABORTED;
      DebugPrint(EFI_D_ERROR, "i915: after DDI\n");
       //g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
      //return EFI_UNSUPPORTED;

      //test: could be Windows hanging, it's not
      //g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
      //we failed here
      //return EFI_UNSUPPORTED;

      status = EnablePipe();
          DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
goto error;
    }
    status = RETURN_ABORTED;
      UINT32 counter = 0;
       UINT64 reg = _PIPEACONF;
    if (controller->OutputPath.ConType == eDP)
    {
        reg = _PIPEEDPCONF;
    }
      for (;;)
      {
          counter += 1;
          if (counter >= 16384)
          {
              DebugPrint(EFI_D_ERROR, "i915: failed to enable PIPE\n");
              break;
          }
          if (controller->read32(reg) & I965_PIPECONF_ACTIVE)
          {
              DebugPrint(EFI_D_ERROR, "i915: pipe enabled\n");
              break;
          }
      }
          status = EnableDDI();
              DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is%u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
goto error;
    }
    status = RETURN_ABORTED;
      
      status = SetupAndEnablePlane();
          DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
goto error;
    }
    status = RETURN_ABORTED;
     status =  i915GraphicsFramebufferConfigure(controller);
          DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
    if (status != EFI_SUCCESS)
    {
goto error;
    }
    status = RETURN_ABORTED; 
    DebugPrint(EFI_D_ERROR, "i915: progressed to line %d, status is %u\n",
               __LINE__, status);
   // controller->write32(PP_CONTROL, 0);

    UINT32 port = controller->OutputPath.Port;
        controller->write32(PP_CONTROL, 7);
   // controller->write32(PP_CONTROL, 0);
   // controller->write32(PP_CONTROL, 7);
    //controller->write32(_BXT_BLC_PWM_FREQ1, max);
  //      controller->write32(_BXT_BLC_PWM_DUTY1, max);
  //  controller->write32(0xc8254, (1875 << 15) | (1875));
   // DebugPrint(EFI_D_ERROR, "PP_CTL:  %08x, PP_STAT  %08x \n", controller->read32(PP_CONTROL), controller->read32(PP_STATUS));
  //  DebugPrint(EFI_D_ERROR, "DP_TP_CTL:  %08x\n",controller->read32(DP_TP_CTL(controller->OutputPath.Port)));
    PrintReg(PP_CONTROL, "PP_CONTROL");
        PrintReg(_BXT_BLC_PWM_FREQ1, "_BXT_BLC_PWM_FREQ1");
    PrintReg(_BXT_BLC_PWM_DUTY1, "_BXT_BLC_PWM_DUTY1");
    PrintReg(PP_STATUS, "PP_STATUS");
    PrintReg(DP_TP_CTL(controller->OutputPath.Port), "DP_TP_CTL");
    PrintReg(_PIPEEDPCONF, "_PIPEEDPCONF");
    PrintReg(_PIPEACONF, "_PIPEACONF");
    PrintReg(_DSPAOFFSET, "_DSPAOFFSET");
    PrintReg(_DSPAPOS, "_DSPAPOS");

PrintReg(_DSPASTRIDE, "_DSPASTRIDE");
    PrintReg(_DSPASIZE, "_DSPASIZE");
    PrintReg(_DSPACNTR, "_DSPACNTR");
    PrintReg(_DSPASURF, "_DSPASURF");
    PrintReg(DDI_BUF_CTL(port), "DDI_BUF_CTL");
    PrintReg(_TRANS_DDI_FUNC_CTL_EDP, "_TRANS_DDI_FUNC_CTL_EDP");
    PrintReg(_TRANS_EDP_MSA_MISC, "_TRANS_EDP_MSA_MISC");
    PrintReg(_SKL_BOTTOM_COLOR_A, "_SKL_BOTTOM_COLOR_A");
    PrintReg(_GAMMA_MODE_A, "_GAMMA_MODE_A");
    PrintReg(_LGC_PALETTE_A, "_LGC_PALETTE_A");
    PrintReg(DISPIO_CR_TX_BMU_CR0, "DISPIO_CR_TX_BMU_CR0");
    PrintReg(PP_ON, "PP_ON");
    PrintReg(PP_OFF, "PP_OFF");
    PrintReg(PP_DIVISOR, "PP_DIVISOR");
    PrintReg(DPLL_CTRL1, "DPLL_CTRL1");
    PrintReg(LCPLL2_CTL, "LCPLL2_CTL");
    PrintReg(LCPLL1_CTL, "LCPLL1_CTL");
    PrintReg(DPLL_CTRL2, "DPLL_CTRL2");
    DebugPrint(EFI_D_ERROR, "i195: Controller: LR: %u, LC: %u, Port: %u, ContType: %u, DPLL: %u\n",
        controller->OutputPath.LinkRate, controller->OutputPath.LaneCount, 
        controller->OutputPath.Port, controller->OutputPath.ConType, controller->OutputPath.DPLL);

 controller->write32(LCPLL2_CTL, controller->read32(LCPLL2_CTL) & ~(LCPLL_PLL_ENABLE));
    controller->write32(LCPLL1_CTL, controller->read32(LCPLL1_CTL) & ~(LCPLL_PLL_ENABLE));
    gBS->Stall(6000);
    controller->write32(LCPLL2_CTL, controller->read32(LCPLL2_CTL) | LCPLL_PLL_ENABLE);
    controller->write32(LCPLL1_CTL, controller->read32(LCPLL1_CTL) | LCPLL_PLL_ENABLE);
/* 			UINT32 val=controller->read32(DDI_BUF_CTL(port));
            	val &= ~(0xF << 24);

		val |= DDI_BUF_TRANS_SELECT(8);


    controller->write32(DDI_BUF_CTL(port), val); */

        g_already_set++;
  //  setDisplayGraphicsMode(ModeNumber);
    return EFI_SUCCESS;

error:
    DebugPrint(EFI_D_ERROR, "exiting with error");
    return status;
}

STATIC UINT8 edid_fallback[] = {
    // generic 1280x720
    0, 255, 255, 255, 255, 255, 255, 0, 34, 240, 84, 41, 1, 0, 0,
    0, 4, 23, 1, 4, 165, 52, 32, 120, 35, 252, 129, 164, 85, 77,
    157, 37, 18, 80, 84, 33, 8, 0, 209, 192, 129, 192, 129, 64, 129,
    128, 149, 0, 169, 64, 179, 0, 1, 1, 26, 29, 0, 128, 81, 208,
    28, 32, 64, 128, 53, 0, 77, 187, 16, 0, 0, 30, 0, 0, 0,
    254, 0, 55, 50, 48, 112, 32, 32, 32, 32, 32, 32, 32, 32, 10,
    0, 0, 0, 253, 0, 24, 60, 24, 80, 17, 0, 10, 32, 32, 32,
    32, 32, 32, 0, 0, 0, 252, 0, 72, 80, 32, 90, 82, 95, 55,
    50, 48, 112, 10, 32, 32, 0, 161
    // the test monitor
    // 0,255,255,255,255,255,255,0,6,179,192,39,141,30,0,0,49,26,1,3,128,60,34,120,42,83,165,167,86,82,156,38,17,80,84,191,239,0,209,192,179,0,149,0,129,128,129,64,129,192,113,79,1,1,2,58,128,24,113,56,45,64,88,44,69,0,86,80,33,0,0,30,0,0,0,255,0,71,67,76,77,84,74,48,48,55,56,50,49,10,0,0,0,253,0,50,75,24,83,17,0,10,32,32,32,32,32,32,0,0,0,252,0,65,83,85,83,32,86,90,50,55,57,10,32,32,1,153,2,3,34,113,79,1,2,3,17,18,19,4,20,5,14,15,29,30,31,144,35,9,23,7,131,1,0,0,101,3,12,0,32,0,140,10,208,138,32,224,45,16,16,62,150,0,86,80,33,0,0,24,1,29,0,114,81,208,30,32,110,40,85,0,86,80,33,0,0,30,1,29,0,188,82,208,30,32,184,40,85,64,86,80,33,0,0,30,140,10,208,144,32,64,49,32,12,64,85,0,86,80,33,0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,237
};


EFI_STATUS SetupPPS() {
        gBS->Stall(6000);
        UINT32 max= DIV_ROUND_CLOSEST(KHz(cnp_rawclk(controller)),
				 200);
    controller->write32(_BXT_BLC_PWM_FREQ1, max);
        controller->write32(_BXT_BLC_PWM_DUTY1, max);

   /*  controller->write32(0x00048250, 0x80000000);
    controller->write32(0x00048254, 0x00000000);
    controller->write32(0x00048350, 0x00000000);
    controller->write32(0x00048354, 0x00000000);
    controller->write32(0x00048360, 0x00000000);
    controller->write32(0x000c8250, 0x80000000);
    controller->write32(0x000c8254, 0x00005eb2); */
    UINT32 val = controller->read32(0xc2000);
    val |= 1;
    controller->write32(0xc2000, val);
//    controller->write32(0xc2000, val);
    controller->write32(0xc8250, (1 << 31) | (0 << 29));
        gBS->Stall(6000);
    intel_dp_pps_init(controller);

  //  controller->write32(0xc8250, 1 << 31);
    return EFI_SUCCESS;
}
EFI_STATUS DisplayInit(i915_CONTROLLER *iController)
{
    EFI_STATUS Status;

    controller = iController;
    setOutputPath();
    /* 1. Enable PCH reset handshake. */
    // intel_pch_reset_handshake(dev_priv, !HAS_PCH_NOP(dev_priv));
    controller->write32(HSW_NDE_RSTWRN_OPT,
                        controller->read32(HSW_NDE_RSTWRN_OPT) |
                            RESET_PCH_HANDSHAKE_ENABLE);

    // DOESN'T APPLY
    ///* 2-3. */
    // icl_combo_phys_init(dev_priv);

    // if (resume && dev_priv->csr.dmc_payload)
    //	intel_csr_load_program(dev_priv);

    // power well enable, we are requesting these to be enabled
    //#define   SKL_PW_CTL_IDX_PW_2			15
    //#define   SKL_PW_CTL_IDX_PW_1			14
    //#define   SKL_PW_CTL_IDX_DDI_D			4
    //#define   SKL_PW_CTL_IDX_DDI_C			3
    //#define   SKL_PW_CTL_IDX_DDI_B			2
    //#define   SKL_PW_CTL_IDX_DDI_A_E		1
    //#define   SKL_PW_CTL_IDX_MISC_IO		0
    controller->write32(HSW_PWR_WELL_CTL1,
                        controller->read32(HSW_PWR_WELL_CTL1) | 0xA00002AAu);
    for (UINT32 counter = 0;; counter++)
    {
        UINT32 stat = controller->read32(HSW_PWR_WELL_CTL1);
        if (counter > 16384)
        {
            DebugPrint(EFI_D_ERROR, "i915: power well enabling timed out %08x\n",
                       stat);
            break;
        }
        if (stat & 0x50000155u)
        {
            DebugPrint(EFI_D_ERROR, "i915: power well enabled %08x\n", stat);
            break;
        }
    }
    SetupPPS();
        DebugPrint(EFI_D_ERROR, "PP_CTL:  %08x, PP_STAT  %08x \n", controller->read32(PP_CONTROL), controller->read32(PP_STATUS));

    controller->write32(PP_CONTROL, 8);
    controller->write32(PP_CONTROL,0);
    controller->write32(PP_CONTROL, 8);
    controller->write32(PP_CONTROL,0);
    controller->write32(PP_CONTROL,67);
        DebugPrint(EFI_D_ERROR, "PP_CTL:  %08x, PP_STAT  %08x \n", controller->read32(PP_CONTROL), controller->read32(PP_STATUS));

    gBS->Stall(500000);
        DebugPrint(EFI_D_ERROR, "PP_CTL:  %08x, PP_STAT  %08x \n", controller->read32(PP_CONTROL), controller->read32(PP_STATUS));
    //controller->write32(PP_CONTROL, 103);
    // disable VGA
    UINT32 vgaword = controller->read32(VGACNTRL);
    controller->write32(VGACNTRL, (vgaword & ~VGA_2X_MODE) | VGA_DISP_DISABLE);
    // DebugPrint(EFI_D_ERROR,"i915: bars %08x %08x %08x
    // %08x\n",Pci.Device.Bar[0],Pci.Device.Bar[1],Pci.Device.Bar[2],Pci.Device.Bar[3]);

    ///* 5. Enable CDCLK. */
    // icl_init_cdclk(dev_priv);
    // 080002a1 on test machine
     DebugPrint(EFI_D_ERROR,"i915: CDCLK = %08x\n",controller->read32(CDCLK_CTL)); //there seems no need to do so

    ///* 6. Enable DBUF. */
    // icl_dbuf_enable(dev_priv);
    controller->write32(DBUF_CTL_S1,
                        controller->read32(DBUF_CTL_S1) | DBUF_POWER_REQUEST);
    controller->write32(DBUF_CTL_S2,
                        controller->read32(DBUF_CTL_S2) | DBUF_POWER_REQUEST);
    controller->read32(DBUF_CTL_S2);
    for (UINT32 counter = 0;; counter++)
    {
        if (counter > 16384)
        {
            DebugPrint(EFI_D_ERROR, "i915: DBUF timeout\n");
            break;
        }
        if (controller->read32(DBUF_CTL_S1) & controller->read32(DBUF_CTL_S2) &
            DBUF_POWER_STATE)
        {
            DebugPrint(EFI_D_ERROR, "i915: DBUF good\n");
            break;
        }
    }

    ///* 7. Setup MBUS. */
    // icl_mbus_init(dev_priv);
    controller->write32(MBUS_ABOX_CTL, MBUS_ABOX_BT_CREDIT_POOL1(16) |
                                           MBUS_ABOX_BT_CREDIT_POOL2(16) |
                                           MBUS_ABOX_B_CREDIT(1) |
                                           MBUS_ABOX_BW_CREDIT(1));

    // set up display buffer
    // the value is from host
    DebugPrint(EFI_D_ERROR, "i915: _PLANE_BUF_CFG_1_A = %08x\n",
               controller->read32(_PLANE_BUF_CFG_1_A));
    controller->write32(_PLANE_BUF_CFG_1_A, 0x035b0000);
    DebugPrint(EFI_D_ERROR, "i915: _PLANE_BUF_CFG_1_A = %08x (after)\n",
               controller->read32(_PLANE_BUF_CFG_1_A));

    // initialize output
    // need workaround: always initialize DDI
    // intel_dig_port->hdmi.hdmi_reg = DDI_BUF_CTL(port);
    // intel_ddi_init(PORT_A);
    UINT32 found = controller->read32(SFUSE_STRAP);
    DebugPrint(EFI_D_ERROR, "i915: SFUSE_STRAP = %08x\n", found);
    // UINT32* port = &controller->OutputPath.Port;
    /*         UINT32* port = &(controller->OutputPath.Port);

    *port = PORT_A;
    if (found & SFUSE_STRAP_DDIB_DETECTED)
    {
        *port = PORT_B; //intel_ddi_init(PORT_B);
    }
    else if (found & SFUSE_STRAP_DDIC_DETECTED)
    {
        *port = PORT_C; //intel_ddi_init(PORT_C);
    }
    else if (found & SFUSE_STRAP_DDID_DETECTED)
    {
        *port = PORT_D; //intel_ddi_init(PORT_D);
    } */
    // if (found & SFUSE_STRAP_DDIF_DETECTED)
    //	intel_ddi_init(dev_priv, PORT_F);

    // reset GMBUS
    // intel_i2c_reset(dev_priv);
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
            DebugPrint(EFI_D_ERROR, "%02x ",
                       ((UINT8 *)(&controller->edid))[i * 8 + j]);
        }
        DebugPrint(EFI_D_ERROR, "\n");
    }
    return EFI_SUCCESS;
}
