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