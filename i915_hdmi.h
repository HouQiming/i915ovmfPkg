#define  DPLL_CTRL1_HDMI_MODE(id)        (1 << ((id) * 6 + 5))
#define _DPLL1_CFGCR1    0x6C040
#define _DPLL2_CFGCR1    0x6C048
#define _DPLL3_CFGCR1    0x6C050
#define  DPLL_CFGCR1_FREQ_ENABLE    (1 << 31)
#define  DPLL_CFGCR1_DCO_FRACTION_MASK    (0x7fff << 9)
#define  DPLL_CFGCR1_DCO_FRACTION(x)    ((x) << 9)
#define  DPLL_CFGCR1_DCO_INTEGER_MASK    (0x1ff)

#define _DPLL1_CFGCR2    0x6C044
#define _DPLL2_CFGCR2    0x6C04C
#define _DPLL3_CFGCR2    0x6C054
#define  DPLL_CFGCR2_QDIV_RATIO_MASK    (0xff << 8)
#define  DPLL_CFGCR2_QDIV_RATIO(x)    ((x) << 8)
#define  DPLL_CFGCR2_QDIV_MODE(x)    ((x) << 7)
#define  DPLL_CFGCR2_KDIV_MASK        (3 << 5)
#define  DPLL_CFGCR2_KDIV(x)        ((x) << 5)
#define  DPLL_CFGCR2_KDIV_5 (0 << 5)
#define  DPLL_CFGCR2_KDIV_2 (1 << 5)
#define  DPLL_CFGCR2_KDIV_3 (2 << 5)
#define  DPLL_CFGCR2_KDIV_1 (3 << 5)
#define  DPLL_CFGCR2_PDIV_MASK        (7 << 2)
#define  DPLL_CFGCR2_PDIV(x)        ((x) << 2)
#define  DPLL_CFGCR2_PDIV_1 (0 << 2)
#define  DPLL_CFGCR2_PDIV_2 (1 << 2)
#define  DPLL_CFGCR2_PDIV_3 (2 << 2)
#define  DPLL_CFGCR2_PDIV_7 (4 << 2)
#define  DPLL_CFGCR2_CENTRAL_FREQ_MASK    (3)
/* DCO freq must be within +1%/-6%  of the DCO central freq */
#define SKL_DCO_MAX_PDEVIATION    100
#define SKL_DCO_MAX_NDEVIATION    600
struct skl_wrpll_params {
    UINT32 dco_fraction;
    UINT32 dco_integer;
    UINT32 qdiv_ratio;
    UINT32 qdiv_mode;
    UINT32 kdiv;
    UINT32 pdiv;
    UINT32 central_freq;
};

static const int even_dividers[] = {4, 6, 8, 10, 12, 14, 16, 18, 20,
                                    24, 28, 30, 32, 36, 40, 42, 44,
                                    48, 52, 54, 56, 60, 64, 66, 68,
                                    70, 72, 76, 78, 80, 84, 88, 90,
                                    92, 96, 98};
static const int odd_dividers[] = {3, 5, 7, 9, 15, 21, 35};
static const struct {
    const int *list;
    int n_dividers;
} dividers[] = {
        {even_dividers, ARRAY_SIZE(even_dividers)},
        {odd_dividers,  ARRAY_SIZE(odd_dividers)},
};

struct skl_wrpll_context {
    UINT64 min_deviation;        /* current minimal deviation */
    UINT64 central_freq;        /* chosen central freq */
    UINT64 dco_freq;            /* chosen dco freq */
    UINT64 p;            /* chosen divider */
};
EFI_STATUS SetupClockHDMI(i915_CONTROLLER* controller);
EFI_STATUS SetupTranscoderAndPipeHDMI(i915_CONTROLLER* controller);