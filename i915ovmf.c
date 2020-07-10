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
#include "QemuFwCfgLib.h"
#include "i915ovmf.h"
#include "i915_display.h"
#include "i915_gop.h"
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Acpi.h>

i915_CONTROLLER g_private={SIGNATURE_32('i','9','1','5')};

static void write32(UINT64 reg, UINT32 data){
	g_private.PciIo->Mem.Write (
		g_private.PciIo,
		EfiPciIoWidthFillUint32,
		PCI_BAR_IDX0,
		reg,
		1,
		&data
	);
}

static UINT32 read32(UINT64 reg){
	UINT32 data=0;
	g_private.PciIo->Mem.Read (
		g_private.PciIo,
		EfiPciIoWidthFillUint32,
		PCI_BAR_IDX0,
		reg,
		1,
		&data
	);
	return data;
}

static UINT64 read64(UINT64 reg){
	UINT64 data=0;
	g_private.PciIo->Mem.Read (
		g_private.PciIo,
		EfiPciIoWidthFillUint64,
		PCI_BAR_IDX0,
		reg,
		1,
		&data
	);
	return data;
}

//
// selector and size of ASSIGNED_IGD_FW_CFG_OPREGION
//
STATIC FIRMWARE_CONFIG_ITEM mOpRegionItem;
STATIC UINTN                mOpRegionSize;
//
// value read from ASSIGNED_IGD_FW_CFG_BDSM_SIZE, converted to UINTN
//
STATIC UINTN                mBdsmSize;

/**
  Allocate memory in the 32-bit address space, with the requested UEFI memory
  type and the requested alignment.

  @param[in] MemoryType        Assign MemoryType to the allocated pages as
                               memory type.

  @param[in] NumberOfPages     The number of pages to allocate.

  @param[in] AlignmentInPages  On output, Address will be a whole multiple of
                               EFI_PAGES_TO_SIZE (AlignmentInPages).
                               AlignmentInPages must be a power of two.

  @param[out] Address          Base address of the allocated area.

  @retval EFI_SUCCESS            Allocation successful.

  @retval EFI_INVALID_PARAMETER  AlignmentInPages is not a power of two (a
                                 special case of which is when AlignmentInPages
                                 is zero).

  @retval EFI_OUT_OF_RESOURCES   Integer overflow detected.

  @return                        Error codes from gBS->AllocatePages().
**/
STATIC
EFI_STATUS
Allocate32BitAlignedPagesWithType (
  IN  EFI_MEMORY_TYPE      MemoryType,
  IN  UINTN                NumberOfPages,
  IN  UINTN                AlignmentInPages,
  OUT EFI_PHYSICAL_ADDRESS *Address
  )
{
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS PageAlignedAddress;
  EFI_PHYSICAL_ADDRESS FullyAlignedAddress;
  UINTN                BottomPages;
  UINTN                TopPages;

  //
  // AlignmentInPages must be a power of two.
  //
  if (AlignmentInPages == 0 ||
      (AlignmentInPages & (AlignmentInPages - 1)) != 0) {
    return EFI_INVALID_PARAMETER;
  }
  //
  // (NumberOfPages + (AlignmentInPages - 1)) must not overflow UINTN.
  //
  if (AlignmentInPages - 1 > MAX_UINTN - NumberOfPages) {
    return EFI_OUT_OF_RESOURCES;
  }
  //
  // EFI_PAGES_TO_SIZE (AlignmentInPages) must not overflow UINTN.
  //
  if (AlignmentInPages > (MAX_UINTN >> EFI_PAGE_SHIFT)) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Allocate with sufficient padding for alignment.
  //
  PageAlignedAddress = BASE_4GB - 1;
  //PageAlignedAddress = BASE_2GB - 1;
  Status = gBS->AllocatePages (
                  AllocateMaxAddress,
                  MemoryType,
                  NumberOfPages + (AlignmentInPages - 1),
                  &PageAlignedAddress
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  FullyAlignedAddress = ALIGN_VALUE (
                          PageAlignedAddress,
                          (UINT64)EFI_PAGES_TO_SIZE (AlignmentInPages)
                          );

  //
  // Release bottom and/or top padding.
  //
  BottomPages = EFI_SIZE_TO_PAGES (
                  (UINTN)(FullyAlignedAddress - PageAlignedAddress)
                  );
  TopPages = (AlignmentInPages - 1) - BottomPages;
  if (BottomPages > 0) {
    Status = gBS->FreePages (PageAlignedAddress, BottomPages);
    ASSERT_EFI_ERROR (Status);
  }
  if (TopPages > 0) {
    Status = gBS->FreePages (
                    FullyAlignedAddress + EFI_PAGES_TO_SIZE (NumberOfPages),
                    TopPages
                    );
    ASSERT_EFI_ERROR (Status);
  }

  *Address = FullyAlignedAddress;
  return EFI_SUCCESS;
}

//CHAR8 OPREGION_SIGNATURE[]="IntelGraphicsMem";

typedef struct {
  UINT16 VendorId;
  UINT8  ClassCode[3];
  UINTN  Segment;
  UINTN  Bus;
  UINTN  Device;
  UINTN  Function;
  CHAR8  Name[sizeof "0000:00:02.0"];
} CANDIDATE_PCI_INFO;

STATIC CHAR8* GetPciName (IN CANDIDATE_PCI_INFO *PciInfo) {
	return PciInfo->Name;

}

/**
  Populate the CANDIDATE_PCI_INFO structure for a PciIo protocol instance.

  @param[in] PciIo     EFI_PCI_IO_PROTOCOL instance to interrogate.

  @param[out] PciInfo  CANDIDATE_PCI_INFO structure to fill.

  @retval EFI_SUCCESS  PciInfo has been filled in. PciInfo->Name has been set
                       to the empty string.

  @return              Error codes from PciIo->Pci.Read() and
                       PciIo->GetLocation(). The contents of PciInfo are
                       indeterminate.
**/
STATIC
EFI_STATUS
InitPciInfo (
  IN  EFI_PCI_IO_PROTOCOL *PciIo,
  OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  EFI_STATUS Status;

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint16,
                        PCI_VENDOR_ID_OFFSET,
                        1,                    // Count
                        &PciInfo->VendorId
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint8,
                        PCI_CLASSCODE_OFFSET,
                        sizeof PciInfo->ClassCode,
                        PciInfo->ClassCode
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PciIo->GetLocation (
                    PciIo,
                    &PciInfo->Segment,
                    &PciInfo->Bus,
                    &PciInfo->Device,
                    &PciInfo->Function
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PciInfo->Name[0] = '\0';
  return EFI_SUCCESS;
}

#define ASSIGNED_IGD_FW_CFG_OPREGION  "etc/igd-opregion"
#define ASSIGNED_IGD_FW_CFG_BDSM_SIZE "etc/igd-bdsm-size"

//
// Alignment constants. UEFI page allocation automatically satisfies the
// requirements for the OpRegion, thus we only need to define an alignment
// constant for IGD stolen memory.
//
#define ASSIGNED_IGD_BDSM_ALIGN SIZE_1MB

//
// PCI config space registers. The naming follows the PCI_*_OFFSET pattern seen
// in MdePkg/Include/IndustryStandard/Pci*.h.
//
#define ASSIGNED_IGD_PCI_BDSM_OFFSET 0x5C
#define ASSIGNED_IGD_PCI_ASLS_OFFSET 0xFC

//
// PCI location and vendor
//
#define ASSIGNED_IGD_PCI_BUS       0x00
#define ASSIGNED_IGD_PCI_DEVICE    0x02
#define ASSIGNED_IGD_PCI_FUNCTION  0x0
#define ASSIGNED_IGD_PCI_VENDOR_ID 0x8086

/**
  Set up the OpRegion for the device identified by PciIo.

  @param[in] PciIo        The device to set up the OpRegion for.

  @param[in,out] PciInfo  On input, PciInfo must have been initialized from
                          PciIo with InitPciInfo(). SetupOpRegion() may call
                          GetPciName() on PciInfo, possibly modifying it.

  @retval EFI_SUCCESS            OpRegion setup successful.

  @retval EFI_INVALID_PARAMETER  mOpRegionSize is zero.

  @return                        Error codes propagated from underlying
                                 functions.
**/
STATIC
EFI_STATUS
SetupOpRegion (
  IN     EFI_PCI_IO_PROTOCOL *PciIo,
  IN OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  UINTN                OpRegionPages;
  UINTN                OpRegionResidual;
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS Address;
  UINT8                *BytePointer;

  if (mOpRegionSize == 0) {
    return EFI_INVALID_PARAMETER;
  }
  OpRegionPages = EFI_SIZE_TO_PAGES (mOpRegionSize<8192?8192:mOpRegionSize);
  OpRegionResidual = EFI_PAGES_TO_SIZE (OpRegionPages) - mOpRegionSize;

  //
  // While QEMU's "docs/igd-assign.txt" specifies reserved memory, Intel's IGD
  // OpRegion spec refers to ACPI NVS.
  //
  Status = Allocate32BitAlignedPagesWithType (
             EfiACPIMemoryNVS,
             OpRegionPages,
             1,                // AlignmentInPages
             &Address
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to allocate OpRegion: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    return Status;
  }

  //
  // Download OpRegion contents from fw_cfg, zero out trailing portion.
  //
  BytePointer = (UINT8 *)(UINTN)Address;
  QemuFwCfgSelectItem (mOpRegionItem);
  QemuFwCfgReadBytes (mOpRegionSize, BytePointer);
  if(OpRegionResidual){
      ZeroMem (BytePointer + mOpRegionSize, OpRegionResidual);
  }
  
  //for(int i=0;i<sizeof(OPREGION_SIGNATURE);i++){
  //    BytePointer[i]=(UINT8)OPREGION_SIGNATURE[i];
  //}
  //BytePointer[0x43f]=0x20;
  
  //
  // Write address of OpRegion to PCI config space.
  //
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        ASSIGNED_IGD_PCI_ASLS_OFFSET,
                        1,                            // Count
                        &Address
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to write OpRegion address: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    goto FreeOpRegion;
  }

  DebugPrint(EFI_D_ERROR, "i915: %a: OpRegion @ 0x%Lx size 0x%Lx in %d pages\n", __FUNCTION__,
    Address, (UINT64)mOpRegionSize,(int)OpRegionPages);
  return EFI_SUCCESS;

FreeOpRegion:
  gBS->FreePages (Address, OpRegionPages);
  return Status;
}


/**
  Set up stolen memory for the device identified by PciIo.

  @param[in] PciIo        The device to set up stolen memory for.

  @param[in,out] PciInfo  On input, PciInfo must have been initialized from
                          PciIo with InitPciInfo(). SetupStolenMemory() may
                          call GetPciName() on PciInfo, possibly modifying it.

  @retval EFI_SUCCESS            Stolen memory setup successful.

  @retval EFI_INVALID_PARAMETER  mBdsmSize is zero.

  @return                        Error codes propagated from underlying
                                 functions.
**/
STATIC
EFI_STATUS
SetupStolenMemory (
  IN     EFI_PCI_IO_PROTOCOL *PciIo,
  IN OUT CANDIDATE_PCI_INFO  *PciInfo
  )
{
  UINTN                BdsmPages;
  EFI_STATUS           Status;
  EFI_PHYSICAL_ADDRESS Address;

  if (mBdsmSize == 0) {
    return EFI_INVALID_PARAMETER;
  }
  BdsmPages = EFI_SIZE_TO_PAGES (mBdsmSize);

  Status = Allocate32BitAlignedPagesWithType (
             EfiReservedMemoryType,//
             BdsmPages,
             EFI_SIZE_TO_PAGES ((UINTN)ASSIGNED_IGD_BDSM_ALIGN),
             &Address
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to allocate stolen memory: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    return Status;
  }

  //
  // Zero out stolen memory.
  //
  ZeroMem ((VOID *)(UINTN)Address, EFI_PAGES_TO_SIZE (BdsmPages));

  //
  // Write address of stolen memory to PCI config space.
  //
  Status = PciIo->Pci.Write (
                        PciIo,
                        EfiPciIoWidthUint32,
                        ASSIGNED_IGD_PCI_BDSM_OFFSET,
                        1,                            // Count
                        &Address
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %a: failed to write stolen memory address: %r\n",
      __FUNCTION__, GetPciName (PciInfo), Status));
    goto FreeStolenMemory;
  }

  DEBUG ((DEBUG_INFO, "%a: %a: stolen memory @ 0x%Lx size 0x%Lx\n",
    __FUNCTION__, GetPciName (PciInfo), Address, (UINT64)mBdsmSize));
  return EFI_SUCCESS;

FreeStolenMemory:
  gBS->FreePages (Address, BdsmPages);
  return Status;
}

STATIC UINT8 edid_fallback[]={
	//generic 1280x720
	0,255,255,255,255,255,255,0,34,240,84,41,1,0,0,0,4,23,1,4,165,52,32,120,35,252,129,164,85,77,157,37,18,80,84,33,8,0,209,192,129,192,129,64,129,128,149,0,169,64,179,0,1,1,26,29,0,128,81,208,28,32,64,128,53,0,77,187,16,0,0,30,0,0,0,254,0,55,50,48,112,32,32,32,32,32,32,32,32,10,0,0,0,253,0,24,60,24,80,17,0,10,32,32,32,32,32,32,0,0,0,252,0,72,80,32,90,82,95,55,50,48,112,10,32,32,0,161
	//the test monitor
	//0,255,255,255,255,255,255,0,6,179,192,39,141,30,0,0,49,26,1,3,128,60,34,120,42,83,165,167,86,82,156,38,17,80,84,191,239,0,209,192,179,0,149,0,129,128,129,64,129,192,113,79,1,1,2,58,128,24,113,56,45,64,88,44,69,0,86,80,33,0,0,30,0,0,0,255,0,71,67,76,77,84,74,48,48,55,56,50,49,10,0,0,0,253,0,50,75,24,83,17,0,10,32,32,32,32,32,32,0,0,0,252,0,65,83,85,83,32,86,90,50,55,57,10,32,32,1,153,2,3,34,113,79,1,2,3,17,18,19,4,20,5,14,15,29,30,31,144,35,9,23,7,131,1,0,0,101,3,12,0,32,0,140,10,208,138,32,224,45,16,16,62,150,0,86,80,33,0,0,24,1,29,0,114,81,208,30,32,110,40,85,0,86,80,33,0,0,30,1,29,0,188,82,208,30,32,184,40,85,64,86,80,33,0,0,30,140,10,208,144,32,64,49,32,12,64,85,0,86,80,33,0,0,24,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,237
};

STATIC EFI_STATUS SetupFwcfgStuff(EFI_PCI_IO_PROTOCOL *PciIo){
	EFI_STATUS OpRegionStatus = QemuFwCfgFindFile (
	                   ASSIGNED_IGD_FW_CFG_OPREGION,
	                   &mOpRegionItem,
	                   &mOpRegionSize
	                   );
	FIRMWARE_CONFIG_ITEM BdsmItem;
	UINTN                BdsmItemSize;
	EFI_STATUS BdsmStatus = QemuFwCfgFindFile (
	               ASSIGNED_IGD_FW_CFG_BDSM_SIZE,
	               &BdsmItem,
	               &BdsmItemSize
	               );
	//
	// If neither fw_cfg file is available, assume no IGD is assigned.
	//
	if (EFI_ERROR (OpRegionStatus) && EFI_ERROR (BdsmStatus)) {
	  return EFI_UNSUPPORTED;
	}
	
	//
	// Require all fw_cfg files that are present to be well-formed.
	//
	if (!EFI_ERROR (OpRegionStatus) && mOpRegionSize == 0)  {
	  DEBUG ((DEBUG_ERROR, "%a: %a: zero size\n", __FUNCTION__,
	    ASSIGNED_IGD_FW_CFG_OPREGION));
	  return EFI_PROTOCOL_ERROR;
	}
	
	if (!EFI_ERROR (BdsmStatus)) {
	  UINT64 BdsmSize;
	
	  if (BdsmItemSize != sizeof BdsmSize) {
	    DEBUG ((DEBUG_ERROR, "%a: %a: invalid fw_cfg size: %Lu\n", __FUNCTION__,
	      ASSIGNED_IGD_FW_CFG_BDSM_SIZE, (UINT64)BdsmItemSize));
	    return EFI_PROTOCOL_ERROR;
	  }
	  QemuFwCfgSelectItem (BdsmItem);
	  QemuFwCfgReadBytes (BdsmItemSize, &BdsmSize);
	
	  if (BdsmSize == 0 || BdsmSize > MAX_UINTN) {
	    DEBUG ((DEBUG_ERROR, "%a: %a: invalid value: %Lu\n", __FUNCTION__,
	      ASSIGNED_IGD_FW_CFG_BDSM_SIZE, BdsmSize));
	    return EFI_PROTOCOL_ERROR;
	  }
	  DEBUG((DEBUG_INFO,"BdsmSize=%Lu\n",BdsmSize));
	  mBdsmSize = (UINTN)BdsmSize;
	}else{
	    //assume 64M
	    DEBUG((DEBUG_INFO,"BdsmSize not found\n"));
	    //mBdsmSize = (UINTN)(64<<20);
	}
	
	CANDIDATE_PCI_INFO PciInfo={};
	InitPciInfo (PciIo, &PciInfo);
	if (mOpRegionSize > 0) {
	  SetupOpRegion (PciIo, &PciInfo);
	}
	if (mBdsmSize > 0) {
	  SetupStolenMemory (PciIo, &PciInfo);
	}
	return EFI_SUCCESS;
}

EFI_STATUS EFIAPI i915ControllerDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL    *This,
  IN EFI_HANDLE                     Controller,
  IN EFI_DEVICE_PATH_PROTOCOL       *RemainingDevicePath
  )
{
	EFI_TPL                           OldTpl;
	EFI_STATUS                        Status;
	i915_CONTROLLER           *Private;
	PCI_TYPE00          Pci;
	
	OldTpl = gBS->RaiseTPL (TPL_CALLBACK);
	DebugPrint(EFI_D_ERROR,"i915: start\n");
	
	Private = &g_private;
	
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
	  goto RestoreTpl;
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
	                          EFI_PCI_DEVICE_ENABLE,// | EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY,
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
	
	/* 1. Enable PCH reset handshake. */
	//intel_pch_reset_handshake(dev_priv, !HAS_PCH_NOP(dev_priv));
	write32(HSW_NDE_RSTWRN_OPT,read32(HSW_NDE_RSTWRN_OPT)|RESET_PCH_HANDSHAKE_ENABLE);
	
	//DOESN'T APPLY
	///* 2-3. */
	//icl_combo_phys_init(dev_priv);
		
	//if (resume && dev_priv->csr.dmc_payload)
	//	intel_csr_load_program(dev_priv);
	
	//power well enable, we are requesting these to be enabled
	//#define   SKL_PW_CTL_IDX_PW_2			15
	//#define   SKL_PW_CTL_IDX_PW_1			14
	//#define   SKL_PW_CTL_IDX_DDI_D			4
	//#define   SKL_PW_CTL_IDX_DDI_C			3
	//#define   SKL_PW_CTL_IDX_DDI_B			2
	//#define   SKL_PW_CTL_IDX_DDI_A_E		1
	//#define   SKL_PW_CTL_IDX_MISC_IO		0
	write32(HSW_PWR_WELL_CTL1,read32(HSW_PWR_WELL_CTL1)|0xA00002AAu);
	for(UINT32 counter=0;;counter++){
		UINT32 stat=read32(HSW_PWR_WELL_CTL1);
		if(counter>16384){
			DebugPrint(EFI_D_ERROR,"i915: power well enabling timed out %08x\n",stat);
			break;
		}
		if(stat&0x50000155u){
			DebugPrint(EFI_D_ERROR,"i915: power well enabled %08x\n",stat);
			break;
		}
	}
	//disable VGA
	UINT32 vgaword=read32(VGACNTRL);
	write32(VGACNTRL,(vgaword&~VGA_2X_MODE)|VGA_DISP_DISABLE);
	//DebugPrint(EFI_D_ERROR,"i915: bars %08x %08x %08x %08x\n",Pci.Device.Bar[0],Pci.Device.Bar[1],Pci.Device.Bar[2],Pci.Device.Bar[3]);
	
	///* 5. Enable CDCLK. */
	//icl_init_cdclk(dev_priv);
	//080002a1 on test machine
	//DebugPrint(EFI_D_ERROR,"i915: CDCLK = %08x\n",read32(CDCLK_CTL));
	//there seems no need to do so
	
	///* 6. Enable DBUF. */
	//icl_dbuf_enable(dev_priv);
	I915_WRITE(DBUF_CTL_S1, I915_READ(DBUF_CTL_S1) | DBUF_POWER_REQUEST);
	I915_WRITE(DBUF_CTL_S2, I915_READ(DBUF_CTL_S2) | DBUF_POWER_REQUEST);
	read32(DBUF_CTL_S2);
	for(UINT32 counter=0;;counter++){
		if(counter>16384){
			DebugPrint(EFI_D_ERROR,"i915: DBUF timeout\n");
			break;
		}
		if(read32(DBUF_CTL_S1)&read32(DBUF_CTL_S2)&DBUF_POWER_STATE){
			DebugPrint(EFI_D_ERROR,"i915: DBUF good\n");
			break;
		}
	}
	
	///* 7. Setup MBUS. */
	//icl_mbus_init(dev_priv);
	I915_WRITE(MBUS_ABOX_CTL, 
		MBUS_ABOX_BT_CREDIT_POOL1(16) |
		MBUS_ABOX_BT_CREDIT_POOL2(16) |
		MBUS_ABOX_B_CREDIT(1) |
		MBUS_ABOX_BW_CREDIT(1)
	);
	
	//set up display buffer
	//the value is from host
	DebugPrint(EFI_D_ERROR,"i915: _PLANE_BUF_CFG_1_A = %08x\n",read32(_PLANE_BUF_CFG_1_A));
	write32(_PLANE_BUF_CFG_1_A,0x035b0000);
	DebugPrint(EFI_D_ERROR,"i915: _PLANE_BUF_CFG_1_A = %08x (after)\n",read32(_PLANE_BUF_CFG_1_A));
	
	//initialize output
	//need workaround: always initialize DDI
	//intel_dig_port->hdmi.hdmi_reg = DDI_BUF_CTL(port);
	//intel_ddi_init(PORT_A);
	UINT32 found = I915_READ(SFUSE_STRAP);
	DebugPrint(EFI_D_ERROR,"i915: SFUSE_STRAP = %08x\n",found);
	port=PORT_A;
	if (found & SFUSE_STRAP_DDIB_DETECTED){
		port=PORT_B;//intel_ddi_init(PORT_B);
	}else if (found & SFUSE_STRAP_DDIC_DETECTED){
		port=PORT_C;//intel_ddi_init(PORT_C);
	}else if (found & SFUSE_STRAP_DDID_DETECTED){
		port=PORT_D;//intel_ddi_init(PORT_D);
	}
	//if (found & SFUSE_STRAP_DDIF_DETECTED)
	//	intel_ddi_init(dev_priv, PORT_F);
	
	//reset GMBUS
	//intel_i2c_reset(dev_priv);
	I915_WRITE(GMBUS0, 0);
	I915_WRITE(GMBUS4, 0);
	
	// query EDID and initialize the mode
	// it somehow fails on real hardware
	Status = ReadEDID(&g_private.edid);
	if (EFI_ERROR (Status)) {
		DebugPrint(EFI_D_ERROR,"i915: failed to read EDID\n");
		for(UINT32 i=0;i<128;i++){
			((UINT8*)&g_private.edid)[i]=edid_fallback[i];
		}
	}
	DebugPrint(EFI_D_ERROR,"i915: got EDID:\n");
	for(UINT32 i=0;i<16;i++){
		for(UINT32 j=0;j<8;j++){
			DebugPrint(EFI_D_ERROR,"%02x ",((UINT8*)(&g_private.edid))[i*8+j]);
		}
		DebugPrint(EFI_D_ERROR,"\n");
	}
	UINT32 pixel_clock = (UINT32)(g_private.edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock) * 10;
	UINT32 x_active = g_private.edid.detailTimings[DETAIL_TIME_SELCTION].horzActive | ((UINT32)(g_private.edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb >> 4) << 8);
	UINT32 y_active =  g_private.edid.detailTimings[DETAIL_TIME_SELCTION].vertActive | ((UINT32)(g_private.edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb >> 4) << 8);
	DebugPrint(EFI_D_ERROR,"i915: %ux%u clock=%u\n",x_active,y_active,pixel_clock);
	g_mode_info[0].HorizontalResolution=x_active;
	g_mode_info[0].VerticalResolution=y_active;
	g_mode_info[0].PixelsPerScanLine = ((x_active*4+63)&-64)>>2;
	g_mode_info[0].PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
	//get BAR 0 address and size
	EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *bar0Desc;
	Private->PciIo->GetBarAttributes (
		Private->PciIo,
		PCI_BAR_IDX0,
		NULL,
		(VOID**) &bar0Desc
	);
	EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *bar2Desc;
	Private->PciIo->GetBarAttributes (
		Private->PciIo,
		PCI_BAR_IDX1,
		NULL,
		(VOID**) &bar2Desc
	);
	DebugPrint(EFI_D_ERROR,"i915: bar ranges - %llx %llx, %llx %llx\n",
		bar0Desc->AddrRangeMin,bar0Desc->AddrLen,
		bar2Desc->AddrRangeMin,bar2Desc->AddrLen);
	UINT32 bar0Size=bar0Desc->AddrLen;
	EFI_PHYSICAL_ADDRESS mmio_base = bar0Desc->AddrRangeMin;
	
	//get BAR 2 address
	EFI_PHYSICAL_ADDRESS aperture_base = bar2Desc->AddrRangeMin;
	DebugPrint(EFI_D_ERROR,"i915: aperture at %p\n",aperture_base);
	//Private->PciIo->Pci.Write (Private->PciIo,EfiPciIoWidthUint32,0x18,1,&aperture_base);
	//Private->PciIo->Pci.Read (Private->PciIo,EfiPciIoWidthUint32,0x18,1,&bar_work);
	//DebugPrint(EFI_D_ERROR,"i915: aperture confirmed at %016x\n",bar_work);
	//GVT-g gmadr issue
	g_private.gmadr=0;
	g_private.is_gvt=0;
	if(read64(0x78000)==0x4776544776544776ULL){
		g_private.gmadr=read32(0x78040);
		g_private.is_gvt=1;
		//apertureSize=read32(0x78044);
	}
	DebugPrint(EFI_D_ERROR,"i915: gmadr = %08x, size = %08x, hgmadr = %08x, hsize = %08x\n",
		g_private.gmadr,read32(0x78044),read32(0x78048),read32(0x7804c));

	//create Global GTT entries to actually back the framebuffer
	g_private.FbBase=aperture_base+(UINT64)(g_private.gmadr);
	UINTN MaxFbSize=((x_active*4+64)&-64)*y_active;
	UINTN Pages = EFI_SIZE_TO_PAGES ((MaxFbSize+65535)&-65536);
	EFI_PHYSICAL_ADDRESS fb_backing=(EFI_PHYSICAL_ADDRESS)AllocateReservedPages(Pages);
	if(!fb_backing){
		DebugPrint(EFI_D_ERROR,"i915: failed to allocate framebuffer\n");
		Status=EFI_OUT_OF_RESOURCES;
		goto FreeGopDevicePath;
	}
	EFI_PHYSICAL_ADDRESS ggtt_base=mmio_base+(bar0Size>>1);
	UINT64* ggtt=(UINT64*)ggtt_base;
	DebugPrint(EFI_D_ERROR,"i915: ggtt_base at %p, entries: %08x %08x, backing fb: %p, %x bytes\n",ggtt_base,ggtt[0],ggtt[g_private.gmadr>>12],fb_backing,MaxFbSize);
	for(UINTN i=0;i<MaxFbSize;i+=4096){
		//create one PTE entry for each page
		//cache is whatever cache used by the linux driver on my host
		EFI_PHYSICAL_ADDRESS addr=fb_backing+i;
		ggtt[(g_private.gmadr+i)>>12]=((UINT32)(addr>>32)&0x7F0u)|((UINT32)addr&0xFFFFF000u)|11;
	}

	//setup OpRegion from fw_cfg (IgdAssignmentDxe)
	DebugPrint(EFI_D_ERROR,"i915: before QEMU shenanigans\n");
	QemuFwCfgInitialize();
	if(QemuFwCfgIsAvailable()){
		//setup opregion
		Status=SetupFwcfgStuff(Private->PciIo);
		DebugPrint(EFI_D_ERROR,"i915: SetupFwcfgStuff returns %d\n",Status);
	}
	DebugPrint(EFI_D_ERROR,"i915: after QEMU shenanigans\n");
	
	//TODO: turn on backlight if found in OpRegion, need eDP initialization first...
	
	//
	// Start the GOP software stack.
	//
	EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;
	GraphicsOutput            = &Private->GraphicsOutput;
	GraphicsOutput->QueryMode = i915GraphicsOutputQueryMode;
	GraphicsOutput->SetMode   = i915GraphicsOutputSetMode;
	GraphicsOutput->Blt       = i915GraphicsOutputBlt;
	GraphicsOutput->Mode = &g_mode;
	Status = GraphicsOutput->SetMode (GraphicsOutput, 0);
	if (EFI_ERROR (Status)) {
		goto FreeGopDevicePath;
	}
	
	
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
	//we don't support this, Windows can clean up our mess without this anyway
	return EFI_UNSUPPORTED;
}


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
			//DebugPrint(EFI_D_ERROR,"i915: bars %08x %08x %08x %08x\n",Pci.Device.Bar[0],Pci.Device.Bar[1],Pci.Device.Bar[2],Pci.Device.Bar[3]);
			//Status=EFI_UNSUPPORTED;
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
	g_SystemTable=SystemTable;
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
