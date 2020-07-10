#include <Uefi.h>
#include "i915_gmbus.h"
#include <Library/DebugLib.h>

EFI_STATUS gmbusWait(i915_CONTROLLER *controller, UINT32 wanted) {
    UINTN counter = 0;
    for (;;) {
        UINT32 status = controller->read32(gmbusStatus);
        counter += 1;
        if (counter >= 1024) {
            //failed
            DebugPrint(EFI_D_ERROR, "i915: gmbus timeout\n");
            return EFI_DEVICE_ERROR;
        }
        if (status & GMBUS_SATOER) {
            //failed
            DebugPrint(EFI_D_ERROR, "i915: gmbus error\n");
            return EFI_DEVICE_ERROR;
        }
        if (status & wanted) {
            //worked
            return EFI_SUCCESS;
        }
    }
}