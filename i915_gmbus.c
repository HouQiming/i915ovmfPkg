#include <Uefi.h>
#include "i915_gmbus.h"
#include "i915_debug.h"

EFI_STATUS gmbusWait(i915_CONTROLLER *controller, UINT32 wanted)
{
    UINTN counter = 0;

    for (;;)
    {
        UINT32 status = controller->read32(gmbusStatus);
        counter += 1;
        if (counter >= 1024)
        {
            //failed
            PRINT_DEBUG(EFI_D_ERROR, "gmbus timeout\n");
            return EFI_DEVICE_ERROR;
        }
        if (status & GMBUS_SATOER)
        {
            //failed
            PRINT_DEBUG(EFI_D_ERROR, "gmbus error on %d\n", wanted);
            return EFI_DEVICE_ERROR;
        }
        if (status & wanted)
        {
            //worked
            return EFI_SUCCESS;
        }
    }
}