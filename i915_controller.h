#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/GraphicsOutput.h>

#pragma once
#pragma pack(1)
typedef struct {
    UINT8 magic[8];
    UINT16 vendorId;
    UINT16 productId;
    UINT32 serialNumber;
    UINT8 manufactureWeek;
    UINT8 manufactureYear;
    UINT8 structVersion;
    UINT8 structRevision;
    UINT8 inputParameters;
    UINT8 screenWidth;
    UINT8 screenHeight;
    UINT8 gamma;
    UINT8 features;
    UINT8 colorCoordinates[10];
    UINT8 estTimings1;
    UINT8 estTimings2;
    UINT8 vendorTimings;
    struct {
        UINT8 resolution;
        UINT8 frequency;
    } standardTimings[8];
    struct {
        UINT16 pixelClock;
        UINT8 horzActive;
        UINT8 horzBlank;
        UINT8 horzActiveBlankMsb;
        UINT8 vertActive;
        UINT8 vertBlank;
        UINT8 vertActiveBlankMsb;
        UINT8 horzSyncOffset;
        UINT8 horzSyncPulse;
        UINT8 vertSync;
        UINT8 syncMsb;
        UINT8 dimensionWidth;
        UINT8 dimensionHeight;
        UINT8 dimensionMsb;
        UINT8 horzBorder;
        UINT8 vertBorder;
        UINT8 features;
    } detailTimings[4];
    UINT8 numExtensions;
    UINT8 checksum;
} EDID;
#pragma pack()
typedef enum ConnectorTypes {HDMI, DVI, VGA, eDP, DPSST, DPMST} ConnectorType;
typedef struct {
    UINT64 Signature;
    EFI_HANDLE Handle;
    EFI_PCI_IO_PROTOCOL *PciIo;
    EFI_GRAPHICS_OUTPUT_PROTOCOL GraphicsOutput;
    EFI_DEVICE_PATH_PROTOCOL *GopDevicePath;
    EDID edid;
    EFI_PHYSICAL_ADDRESS FbBase;
    UINT32 stride;
    UINT32 gmadr;
    UINT32 is_gvt;
    UINT8 generation;
    UINTN fbsize;
    void (*write32)(UINT64 reg, UINT32 data);

    UINT32 (*read32)(UINT64 reg);

    UINT64 (*read64)(UINT64 reg);
    struct {
        UINT32 Port;
        UINT32 AuxCh;
        ConnectorType ConType;
        UINT8 DPLL;
    } OutputPath;
} i915_CONTROLLER;
