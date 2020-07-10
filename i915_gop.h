#include <Uefi.h>
#include "i915_controller.h"
#include <Protocol/GraphicsOutput.h>
#include <Library/DebugLib.h>
#include <Library/FrameBufferBltLib.h>

STATIC FRAME_BUFFER_CONFIGURE        *g_i915FrameBufferBltConfigure;
STATIC UINTN                         g_i915FrameBufferBltConfigureSize;
STATIC INTN g_already_set;
EFI_STATUS i915GraphicsFrambufferConfigure(i915_CONTROLLER* controller, UINTN fbSize);