#include <Uefi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
//#include <Library/FrameBufferBltLib.h>
#include <Library/MemoryAllocationLib.h>

//TODO: get the basics right before going driver, easier to debug this way

static EFI_BOOT_SERVICES* gBS=NULL;

EFI_STATUS EFIAPI efi_main (
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
  )
{
	gBS=SystemTable->BootServices;
	////////////
	DebugPrint (EFI_D_ERROR, "Driver starts!\n");
	/////
	EFI_STATUS Status;
	UINTN                             HandleBufSize;
	EFI_HANDLE                        *HandleBuf;
	UINTN                             HandleCount;
	HandleBufSize = sizeof (EFI_HANDLE);
	HandleBuf     = (EFI_HANDLE *) AllocateZeroPool (HandleBufSize);
	if (HandleBuf == NULL) {
	  DebugPrint (EFI_D_ERROR, "Failed to allocate HandleBuf!\n");
	  goto Done;
	}

	Status = gBS->LocateHandle (
	              ByProtocol,
	              &gEfiPciIoProtocolGuid,
	              NULL,
	              &HandleBufSize,
	              HandleBuf
	             );

	if (Status == EFI_BUFFER_TOO_SMALL) {
	  HandleBuf = ReallocatePool (sizeof (EFI_HANDLE), HandleBufSize, HandleBuf);
	  if (HandleBuf == NULL) {
	    DebugPrint (EFI_D_ERROR, "Failed to allocate HandleBuf 2!\n");
	    goto Done;
	  }

	  Status = gBS->LocateHandle (
	                ByProtocol,
	                &gEfiPciIoProtocolGuid,
	                NULL,
	                &HandleBufSize,
	                HandleBuf
	               );
	}
	HandleCount=HandleBufSize/sizeof(EFI_HANDLE);
	DebugPrint (EFI_D_ERROR, "Found %d handles\n",HandleCount);
Done: /////
	DebugPrint (EFI_D_ERROR, "Driver ends!\n");
	////////////
	SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
	return EFI_SUCCESS;
}
