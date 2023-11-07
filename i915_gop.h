#pragma once
#ifndef i915_GOPH
#define i915_GOPH
#include <Uefi.h>
#include "i915_display.h"
#include <Protocol/GraphicsOutput.h>
#include "i915_debug.h"
#include <Library/FrameBufferBltLib.h>
#include "i915_reg.h"
#include <Library/MemoryAllocationLib.h>

EFI_STATUS i915GraphicsFramebufferConfigure(i915_CONTROLLER *controller);

EFI_STATUS i915GraphicsSetupOutput(EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput, UINT32 x_active, UINT32 y_active);
#endif