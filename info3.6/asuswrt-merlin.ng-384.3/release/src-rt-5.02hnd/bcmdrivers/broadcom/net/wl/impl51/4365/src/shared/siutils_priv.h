/*
 * Include file private to the SOC Interconnect support files.
 *
 * Copyright (C) 2017, Broadcom. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: siutils_priv.h 580000 2015-08-17 22:08:34Z $
 */

#ifndef	_siutils_priv_h_
#define	_siutils_priv_h_

#ifdef BCMDBG
#define	SI_ERROR(args)	printf args
#else
#define	SI_ERROR(args)
#endif	/* BCMDBG */

#ifdef BCMDBG
#define	SI_MSG(args)	printf args
#else
#define	SI_MSG(args)
#endif	/* BCMDBG */

#ifdef BCMDBG_SI
#define	SI_VMSG(args)	printf args
#else
#define	SI_VMSG(args)
#endif

#define SI_PRINT(args)	printf args

#define	IS_SIM(chippkg)	((chippkg == HDLSIM_PKG_ID) || (chippkg == HWSIM_PKG_ID))

typedef uint32 (*si_intrsoff_t)(void *intr_arg);
typedef void (*si_intrsrestore_t)(void *intr_arg, uint32 arg);
typedef bool (*si_intrsenabled_t)(void *intr_arg);


#define SI_GPIO_MAX		16

typedef struct gci_gpio_item {
	void			*arg;
	uint8			gci_gpio;
	uint8			status;
	gci_gpio_handler_t	handler;
	struct gci_gpio_item	*next;
} gci_gpio_item_t;

#define AI_SLAVE_WRAPPER	0
#define AI_MASTER_WRAPPER	1

typedef struct axi_wrapper
{
	uint32	mfg;
	uint32	cid;
	uint32	rev;
	uint32	wrapper_type;
	uint32	wrapper_addr;
	uint32	wrapper_size;
} axi_wrapper_t;

#define SI_MAX_AXI_WRAPPERS	32

typedef struct si_cores_info {
	void	*regs[SI_MAXCORES];	/* other regs va */

	uint	coreid[SI_MAXCORES];	/* id of each core */
	uint32	coresba[SI_MAXCORES];	/* backplane address of each core */
	void	*regs2[SI_MAXCORES];	/* va of each core second register set (usbh20) */
	uint32	coresba2[SI_MAXCORES];	/* address of each core second register set (usbh20) */
	uint32	coresba_size[SI_MAXCORES]; /* backplane address space size */
	uint32	coresba2_size[SI_MAXCORES]; /* second address space size */

	void	*wrappers[SI_MAXCORES];	/* other cores wrapper va */
	uint32	wrapba[SI_MAXCORES];	/* address of controlling wrapper */

	void	*wrappers2[SI_MAXCORES];	/* other cores wrapper va */
	uint32	wrapba2[SI_MAXCORES];	/* address of controlling wrapper */

	uint32	cia[SI_MAXCORES];	/* erom cia entry for each core */
	uint32	cib[SI_MAXCORES];	/* erom cia entry for each core */
} si_cores_info_t;

/* misc si info needed by some of the routines */
typedef struct si_info {
	struct si_pub pub;		/* back plane public state (must be first field) */

	void	*osh;			/* osl os handle */
	void	*sdh;			/* bcmsdh handle */

	uint	dev_coreid;		/* the core provides driver functions */
	void	*intr_arg;		/* interrupt callback function arg */
	si_intrsoff_t intrsoff_fn;	/* turns chip interrupts off */
	si_intrsrestore_t intrsrestore_fn; /* restore chip interrupts */
	si_intrsenabled_t intrsenabled_fn; /* check if interrupts are enabled */

	void *pch;			/* PCI/E core handle */

	bool	memseg;			/* flag to toggle MEM_SEG register */

	char *vars;
	uint varsz;

	void	*curmap;		/* current regs va */

	uint	curidx;			/* current core index */
	uint	numcores;		/* # discovered cores */

	void	*curwrap;		/* current wrapper va */

	uint32	oob_router;		/* oob router registers for axi */

	void *cores_info;
	/* Store NVRAM data so that it is available after reclaim. */
	uint32 nvram_min_mask;
	bool min_mask_valid;
	uint32 nvram_max_mask;
	bool max_mask_valid;
	gci_gpio_item_t	*gci_gpio_head;	/* gci gpio interrupts head */
	uint	chipnew;		/* new chip number */
	uint second_bar0win;		/* Backplane region */
	uint	num_br;		/* # discovered bridges */
	uint32	br_wrapba[SI_MAXBR];	/* address of bridge controlling wrapper */
	uint32	axi_num_wrappers;
	axi_wrapper_t	axi_wrapper[SI_MAX_AXI_WRAPPERS];
	uint32	macclk_mul_fact;	/* Multiplication factor necessary to adjust MAC Clock
	* during ULB Mode operation. One instance where this is used is configuring TSF L-frac
	* register
	*/
} si_info_t;


#define	SI_INFO(sih)	((si_info_t *)(uintptr)sih)

#define	GOODCOREADDR(x, b) (((x) >= (b)) && ((x) < ((b) + SI_MAXCORES * SI_CORE_SIZE)) && \
		ISALIGNED((x), SI_CORE_SIZE))
#define	GOODREGS(regs)	((regs) != NULL && ISALIGNED((uintptr)(regs), SI_CORE_SIZE))
#define BADCOREADDR	0
#define	GOODIDX(idx)	(((uint)idx) < SI_MAXCORES)
#define	NOREV		-1		/* Invalid rev */

#define PCI(si)		((BUSTYPE((si)->pub.bustype) == PCI_BUS) &&	\
			 ((si)->pub.buscoretype == PCI_CORE_ID))

#define PCIE_GEN1(si)	((BUSTYPE((si)->pub.bustype) == PCI_BUS) &&	\
			 ((si)->pub.buscoretype == PCIE_CORE_ID))

#define PCIE_GEN2(si)	((BUSTYPE((si)->pub.bustype) == PCI_BUS) &&	\
			 ((si)->pub.buscoretype == PCIE2_CORE_ID))

#define PCIE(si)	(PCIE_GEN1(si) || PCIE_GEN2(si))

#define PCMCIA(si)	((BUSTYPE((si)->pub.bustype) == PCMCIA_BUS) && ((si)->memseg == TRUE))

/* Newer chips can access PCI/PCIE and CC core without requiring to change
 * PCI BAR0 WIN
 */
#define SI_FAST(si) (PCIE(si) || (PCI(si) && ((si)->pub.buscorerev >= 13)))

#define PCIEREGS(si) (((char *)((si)->curmap) + PCI_16KB0_PCIREGS_OFFSET))
#define CCREGS_FAST(si) \
	(((si)->curmap == NULL) ? NULL : ((char *)((si)->curmap) + PCI_16KB0_CCREGS_OFFSET))

/*
 * Macros to disable/restore function core(D11, ENET, ILINE20, etc) interrupts before/
 * after core switching to avoid invalid register accesss inside ISR.
 */
#define INTR_OFF(si, intr_val) \
	if ((si)->intrsoff_fn && (cores_info)->coreid[(si)->curidx] == (si)->dev_coreid) {	\
		intr_val = (*(si)->intrsoff_fn)((si)->intr_arg); }
#define INTR_RESTORE(si, intr_val) \
	if ((si)->intrsrestore_fn && (cores_info)->coreid[(si)->curidx] == (si)->dev_coreid) {	\
		(*(si)->intrsrestore_fn)((si)->intr_arg, intr_val); }

/* dynamic clock control defines */
#define	LPOMINFREQ		25000		/* low power oscillator min */
#define	LPOMAXFREQ		43000		/* low power oscillator max */
#define	XTALMINFREQ		19800000	/* 20 MHz - 1% */
#define	XTALMAXFREQ		20200000	/* 20 MHz + 1% */
#define	PCIMINFREQ		25000000	/* 25 MHz */
#define	PCIMAXFREQ		34000000	/* 33 MHz + fudge */

#define	ILP_DIV_5MHZ		0		/* ILP = 5 MHz */
#define	ILP_DIV_1MHZ		4		/* ILP = 1 MHz */

/* Force fast clock for 4360b0 */
#define PCI_FORCEHT(si)	\
	(((PCIE_GEN1(si)) && (si->pub.chip == BCM4311_CHIP_ID) && ((si->pub.chiprev <= 1))) || \
	((PCI(si) || PCIE_GEN1(si)) && (si->pub.chip == BCM4321_CHIP_ID)) || \
	(PCIE_GEN1(si) && (si->pub.chip == BCM4716_CHIP_ID)) || \
	(PCIE_GEN1(si) && (si->pub.chip == BCM4748_CHIP_ID)))

/* GPIO Based LED powersave defines */
#define DEFAULT_GPIO_ONTIME	10		/* Default: 10% on */
#define DEFAULT_GPIO_OFFTIME	90		/* Default: 10% on */

#ifndef DEFAULT_GPIOTIMERVAL
#define DEFAULT_GPIOTIMERVAL  ((DEFAULT_GPIO_ONTIME << GPIO_ONTIME_SHIFT) | DEFAULT_GPIO_OFFTIME)
#endif

/* Silicon Backplane externs */
extern void sb_scan(si_t *sih, void *regs, uint devid);
extern uint sb_coreid(si_t *sih);
extern uint sb_intflag(si_t *sih);
extern uint sb_flag(si_t *sih);
extern void sb_setint(si_t *sih, int siflag);
extern uint sb_corevendor(si_t *sih);
extern uint sb_corerev(si_t *sih);
extern uint sb_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val);
extern uint32 *sb_corereg_addr(si_t *sih, uint coreidx, uint regoff);
extern bool sb_iscoreup(si_t *sih);
extern void *sb_setcoreidx(si_t *sih, uint coreidx);
extern uint32 sb_core_cflags(si_t *sih, uint32 mask, uint32 val);
extern void sb_core_cflags_wo(si_t *sih, uint32 mask, uint32 val);
extern uint32 sb_core_sflags(si_t *sih, uint32 mask, uint32 val);
extern void sb_commit(si_t *sih);
extern uint32 sb_base(uint32 admatch);
extern uint32 sb_size(uint32 admatch);
extern void sb_core_reset(si_t *sih, uint32 bits, uint32 resetbits);
extern void sb_core_disable(si_t *sih, uint32 bits);
extern uint32 sb_addrspace(si_t *sih, uint asidx);
extern uint32 sb_addrspacesize(si_t *sih, uint asidx);
extern int sb_numaddrspaces(si_t *sih);

extern uint32 sb_set_initiator_to(si_t *sih, uint32 to, uint idx);

extern bool sb_taclear(si_t *sih, bool details);

#ifdef BCMDBG
extern void sb_view(si_t *sih, bool verbose);
extern void sb_viewall(si_t *sih, bool verbose);
#endif
#if defined(BCMDBG_DUMP)
extern void sb_dump(si_t *sih, struct bcmstrbuf *b);
#endif
#if defined(BCMDBG) || defined(BCMDBG_DUMP)|| defined(BCMDBG_PHYDUMP)
extern void sb_dumpregs(si_t *sih, struct bcmstrbuf *b);
#endif /* BCMDBG || BCMDBG_DUMP|| BCMDBG_PHYDUMP */

/* Wake-on-wireless-LAN (WOWL) */
extern bool sb_pci_pmecap(si_t *sih);
struct osl_info;
extern bool sb_pci_fastpmecap(struct osl_info *osh);
extern bool sb_pci_pmeclr(si_t *sih);
extern void sb_pci_pmeen(si_t *sih);
extern uint sb_pcie_readreg(void *sih, uint addrtype, uint offset);

/* AMBA Interconnect exported externs */
extern si_t *ai_attach(uint pcidev, osl_t *osh, void *regs, uint bustype,
                       void *sdh, char **vars, uint *varsz);
extern si_t *ai_kattach(osl_t *osh);
extern void ai_scan(si_t *sih, void *regs, uint devid);

extern uint ai_flag(si_t *sih);
extern uint ai_flag_alt(si_t *sih);
extern void ai_setint(si_t *sih, int siflag);
extern uint ai_coreidx(si_t *sih);
extern uint ai_corevendor(si_t *sih);
extern uint ai_corerev(si_t *sih);
extern uint32 *ai_corereg_addr(si_t *sih, uint coreidx, uint regoff);
extern bool ai_iscoreup(si_t *sih);
extern void *ai_setcoreidx(si_t *sih, uint coreidx);
extern void *ai_setcoreidx_2ndwrap(si_t *sih, uint coreidx);
extern uint32 ai_core_cflags(si_t *sih, uint32 mask, uint32 val);
extern void ai_core_cflags_wo(si_t *sih, uint32 mask, uint32 val);
extern uint32 ai_core_sflags(si_t *sih, uint32 mask, uint32 val);
extern uint ai_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val);
extern void ai_core_reset(si_t *sih, uint32 bits, uint32 resetbits);
extern void ai_d11rsdb_core_reset(si_t *sih, uint32 bits,
	uint32 resetbits, void *p, void *s);
extern void ai_d11rsdb_core1_alt_reg_clk_en(si_t *sih);
extern void ai_d11rsdb_core1_alt_reg_clk_dis(si_t *sih);

extern void ai_core_disable(si_t *sih, uint32 bits);
extern void ai_d11rsdb_core_disable(const si_info_t *sii, uint32 bits,
	aidmp_t *pmacai, aidmp_t *smacai);
extern int ai_numaddrspaces(si_t *sih);
extern uint32 ai_addrspace(si_t *sih, uint asidx);
extern uint32 ai_addrspacesize(si_t *sih, uint asidx);
extern void ai_coreaddrspaceX(si_t *sih, uint asidx, uint32 *addr, uint32 *size);
extern uint ai_wrap_reg(si_t *sih, uint32 offset, uint32 mask, uint32 val);
extern void ai_enable_backplane_timeouts(si_t *sih);
extern uint32 ai_clear_backplane_to(si_t *sih);

#ifdef BCM_BACKPLANE_TIMEOUT
uint32 ai_clear_backplane_to_fast(si_t *sih, void * addr);
#endif /* BCM_BACKPLANE_TIMEOUT */

#if defined(AXI_TIMEOUTS) || defined(BCM_BACKPLANE_TIMEOUT)
extern uint32 ai_clear_backplane_to_per_core(si_t *sih, uint coreid, uint coreunit, void * wrap);
#endif /* AXI_TIMEOUTS || BCM_BACKPLANE_TIMEOUT */

#ifdef BCMDBG
extern void ai_view(si_t *sih, bool verbose);
extern void ai_viewall(si_t *sih, bool verbose);
#endif
#if defined(BCMDBG) || defined(BCMDBG_DUMP)|| defined(BCMDBG_PHYDUMP)
extern void ai_dumpregs(si_t *sih, struct bcmstrbuf *b);
#endif /* BCMDBG || BCMDBG_DUMP|| BCMDBG_PHYDUMP */

#ifdef SI_ENUM_BASE_VARIABLE
extern void si_enum_base_init(si_t *sih, uint bustype);
#endif /* SI_ENUM_BASE_VARIABLE */

#if defined(DSLCPE) && defined(DSLCPE_WOC)
/* UBUS Interconnect exported externs */
extern void ub_scan(si_t *sih, void *regs, uint devid);
extern uint ub_flag(si_t *sih);
extern void ub_setint(si_t *sih, int siflag);
extern uint ub_coreidx(si_t *sih);
extern uint ub_corevendor(si_t *sih);
extern uint ub_corerev(si_t *sih);
extern bool ub_iscoreup(si_t *sih);
extern void *ub_setcoreidx(si_t *sih, uint coreidx);
extern uint32 ub_core_cflags(si_t *sih, uint32 mask, uint32 val);
extern void ub_core_cflags_wo(si_t *sih, uint32 mask, uint32 val);
extern uint32 ub_core_sflags(si_t *sih, uint32 mask, uint32 val);
extern uint ub_corereg(si_t *sih, uint coreidx, uint regoff, uint mask, uint val);
extern void ub_core_reset(si_t *sih, uint32 bits, uint32 resetbits);
extern void ub_core_disable(si_t *sih, uint32 bits);
extern int ub_numaddrspaces(si_t *sih);
extern uint32 ub_addrspace(si_t *sih, uint asidx);
extern uint32 ub_addrspacesize(si_t *sih, uint asidx);
#ifdef BCMDBG
extern void ub_view(si_t *sih, bool verbose);
#endif
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
extern void ub_dumpregs(si_t *sih, struct bcmstrbuf *b);
#endif
#else
#define ub_scan(a, b, c) do {} while (0)
#define ub_flag(a) (0)
#define ub_setint(a, b) do {} while (0)
#define ub_coreidx(a) (0)
#define ub_corevendor(a) (0)
#define ub_corerev(a) (0)
#define ub_iscoreup(a) (0)
#define ub_setcoreidx(a, b) (0)
#define ub_core_cflags(a, b, c) (0)
#define ub_core_cflags_wo(a, b, c) do {} while (0)
#define ub_core_sflags(a, b, c) (0)
#define ub_corereg(a, b, c, d, e) (0)
#define ub_core_reset(a, b, c) do {} while (0)
#define ub_core_disable(a, b) do {} while (0)
#define ub_numaddrspaces(a) (0)
#define ub_addrspace(a, b)  (0)
#define ub_addrspacesize(a, b) (0)
#define ub_view(a, b) do {} while (0)
#define ub_dumpregs(a, b) do {} while (0)
#endif /* defined(DSLCPE) && defined(DSLCPE_WOC) */

#endif	/* _siutils_priv_h_ */
