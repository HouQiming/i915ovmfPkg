#include "i915_controller.h"
#include <Library/DebugLib.h>
#include "i915_gmbus.h"
#include "i915_ddi.h"
#include "i915_dp.h"
#include "i915_hdmi.h"
#include "i915_reg.h"
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_STATUS SetupClockeDP(i915_CONTROLLER* controller) {
    
    UINT32 ctrl1;
    
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

/* Display Port */
#define DP_A			_MMIO(0x64000) /* eDP */
#define DP_B			_MMIO(0x64100)
#define DP_C			_MMIO(0x64200)
#define DP_D			_MMIO(0x64300)


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
/**
 * struct drm_dp_aux_msg - DisplayPort AUX channel transaction
 * @address: address of the (first) register to access
 * @request: contains the type of transaction (see DP_AUX_* macros)
 * @reply: upon completion, contains the reply type of the transaction
 * @buffer: pointer to a transmission or reception buffer
 * @size: size of @buffer
 */
struct drm_dp_aux_msg {
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


static INT32 intel_dp_aux_transfer(struct drm_dp_aux_msg *msg) {

    return 0;
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
			      unsigned int offset, void *buffer, UINT32 size)
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
	for (retry = 0; retry < 32; retry++) {
		if (ret != 0 && ret != -RETURN_TIMEOUT) {
			gBS->Stall(AUX_RETRY_INTERVAL);
		}

		ret = intel_dp_aux_transfer(&msg);
		if (ret >= 0) {
			native_reply = msg.reply & DP_AUX_NATIVE_REPLY_MASK;
			if (native_reply == DP_AUX_NATIVE_REPLY_ACK) {
				if (ret == size)
					goto unlock;

				ret = -RETURN_PROTOCOL_ERROR;
			} else
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

	DebugPrint(EFI_D_ERROR, "Too many retries, giving up. First error: %d\n", err);
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
INT32 drm_dp_dpcd_read( unsigned int offset,
			 void *buffer, UINT32 size)
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
					 buffer, 1);
		if (ret != 1)
			goto out;
//	}

//	if (aux->is_remote)
//		ret = drm_dp_mst_dpcd_read(aux, offset, buffer, size);
//	else
		ret = drm_dp_dpcd_access(DP_AUX_NATIVE_READ, offset,
					 buffer, size);

out:
	return ret;
}
BOOLEAN
intel_dp_get_link_status(UINT8 link_status[DP_LINK_STATUS_SIZE])
{
	return drm_dp_dpcd_read(DP_LANE0_1_STATUS, link_status,
				DP_LINK_STATUS_SIZE) == DP_LINK_STATUS_SIZE;
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

	for (lane = 0; lane < lane_count; lane++) {
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
	int s = ((lane & 1) ?
		 DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT :
		 DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT);
	UINT8 l = dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_VOLTAGE_SWING_SHIFT;
}

UINT8 drm_dp_get_adjust_request_pre_emphasis(const UINT8 link_status[DP_LINK_STATUS_SIZE],
					  int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT :
		 DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT);
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
intel_dp_pre_emphasis_max( UINT8 voltage_swing)
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
		switch (voltage_swing & DP_TRAIN_VOLTAGE_SWING_MASK) {
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
struct intel_dp {
    UINT8 lane_count;
	UINT8 train_set[4];
    int link_rate;
    i915_CONTROLLER* controller;
	
};

void intel_dp_get_adjust_train(struct intel_dp *intel_dp,
			       const UINT8 link_status[DP_LINK_STATUS_SIZE])
{
	UINT8 v = 0;
	UINT8 p = 0;
	int lane;
	UINT8 voltage_max;
	UINT8 preemph_max;

	for (lane = 0; lane < intel_dp->lane_count; lane++) {
		UINT8 this_v = drm_dp_get_adjust_request_voltage(link_status, lane);
		UINT8 this_p = drm_dp_get_adjust_request_pre_emphasis(link_status, lane);

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

	for (lane = 0; lane < 4; lane++)
		intel_dp->train_set[lane] = v | p;
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
INT32 drm_dp_dpcd_write( unsigned int offset,
			  void *buffer, UINT32 size)
{
	int ret;

	//if (aux->is_remote)
	//	ret = drm_dp_mst_dpcd_write(aux, offset, buffer, size);
	//else
		ret = drm_dp_dpcd_access(DP_AUX_NATIVE_WRITE, offset,
					 buffer, size);

	return ret;
}
static void intel_dp_set_signal_levels(struct intel_dp *intel_dp) {
    //Write to Appropraite DDI_BUF_CTL
}

static BOOLEAN
intel_dp_update_link_train(struct intel_dp *intel_dp)
{
	int ret;

	intel_dp_set_signal_levels(intel_dp);

	ret = drm_dp_dpcd_write(DP_TRAINING_LANE0_SET,
				intel_dp->train_set, intel_dp->lane_count);

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
#define   DP_LINK_TRAIN_PAT_1_CPT	(0 << 8)
#define   DP_LINK_TRAIN_PAT_2_CPT	(1 << 8)
#define   DP_LINK_TRAIN_PAT_IDLE_CPT	(2 << 8)
#define   DP_LINK_TRAIN_OFF_CPT		(3 << 8)
#define   DP_LINK_TRAIN_MASK_CPT	(7 << 8)
#define   DP_LINK_TRAIN_SHIFT_CPT	8
static void
g4x_set_link_train(struct intel_dp *intel_dp,
		   UINT8 dp_train_pat)
{
	UINT32 DP = intel_dp->controller->read32(DP_TP_CTL(intel_dp->controller->OutputPath.Port));

DP &= ~DP_LINK_TRAIN_MASK_CPT;

	switch (dp_train_pat & DP_TRAINING_PATTERN_MASK) {
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

void
intel_dp_program_link_training_pattern(struct intel_dp *intel_dp,
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

	intel_dp_program_link_training_pattern(intel_dp, dp_train_pat);

	buf[0] = dp_train_pat;
	if ((dp_train_pat & DP_TRAINING_PATTERN_MASK) ==
	    DP_TRAINING_PATTERN_DISABLE) {
		/* don't write DP_TRAINING_LANEx_SET on disable */
		len = 1;
	} else {
        for (int i=0; i<intel_dp->lane_count; i++) {
            buf[i+1] = intel_dp->train_set[i];
        }
		/* DP_TRAINING_LANEx_SET follow DP_TRAINING_PATTERN_SET */
		//memcpy(buf + 1, intel_dp->train_set, intel_dp->lane_count);
		len = intel_dp->lane_count + 1;
	}

	ret = drm_dp_dpcd_write( DP_TRAINING_PATTERN_SET,
				buf, len);

	return ret == len;
}

static BOOLEAN
intel_dp_reset_link_train(struct intel_dp *intel_dp,
			UINT8 dp_train_pat)
{
	//memset(intel_dp->train_set, 0, sizeof(intel_dp->train_set));
	//intel_dp_set_signal_levels(intel_dp);
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
/* Enable corresponding port and start training pattern 1 */
static BOOLEAN
intel_dp_link_training_clock_recovery(struct intel_dp *intel_dp)
{
	//struct drm_i915_private *i915 = dp_to_i915(intel_dp);
	UINT8 voltage;
	int voltage_tries, cr_tries, max_cr_tries;
	BOOLEAN max_vswing_reached = FALSE;
	UINT8 link_config[2];
	UINT8 link_bw, rate_select;

	/* if (intel_dp->prepare_link_retrain)
		intel_dp->prepare_link_retrain(intel_dp);
 */
	intel_dp_compute_rate(intel_dp, intel_dp->link_rate,
			      &link_bw, &rate_select); //WHAT RATE IS PLUGGED IN?

	if (link_bw)
		DebugPrint(EFI_D_ERROR, "Using LINK_BW_SET value %02x\n", link_bw);
	else
		DebugPrint(EFI_D_ERROR, "Using LINK_RATE_SET value %02x\n", rate_select);

	/* Write the link configuration data */
	link_config[0] = link_bw;
	link_config[1] = intel_dp->lane_count;
/* 	if (drm_dp_enhanced_frame_cap(intel_dp->dpcd))
		link_config[1] |= DP_LANE_COUNT_ENHANCED_FRAME_EN; */
	drm_dp_dpcd_write( DP_LINK_BW_SET, link_config, 2);

	/* eDP 1.4 rate select method. */
	if (!link_bw)
		drm_dp_dpcd_write( DP_LINK_RATE_SET,
				  &rate_select, 1);

	link_config[0] = 0;
	link_config[1] = DP_SET_ANSI_8B10B;
	drm_dp_dpcd_write( DP_DOWNSPREAD_CTRL, link_config, 2);

	//intel_dp->DP |= DP_PORT_EN;

	/* clock recovery */
	if (!intel_dp_reset_link_train(intel_dp,
				       DP_TRAINING_PATTERN_1 |
				       DP_LINK_SCRAMBLING_DISABLE)) {
		DebugPrint(EFI_D_ERROR, "failed to enable link training\n");
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
	for (cr_tries = 0; cr_tries < max_cr_tries; ++cr_tries) {
		UINT8 link_status[DP_LINK_STATUS_SIZE];
        gBS->Stall(600);
		//drm_dp_link_train_clock_recovery_delay(intel_dp->dpcd);

		if (!intel_dp_get_link_status( link_status)) {
			DebugPrint(EFI_D_ERROR,  "failed to get link status\n");
			return FALSE;
		}

		if (drm_dp_clock_recovery_ok(link_status, intel_dp->lane_count)) {
			DebugPrint(EFI_D_ERROR,  "clock recovery OK\n");
			return TRUE;
		}

		if (voltage_tries == 5) {
			DebugPrint(EFI_D_ERROR, 
				    "Same voltage tried 5 times\n");
			return FALSE;
		}

		if (max_vswing_reached) {
			DebugPrint(EFI_D_ERROR,  "Max Voltage Swing reached\n");
			return FALSE;
		}

		voltage = intel_dp->train_set[0] & DP_TRAIN_VOLTAGE_SWING_MASK;

		/* Update training set as requested by target */
		intel_dp_get_adjust_train(intel_dp, link_status);
		if (!intel_dp_update_link_train(intel_dp)) {
			DebugPrint(EFI_D_ERROR, 
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
	DebugPrint(EFI_D_ERROR, 
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
		return FALSE;
	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS)
			return FALSE;
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
				     training_pattern)) {
		//drm_err(&i915->drm, "failed to start channel equalization\n");
		return FALSE;
	}

	for (tries = 0; tries < 5; tries++) {
        gBS->Stall(600);
		//drm_dp_link_train_channel_eq_delay(intel_dp->dpcd);
		if (!intel_dp_get_link_status(link_status)) {
			//drm_err(&i915->drm,
			//	"failed to get link status\n");
			break;
		}

		/* Make sure clock is still ok */
		if (!drm_dp_clock_recovery_ok(link_status,
					      intel_dp->lane_count)) {
			//intel_dp_dump_link_status(link_status);
			/* drm_dbg_kms(&i915->drm,
				    "Clock recovery check failed, cannot "
				    "continue channel equalization\n"); */
			break;
		}

		if (drm_dp_channel_eq_ok(link_status,
					 intel_dp->lane_count)) {
			channel_eq = TRUE;
			/* drm_dbg_kms(&i915->drm, "Channel EQ done. DP Training "
				    "successful\n"); */
			break;
		}

		/* Update training set as requested by target */
		intel_dp_get_adjust_train(intel_dp, link_status);
		if (!intel_dp_update_link_train(intel_dp)) {
			/* drm_err(&i915->drm,
				"failed to update link training\n"); */
			break;
		}
	}

	/* Try 5 times, else fail and try at lower BW */
	if (tries == 5) {
		//intel_dp_dump_link_status(link_status);
		/* drm_dbg_kms(&i915->drm,
			    "Channel equalization failed 5 times\n"); */
	}

	UINT32 DP = intel_dp->controller->read32(DP_TP_CTL(intel_dp->controller->OutputPath.Port));

        DP &= ~DP_LINK_TRAIN_MASK_CPT;

	
		DP |= DP_TP_CTL_LINK_TRAIN_IDLE;
	
    intel_dp->controller->write32(DP_TP_CTL(intel_dp->controller->OutputPath.Port), DP);	
    return channel_eq;

}
EFI_STATUS TrainDisplayPort(i915_CONTROLLER* controller) {
    UINT32 port = controller->OutputPath.Port;
    UINT32 val = 0;

    val |= DP_TP_CTL_ENABLE;
    val |= DP_TP_CTL_MODE_SST;
    val |= DP_TP_CTL_LINK_TRAIN_PAT1;
    controller->write32(DP_TP_CTL(port), val);
                    val = DDI_BUF_CTL_ENABLE;

    val |= DDI_BUF_TRANS_SELECT(0);
    val |= DDI_A_4_LANES;
    val |= DDI_PORT_WIDTH(4);
    controller->write32(DDI_BUF_CTL(port), val);

     for (UINT32 counter = 0;;)
        {
            //controller->read32(reg);
            counter += 1;
            if (counter >= 16384)
            {
                break;
            }
        }
    struct intel_dp intel_dp;
    intel_dp.controller =controller;
    intel_dp.lane_count = 4;
    intel_dp_link_training_clock_recovery(&intel_dp);
    intel_dp_link_training_channel_equalization(&intel_dp);
    intel_dp_set_link_train(&intel_dp,
				DP_TRAINING_PATTERN_DISABLE);
                	UINT32 DP = controller->read32(DP_TP_CTL(port));

        DP &= ~DP_LINK_TRAIN_MASK_CPT;

	
		DP |= DP_TP_CTL_LINK_TRAIN_NORMAL;
	
    controller->write32(DP_TP_CTL(port), DP);	
    return EFI_SUCCESS;
}