#include <Uefi.h>

#include "i915_display.h"
#include "i915_gop.h"
static i915_CONTROLLER* controller;
static EFI_STATUS ReadEDID(EDID* result){
	UINT32 pin=0;
	//it's an INTEL GPU, there's no way we could be big endian
	UINT32* p=(UINT32*)result;
	//try all the pins on GMBUS
	for(pin=1;pin<=6;pin++){
		DebugPrint(EFI_D_ERROR,"i915: trying pin %d\n",pin);
		controller->write32(gmbusSelect, pin);
		if(EFI_ERROR(gmbusWait(controller,GMBUS_HW_RDY))){
			//it's DP, need to hack AUX_CHAN
			continue;
		}
		//set read offset: i2cWrite(0x50, &offset, 1);
		controller->write32(gmbusData, 0);
		controller->write32(gmbusCommand, (0x50<<GMBUS_SLAVE_ADDR_SHIFT)|(1<<GMBUS_BYTE_COUNT_SHIFT)|GMBUS_SLAVE_WRITE|GMBUS_CYCLE_WAIT|GMBUS_SW_RDY);
		//gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
		gmbusWait(controller, GMBUS_HW_RDY);
		//read the edid: i2cRead(0x50, &edid, 128);
		//note that we could fail here!
		controller->write32(gmbusCommand, (0x50<<GMBUS_SLAVE_ADDR_SHIFT)|(128<<GMBUS_BYTE_COUNT_SHIFT)|GMBUS_SLAVE_READ|GMBUS_CYCLE_WAIT|GMBUS_SW_RDY);
		UINT32 i=0;
		for(i=0;i<128;i+=4){
			if(EFI_ERROR(gmbusWait(controller,GMBUS_HW_RDY))){break;}
			p[i>>2]=controller->read32(gmbusData);
		}
		//gmbusWait(controller,GMBUS_HW_WAIT_PHASE);
		gmbusWait(controller,GMBUS_HW_RDY);
		for(UINT32 i=0;i<16;i++){
			for(UINT32 j=0;j<8;j++){
				DebugPrint(EFI_D_ERROR,"%02x ",((UINT8*)(p))[i*8+j]);
			}
			DebugPrint(EFI_D_ERROR,"\n");
		}
		if(i>=128&&*(UINT64*)result->magic==0x00FFFFFFFFFFFF00uLL){return EFI_SUCCESS;}
	}
	//try DP AUX CHAN - Skylake
	//controller->write32(_DPA_AUX_CH_CTL+(1<<8),0x1234)
	//controller->write32(_DPA_AUX_CH_CTL+(0x600),0x1234);
	//controller->write32(_DPA_AUX_CH_CTL+(0<<8),0x1234);
	//controller->write32(_DPA_AUX_CH_DATA1+(0<<8),0xabcd);
	//controller->write32(_DPA_AUX_CH_DATA2+(0<<8),0xabcd);
	//controller->write32(_DPA_AUX_CH_DATA3+(0<<8),0xabcd);
	//DebugPrint(EFI_D_ERROR,"i915: SKL CTL %08x\n",controller->read32(_DPA_AUX_CH_CTL+(0<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",controller->read32(_DPA_AUX_CH_DATA1+(0<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",controller->read32(_DPA_AUX_CH_DATA2+(0<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: SKL DATA %08x\n",controller->read32(_DPA_AUX_CH_DATA3+(0<<8)));
	//controller->write32(_PCH_DP_B+(1<<8),0x1234);
	//DebugPrint(EFI_D_ERROR,"i915: SKL %08x\n",controller->read32(_DPA_AUX_CH_CTL+(1<<8)));
	//DebugPrint(EFI_D_ERROR,"i915: PCH %08x\n",controller->read32(_PCH_DP_B+(1<<8)));
	for(pin=0;pin<=5;pin++){
		DebugPrint(EFI_D_ERROR,"i915: trying DP aux %d\n",pin);
		//aux message header is 3-4 bytes: ctrl8 addr16 len8
		//the data is big endian
		//len is receive buffer size-1
		//i2c init
		UINT32 send_ctl=(
	      DP_AUX_CH_CTL_SEND_BUSY |
	      DP_AUX_CH_CTL_DONE |
	      DP_AUX_CH_CTL_TIME_OUT_ERROR |
	      DP_AUX_CH_CTL_TIME_OUT_MAX |
	      DP_AUX_CH_CTL_RECEIVE_ERROR |
	      (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
	      DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
	      DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
		);
		/* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
		controller->write32(_DPA_AUX_CH_DATA1+(pin<<8), ((AUX_I2C_MOT|AUX_I2C_WRITE)<<28)|(0x50<<8)|0);
		controller->write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
		UINT32 aux_status;
		UINT32 counter=0;
		for(;;){
			aux_status=controller->read32(_DPA_AUX_CH_CTL+(pin<<8));
			if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
			counter+=1;
			if(counter>=16384){
				DebugPrint(EFI_D_ERROR,"i915:DP AUX channel timeout");
				break;
			}
		}
		controller->write32(_DPA_AUX_CH_CTL+(pin<<8), 
			aux_status |
			DP_AUX_CH_CTL_DONE |
			DP_AUX_CH_CTL_TIME_OUT_ERROR |
			DP_AUX_CH_CTL_RECEIVE_ERROR
		);
		//i2c send 1 byte
		send_ctl=(
		     DP_AUX_CH_CTL_SEND_BUSY |
		     DP_AUX_CH_CTL_DONE |
		     DP_AUX_CH_CTL_TIME_OUT_ERROR |
		     DP_AUX_CH_CTL_TIME_OUT_MAX |
		     DP_AUX_CH_CTL_RECEIVE_ERROR |
		     (5 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		     DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		     DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
		);
		controller->write32(_DPA_AUX_CH_DATA1+(pin<<8), (AUX_I2C_WRITE<<28)|(0x50<<8)|0);
		controller->write32(_DPA_AUX_CH_DATA2+(pin<<8), 0);
		controller->write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
		counter=0;
		for(;;){
			aux_status=controller->read32(_DPA_AUX_CH_CTL+(pin<<8));
			if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
			counter+=1;
			if(counter>=16384){
				DebugPrint(EFI_D_ERROR,"i915:DP AUX channel timeout");
				break;
			}
		}
		controller->write32(_DPA_AUX_CH_CTL+(pin<<8), 
			aux_status |
			DP_AUX_CH_CTL_DONE |
			DP_AUX_CH_CTL_TIME_OUT_ERROR |
			DP_AUX_CH_CTL_RECEIVE_ERROR
		);
		if (aux_status & (DP_AUX_CH_CTL_TIME_OUT_ERROR|DP_AUX_CH_CTL_RECEIVE_ERROR)){
			continue;
		}
		//i2c read 1 byte * 128
		DebugPrint(EFI_D_ERROR,"i915: reading DP aux %d\n",pin);
		//aux message header is 3-4 bytes: ctrl8 addr16 len8
		//the data is big endian
		//len is receive buffer size-1
		//i2c init
		send_ctl=(
		     DP_AUX_CH_CTL_SEND_BUSY |
		     DP_AUX_CH_CTL_DONE |
		     DP_AUX_CH_CTL_TIME_OUT_ERROR |
		     DP_AUX_CH_CTL_TIME_OUT_MAX |
		     DP_AUX_CH_CTL_RECEIVE_ERROR |
		     (3 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
		     DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
		     DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
		);
		/* Must try at least 3 times according to DP spec, WHICH WE DON'T CARE */
		controller->write32(_DPA_AUX_CH_DATA1+(pin<<8), ((AUX_I2C_MOT|AUX_I2C_READ)<<28)|(0x50<<8)|0);
		controller->write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
		counter=0;
		for(;;){
			aux_status=controller->read32(_DPA_AUX_CH_CTL+(pin<<8));
			if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
			counter+=1;
			if(counter>=16384){
				DebugPrint(EFI_D_ERROR,"i915: DP AUX channel timeout");
				break;
			}
		}
		controller->write32(_DPA_AUX_CH_CTL+(pin<<8), 
			aux_status |
			DP_AUX_CH_CTL_DONE |
			DP_AUX_CH_CTL_TIME_OUT_ERROR |
			DP_AUX_CH_CTL_RECEIVE_ERROR
		);
		UINT32 i=0;
		for(i=0;i<128;i++){
			send_ctl=(
			     DP_AUX_CH_CTL_SEND_BUSY |
			     DP_AUX_CH_CTL_DONE |
			     DP_AUX_CH_CTL_TIME_OUT_ERROR |
			     DP_AUX_CH_CTL_TIME_OUT_MAX |
			     DP_AUX_CH_CTL_RECEIVE_ERROR |
			     (4 << DP_AUX_CH_CTL_MESSAGE_SIZE_SHIFT) |
			     DP_AUX_CH_CTL_FW_SYNC_PULSE_SKL(32) |
			     DP_AUX_CH_CTL_SYNC_PULSE_SKL(32)
			);
			controller->write32(_DPA_AUX_CH_DATA1+(pin<<8), (AUX_I2C_READ<<28)|(0x50<<8)|0);
			controller->write32(_DPA_AUX_CH_CTL+(pin<<8), send_ctl);
			counter=0;
			for(;;){
				aux_status=controller->read32(_DPA_AUX_CH_CTL+(pin<<8));
				if(!(aux_status&DP_AUX_CH_CTL_SEND_BUSY)){break;}
				counter+=1;
				if(counter>=16384){
					DebugPrint(EFI_D_ERROR,"i915: DP AUX channel timeout");
					break;
				}
			}
			controller->write32(_DPA_AUX_CH_CTL+(pin<<8), 
				aux_status |
				DP_AUX_CH_CTL_DONE |
				DP_AUX_CH_CTL_TIME_OUT_ERROR |
				DP_AUX_CH_CTL_RECEIVE_ERROR
			);
			UINT32 word=controller->read32(_DPA_AUX_CH_DATA1+(pin<<8));
			((UINT8*)p)[i]=(word>>16)&0xff;
		}
		for(UINT32 i=0;i<16;i++){
			for(UINT32 j=0;j<8;j++){
				DebugPrint(EFI_D_ERROR,"%02x ",((UINT8*)(p))[i*8+j]);
			}
			DebugPrint(EFI_D_ERROR,"\n");
		}
		if(i>=128&&*(UINT64*)result->magic==0x00FFFFFFFFFFFF00uLL){return EFI_SUCCESS;}
	}
	return EFI_NOT_FOUND;
}
struct dpll {
	/* given values */
	int n;
	int m1, m2;
	int p1, p2;
	/* derived values */
	int	dot;
	int	vco;
	int	m;
	int	p;
};

struct intel_limit {
	struct {
		int min, max;
	} dot, vco, n, m, m1, m2, p, p1;

	struct {
		int dot_limit;
		int p2_slow, p2_fast;
	} p2;
};

//intel_limits_i9xx_sdvo
//static const struct intel_limit g_limits = {
//	.dot = { .min = 20000, .max = 400000 },
//	.vco = { .min = 1400000, .max = 2800000 },
//	.n = { .min = 1, .max = 6 },
//	.m = { .min = 70, .max = 120 },
//	.m1 = { .min = 8, .max = 18 },
//	.m2 = { .min = 3, .max = 7 },
//	.p = { .min = 5, .max = 80 },
//	.p1 = { .min = 1, .max = 8 },
//	.p2 = { .dot_limit = 200000,
//		.p2_slow = 10, .p2_fast = 5 },
//};

#define DPLL_CTRL1		(0x6C058)
#define  DPLL_CTRL1_HDMI_MODE(id)		(1 << ((id) * 6 + 5))
#define  DPLL_CTRL1_SSC(id)			(1 << ((id) * 6 + 4))
#define  DPLL_CTRL1_LINK_RATE_MASK(id)		(7 << ((id) * 6 + 1))
#define  DPLL_CTRL1_LINK_RATE_SHIFT(id)		((id) * 6 + 1)
#define  DPLL_CTRL1_LINK_RATE(linkrate, id)	((linkrate) << ((id) * 6 + 1))
#define  DPLL_CTRL1_OVERRIDE(id)		(1 << ((id) * 6))
#define  DPLL_CTRL1_LINK_RATE_2700		0
#define  DPLL_CTRL1_LINK_RATE_1350		1
#define  DPLL_CTRL1_LINK_RATE_810		2
#define  DPLL_CTRL1_LINK_RATE_1620		3
#define  DPLL_CTRL1_LINK_RATE_1080		4
#define  DPLL_CTRL1_LINK_RATE_2160		5

#define DPLL_STATUS	(0x6C060)
#define  DPLL_LOCK(id) (1 << ((id) * 8))

#define LCPLL1_CTL		(0x46010)
#define LCPLL2_CTL		(0x46014)
#define  LCPLL_PLL_ENABLE	(1 << 31)

/* DPLL cfg */
#define _DPLL1_CFGCR1	0x6C040
#define _DPLL2_CFGCR1	0x6C048
#define _DPLL3_CFGCR1	0x6C050
#define  DPLL_CFGCR1_FREQ_ENABLE	(1 << 31)
#define  DPLL_CFGCR1_DCO_FRACTION_MASK	(0x7fff << 9)
#define  DPLL_CFGCR1_DCO_FRACTION(x)	((x) << 9)
#define  DPLL_CFGCR1_DCO_INTEGER_MASK	(0x1ff)

#define _DPLL1_CFGCR2	0x6C044
#define _DPLL2_CFGCR2	0x6C04C
#define _DPLL3_CFGCR2	0x6C054
#define  DPLL_CFGCR2_QDIV_RATIO_MASK	(0xff << 8)
#define  DPLL_CFGCR2_QDIV_RATIO(x)	((x) << 8)
#define  DPLL_CFGCR2_QDIV_MODE(x)	((x) << 7)
#define  DPLL_CFGCR2_KDIV_MASK		(3 << 5)
#define  DPLL_CFGCR2_KDIV(x)		((x) << 5)
#define  DPLL_CFGCR2_KDIV_5 (0 << 5)
#define  DPLL_CFGCR2_KDIV_2 (1 << 5)
#define  DPLL_CFGCR2_KDIV_3 (2 << 5)
#define  DPLL_CFGCR2_KDIV_1 (3 << 5)
#define  DPLL_CFGCR2_PDIV_MASK		(7 << 2)
#define  DPLL_CFGCR2_PDIV(x)		((x) << 2)
#define  DPLL_CFGCR2_PDIV_1 (0 << 2)
#define  DPLL_CFGCR2_PDIV_2 (1 << 2)
#define  DPLL_CFGCR2_PDIV_3 (2 << 2)
#define  DPLL_CFGCR2_PDIV_7 (4 << 2)
#define  DPLL_CFGCR2_CENTRAL_FREQ_MASK	(3)

struct skl_wrpll_params {
	UINT32 dco_fraction;
	UINT32 dco_integer;
	UINT32 qdiv_ratio;
	UINT32 qdiv_mode;
	UINT32 kdiv;
	UINT32 pdiv;
	UINT32 central_freq;
};

static const int even_dividers[] = {  4,  6,  8, 10, 12, 14, 16, 18, 20,
				     24, 28, 30, 32, 36, 40, 42, 44,
				     48, 52, 54, 56, 60, 64, 66, 68,
				     70, 72, 76, 78, 80, 84, 88, 90,
				     92, 96, 98 };
static const int odd_dividers[] = { 3, 5, 7, 9, 15, 21, 35 };
static const struct {
	const int *list;
	int n_dividers;
} dividers[] = {
	{ even_dividers, ARRAY_SIZE(even_dividers) },
	{ odd_dividers, ARRAY_SIZE(odd_dividers) },
};

struct skl_wrpll_context {
	UINT64 min_deviation;		/* current minimal deviation */
	UINT64 central_freq;		/* chosen central freq */
	UINT64 dco_freq;			/* chosen dco freq */
	UINT64 p;			/* chosen divider */
};

static void skl_wrpll_get_multipliers(UINT64 p,
				      UINT64 *p0 /* out */,
				      UINT64 *p1 /* out */,
				      UINT64 *p2 /* out */)
{
	/* even dividers */
	if (p % 2 == 0) {
		UINT64 half = p / 2;

		if (half == 1 || half == 2 || half == 3 || half == 5) {
			*p0 = 2;
			*p1 = 1;
			*p2 = half;
		} else if (half % 2 == 0) {
			*p0 = 2;
			*p1 = half / 2;
			*p2 = 2;
		} else if (half % 3 == 0) {
			*p0 = 3;
			*p1 = half / 3;
			*p2 = 2;
		} else if (half % 7 == 0) {
			*p0 = 7;
			*p1 = half / 7;
			*p2 = 2;
		}
	} else if (p == 3 || p == 9) {  /* 3, 5, 7, 9, 15, 21, 35 */
		*p0 = 3;
		*p1 = 1;
		*p2 = p / 3;
	} else if (p == 5 || p == 7) {
		*p0 = p;
		*p1 = 1;
		*p2 = 1;
	} else if (p == 15) {
		*p0 = 3;
		*p1 = 1;
		*p2 = 5;
	} else if (p == 21) {
		*p0 = 7;
		*p1 = 1;
		*p2 = 3;
	} else if (p == 35) {
		*p0 = 7;
		*p1 = 1;
		*p2 = 5;
	}
}

#define KHz(x) (1000 * (x))
#define MHz(x) KHz(1000 * (x))

static void skl_wrpll_params_populate(struct skl_wrpll_params *params,
				      UINT64 afe_clock,
				      UINT64 central_freq,
				      UINT64 p0, UINT64 p1, UINT64 p2)
{
	UINT64 dco_freq;

	switch (central_freq) {
	case 9600000000ULL:
		params->central_freq = 0;
		break;
	case 9000000000ULL:
		params->central_freq = 1;
		break;
	case 8400000000ULL:
		params->central_freq = 3;
	}

	switch (p0) {
	case 1:
		params->pdiv = 0;
		break;
	case 2:
		params->pdiv = 1;
		break;
	case 3:
		params->pdiv = 2;
		break;
	case 7:
		params->pdiv = 4;
		break;
	default:
		DebugPrint(EFI_D_ERROR,"Incorrect PDiv\n");
	}

	switch (p2) {
	case 5:
		params->kdiv = 0;
		break;
	case 2:
		params->kdiv = 1;
		break;
	case 3:
		params->kdiv = 2;
		break;
	case 1:
		params->kdiv = 3;
		break;
	default:
		DebugPrint(EFI_D_ERROR,"Incorrect KDiv\n");
	}

	params->qdiv_ratio = p1;
	params->qdiv_mode = (params->qdiv_ratio == 1) ? 0 : 1;

	dco_freq = p0 * p1 * p2 * afe_clock;

	/*
	 * Intermediate values are in Hz.
	 * Divide by MHz to match bsepc
	 */
	params->dco_integer = (dco_freq)/(24 * MHz(1));
	params->dco_fraction = (((dco_freq)/(24) - params->dco_integer * MHz(1)) * 0x8000)/(MHz(1));
}

/* DCO freq must be within +1%/-6%  of the DCO central freq */
#define SKL_DCO_MAX_PDEVIATION	100
#define SKL_DCO_MAX_NDEVIATION	600

static void skl_wrpll_try_divider(struct skl_wrpll_context *ctx,
				  UINT64 central_freq,
				  UINT64 dco_freq,
				  UINT64 divider)
{
	UINT64 deviation;
	INT64 abs_diff=(INT64)dco_freq-(INT64)central_freq;
	if(abs_diff<0){abs_diff=-abs_diff;}

	deviation = (10000 * (UINT64)abs_diff)/(central_freq);

	/* positive deviation */
	if (dco_freq >= central_freq) {
		if (deviation < SKL_DCO_MAX_PDEVIATION &&
		    deviation < ctx->min_deviation) {
			ctx->min_deviation = deviation;
			ctx->central_freq = central_freq;
			ctx->dco_freq = dco_freq;
			ctx->p = divider;
		}
	/* negative deviation */
	} else if (deviation < SKL_DCO_MAX_NDEVIATION &&
		   deviation < ctx->min_deviation) {
		ctx->min_deviation = deviation;
		ctx->central_freq = central_freq;
		ctx->dco_freq = dco_freq;
		ctx->p = divider;
	}
}

static UINT32 port=PORT_B;
static EFI_SYSTEM_TABLE     *g_SystemTable=NULL;
STATIC EFI_STATUS EFIAPI i915GraphicsOutputSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN  UINT32                       ModeNumber
  )
{

	DebugPrint(EFI_D_ERROR,"i915: set mode %u\n",ModeNumber);
	if(g_already_set){
		DebugPrint(EFI_D_ERROR,"i915: mode already set\n");
		return EFI_SUCCESS;
	}
	g_already_set=1;
	
	write32(_PIPEACONF,0);
	
	//setup DPLL (old GPU, doesn't apply here)
	//UINT32 refclock = 96000;
	//UINT32 pixel_clock = (UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock) * 10;
	//UINT32 multiplier = 1;
	////if(pixel_clock >= 100000) {
	////	multiplier = 1;
	////}else if(pixel_clock >= 50000) {
	////	multiplier = 2;
	////}else{
	////	//assert(pixel_clock >= 25000);
	////	multiplier = 4;
	////}
	//struct dpll final_params,params;
	//INT32 target=(INT32)(pixel_clock * multiplier);
	//INT32 best_err=target;
	//DebugPrint(EFI_D_ERROR,"i915: before DPLL compute\n");
	//for(params.n=g_limits.n.min;params.n<=g_limits.n.max;params.n++)
	//for(params.m1=g_limits.m1.max;params.m1>=g_limits.m1.min;params.m1--)
	//for(params.m2=g_limits.m2.max;params.m2>=g_limits.m2.min;params.m2--)
	//for(params.p1=g_limits.p1.max;params.p1>=g_limits.p1.min;params.p1--)
	//for(params.p2=g_limits.p2.p2_slow;params.p2>=g_limits.p2.p2_fast;params.p2-=5){
	//	if(params.p2!=5&&params.p2!=7&&params.p2!=10&&params.p2!=14){continue;}
	//	params.m = 5 * (params.m1 + 2) + (params.m2 + 2);
	//	params.p = params.p1*params.p2;
	//	if(params.m < g_limits.m.min || params.m > g_limits.m.max){continue;}
	//	if(params.p < g_limits.p.min || params.p > g_limits.p.max){continue;}
	//	params.vco = (refclock * params.m + (params.n + 2) / 2) / (params.n + 2);
	//	params.dot = (params.vco + params.p / 2) / params.p;
	//	if(params.dot < g_limits.dot.min || params.dot > g_limits.dot.max){continue;}
	//	if(params.vco < g_limits.vco.min || params.vco > g_limits.vco.max){continue;}
	//	INT32 err=(INT32)params.dot-target;
	//	if(err<0){err=-err;}
	//	if(best_err>err){
	//		best_err=err;
	//		final_params=params;
	//	}
	//}
	
	//params=final_params;
	
	//DebugPrint(EFI_D_ERROR,"i915: DPLL params: n=%d m1=%d m2=%d p1=%d p2=%d\n",
	//	params.n,params.m1,params.m2,params.p1,params.p2);
	//DebugPrint(EFI_D_ERROR,"i915: DPLL params: m=%d p=%d vco=%d dot=%d, target=%d\n",
	//	params.m,params.p,params.vco,params.dot,target);
	
	//write32(_FPA0, params.n << 16 | params.m1 << 8 | params.m2);
	//write32(_FPA1, params.n << 16 | params.m1 << 8 | params.m2);
	
	//write32(_DPLL_A, 0);
	
	////UINT32 dplla=DPLLB_MODE_DAC_SERIAL | DPLL_VGA_MODE_DIS | DPLL_SDVO_HIGH_SPEED | DPLL_VCO_ENABLE;
	//UINT32 dplla=DPLLB_MODE_DAC_SERIAL | DPLL_VGA_MODE_DIS | DPLL_VCO_ENABLE;
	//dplla |= (1 << (params.p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT;
	//switch (params.p2) {
	//case 5:
	//	dplla |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_5;
	//	break;
	//case 7:
	//	dplla |= DPLLB_LVDS_P2_CLOCK_DIV_7;
	//	break;
	//case 10:
	//	dplla |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_10;
	//	break;
	//case 14:
	//	dplla |= DPLLB_LVDS_P2_CLOCK_DIV_14;
	//	break;
	//}
	//dplla |= (6 << PLL_LOAD_PULSE_PHASE_SHIFT);
	////this is 0 anyway
	//dplla |= PLL_REF_INPUT_DREFCLK;
	
	//write32(_DPLL_A, dplla);
	//read32(_DPLL_A);
	//DebugPrint(EFI_D_ERROR,"i915: DPLL set %08x, read %08x\n",dplla,read32(_DPLL_A));
	
	////it's pointless to wait in GVT-g
	//if(!controller->is_gvt){
	//	//MicroSecondDelay is unusable
	//	for(UINT32 counter=0;counter<16384;counter++){
	//		read32(_DPLL_A);
	//	}
	//}
	
	//write32(_DPLL_A_MD, (multiplier-1)<<DPLL_MD_UDI_MULTIPLIER_SHIFT);
	//DebugPrint(EFI_D_ERROR,"i915: DPLL_MD set\n");
	
	//for(int i = 0; i < 3; i++) {
	//	write32(_DPLL_A, dplla);
	//	read32(_DPLL_A);
	
	//	if(!controller->is_gvt){
	//		for(UINT32 counter=0;counter<16384;counter++){
	//			read32(_DPLL_A);
	//		}
	//	}
	//}
	//DebugPrint(EFI_D_ERROR,"i915: DPLL all set %08x, read %08x\n",dplla,read32(_DPLL_A));
	
	//SkyLake shared DPLL sequence: it's completely different!
	/* DPLL 1 */
	//.ctl = LCPLL2_CTL,
	//.cfgcr1 = _DPLL1_CFGCR1,
	//.cfgcr2 = _DPLL1_CFGCR2,
	
	//intel_encoders_pre_pll_enable(crtc, pipe_config, old_state);
	
	UINT32 ctrl1, cfgcr1, cfgcr2;
	struct skl_wrpll_params wrpll_params = { 0, };
	
	/*
	 * See comment in intel_dpll_hw_state to understand why we always use 0
	 * as the DPLL id in this function.
	 */
	ctrl1 = DPLL_CTRL1_OVERRIDE(0);
	ctrl1 |= DPLL_CTRL1_HDMI_MODE(0);
	
	{
		//clock in Hz
		UINT64 clock=(UINT64)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock)*10000;
		UINT64 afe_clock = clock * 5; /* AFE Clock is 5x Pixel clock */
		UINT64 dco_central_freq[3] = { 8400000000ULL, 9000000000ULL, 9600000000ULL };
		
		struct skl_wrpll_context ctx={0};
		UINT64 dco, d, i;
		UINT64 p0, p1, p2;
	
		ctx.min_deviation = 1ULL<<62;
	
		for (d = 0; d < ARRAY_SIZE(dividers); d++) {
			for (dco = 0; dco < ARRAY_SIZE(dco_central_freq); dco++) {
				for (i = 0; i < dividers[d].n_dividers; i++) {
					UINT64 p = dividers[d].list[i];
					UINT64 dco_freq = p * afe_clock;
	
					skl_wrpll_try_divider(&ctx,
							      dco_central_freq[dco],
							      dco_freq,
							      p);
					/*
					 * Skip the remaining dividers if we're sure to
					 * have found the definitive divider, we can't
					 * improve a 0 deviation.
					 */
					if (ctx.min_deviation == 0)
						goto skip_remaining_dividers;
				}
			}

	skip_remaining_dividers:
			/*
			 * If a solution is found with an even divider, prefer
			 * this one.
			 */
			if (d == 0 && ctx.p)
				break;
		}
		
		if (!ctx.p) {
			DebugPrint(EFI_D_ERROR,"i915: No valid divider found for %dHz\n", clock);
			return EFI_UNSUPPORTED;
		}
		
		/*
		 * gcc incorrectly analyses that these can be used without being
		 * initialized. To be fair, it's hard to guess.
		 */
		p0 = p1 = p2 = 0;
		skl_wrpll_get_multipliers(ctx.p, &p0, &p1, &p2);
		skl_wrpll_params_populate(&wrpll_params, afe_clock, ctx.central_freq,
					  p0, p1, p2);
	}
	
	cfgcr1 = DPLL_CFGCR1_FREQ_ENABLE |
		DPLL_CFGCR1_DCO_FRACTION(wrpll_params.dco_fraction) |
		wrpll_params.dco_integer;
	
	cfgcr2 = DPLL_CFGCR2_QDIV_RATIO(wrpll_params.qdiv_ratio) |
		DPLL_CFGCR2_QDIV_MODE(wrpll_params.qdiv_mode) |
		DPLL_CFGCR2_KDIV(wrpll_params.kdiv) |
		DPLL_CFGCR2_PDIV(wrpll_params.pdiv) |
		wrpll_params.central_freq;
	
	UINT32 val = read32(DPLL_CTRL1);
	
	//it's clock id!
	//how's port clock comptued?
	//UINT64 clock_khz=(UINT64)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].pixelClock)*10;
	//UINT32 id=DPLL_CTRL1_LINK_RATE_810;
	//if(clock_khz>>1 >=135000){
	//	id=DPLL_CTRL1_LINK_RATE_1350;
	//}else if(clock_khz>>1 >=270000){
	//	id=DPLL_CTRL1_LINK_RATE_2700;
	//}
	//hack: anything else hangs
	UINT32 id=DPLL_CTRL1_LINK_RATE_1350;
	
	val &= ~(DPLL_CTRL1_HDMI_MODE(id) |
		 DPLL_CTRL1_SSC(id) |
		 DPLL_CTRL1_LINK_RATE_MASK(id));
	val |= ctrl1 << (id * 6);
	
	//DPLL 1
	write32(DPLL_CTRL1, val);
	read32(DPLL_CTRL1);
	
	write32(_DPLL1_CFGCR1, cfgcr1);
	write32(_DPLL1_CFGCR2, cfgcr2);
	read32(_DPLL1_CFGCR1);
	read32(_DPLL1_CFGCR2);
	
	//845 80400173 3a5
	DebugPrint(EFI_D_ERROR,"i915: DPLL_CTRL1 = %08x\n", read32(DPLL_CTRL1));
	DebugPrint(EFI_D_ERROR,"i915: _DPLL1_CFGCR1 = %08x\n", read32(_DPLL1_CFGCR1));
	DebugPrint(EFI_D_ERROR,"i915: _DPLL1_CFGCR2 = %08x\n", read32(_DPLL1_CFGCR2));
	
	/* the enable bit is always bit 31 */
	write32(LCPLL2_CTL, read32(LCPLL2_CTL) | LCPLL_PLL_ENABLE);
	
	for(UINT32 counter=0;;counter++){
		if(read32(DPLL_STATUS)&DPLL_LOCK(1)){
			DebugPrint(EFI_D_ERROR,"i915: DPLL %d locked\n", 1);
			break;
		}
		if(counter>16384){
			DebugPrint(EFI_D_ERROR,"i915: DPLL %d not locked\n", 1);
			break;
		}
	}
		
	//intel_encoders_pre_enable(crtc, pipe_config, old_state);
	//could be intel_ddi_pre_enable_hdmi
	//intel_ddi_clk_select(encoder, crtc_state);
	DebugPrint(EFI_D_ERROR,"i915: port is %d\n", port);
	{
		UINT32 val = read32(DPLL_CTRL2);
		
		//val &= ~(DPLL_CTRL2_DDI_CLK_OFF(PORT_A) |
		//	 DPLL_CTRL2_DDI_CLK_SEL_MASK(PORT_A));
		//val |= (DPLL_CTRL2_DDI_CLK_SEL(id, PORT_A) |
		//	DPLL_CTRL2_DDI_SEL_OVERRIDE(PORT_A));
		
		val &= ~(DPLL_CTRL2_DDI_CLK_OFF(port) |
			 DPLL_CTRL2_DDI_CLK_SEL_MASK(port));
		val |= (DPLL_CTRL2_DDI_CLK_SEL(id, port) |
			DPLL_CTRL2_DDI_SEL_OVERRIDE(port));
		
		write32(DPLL_CTRL2, val);
	}
	DebugPrint(EFI_D_ERROR,"i915: DPLL_CTRL2 = %08x\n", read32(DPLL_CTRL2));
	
	//intel_prepare_hdmi_ddi_buffers(encoder, level);
	//the driver doesn't seem to do this for port A
	write32(DDI_BUF_TRANS_LO(port, 9), 0x80003015u);
	write32(DDI_BUF_TRANS_HI(port, 9), 0xcdu);
	
	//intel_hdmi_prepare(encoder, pipe_config);
	//hdmi_reg=DDI_BUF_CTL(port)
	
	DebugPrint(EFI_D_ERROR,"i915: progressed to line %d\n", __LINE__);
	
	//it's Type C
	//icl_enable_phy_clock_gating(dig_port);
	
	//if (IS_GEN9_BC(dev_priv))
	//	skl_ddi_set_iboost(encoder, level, INTEL_OUTPUT_HDMI);
	{
		UINT32 tmp;
	
		tmp = read32(DISPIO_CR_TX_BMU_CR0);
		tmp &= ~(BALANCE_LEG_MASK(port) | BALANCE_LEG_DISABLE(port));
		tmp |= 1 << BALANCE_LEG_SHIFT(port);
		write32(DISPIO_CR_TX_BMU_CR0, tmp);
	}
	
	//intel_ddi_enable_pipe_clock(crtc_state);
	write32(_TRANS_CLK_SEL_A, TRANS_CLK_SEL_PORT(port));
	DebugPrint(EFI_D_ERROR,"i915: progressed to line %d, TRANS_CLK_SEL_PORT(port) is %08x\n", __LINE__, TRANS_CLK_SEL_PORT(port));
	
	//we got here	
	
	//intel_dig_port->set_infoframes(encoder,
	//			       crtc_state->has_infoframe,
	//			       crtc_state, conn_state);
	
	//if (intel_crtc_has_dp_encoder(pipe_config))
	//	intel_dp_set_m_n(pipe_config, M1_N1);
	
	//program PIPE_A
	UINT32 horz_active = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActive
			| ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb >> 4) << 8);
	UINT32 horz_blank = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzBlank
			| ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzActiveBlankMsb & 0xF) << 8);
	UINT32 horz_sync_offset = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzSyncOffset
			| ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 6) << 8);
	UINT32 horz_sync_pulse = controller->edid.detailTimings[DETAIL_TIME_SELCTION].horzSyncPulse
			| (((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 4) & 0x3) << 8);
	
	UINT32 horizontal_active = horz_active;
	UINT32 horizontal_syncStart = horz_active + horz_sync_offset;
	UINT32 horizontal_syncEnd = horz_active + horz_sync_offset + horz_sync_pulse;
	UINT32 horizontal_total = horz_active + horz_blank;
	
	UINT32 vert_active =  controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActive
			| ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb >> 4) << 8);
	UINT32 vert_blank = controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertBlank
			| ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertActiveBlankMsb & 0xF) << 8);
	UINT32 vert_sync_offset = (controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertSync >> 4)
			| (((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb >> 2) & 0x3) << 4);
	UINT32 vert_sync_pulse = (controller->edid.detailTimings[DETAIL_TIME_SELCTION].vertSync & 0xF)
			| ((UINT32)(controller->edid.detailTimings[DETAIL_TIME_SELCTION].syncMsb & 0x3) << 4);
	
	UINT32 vertical_active = vert_active;
	UINT32 vertical_syncStart = vert_active + vert_sync_offset;
	UINT32 vertical_syncEnd = vert_active + vert_sync_offset + vert_sync_pulse;
	UINT32 vertical_total = vert_active + vert_blank;
	
	write32(VSYNCSHIFT_A, 0);
	
	write32(HTOTAL_A,
		   (horizontal_active - 1) |
		   ((horizontal_total - 1) << 16));
	write32(HBLANK_A,
		   (horizontal_active - 1) |
		   ((horizontal_total - 1) << 16));
	write32(HSYNC_A,
		   (horizontal_syncStart - 1) |
		   ((horizontal_syncEnd - 1) << 16));
	
	write32(VTOTAL_A,
		   (vertical_active - 1) |
		   ((vertical_total - 1) << 16));
	write32(VBLANK_A,
		   (vertical_active - 1) |
		   ((vertical_total - 1) << 16));
	write32(VSYNC_A,
		   (vertical_syncStart - 1) |
		   ((vertical_syncEnd - 1) << 16));
	
	write32(PIPEASRC,((horizontal_active-1)<<16)|(vertical_active-1));
	UINT32 multiplier=1;
	write32(PIPE_MULT_A, multiplier - 1);
	
	DebugPrint(EFI_D_ERROR,"i915: HTOTAL_A (%x) = %08x\n",HTOTAL_A,read32(HTOTAL_A));
	DebugPrint(EFI_D_ERROR,"i915: HBLANK_A (%x) = %08x\n",HBLANK_A,read32(HBLANK_A));
	DebugPrint(EFI_D_ERROR,"i915: HSYNC_A (%x) = %08x\n",HSYNC_A,read32(HSYNC_A));
	DebugPrint(EFI_D_ERROR,"i915: VTOTAL_A (%x) = %08x\n",VTOTAL_A,read32(VTOTAL_A));
	DebugPrint(EFI_D_ERROR,"i915: VBLANK_A (%x) = %08x\n",VBLANK_A,read32(VBLANK_A));
	DebugPrint(EFI_D_ERROR,"i915: VSYNC_A (%x) = %08x\n",VSYNC_A,read32(VSYNC_A));
	DebugPrint(EFI_D_ERROR,"i915: PIPEASRC (%x) = %08x\n",PIPEASRC,read32(PIPEASRC));
	DebugPrint(EFI_D_ERROR,"i915: BCLRPAT_A (%x) = %08x\n",BCLRPAT_A,read32(BCLRPAT_A));
	DebugPrint(EFI_D_ERROR,"i915: VSYNCSHIFT_A (%x) = %08x\n",VSYNCSHIFT_A,read32(VSYNCSHIFT_A));
	DebugPrint(EFI_D_ERROR,"i915: PIPE_MULT_A (%x) = %08x\n",PIPE_MULT_A,read32(PIPE_MULT_A));
	
	DebugPrint(EFI_D_ERROR,"i915: before pipe gamma\n");
	
	//intel_color_load_luts(pipe_config);
	//intel_color_commit(pipe_config);
	DebugPrint(EFI_D_ERROR,"i915: before gamma\n");	
	for (UINT32 i = 0; i < 256; i++) {
		UINT32 word = (i << 16) | (i << 8) | i;
		write32(_LGC_PALETTE_A+i*4, word);
	}
	DebugPrint(EFI_D_ERROR,"i915: before pipe gamma\n");
	//DebugPrint(EFI_D_ERROR,"i915: _PIPEACONF: %08x\n",read32(_PIPEACONF));
	//g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);	
	//return EFI_UNSUPPORTED;
	write32(_PIPEACONF,PIPECONF_PROGRESSIVE|PIPECONF_GAMMA_MODE_8BIT);
	//write32(_SKL_BOTTOM_COLOR_A,SKL_BOTTOM_COLOR_GAMMA_ENABLE);
	//write32(_SKL_BOTTOM_COLOR_A,0);
	//write32(_SKL_BOTTOM_COLOR_A,0x335577);
	write32(_SKL_BOTTOM_COLOR_A,0);
	write32(_GAMMA_MODE_A,GAMMA_MODE_MODE_8BIT);
	
	//bad setup causes hanging when enabling trans / pipe, but what is it?
	//we got here
	//ddi
	DebugPrint(EFI_D_ERROR,"i915: before DDI\n");	
	write32(_TRANSA_MSA_MISC, TRANS_MSA_SYNC_CLK|TRANS_MSA_8_BPC);
	write32(_TRANS_DDI_FUNC_CTL_A, (
		TRANS_DDI_FUNC_ENABLE|TRANS_DDI_SELECT_PORT(port)|TRANS_DDI_PHSYNC|TRANS_DDI_PVSYNC|TRANS_DDI_BPC_8|TRANS_DDI_MODE_SELECT_HDMI
	));
	DebugPrint(EFI_D_ERROR,"i915: after DDI\n");
	//g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);	
	//return EFI_UNSUPPORTED;
	
	//test: could be Windows hanging, it's not
	//g_SystemTable->RuntimeServices->ResetSystem(EfiResetShutdown,0,0,NULL);
	//we failed here
	//return EFI_UNSUPPORTED;
	
	write32(_PIPEACONF,PIPECONF_ENABLE|PIPECONF_PROGRESSIVE|PIPECONF_GAMMA_MODE_8BIT);
	UINT32 counter=0;
	for(;;){
		counter+=1;
		if(counter>=16384){
			DebugPrint(EFI_D_ERROR,"i915: failed to enable PIPE\n");
			break;
		}
		if(read32(_PIPEACONF)&I965_PIPECONF_ACTIVE){
			DebugPrint(EFI_D_ERROR,"i915: pipe enabled\n");
			break;
		}
	}
	
	//if (pipe_config->has_pch_encoder)
	//	lpt_pch_enable(old_intel_state, pipe_config);
	
	//if (intel_crtc_has_type(pipe_config, INTEL_OUTPUT_DP_MST))
	//	intel_ddi_set_vc_payload_alloc(pipe_config, true);
	
	//intel_encoders_enable(crtc, pipe_config, old_state);
	/* Display WA #1143: skl,kbl,cfl */
	UINT32 saved_port_bits = I915_READ(DDI_BUF_CTL(port)) & (DDI_BUF_PORT_REVERSAL | DDI_A_4_LANES);
	
	//if (IS_GEN9_BC(dev_priv)) 
	{
		/*
		 * For some reason these chicken bits have been
		 * stuffed into a transcoder register, event though
		 * the bits affect a specific DDI port rather than
		 * a specific transcoder.
		 */
		UINT32 reg = CHICKEN_TRANS_A;
		if(port==PORT_B){reg = CHICKEN_TRANS_A;}
		if(port==PORT_C){reg = CHICKEN_TRANS_B;}
		if(port==PORT_D){reg = CHICKEN_TRANS_C;}
		//if(port==PORT_E){reg = CHICKEN_TRANS_A;}
		UINT32 val;
	
		val = I915_READ(reg);
	
		if (port == PORT_E)
			val |= DDIE_TRAINING_OVERRIDE_ENABLE |
				DDIE_TRAINING_OVERRIDE_VALUE;
		else
			val |= DDI_TRAINING_OVERRIDE_ENABLE |
				DDI_TRAINING_OVERRIDE_VALUE;
	
		I915_WRITE(reg, val);
		read32(reg);
	
		//... don't have timer
		for(UINT32 counter=0;;){
			read32(reg);
			counter+=1;
			if(counter>=16384){
				break;
			}
		}
		//udelay(1);
	
		if (port == PORT_E)
			val &= ~(DDIE_TRAINING_OVERRIDE_ENABLE |
				 DDIE_TRAINING_OVERRIDE_VALUE);
		else
			val &= ~(DDI_TRAINING_OVERRIDE_ENABLE |
				 DDI_TRAINING_OVERRIDE_VALUE);
	
		I915_WRITE(reg, val);
	}
	
	/* In HDMI/DVI mode, the port width, and swing/emphasis values
	 * are ignored so nothing special needs to be done besides
	 * enabling the port.
	 */
	I915_WRITE(DDI_BUF_CTL(port), saved_port_bits | DDI_BUF_CTL_ENABLE);
	DebugPrint(EFI_D_ERROR,"DDI_BUF_CTL(port) = %08x\n",read32(DDI_BUF_CTL(port)));
	
	//plane
	UINT32 stride=(horizontal_active*4+63)&-64;
	controller->stride=stride;
	write32(_DSPAOFFSET,0);
	write32(_DSPAPOS,0);
	write32(_DSPASTRIDE,stride>>6);
	write32(_DSPASIZE,(horizontal_active - 1) | ((vertical_active-1)<<16));
	write32(_DSPACNTR,DISPLAY_PLANE_ENABLE|PLANE_CTL_FORMAT_XRGB_8888|PLANE_CTL_PLANE_GAMMA_DISABLE);
	write32(_DSPASURF,controller->gmadr);
	
	//write32(_DSPAADDR,0);
	//word=read32(_DSPACNTR);
	//write32(_DSPACNTR,(word&~PLANE_CTL_FORMAT_MASK)|DISPLAY_PLANE_ENABLE|PLANE_CTL_FORMAT_XRGB_8888);
	//|PLANE_CTL_ORDER_RGBX
	
	DebugPrint(EFI_D_ERROR,"i915: plane enabled, dspcntr: %08x, FbBase: %p\n",read32(_DSPACNTR),controller->FbBase);
	
	i915GraphicsFrambufferConfigure(controller, stride*vertical_active);
	
	return EFI_SUCCESS;
}