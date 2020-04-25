#include <Uefi.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverSupportedEfiVersion.h>
#include <Protocol/DevicePath.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/PcdLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

#define PCH_DISPLAY_BASE	0xc0000u

EFI_STATUS EFIAPI i915ControllerDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
	EFI_STATUS          Status;
	EFI_PCI_IO_PROTOCOL *PciIo;
	PCI_TYPE00          Pci;
	EFI_DEV_PATH        *Node;

	//
	// Open the PCI I/O Protocol
	//
	Status = gBS->OpenProtocol (
	                Controller,
	                &gEfiPciIoProtocolGuid,
	                (VOID **) &PciIo,
	                This->DriverBindingHandle,
	                Controller,
	                EFI_OPEN_PROTOCOL_BY_DRIVER
	                );
	if (EFI_ERROR (Status)) {
	  return Status;
	}

	//
	// Read the PCI Configuration Header from the PCI Device
	//
	Status = PciIo->Pci.Read (
	                      PciIo,
	                      EfiPciIoWidthUint32,
	                      0,
	                      sizeof (Pci) / sizeof (UINT32),
	                      &Pci
	                      );
	if (EFI_ERROR (Status)) {
	  goto Done;
	}

	Status = EFI_UNSUPPORTED;
	if (Pci.Hdr.VendorId == 0x8086&&IS_PCI_DISPLAY(&Pci)){
		Status = EFI_SUCCESS;
		//
		// If this is an Intel graphics controller,
		// go further check RemainingDevicePath validation
		//
		if (RemainingDevicePath != NULL) {
		  Node = (EFI_DEV_PATH *) RemainingDevicePath;
		  //
		  // Check if RemainingDevicePath is the End of Device Path Node, 
		  // if yes, return EFI_SUCCESS
		  //
		  if (!IsDevicePathEnd (Node)) {
		    //
		    // If RemainingDevicePath isn't the End of Device Path Node,
		    // check its validation
		    //
		    if (Node->DevPath.Type != ACPI_DEVICE_PATH ||
		        Node->DevPath.SubType != ACPI_ADR_DP ||
		        DevicePathNodeLength(&Node->DevPath) != sizeof(ACPI_ADR_DEVICE_PATH)) {
		      Status = EFI_UNSUPPORTED;
		    }
		  }
		}
		if(Status==EFI_SUCCESS){
			DebugPrint(EFI_D_ERROR,"i915: found device %04x-%04x %p\n",Pci.Hdr.VendorId,Pci.Hdr.DeviceId,RemainingDevicePath);
		}
	}
	
Done:
	gBS->CloseProtocol (
	      Controller,
	      &gEfiPciIoProtocolGuid,
	      This->DriverBindingHandle,
	      Controller
	      );
	return Status;
}

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION g_mode_info[] = {
  {
    0,    // Version
    1024,  // HorizontalResolution
    768,  // VerticalResolution
  }
};

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE g_mode = {
  ARRAY_SIZE (g_mode_info),                // MaxMode
  0,                                              // Mode
  g_mode_info,                             // Info
  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),  // SizeOfInfo
};

STATIC EFI_STATUS EFIAPI i915GraphicsOutputQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *ModeInfo;
  DebugPrint(EFI_D_ERROR,"i915: query mode\n");

  if (Info == NULL || SizeOfInfo == NULL ||
      ModeNumber >= g_mode.MaxMode) {
    return EFI_INVALID_PARAMETER;
  }
  ModeInfo = &g_mode_info[ModeNumber];

  *Info = AllocateCopyPool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION), ModeInfo);
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI i915GraphicsOutputSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN  UINT32                       ModeNumber
  )
{
	DebugPrint(EFI_D_ERROR,"i915: set mode %u\n",ModeNumber);
	return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI i915GraphicsOutputBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer, OPTIONAL
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION     BltOperation,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
{
	DebugPrint(EFI_D_ERROR,"i915: blt\n");
	return EFI_SUCCESS;
}

typedef struct {
  UINT64                                Signature;
  EFI_HANDLE                            Handle;
  EFI_PCI_IO_PROTOCOL                   *PciIo;
  EFI_GRAPHICS_OUTPUT_PROTOCOL          GraphicsOutput;
  EFI_DEVICE_PATH_PROTOCOL              *GopDevicePath;
} I915_VIDEO_PRIVATE_DATA;

EFI_STATUS EFIAPI i915ControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
	EFI_TPL                           OldTpl;
	EFI_STATUS                        Status;
	I915_VIDEO_PRIVATE_DATA           *Private;
	PCI_TYPE00          Pci;
	
	OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
	DebugPrint(EFI_D_ERROR,"i915: start\n");
	
	Private = AllocateZeroPool (sizeof (I915_VIDEO_PRIVATE_DATA));
	if (Private == NULL) {
	  Status = EFI_OUT_OF_RESOURCES;
	  goto RestoreTpl;
	}
	
	Private->Signature  = SIGNATURE_32('i','9','1','5');
	
	Status = gBS->OpenProtocol (
	                Controller,
	                &gEfiPciIoProtocolGuid,
	                (VOID **) &Private->PciIo,
	                This->DriverBindingHandle,
	                Controller,
	                EFI_OPEN_PROTOCOL_BY_DRIVER
	                );
	if (EFI_ERROR (Status)) {
	  goto FreePrivate;
	}
	
	Status = Private->PciIo->Pci.Read (
	                      Private->PciIo,
	                      EfiPciIoWidthUint32,
	                      0,
	                      sizeof (Pci) / sizeof (UINT32),
	                      &Pci
	                      );
	if (EFI_ERROR (Status)) {
	  goto ClosePciIo;
	}
	
	Status = Private->PciIo->Attributes (
	                          Private->PciIo,
	                          EfiPciIoAttributeOperationEnable,
	                          EFI_PCI_DEVICE_ENABLE | EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY | EFI_PCI_IO_ATTRIBUTE_VGA_IO,
	                          NULL
	                          );
	if (EFI_ERROR (Status)) {
	  goto ClosePciIo;
	}
	
	DebugPrint(EFI_D_ERROR,"i915: set pci attrs\n");
	
	//
	// Get ParentDevicePath
	//
	EFI_DEVICE_PATH_PROTOCOL          *ParentDevicePath;
	Status = gBS->HandleProtocol (
	                Controller,
	                &gEfiDevicePathProtocolGuid,
	                (VOID **) &ParentDevicePath
	                );
	if (EFI_ERROR (Status)) {
	  goto ClosePciIo;
	}

	//
	// Set Gop Device Path
	//
	ACPI_ADR_DEVICE_PATH              AcpiDeviceNode;
	ZeroMem (&AcpiDeviceNode, sizeof (ACPI_ADR_DEVICE_PATH));
	AcpiDeviceNode.Header.Type = ACPI_DEVICE_PATH;
	AcpiDeviceNode.Header.SubType = ACPI_ADR_DP;
	AcpiDeviceNode.ADR = ACPI_DISPLAY_ADR (1, 0, 0, 1, 0, ACPI_ADR_DISPLAY_TYPE_VGA, 0, 0);
	SetDevicePathNodeLength (&AcpiDeviceNode.Header, sizeof (ACPI_ADR_DEVICE_PATH));

	Private->GopDevicePath = AppendDevicePathNode (
	                                    ParentDevicePath,
	                                    (EFI_DEVICE_PATH_PROTOCOL *) &AcpiDeviceNode
	                                    );
	if (Private->GopDevicePath == NULL) {
	  Status = EFI_OUT_OF_RESOURCES;
	  goto ClosePciIo;
	}
	DebugPrint(EFI_D_ERROR,"i915: made gop path\n");
	
	//
	// Create new child handle and install the device path protocol on it.
	//
	Status = gBS->InstallMultipleProtocolInterfaces (
	                &Private->Handle,
	                &gEfiDevicePathProtocolGuid,
	                Private->GopDevicePath,
	                NULL
	                );
	if (EFI_ERROR (Status)) {
	  goto FreeGopDevicePath;
	}
	DebugPrint(EFI_D_ERROR,"i915: installed child handle\n");
	
	//
	// Start the GOP software stack.
	//
	EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;
	GraphicsOutput            = &Private->GraphicsOutput;
	GraphicsOutput->QueryMode = i915GraphicsOutputQueryMode;
	GraphicsOutput->SetMode   = i915GraphicsOutputSetMode;
	GraphicsOutput->Blt       = i915GraphicsOutputBlt;
	GraphicsOutput->Mode = &g_mode;
	
	Status = gBS->InstallMultipleProtocolInterfaces (
	                &Private->Handle,
	                &gEfiGraphicsOutputProtocolGuid,
	                &Private->GraphicsOutput,
	                NULL
	                );
	if (EFI_ERROR (Status)) {
	  goto Destructi915Graphics;
	}
	
	//
	// Reference parent handle from child handle.
	//
	EFI_PCI_IO_PROTOCOL               *ChildPciIo;
	Status = gBS->OpenProtocol (
	              Controller,
	              &gEfiPciIoProtocolGuid,
	              (VOID **) &ChildPciIo,
	              This->DriverBindingHandle,
	              Private->Handle,
	              EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
	              );
	if (EFI_ERROR (Status)) {
	  goto UninstallGop;
	}
	
	DebugPrint(EFI_D_ERROR,"i915: gop ready\n");
	
	gBS->RestoreTPL (OldTpl);
	return EFI_SUCCESS;

UninstallGop:
	gBS->UninstallProtocolInterface (Private->Handle,
           &gEfiGraphicsOutputProtocolGuid, &Private->GraphicsOutput);

Destructi915Graphics:

ClosePciIo:
	gBS->CloseProtocol (Controller, &gEfiPciIoProtocolGuid,
           This->DriverBindingHandle, Controller);

FreeGopDevicePath:
	FreePool (Private->GopDevicePath);
	
FreePrivate:
	FreePool (Private);
	
RestoreTpl:
	gBS->RestoreTPL (OldTpl);
	return Status;
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

EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL gi915SupportedEfiVersion = {
  sizeof (EFI_DRIVER_SUPPORTED_EFI_VERSION_PROTOCOL), // Size of Protocol structure.
  0                                                   // Version number to be filled at start up.
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
	
	gi915SupportedEfiVersion.FirmwareVersion = PcdGet32 (PcdDriverSupportedEfiVersion);
	Status = gBS->InstallMultipleProtocolInterfaces (
	                &ImageHandle,
	                &gEfiDriverSupportedEfiVersionProtocolGuid,
	                &gi915SupportedEfiVersion,
	                NULL
	                );
	ASSERT_EFI_ERROR (Status);
	
	return EFI_SUCCESS;
}
