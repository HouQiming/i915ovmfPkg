#include <Uefi.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/DevicePath.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

EFI_STATUS EFIAPI i915ControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
	DebugPrint(EFI_D_ERROR,"i915ControllerDriverSupported\n");
	//TODO
	return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI i915ControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
	DebugPrint(EFI_D_ERROR,"i915ControllerDriverStart\n");
	//TODO
	return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI i915ControllerDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN UINTN                          NumberOfChildren,
  IN EFI_HANDLE                     *ChildHandleBuffer
  )
{
	DebugPrint(EFI_D_ERROR,"i915ControllerDriverStop\n");
	//TODO
	return EFI_UNSUPPORTED;
}

EFI_DRIVER_BINDING_PROTOCOL gi915DriverBinding = {
  i915ControllerDriverSupported,
  i915ControllerDriverStart,
  i915ControllerDriverStop,
  0x10,
  NULL,
  NULL
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mi915DriverNameTable[] = {
  { "eng;en", L"i915 Driver" },
  { NULL , NULL }
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_UNICODE_STRING_TABLE mi915ControllerNameTable[] = {
  { "eng;en", L"i915 PCI Thing" },
  { NULL , NULL }
};

GLOBAL_REMOVE_IF_UNREFERENCED extern EFI_COMPONENT_NAME_PROTOCOL  gi915ComponentName;

EFI_STATUS
EFIAPI
i915ComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mi915DriverNameTable,
           DriverName,
           (BOOLEAN)(This == &gi915ComponentName)
           );
}

EFI_STATUS
EFIAPI
i915ComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL                     *This,
  IN  EFI_HANDLE                                      ControllerHandle,
  IN  EFI_HANDLE                                      ChildHandle        OPTIONAL,
  IN  CHAR8                                           *Language,
  OUT CHAR16                                          **ControllerName
  )
{
  EFI_STATUS                      Status;

  //
  // This is a device driver, so ChildHandle must be NULL.
  //
  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  //
  // Make sure this driver is currently managing ControllHandle
  //
  Status = EfiTestManagedDevice (
             ControllerHandle,
             gi915DriverBinding.DriverBindingHandle,
             &gEfiPciIoProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get the Cirrus Logic 5430's Device structure
  //
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mi915ControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &gi915ComponentName)
           );
}

GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME_PROTOCOL  gi915ComponentName = {
  i915ComponentNameGetDriverName,
  i915ComponentNameGetControllerName,
  "eng"
};

GLOBAL_REMOVE_IF_UNREFERENCED EFI_COMPONENT_NAME2_PROTOCOL gi915ComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) i915ComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) i915ComponentNameGetControllerName,
  "en"
};

EFI_STATUS EFIAPI efi_main (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
	////////////
	DebugPrint (EFI_D_ERROR, "Driver starts!\n");
	EFI_STATUS Status;
	Status = EfiLibInstallDriverBindingComponentName2 (
	           ImageHandle,
	           SystemTable,
	           &gi915DriverBinding,
	           ImageHandle,
	           &gi915ComponentName,
	           &gi915ComponentName2
	           );
	ASSERT_EFI_ERROR (Status);
	return EFI_SUCCESS;
}
