#include <Library/DebugLib.h>

#ifndef DEBUG_LINE_NUMBER
#define DEBUG_LINE_NUMBER __LINE__
#endif

#ifndef PRINT_DEBUG
#ifndef MDEPKG_NDEBUG
//%a is used instead of %s due to specific implementations in edk2. Mainly ASCII vs unicode.
#define PRINT_DEBUG(ERR_LEVEL, ...)                                                 \
    do                                                                              \
    {                                                                               \
        DebugPrint(ERR_LEVEL, "i915 Message: %a(%d)", __func__, DEBUG_LINE_NUMBER); \
        DebugPrint(ERR_LEVEL, __VA_ARGS__);                                         \
    } while (0)
#else
#define PRINT_DEBUG(ERR_LEVEL, ...)              \
    do                                           \
    {                                            \
        DebugPrint(ERR_LEVEL, "i915 Message: "); \
        DebugPrint(ERR_LEVEL, __VA_ARGS__);      \
    } while (0)
#endif
#endif