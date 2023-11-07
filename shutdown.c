#include "../edk2/MdePkg/Include/Uefi.h"

EFI_STATUS
        EFIAPI
efi_main (
        IN
EFI_HANDLE ImageHandle,
        IN
EFI_SYSTEM_TABLE *SystemTable
)
{
SystemTable->RuntimeServices->
ResetSystem(EfiResetShutdown,
0,0,NULL);
return
EFI_SUCCESS;
}
