#define PCH_DISPLAY_BASE    0xc0000u
#define DETAIL_TIME_SELCTION 0
#define DPLL_CTRL1        (0x6C058)
#define  DPLL_CTRL1_SSC(id)            (1 << ((id) * 6 + 4))
#define  DPLL_CTRL1_LINK_RATE_MASK(id)        (7 << ((id) * 6 + 1))
#define  DPLL_CTRL1_LINK_RATE_SHIFT(id)        ((id) * 6 + 1)
#define  DPLL_CTRL1_LINK_RATE(linkrate, id)    ((linkrate) << ((id) * 6 + 1))
#define  DPLL_CTRL1_OVERRIDE(id)        (1 << ((id) * 6))
#define  DPLL_CTRL1_LINK_RATE_2700        0
#define  DPLL_CTRL1_LINK_RATE_1350        1
#define  DPLL_CTRL1_LINK_RATE_810        2
#define  DPLL_CTRL1_LINK_RATE_1620        3
#define  DPLL_CTRL1_LINK_RATE_1080        4
#define  DPLL_CTRL1_LINK_RATE_2160        5
#define _PICK_EVEN(__index, __a, __b) ((__a) + (__index) * ((__b) - (__a)))
#define _PORT(port, a, b)        _PICK_EVEN(port, a, b)

#define _DDI_BUF_TRANS_A        0x64E00
#define _DDI_BUF_TRANS_B        0x64E60
#define DDI_BUF_TRANS_LO(port, i)    (_PORT(port, _DDI_BUF_TRANS_A,_DDI_BUF_TRANS_B) + (i) * 8) //Writes to DWORD 0 At the specified Port and Entry Num
#define  DDI_BUF_BALANCE_LEG_ENABLE    (1 << 31)
#define DDI_BUF_TRANS_HI(port, i)    (_PORT(port, _DDI_BUF_TRANS_A,_DDI_BUF_TRANS_B) + (i) * 8 + 4)//Writes to DWORD 1 At the specified Port and Entry Num
#define _DDI_BUF_CTL_A                0x64000
#define _DDI_BUF_CTL_B                0x64100
#define DDI_BUF_CTL(port) _PORT(port, _DDI_BUF_CTL_A, _DDI_BUF_CTL_B)
#define  DDI_BUF_CTL_ENABLE            (1 << 31)
#define  DDI_BUF_TRANS_SELECT(n)    ((n) << 24)
#define  DDI_BUF_EMP_MASK            (0xf << 24)
#define  DDI_BUF_PORT_REVERSAL            (1 << 16)
#define  DDI_BUF_IS_IDLE            (1 << 7)
#define  DDI_A_4_LANES                (1 << 4)
#define  DDI_PORT_WIDTH(width)            (((width) - 1) << 1)
#define  DDI_PORT_WIDTH_MASK            (7 << 1)
#define  DDI_PORT_WIDTH_SHIFT            1
#define  DDI_INIT_DISPLAY_DETECTED        (1 << 0)

#define DPLL_STATUS    (0x6C060)
#define  DPLL_LOCK(id) (1 << ((id) * 8))

#define LCPLL1_CTL        (0x46010)
#define LCPLL2_CTL        (0x46014)
#define  LCPLL_PLL_ENABLE    (1 << 31)
#define PLL_REF_SDVO_HDMI_MULTIPLIER_SHIFT     9
#define PLL_REF_SDVO_HDMI_MULTIPLIER_MASK      (7 << 9)
#define PLL_REF_SDVO_HDMI_MULTIPLIER(x)    (((x) - 1) << 9)
#define DPLL_FPA1_P1_POST_DIV_SHIFT            0
#define DPLL_FPA1_P1_POST_DIV_MASK             0xff

#define _DPLL_A_MD (PCH_DISPLAY_BASE + 0x601c)
#define _DPLL_B_MD (PCH_DISPLAY_BASE + 0x6020)
#define   DPLL_MD_UDI_MULTIPLIER_MASK        0x00003f00
#define   DPLL_MD_UDI_MULTIPLIER_SHIFT        8
/*
 * SDVO/UDI pixel multiplier for VGA, same as DPLL_MD_UDI_MULTIPLIER_MASK.
 * This best be set to the default value (3) or the CRT won't work. No,
 * I don't entirely understand what this does...
 */
#define   DPLL_MD_VGA_UDI_MULTIPLIER_MASK    0x0000003f
#define   DPLL_MD_VGA_UDI_MULTIPLIER_SHIFT    0

#define DPLL_CTRL2                (0x6C05C)
#define  DPLL_CTRL2_DDI_CLK_OFF(port)        (1 << ((port) + 15))
#define  DPLL_CTRL2_DDI_CLK_SEL_MASK(port)    (3 << ((port) * 3 + 1))
#define  DPLL_CTRL2_DDI_CLK_SEL_SHIFT(port)    ((port) * 3 + 1)
#define  DPLL_CTRL2_DDI_CLK_SEL(clk, port)    ((clk) << ((port) * 3 + 1))
#define  DPLL_CTRL2_DDI_SEL_OVERRIDE(port)     (1 << ((port) * 3))

/* Pipe A timing regs */
#define HTOTAL_A    0x60000
#define HBLANK_A    0x60004
#define HSYNC_A    0x60008
#define VTOTAL_A    0x6000c
#define VBLANK_A    0x60010
#define VSYNC_A    0x60014
#define PIPEASRC    0x6001c
#define BCLRPAT_A    0x60020
#define VSYNCSHIFT_A    0x60028
#define PIPE_MULT_A    0x6002c

/* Pipe B timing regs */
#define HTOTAL_B    0x61000
#define HBLANK_B    0x61004
#define HSYNC_B    0x61008
#define VTOTAL_B    0x6100c
#define VBLANK_B    0x61010
#define VSYNC_B    0x61014
#define PIPEBSRC    0x6101c
#define BCLRPAT_B    0x61020
#define VSYNCSHIFT_B    0x61028
#define PIPE_MULT_B    0x6102c

/* Pipe eDP timing regs */
#define HTOTAL_EDP    0x6f000
#define HBLANK_EDP    0x6f004
#define HSYNC_EDP    0x6f008
#define VTOTAL_EDP    0x6f00c
#define VBLANK_EDP   0x6f010
#define VSYNC_EDP    0x6f014
#define PIPEEDPSRC    0x6f01c
#define BCLRPAT_EDP  0x6f020
#define VSYNCSHIFT_EDP    0x6f028
#define SFUSE_STRAP			0xc2014
#define  SFUSE_STRAP_FUSE_LOCK		(1 << 13)
#define  SFUSE_STRAP_RAW_FREQUENCY	(1 << 8)