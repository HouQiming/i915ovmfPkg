#ifndef i915_OVMFH
#define i915_OVMFH
#include "i915_reg.h"
//registers are in bar 0
//frame buffer is in bar 2

#define HSW_PWR_WELL_CTL1            (0x45400)
#define HSW_PWR_WELL_CTL2            (0x45404)
#define HSW_PWR_WELL_CTL3            (0x45408)
#define HSW_PWR_WELL_CTL4            (0x4540C)


#define I915_READ read32
#define I915_WRITE write32
#endif