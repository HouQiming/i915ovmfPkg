#include "i915_reg.h"
#include "i915_controller.h"

#define GMBUS0 (PCH_DISPLAY_BASE+0x5100)
#define gmbusSelect (PCH_DISPLAY_BASE+0x5100)
#define   GMBUS_AKSV_SELECT    (1 << 11)
#define   GMBUS_RATE_100KHZ    (0 << 8)
#define   GMBUS_RATE_50KHZ    (1 << 8)
#define   GMBUS_RATE_400KHZ    (2 << 8) /* reserved on Pineview */
#define   GMBUS_RATE_1MHZ    (3 << 8) /* reserved on Pineview */
#define   GMBUS_HOLD_EXT    (1 << 7) /* 300ns hold time, rsvd on Pineview */
#define   GMBUS_BYTE_CNT_OVERRIDE (1 << 6)
#define   GMBUS_PIN_DISABLED    0
#define   GMBUS_PIN_SSC        1
#define   GMBUS_PIN_VGADDC    2
#define   GMBUS_PIN_PANEL    3
#define   GMBUS_PIN_DPD_CHV    3 /* HDMID_CHV */
#define   GMBUS_PIN_DPC        4 /* HDMIC */
#define   GMBUS_PIN_DPB        5 /* SDVO, HDMIB */
#define   GMBUS_PIN_DPD        6 /* HDMID */
#define   GMBUS_PIN_RESERVED    7 /* 7 reserved */
#define   GMBUS_PIN_1_BXT    1 /* BXT+ (atom) and CNP+ (big core) */
#define   GMBUS_PIN_2_BXT    2
#define   GMBUS_PIN_3_BXT    3
#define   GMBUS_PIN_4_CNP    4
#define   GMBUS_PIN_9_TC1_ICP    9
#define   GMBUS_PIN_10_TC2_ICP    10
#define   GMBUS_PIN_11_TC3_ICP    11
#define   GMBUS_PIN_12_TC4_ICP    12

#define gmbusCommand (PCH_DISPLAY_BASE+0x5104)
#define   GMBUS_SW_CLR_INT    (1 << 31)
#define   GMBUS_SW_RDY        (1 << 30)
#define   GMBUS_ENT        (1 << 29) /* enable timeout */
#define   GMBUS_CYCLE_NONE    (0 << 25)
#define   GMBUS_CYCLE_WAIT    (1 << 25)
#define   GMBUS_CYCLE_INDEX    (2 << 25)
#define   GMBUS_CYCLE_STOP    (4 << 25)
#define   GMBUS_BYTE_COUNT_SHIFT 16
#define   GMBUS_BYTE_COUNT_MAX   256U
#define   GEN9_GMBUS_BYTE_COUNT_MAX 511U
#define   GMBUS_SLAVE_INDEX_SHIFT 8
#define   GMBUS_SLAVE_ADDR_SHIFT 1
#define   GMBUS_SLAVE_READ    (1 << 0)
#define   GMBUS_SLAVE_WRITE    (0 << 0)

#define gmbusStatus (PCH_DISPLAY_BASE+0x5108)
#define   GMBUS_INUSE        (1 << 15)
#define   GMBUS_HW_WAIT_PHASE    (1 << 14)
#define   GMBUS_STALL_TIMEOUT    (1 << 13)
#define   GMBUS_INT        (1 << 12)
#define   GMBUS_HW_RDY        (1 << 11)
#define   GMBUS_SATOER        (1 << 10)
#define   GMBUS_ACTIVE        (1 << 9)

#define gmbusData (PCH_DISPLAY_BASE+0x510C)
#define GMBUS4 (PCH_DISPLAY_BASE+0x5110)

EFI_STATUS gmbusWait(i915_CONTROLLER *, UINT32);