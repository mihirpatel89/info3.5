/*

	Copyright 2005, Broadcom Corporation
	All Rights Reserved.

	THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
	KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
	SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
	FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.

*/

#include "rc.h"
#include "shared.h"
#include "version.h"
#include "interface.h"

#include <termios.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <time.h>
#include <errno.h>
#include <paths.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/klog.h>
#ifdef LINUX26
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#endif
#include <wlutils.h>
#include <bcmdevs.h>

#include <linux/major.h>

#ifdef RTCONFIG_BCMWL6
#include <d11.h>
#define WLCONF_PHYTYPE2STR(phy)	((phy) == PHY_TYPE_A ? "a" : \
				 (phy) == PHY_TYPE_B ? "b" : \
				 (phy) == PHY_TYPE_LP ? "l" : \
				 (phy) == PHY_TYPE_G ? "g" : \
				 (phy) == PHY_TYPE_SSN ? "s" : \
				 (phy) == PHY_TYPE_HT ? "h" : \
				 (phy) == PHY_TYPE_AC ? "v" : \
				 (phy) == PHY_TYPE_LCN ? "c" : "n")
#endif

#ifdef RTCONFIG_BCMFA
#include <sysdeps.h>
#endif

int wan_phyid = -1;

void init_devs(void)
{
}

void generate_switch_para(void)
{
	int model, cfg;
	char lan[SWCFG_BUFSIZE], wan[SWCFG_BUFSIZE];
#ifdef RTCONFIG_GMAC3
	char glan[2*SWCFG_BUFSIZE];
	char var[32], *lists, *next;

	int gmac3_enable = nvram_get_int("gmac3_enable");
	memset(glan, 0, sizeof(glan));
#endif

	// generate nvram nvram according to system setting
	model = get_model();

	if (is_routing_enabled()) {
		cfg = nvram_get_int("switch_stb_x");
		if (cfg < SWCFG_DEFAULT || cfg > SWCFG_STB34)
			cfg = SWCFG_DEFAULT;
#ifdef RTCONFIG_MULTICAST_IPTV
		if (cfg == 7)
			cfg = SWCFG_STB3;
#endif
	}
	/* don't do this to save ports */
	//else if (sw_mode() == SW_MODE_REPEATER ||
	//	((sw_mode() == SW_MODE_AP) && (nvram_get_int("wlc_psta")))
	//	cfg = SWCFG_PSTA;
	else if (sw_mode() == SW_MODE_AP && nvram_get_int("re_mode") == 0)
		cfg = SWCFG_BRIDGE;
	else
		cfg = SWCFG_DEFAULT;	// keep wan port, but get ip from bridge

	switch(model) {
		/* BCM5325 series */
		case MODEL_APN12HP:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			/* TODO: switch_wantag? */

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
			//if (!is_routing_enabled())
			//	nvram_set("lan_ifnames", "eth0 eth1");	// override
			switch_gen_config(lan, ports, SWCFG_BRIDGE, 0, "*");
			switch_gen_config(wan, ports, SWCFG_BRIDGE, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
#ifdef RTCONFIG_LANWAN_LED
			// for led, always keep original port map
			cfg = SWCFG_DEFAULT;
#endif
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, cfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			break;
		}

		/* BCM5325 series */
		case MODEL_RTN14UHP:
		{
			/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 0, 1, 2, 3, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

#ifdef RTCONFIG_DUALWAN
			int wan1cfg = nvram_get_int("wans_lanport");

			nvram_unset("vlan1ports");
			nvram_unset("vlan1hwname");
			nvram_unset("vlan2ports");
			nvram_unset("vlan2hwname");

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
			// The first WAN port.
			if (get_wans_dualwan()&WANSCAP_WAN) {
				switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
				nvram_set("vlan1ports", wan);
				nvram_set("vlan1hwname", "et0");
			}

			// The second WAN port.
			if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
				wan1cfg += WAN1PORT1-1;
				if (wancfg != SWCFG_DEFAULT) {
					gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
					nvram_set("vlan0ports", lan);
					gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
					nvram_set("lanports", lan);
				} else {
					switch_gen_config(lan, ports, wan1cfg, 0, "*");
					nvram_set("vlan0ports", lan);
					switch_gen_config(lan, ports, wan1cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
				if (get_wans_dualwan()&WANSCAP_WAN) {
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				} else {
					nvram_set("vlan1ports", wan);
					nvram_set("vlan1hwname", "et0");
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				nvram_set("vlan0ports", lan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				nvram_set("lanports", lan);
			}

			int unit;
			char prefix[8], nvram_ports[16];

			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				memset(prefix, 0, 8);
				sprintf(prefix, "%d", unit);

				memset(nvram_ports, 0, 16);
				sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				} else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					switch_gen_config(wan, ports, wan1cfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				} else
					nvram_unset(nvram_ports);
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
#ifdef RTCONFIG_LANWAN_LED
			// for led, always keep original port map
			wancfg = SWCFG_DEFAULT;
#endif
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		/* BCM5325 series */
		case MODEL_RTN53:
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12VP:
		case MODEL_RTN12HP:
		case MODEL_RTN12HP_B1:
		case MODEL_RTN10P:
		case MODEL_RTN10D1:
		case MODEL_RTN10PV2:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan1ports");
				nvram_unset("vlan1hwname");
				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan1ports", wan);
					nvram_set("vlan1hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan0ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan0ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
					else {
						nvram_set("vlan1ports", wan);
						nvram_set("vlan1hwname", "et0");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan0ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan0ports", lan);
				nvram_set("vlan1ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
#ifdef RTCONFIG_LANWAN_LED
			// for led, always keep original port map
			cfg = SWCFG_DEFAULT;
#endif
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTN10U:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan1ports");
				nvram_unset("vlan1hwname");
				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan1ports", wan);
					nvram_set("vlan1hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan0ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan0ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
					else {
						nvram_set("vlan1ports", wan);
						nvram_set("vlan1hwname", "et0");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan0ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan0ports", lan);
				nvram_set("vlan1ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan0ports", lan);
			nvram_set("vlan1ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		/* BCM53125 series */
		case MODEL_RTN15U:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			int wan1cfg = nvram_get_int("wans_lanport");

			nvram_unset("vlan2ports");
			nvram_unset("vlan2hwname");
			nvram_unset("vlan3ports");
			nvram_unset("vlan3hwname");

			// The first WAN port.
			if (get_wans_dualwan()&WANSCAP_WAN) {
				switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
				nvram_set("vlan2ports", wan);
				nvram_set("vlan2hwname", "et0");
			}

			// The second WAN port.
			if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
				wan1cfg += WAN1PORT1-1;
				if (wancfg != SWCFG_DEFAULT) {
					gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
					nvram_set("vlan1ports", lan);
					gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
					nvram_set("lanports", lan);
				} else {
					switch_gen_config(lan, ports, wan1cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, wan1cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
				if (get_wans_dualwan()&WANSCAP_WAN) {
					nvram_set("vlan3ports", wan);
					nvram_set("vlan3hwname", "et0");
				} else {
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				nvram_set("vlan1ports", lan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				nvram_set("lanports", lan);
			}

			int unit;
			char prefix[8], nvram_ports[16];

			for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
				memset(prefix, 0, 8);
				sprintf(prefix, "%d", unit);

				memset(nvram_ports, 0, 16);
				sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

				if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				} else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
					switch_gen_config(wan, ports, wan1cfg, 1, NULL);
					nvram_set(nvram_ports, wan);
				} else
					nvram_unset(nvram_ports);
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTN16:
		{					/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTAC3200:
		{					/* WAN L1 L2 L3 L4 CPU */	/*vision: WAN L4 L3 L2 L1 */
			int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 5 };
#ifdef RTCONFIG_GMAC3
			if (gmac3_enable)
				ports[SWPORT_COUNT-1] = 8;
#endif

			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0]; // record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}


		case MODEL_RTAC68U:						/* 0  1  2  3  4 */
		case MODEL_RTN18U:						/* 0  1  2  3  4 */
		case MODEL_RTAC53U:
		{				/* WAN L1 L2 L3 L4 CPU */	/*vision: WAN L1 L2 L3 L4 */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
#ifdef RTCONFIG_AMAS
			if (nvram_get_int("re_mode")==1) {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);

				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
			else
#endif
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			} else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
#ifdef RTCONFIG_AMAS
			if (nvram_get_int("re_mode")==1) {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);

				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
			else
#endif
			{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "u");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
#endif
			break;
		}

		case MODEL_DSLAC68U:
		{
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");
				int unit;
				char nv[16];
				int vid;

				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				switch_gen_config(wan, ports, wancfg, 1, "t");
				nvram_set("vlan2ports", wan);
				nvram_set("vlan2hwname", "et0");

				// port for DSL
				if (get_wans_dualwan()&WANSCAP_DSL) {
					switch_gen_config(wan, ports, wancfg, 1, "t");
					char buf[32];
					snprintf(buf, sizeof(buf), "%sports", DSL_WAN_VIF);
					nvram_set(buf, wan);
					snprintf(buf, sizeof(buf), "%shwname", DSL_WAN_VIF);
					nvram_set(buf, "et0");
				}

				// port for LAN/WAN
				if (nvram_match("ewan_dot1q", "1"))
					vid = nvram_get_int("ewan_vid");
				else
					vid = 4;
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, "t");
					snprintf(nv, sizeof(nv), "vlan%dports", vid);
					nvram_set(nv, wan);
					snprintf(nv, sizeof(nv), "vlan%dhwname", vid);
					nvram_set(nv, "et0");
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);

					snprintf(nv, sizeof(nv), "vlan%dports", vid);
					nvram_unset(nv);
					snprintf(nv, sizeof(nv), "vlan%dhwname", vid);
					nvram_unset(nv);
				}

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					if (unit == WAN_UNIT_FIRST)
						snprintf(nv, sizeof(nv), "wanports");
					else
						snprintf(nv, sizeof(nv), "wan%dports", unit);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nv, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nv, wan);
					}
					else
						nvram_unset(nv);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "t");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTAC87U:						/* 0  1  2  3  4 */
		{				/* WAN L1 L2 L3 L4 CPU */	/*vision: WAN L1 L2 L3 L4 */
			int ports[SWPORT_COUNT] = { 0, 5, 3, 2, 1, 7 };
#ifdef RTCONFIG_GMAC3
			if (gmac3_enable)
				ports[SWPORT_COUNT-1] = 8;
#endif
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et1");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et1");
					}
					else {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et1");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_RTAC56S:						/* 0  1  2  3  4 */
		case MODEL_RTAC56U:
		{				/* WAN L1 L2 L3 L4 CPU */	/*vision: L1 L2 L3 L4 WAN  POW*/
			const int ports[SWPORT_COUNT] = { 4, 0, 1, 2, 3, 5 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}

		case MODEL_GTAC5300:
		case MODEL_RTAC86U:
			break;

		case MODEL_RTAC5300:
		{
			char *hw_name = "et1";

#ifdef RTCONFIG_EXT_RTL8365MB
								/*vision:    (L5 L6 L7 L8)*/
			/* WAN L1 L2 L3 L4 (L5 L6 L7 L8) CPU */	/*vision: WAN L1 L2 L3 L4 */
			int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 5, 7 };
#else	// RTCONFIG_EXT_RTL8365MB
			/* WAN L1 L2 L3 L4 CPU */		/*vision: WAN L1 L2 L3 L4 */
			int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 7 };
#endif
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;
			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_GMAC3
			if (gmac3_enable) {
				ports[SWPORT_COUNT-1] = 8;
				hw_name = "et2";
			}
#endif

#ifdef RTCONFIG_DUALWAN
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("vlan3hwname", hw_name);
			else
				nvram_unset("vlan3hwname");
			if (get_wans_dualwan()&WANSCAP_WAN || get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("vlan2hwname", hw_name);
			nvram_set("vlan1hwname", hw_name);

#ifdef RTCONFIG_AMAS
			if (nvram_get_int("re_mode")==1) {

				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
			else
#endif
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan3ports");

				/* The first WAN port. */
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
				}

				/* The second WAN port. */
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN)
						nvram_set("vlan3ports", wan);
					else
						nvram_set("vlan2ports", wan);
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "u");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else	// RTCONFIG_DUALWAN
#ifdef RTCONFIG_AMAS
			if (nvram_get_int("re_mode")==1) {

				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
			else
#endif
			{
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "u");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan1hwname", hw_name);
				nvram_set("vlan2ports", wan);
				nvram_set("vlan2hwname", hw_name);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
#endif
			break;
		}

		case MODEL_RTAC3100:
		case MODEL_RTAC88U:
		{
			char *hw_name = "et0";

#ifdef RTCONFIG_EXT_RTL8365MB
			/* WAN L1 L2 L3 L4 (L5 L6 L7 L8) CPU */	/*vision: (L8 L7 L6 L5) L4 L3 L2 L1 WAN*/
			int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5, 7 };
			hw_name = "et1";
#else	// RTCONFIG_EXT_RTL8365MB
			/* WAN L1 L2 L3 L4 CPU */	/*vision: WAN L1 L2 L3 L4 */
			int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };
			hw_name = "et0";
#endif
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;
			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_GMAC3
			if (gmac3_enable) {
				ports[SWPORT_COUNT-1] = 8;
				hw_name = "et2";
			}
#endif

#ifdef RTCONFIG_DUALWAN
			if (get_wans_dualwan()&WANSCAP_WAN && get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("vlan3hwname", hw_name);
			else
				nvram_unset("vlan3hwname");
			if (get_wans_dualwan()&WANSCAP_WAN || get_wans_dualwan()&WANSCAP_LAN)
				nvram_set("vlan2hwname", hw_name);
			nvram_set("vlan1hwname", hw_name);

#ifdef RTCONFIG_AMAS
			if (nvram_get_int("re_mode")==1) {

				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
			else
#endif
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan3ports");

				/* The first WAN port. */
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
				}

				/* The second WAN port. */
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN)
						nvram_set("vlan3ports", wan);
					else
						nvram_set("vlan2ports", wan);
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "u");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else	// RTCONFIG_DUALWAN
#ifdef RTCONFIG_AMAS
			if (nvram_get_int("re_mode")==1) {

				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
			}
			else
#endif
			{
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan1hwname", hw_name);
			nvram_set("vlan2ports", wan);
			nvram_set("vlan2hwname", hw_name);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
			}
#endif
			break;
		}

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
		{				/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 8 };
			int wancfg = (!nvram_match("switch_wantag", "none")&&!nvram_match("switch_wantag", "")&&!nvram_match("switch_wantag", "hinet")) ? SWCFG_DEFAULT : cfg;

			wan_phyid = ports[0];	// record the phy num of the wan port on the case
#ifdef RTCONFIG_DUALWAN
			if (cfg != SWCFG_BRIDGE) {
				int wan1cfg = nvram_get_int("wans_lanport");

				nvram_unset("vlan2ports");
				nvram_unset("vlan2hwname");
				nvram_unset("vlan3ports");
				nvram_unset("vlan3hwname");

				// The first WAN port.
				if (get_wans_dualwan()&WANSCAP_WAN) {
					switch_gen_config(wan, ports, wancfg, 1, (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4)?"":"u");
					nvram_set("vlan2ports", wan);
					nvram_set("vlan2hwname", "et0");
				}

				// The second WAN port.
				if (get_wans_dualwan()&WANSCAP_LAN && wan1cfg >= 1 && wan1cfg <= 4) {
					wan1cfg += WAN1PORT1-1;
					if (wancfg != SWCFG_DEFAULT) {
						gen_lan_ports(lan, ports, wancfg, wan1cfg, "*");
						nvram_set("vlan1ports", lan);
						gen_lan_ports(lan, ports, wancfg, wan1cfg, NULL);
						nvram_set("lanports", lan);
					}
					else {
						switch_gen_config(lan, ports, wan1cfg, 0, "*");
						nvram_set("vlan1ports", lan);
						switch_gen_config(lan, ports, wan1cfg, 0, NULL);
						nvram_set("lanports", lan);
					}

					switch_gen_config(wan, ports, wan1cfg, 1, (get_wans_dualwan()&WANSCAP_WAN)?"":"u");
					if (get_wans_dualwan()&WANSCAP_WAN) {
						nvram_set("vlan3ports", wan);
						nvram_set("vlan3hwname", "et0");
					}
					else {
						nvram_set("vlan2ports", wan);
						nvram_set("vlan2hwname", "et0");
					}
				} else {
					switch_gen_config(lan, ports, cfg, 0, "*");
					nvram_set("vlan1ports", lan);
					switch_gen_config(lan, ports, cfg, 0, NULL);
					nvram_set("lanports", lan);
				}

				int unit;
				char prefix[8], nvram_ports[16];

				for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; ++unit) {
					memset(prefix, 0, 8);
					sprintf(prefix, "%d", unit);

					memset(nvram_ports, 0, 16);
					sprintf(nvram_ports, "wan%sports", (unit == WAN_UNIT_FIRST)?"":prefix);

					if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_WAN) {
						switch_gen_config(wan, ports, wancfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else if (get_dualwan_by_unit(unit) == WANS_DUALWAN_IF_LAN) {
						switch_gen_config(wan, ports, wan1cfg, 1, NULL);
						nvram_set(nvram_ports, wan);
					}
					else
						nvram_unset(nvram_ports);
				}
			}
			else {
				switch_gen_config(lan, ports, cfg, 0, "*");
				switch_gen_config(wan, ports, wancfg, 1, "");
				nvram_set("vlan1ports", lan);
				nvram_set("vlan2ports", wan);
				switch_gen_config(lan, ports, cfg, 0, NULL);
				switch_gen_config(wan, ports, wancfg, 1, NULL);
				nvram_set("lanports", lan);
				nvram_set("wanports", wan);
				nvram_unset("wan1ports");
			}
#else
			switch_gen_config(lan, ports, cfg, 0, "*");
			switch_gen_config(wan, ports, wancfg, 1, "u");
			nvram_set("vlan1ports", lan);
			nvram_set("vlan2ports", wan);
			switch_gen_config(lan, ports, cfg, 0, NULL);
			switch_gen_config(wan, ports, wancfg, 1, NULL);
			nvram_set("lanports", lan);
			nvram_set("wanports", wan);
#endif
			break;
		}
	}

#ifdef RTCONFIG_GMAC3
	/* gmac3 override */
	if (nvram_get_int("gmac3_enable") == 1) {
		lists = nvram_safe_get("vlan1ports");
		strncpy(glan, lists, strlen(lists));

		foreach(var, lists, next) {
			if (strchr(var, '*') || strchr(var, 'u')) {
				remove_from_list(var, glan, sizeof(glan));
				break;
			}
		}
		/* add port 5, 7 and 8* */
		add_to_list("5", glan, sizeof(glan));
		add_to_list("7", glan, sizeof(glan));
		add_to_list("8*", glan, sizeof(glan));
		nvram_set("vlan1ports", glan);
	}
#endif
}

void enable_jumbo_frame(void)
{
	int enable = nvram_get_int("jumbo_frame_enable");

	if (!nvram_contains_word("rc_support", "switchctrl"))
		return;

#ifndef HND_ROUTER
	switch (get_switch()) {
	case SWITCH_BCM53115:
	case SWITCH_BCM53125:
		eval("et", "robowr", "0x40", "0x01", enable ? "0x1f" : "0x00");
		break;
	case SWITCH_BCM5301x:
#ifdef RTCONFIG_BCMARM
		eval("et", "-i", "eth0", "robowr", "0x40", "0x01", enable ? "0x010001ff" : "0x00", "4");
#else
		eval("et", "robowr", "0x40", "0x01", enable ? "0x010001ff" : "0x00");
#endif
		break;
	}
#else
#ifdef RTCONFIG_EXT_BCM53134
	eval("ethswctl", "-c", "pmdioaccess", "-x", "0x4001", "-l", "4", "-d", enable ? "0x010001ff" : "0x00000000");
#endif
	eval("ethswctl", "-c", "regaccess", "-v", "0x4001", "-l", "4", "-d", enable ? "0x010001ff" : "0x00000000");
#endif
}

#ifdef RTCONFIG_EXT_BCM53134
/*
The default value is to dropping such packets with the DA is 01:80:c2:00:00:02~0F multicast packet.
It will cause multicast protocol can't work, ex: LLDPD packet.
So we set 0x002f bit1 to 0.
*/
void accept_multicast_forward(void)
{
	eval("ethswctl", "-c", "pmdioaccess", "-x", "0x002f", "-l", "1", "-d", "0x00");
}
#endif

void ether_led()
{
	int model;

	model = get_model();
	switch(model) {
//	case MODEL_RTAC68U:
	/* refer to 5301x datasheet page 2770 */
	case MODEL_RTAC56S:
	case MODEL_RTAC56U:
		eval("et", "robowr", "0x00", "0x10", "0x3000");
		break;
	case MODEL_RTN16:
		eval("et", "robowr", "0", "0x18", "0x01ff");
		eval("et", "robowr", "0", "0x1a", "0x01ff");
		break;
	case MODEL_RTAC1200G:
	case MODEL_RTAC1200GP:
		eval("et", "robowr", "0", "0x12", "0x24");
		break;
	}
}

#ifdef HND_ROUTER
void init_switch()
{
	int model = get_model();

	switch(model) {
		case MODEL_GTAC5300:
			/* set wanports in init_nvram for dualwan */
			//nvram_set("wanports", "7");
			nvram_set("lanports", "0 1 2 3");
			break;

		case MODEL_RTAC86U:
			/* set wanports in init_nvram for dualwan */
			//nvram_set("wanports", "7");
			nvram_set("lanports", "3 2 1 0");
			break;
	}

#ifdef RTCONFIG_EMF
	eval("insmod", "emf");
	eval("insmod", "igs");
#endif

	hnd_nat_ac_init(1);

	enable_jumbo_frame();

#ifdef RTCONFIG_EXT_BCM53134
	accept_multicast_forward();
#endif

#ifdef RTCONFIG_LACP
	config_lacp();
#endif
}

int
switch_exist(void)
{
	return 1;
}

void config_switch(void)
{
}

#else
void init_switch()
{
	generate_switch_para();

#if defined(RTCONFIG_EXT_RTL8365MB) || defined(RTCONFIG_EXT_RTL8370MB)
	eval("mknod", "/dev/rtkswitch", "c", "233", "0");
	eval("insmod", "rtl8365mb");
#ifdef RTCONFIG_RESET_SWITCH
	int i, r, gpio_nr = atoi(nvram_safe_get("reset_switch_gpio"));
	led_control(LED_RESET_SWITCH, 0);
	usleep(400 * 1000);

	for (i=0; i<10; ++i) {
		led_control(LED_RESET_SWITCH, 1);
		if ((r = get_gpio(gpio_nr)) != 1) {
			_dprintf("\n! reset LED_RESET_SWITCH failed:%d, reset again !\n", r);
			usleep(10 * 1000);
		} else {
			_dprintf("\nchk LED_RESET_SWITCH:%d\n", r);
			break;
		}
	}
#endif
#endif

#ifdef CONFIG_BCMWL5
	// ctf should be disabled when some functions are enabled
	if ((nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0) ||
	    (check_wl_guest_bw_enable()  && (nvram_get_int("qos_enable") && nvram_get_int("qos_type") == 2)) ||
	    nvram_get_int("ctf_disable_force")
#ifndef RTCONFIG_BCMARM
	|| sw_mode() == SW_MODE_REPEATER
#endif
	|| nvram_get_int("cstats_enable") == 1
//#ifdef RTCONFIG_USB_MODEM
//	|| nvram_get_int("ctf_disable_modem")
//#endif
	) {
		nvram_set("ctf_disable", "1");
	}
#if defined(RTCONFIG_BWDPI)
	else if (check_bwdpi_nvram_setting() && is_router_mode()) {
		nvram_set("ctf_disable", "0");
	}
#endif
	else {
		nvram_set("ctf_disable", "0");
	}
#ifdef RTCONFIG_BCMFA
	fa_nvram_adjust();
#endif

/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
	if (is_router_mode() && nvram_get_int("qos_enable") == 1) {
		// enable netfilter bridge only when phydev is used
		f_write_string("/proc/sys/net/bridge/bridge-nf-call-iptables", "1", 0, 0);
		f_write_string("/proc/sys/net/bridge/bridge-nf-call-ip6tables", "1", 0, 0);
		f_write_string("/proc/sys/net/bridge/bridge-nf-filter-vlan-tagged", "1", 0, 0);
	}
*/
#endif

#ifdef RTCONFIG_SHP
	if (nvram_get_int("qos_enable") == 1 || nvram_get_int("lfp_disable_force")) {
		nvram_set("lfp_disable", "1");
	} else {
		nvram_set("lfp_disable", "0");
	}

	if (nvram_get_int("lfp_disable") == 0) {
		restart_lfp();
	}
#endif

#if defined(RTCONFIG_BCMFA) && !defined(RTCONFIG_BCM_7114)
	if (!nvram_get("ctf_fa_cap")) {
		char ctf_fa_mode_bak[2];
		int nvram_ctf_fa_mode = 0;

		if (nvram_get("ctf_fa_mode"))
		{
			nvram_ctf_fa_mode = 1;
			strcpy(ctf_fa_mode_bak, nvram_safe_get("ctf_fa_mode"));
		}

		nvram_set_int("ctf_fa_mode", 2);
		eval("insmod", "et");
		FILE *fp;
		if ((fp = fopen("/proc/fa", "r"))) {
			/* FA is capable */
			fclose(fp);
			nvram_set_int("ctf_fa_cap", 1);
			ATE_BRCM_SET("ctf_fa_cap", "1");
		} else {
			nvram_set_int("ctf_fa_cap", 0);
			ATE_BRCM_SET("ctf_fa_cap", "0");
		}

		if (nvram_ctf_fa_mode)
			nvram_set("ctf_fa_mode", ctf_fa_mode_bak);
		else
			nvram_unset("ctf_fa_mode");
		nvram_commit();
		ATE_BRCM_UNSET("Ate_power_on_off_ret");
		ATE_BRCM_COMMIT();

		eval("rmmod", "et");

		dbg("FA (hardware Flow Accelarator) cap recorded, rebooting...\n");
		reboot(RB_AUTOBOOT);
		sleep(1);
	} else if (!cfe_nvram_get("ctf_fa_cap")) {
		dbg("commit original FA cap info into cfe nvram space\n");
		ATE_BRCM_SET("ctf_fa_cap", nvram_get("ctf_fa_cap"));
		ATE_BRCM_UNSET("Ate_power_on_off_ret");
		ATE_BRCM_COMMIT();
	}
#endif

	// ctf must be loaded prior to any other modules
	if (nvram_get_int("ctf_disable") == 0)
		eval("insmod", "ctf");

#ifdef RTCONFIG_EMF
	eval("insmod", "emf");
	eval("insmod", "igs");
#endif

	eval("insmod", "et");
	eval("insmod", "bcm57xx");

	enable_jumbo_frame();
	ether_led();

#ifdef RTCONFIG_DSL
	init_switch_dsl();
	config_switch_dsl();
#endif
#ifdef RTCONFIG_EXT_RTL8365MB
	/* link up rtkswitch after bcm rgmii init */
	eval("rtkswitch", "1");
#endif
#ifdef RTCONFIG_EXT_RTL8370MB
	/* link up rtkswitch after bcm rgmii init */
	eval("rtkswitch", "0");
#endif
#ifdef RTCONFIG_LACP
	config_lacp();
#endif

#if defined(RTCONFIG_GMAC3) || defined(RTCONFIG_BCMFA)
	if (nvram_get_int("gmac3_enable")
#ifdef RTCONFIG_BCMFA
		|| nvram_get_int("ctf_fa_mode") == CTF_FA_NORMAL
#endif
	) {
		eval("et", "-i", "eth0", "robowr", "0x4", "0x4", "0", "6");
	}
#endif

}

int
switch_exist(void)
{
	int ret = 1;

	if (get_switch() == SWITCH_UNKNOWN) {
		_dprintf("No switch interface!!!\n");
		ret = 0;
	}

	return ret;
}

void config_switch(void)
{
	generate_switch_para();

}
#endif

#ifdef RTCONFIG_BCMWL6
extern struct nvram_tuple bcm4360ac_defaults[];

static void
set_bcm4360ac_vars(void)
{
	struct nvram_tuple *t;

	/* Restore defaults */
	for (t = bcm4360ac_defaults; t->name; t++) {
		if (!nvram_get(t->name))
			nvram_set(t->name, t->value);
	}
}
#endif

static void
reset_mssid_hwaddr(int unit)
{
	char macaddr_str[18], macbuf[13];
	char *macaddr_strp;
	unsigned char mac_binary[6];
	unsigned long long macvalue, macvalue_local;
	unsigned char *macp;
	int model = get_model();
	int idx, subunit;
	int max_mssid = num_of_mssid_support(unit);
	char tmp[100], prefix[]="wlXXXXXXX_";
	int unit_total = num_of_wl_if ();
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
	int psr = (is_psr(unit)
			|| dpsr_mode()
#ifdef RTCONFIG_DPSTA
			|| dpsta_mode()
#endif
			);
#endif

#ifdef RTCONFIG_PSR_GUEST
	max_mssid++;
#endif

	if (unit > (unit_total - 1))
		return;

	for (idx = 0; idx < unit_total ; idx++) {
		memset(mac_binary, 0x0, 6);
		memset(macbuf, 0x0, 13);

		switch(model) {
			case MODEL_RTN53:
			case MODEL_RTN16:
			case MODEL_RTN15U:
			case MODEL_RTN12:
			case MODEL_RTN12B1:
			case MODEL_RTN12C1:
			case MODEL_RTN12D1:
			case MODEL_RTN12VP:
			case MODEL_RTN12HP:
			case MODEL_RTN12HP_B1:
			case MODEL_APN12HP:
			case MODEL_RTN14UHP:
			case MODEL_RTN10U:
			case MODEL_RTN10P:
			case MODEL_RTN10D1:
			case MODEL_RTN10PV2:
			case MODEL_RTAC53U:
				if (unit == 0)	/* 2.4G */
					snprintf(macaddr_str, sizeof(macaddr_str), "sb/1/macaddr");
				else		/* 5G */
					snprintf(macaddr_str, sizeof(macaddr_str), "0:macaddr");
				break;
			case MODEL_RTN66U:
			case MODEL_RTAC66U:
				snprintf(macaddr_str, sizeof(macaddr_str), "pci/%d/1/macaddr", unit + 1);
				break;
			case MODEL_RTN18U:
			case MODEL_RTAC68U:
			case MODEL_RTAC3200:
			case MODEL_DSLAC68U:
			case MODEL_RTAC87U:
			case MODEL_RTAC56S:
			case MODEL_RTAC56U:
			case MODEL_RTAC5300:
			case MODEL_GTAC5300:
			case MODEL_RTAC88U:
			case MODEL_RTAC86U:
			case MODEL_RTAC3100:
#ifdef RTAC3200
				if (unit < 2)
					snprintf(macaddr_str, sizeof(macaddr_str), "%d:macaddr", 1 - unit);
				else
#endif
				snprintf(macaddr_str, sizeof(macaddr_str), "%d:macaddr", unit);
				break;
			case MODEL_RTAC1200G:
			case MODEL_RTAC1200GP:
				if (unit == 0) 	/* 2.4G */
					snprintf(macaddr_str, sizeof(macaddr_str), "0:macaddr");
				else		/* 5G */
					snprintf(macaddr_str, sizeof(macaddr_str), "sb/1/macaddr");
				break;
			default:
#ifdef RTCONFIG_BCMARM
				snprintf(macaddr_str, sizeof(macaddr_str), "%d:macaddr", unit);
#else
				snprintf(macaddr_str, sizeof(macaddr_str), "pci/%d/1/macaddr", unit + 1);
#endif
				break;
		}

		macaddr_strp = cfe_nvram_get(macaddr_str);

		if (macaddr_strp)
		{
			if (!mssid_mac_validate(macaddr_strp))
				return;

			if (idx != unit)
				continue;

			ether_atoe(macaddr_strp, mac_binary);
			sprintf(macbuf, "%02X%02X%02X%02X%02X%02X",
					mac_binary[0],
					mac_binary[1],
					mac_binary[2],
					mac_binary[3],
					mac_binary[4],
					mac_binary[5]);
			macvalue = strtoll(macbuf, (char **) NULL, 16);

			ETHER_SET_LOCALADDR(mac_binary);
			sprintf(macbuf, "%02X%02X%02X%02X%02X%02X",
					mac_binary[0],
					mac_binary[1],
					mac_binary[2],
					mac_binary[3],
					mac_binary[4],
					mac_binary[5]);
			macvalue_local = strtoll(macbuf, (char **) NULL, 16);

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
			if (!unit && psr) {
				macvalue++;
				macvalue_local++;
			}

#ifdef RTCONFIG_PSR_GUEST
			snprintf(prefix, sizeof(prefix), "wl%d_", unit);
			if (is_psr(unit) && nvram_match(strcat_r(prefix, "psr_guest", tmp), "1")) {
				macvalue--;
				macvalue_local--;
			}
#endif
#endif

			/* including primary ssid */
			for (subunit = 1; subunit < WL_MAXBSSCFG; subunit++)
			{
				macvalue++;
				macvalue_local++;

#ifdef RTCONFIG_PSR_GUEST
				snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#endif
				if ((subunit > max_mssid)
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
#ifdef RTCONFIG_PSR_GUEST
					|| (is_psr(unit) && !nvram_match(strcat_r(prefix, "psr_guest", tmp), "1") && (subunit > 1))
					|| (is_psr(unit) && nvram_match(strcat_r(prefix, "psr_guest", tmp), "1") && (subunit > 4))
#else
					|| (is_psr(unit) && (subunit > 1))
#endif
#endif
				)
					macp = (unsigned char*) &macvalue_local;
				else
					macp = (unsigned char*) &macvalue;

				memset(macaddr_str, 0, sizeof(macaddr_str));
				sprintf(macaddr_str, "%02X:%02X:%02X:%02X:%02X:%02X", *(macp+5), *(macp+4), *(macp+3), *(macp+2), *(macp+1), *(macp+0));
				snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
				nvram_set(strcat_r(prefix, "hwaddr", tmp), macaddr_str);
			}
		} else return;
	}
}

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
void
reset_psr_hwaddr()
{
	char macaddr_name[10], macaddr_str[18], macbuf[13];
	char *macaddr_p;
	unsigned char mac_binary[6];
	unsigned long long macvalue;
	unsigned char *macp;
	int model = get_model();
	int unit = 0;
	int restore = !(is_psr(0)
			|| dpsr_mode()
#ifdef RTCONFIG_DPSTA
			|| dpsta_mode()
#endif
			);

	memset(mac_binary, 0x0, 6);
	memset(macbuf, 0x0, 13);

	switch(model) {
		case MODEL_RTAC3200:
		case MODEL_GTAC5300:
			unit = 1;
			break;
	}

	snprintf(macaddr_name, sizeof(macaddr_name), "%d:macaddr", unit);

	macaddr_p = cfe_nvram_get(macaddr_name);
	if (macaddr_p)
	{
		ether_atoe(macaddr_p, mac_binary);
		sprintf(macbuf, "%02X%02X%02X%02X%02X%02X",
				mac_binary[0],
				mac_binary[1],
				mac_binary[2],
				mac_binary[3],
				mac_binary[4],
				mac_binary[5]);
		macvalue = strtoll(macbuf, (char **) NULL, 16);
		if (!restore) macvalue++;

		macp = (unsigned char*) &macvalue;
		memset(macaddr_str, 0, sizeof(macaddr_str));
		sprintf(macaddr_str, "%02X:%02X:%02X:%02X:%02X:%02X", *(macp+5), *(macp+4), *(macp+3), *(macp+2), *(macp+1), *(macp+0));
		nvram_set(macaddr_name, macaddr_str);
	}
}
#endif

#ifdef RTCONFIG_DHDAP
void load_wl()
{
	char module[80], *modules, *next;
#ifdef RTCONFIG_DPSTA
#ifdef RTCONFIG_BCM_7114
	modules = "dpsta dhd dhd24";	// now 7114 models use this
#else
	modules = "dpsta dhd";
#endif
#else   // DPSTA
#ifdef RTCONFIG_BCM_7114
	modules = "dhd dhd24";		// now 7114 models use this
#else
	modules = "dhd";
#endif
#endif
	int i = 0, maxunit = -1;
	int unit = -1;
	char ifname[16] = {0};
	char instance_base[128];

#ifdef RTCONFIG_BCM_7114
	int dhd_reboot = 0;
	if (!*nvram_safe_get("chipnum") || !*nvram_safe_get("chiprev"))
		dhd_reboot = 1;
#endif


	foreach(module, modules, next) {
		if (strcmp(module, "dhd") == 0 || strcmp(module, "dhd24") == 0) {
			/* Search for existing wl devices and the max unit number used */
			for (i = 1; i <= DEV_NUMIFS; i++) {
			snprintf(ifname, sizeof(ifname), "eth%d", i);
				if (!wl_probe(ifname)) {
					if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit))) {
						maxunit = (unit > maxunit) ? unit : maxunit;
					}
				}
			}
			snprintf(instance_base, sizeof(instance_base), "instance_base=%d", maxunit + 1);
#if defined(RTCONFIG_BCM7) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
			if ((strcmp(module, "dhd") == 0) || (strcmp(module, "dhd24") == 0))
			snprintf(instance_base, sizeof(instance_base), "%s dhd_msg_level=%d", instance_base, nvram_get_int("dhd_msg_level"));
#endif
			eval("insmod", module, instance_base);
		} else {
			eval("insmod", module);
		}
	}
#ifdef RTCONFIG_BCM_7114
	if (dhd_reboot) {
		for(i=0; i< 3; ++i) {
			if (nvram_match("chipnum","0x4366") && *nvram_safe_get("chiprev")) {
					_dprintf("\nrebooting(dhd)...\n");
					reboot(RB_AUTOBOOT);
			}
			sleep(1);
		}
	}
#endif

#ifdef WLCLMLOAD
	download_clmblob_files();
#endif /* WLCLMLOAD */
}
#endif

void init_wl(void)
{
#ifdef RTCONFIG_BCMWL6
	switch(get_model()) {
		case MODEL_DSLAC68U:
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
		case MODEL_RTAC3100:
		case MODEL_RTAC3200:
		case MODEL_RTAC5300:
		case MODEL_GTAC5300:
		case MODEL_RTAC66U:
		case MODEL_RTAC68U:
		case MODEL_RTAC88U:
		case MODEL_RTAC86U:
			set_bcm4360ac_vars();
			break;
	}
#endif
	check_wl_country();
#if defined(RTAC3200) || defined(RTAC68U) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
	wl_disband5grp();
#endif
	set_wltxpower();

#ifdef RTCONFIG_DHDAP
	load_wl();
#else
#ifdef RTCONFIG_DPSTA
	eval("insmod", "dpsta");
#endif
	eval("insmod", "wl");
#endif

#if !defined(RTCONFIG_BCMARM) && defined(NAS_GTK_PER_STA) && defined(PROXYARP)
	eval("insmod", "proxyarp");
#endif

#ifdef RTCONFIG_BRCM_USBAP
	/* We have to load USB modules after loading PCI wl driver so
	 * USB driver can decide its instance number based on PCI wl
	 * instance numbers (in hotplug_usb())
	 */
	eval("insmod", "usbcore");

#ifdef LINUX26
	mount("usbfs", "/proc/bus/usb", "usbfs", MS_MGC_VAL, NULL);
#else
	mount("usbdevfs", "/proc/bus/usb", "usbdevfs", MS_MGC_VAL, NULL);
#endif /* LINUX26 */

	{
		char	insmod_arg[128];
		int	i = 0, maxwl_eth = 0, maxunit = -1;
		char	ifname[16] = {0};
		int	unit = -1;
		char arg1[20] = {0};
		char arg2[20] = {0};
		char arg3[20] = {0};
		char arg4[20] = {0};
		char arg5[20] = {0};
		char arg6[20] = {0};
		char arg7[20] = {0};
		const int wl_wait = 3;	/* max wait time for wl_high to up */

		/* Save QTD cache params in nvram */
		sprintf(arg1, "log2_irq_thresh=%d", nvram_get_int("ehciirqt"));
		sprintf(arg2, "qtdc_pid=%d", nvram_get_int("qtdc_pid"));
		sprintf(arg3, "qtdc_vid=%d", nvram_get_int("qtdc_vid"));
		sprintf(arg4, "qtdc0_ep=%d", nvram_get_int("qtdc0_ep"));
		sprintf(arg5, "qtdc0_sz=%d", nvram_get_int("qtdc0_sz"));
		sprintf(arg6, "qtdc1_ep=%d", nvram_get_int("qtdc1_ep"));
		sprintf(arg7, "qtdc1_sz=%d", nvram_get_int("qtdc1_sz"));

		eval("insmod", "ehci-hcd", arg1, arg2, arg3, arg4, arg5,
				arg6, arg7);

		/* Search for existing PCI wl devices and the max unit number used.
		 * Note that PCI driver has to be loaded before USB hotplug event.
		 * This is enforced in rc.c
		 */
		#define DEV_NUMIFS 8
		for (i = 1; i <= DEV_NUMIFS; i++) {
			sprintf(ifname, "eth%d", i);
			if (!wl_probe(ifname)) {
				if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit,
					sizeof(unit))) {
					maxwl_eth = i;
					maxunit = (unit > maxunit) ? unit : maxunit;
				}
			}
		}

		/* Set instance base (starting unit number) for USB device */
		sprintf(insmod_arg, "instance_base=%d", maxunit + 1);
		eval("insmod", "wl_high", insmod_arg);

		/* Hold until the USB/HSIC interface is up (up to wl_wait sec) */
		sprintf(ifname, "eth%d", maxwl_eth + 1);
		i = wl_wait;
		while (wl_probe(ifname) && i--) {
			sleep(1);
		}
		if (!wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
			cprintf("wl%d is up in %d sec\n", unit, wl_wait - i);
		else
			cprintf("wl%d not up in %d sec\n", unit, wl_wait);
	}
#endif /* __CONFIG_USBAP__ */
}

void init_wl_compact(void)
{
	int model = get_model();

	if (nvram_get_int("init_wl_re") == 0)
	{
		nvram_set_int("init_wl_re", 1);
		return;
	}

#ifdef RTCONFIG_BCMWL6
	switch(model) {
		case MODEL_DSLAC68U:
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
		case MODEL_RTAC3100:
		case MODEL_RTAC3200:
		case MODEL_RTAC5300:
		case MODEL_GTAC5300:
		case MODEL_RTAC66U:
		case MODEL_RTAC68U:
		case MODEL_RTAC88U:
		case MODEL_RTAC86U:
			set_bcm4360ac_vars();
			break;
	}
#endif
	check_wl_country();
#ifndef RTCONFIG_BRCM_USBAP
	if ((model == MODEL_DSLAC68U) ||
		(model == MODEL_RTAC1200G) ||
		(model == MODEL_RTAC1200GP) ||
		(model == MODEL_RTAC3100) ||
		(model == MODEL_RTAC3200) ||
		(model == MODEL_RTAC5300) ||
		(model == MODEL_GTAC5300) ||
		(model == MODEL_RTAC53U) ||
		(model == MODEL_RTAC56U) ||
		(model == MODEL_RTAC56S) ||
		(model == MODEL_RTAC66U) ||
		(model == MODEL_RTAC68U) ||
		(model == MODEL_RTAC87U) ||
		(model == MODEL_RTAC88U) ||
		(model == MODEL_RTAC86U) ||
		(model == MODEL_RTN12HP_B1) ||
		(model == MODEL_RTN18U) ||
		(model == MODEL_RTN66U)) {
#if defined(RTAC3200) || defined(RTAC68U) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
		wl_disband5grp();
#endif
		set_wltxpower();
#ifdef RTCONFIG_DHDAP
		load_wl();
#else
		eval("insmod", "wl");
#endif
#ifndef RTCONFIG_BCMARM
#if defined(NAS_GTK_PER_STA) && defined(PROXYARP)
		eval("insmod", "proxyarp");
#endif
#endif
	}
#endif
}

void fini_wl(void)
{
	int model = get_model();

#if (defined(RTAC3200) || defined(HND_ROUTER) || defined(RTCONFIG_BCM9)/* || defined(RTCONFIG_BCM_7114)*/)
	return;
#endif

#ifndef RTCONFIG_BCMARM
#if defined(NAS_GTK_PER_STA) && defined(PROXYARP)
	eval("rmmod", "proxyarp");
#endif
#endif

#ifndef RTCONFIG_BRCM_USBAP
	if (((model == MODEL_DSLAC68U) ||
		(model == MODEL_RTAC1200G) ||
		(model == MODEL_RTAC1200GP) ||
		(model == MODEL_RTAC3100) ||
		(model == MODEL_RTAC3200) ||
		(model == MODEL_RTAC5300) ||	/* will be removed in 7.x */
		(model == MODEL_GTAC5300) ||	/* will be removed in 7.x */
		(model == MODEL_RTAC66U) ||
		(model == MODEL_RTAC68U) ||
		(model == MODEL_RTAC87U) ||
		(model == MODEL_RTAC88U) ||
		(model == MODEL_RTAC86U) ||
		(model == MODEL_RTN12HP_B1) |
		(model == MODEL_RTN18U) ||
		(model == MODEL_RTN66U))
#ifdef RTCONFIG_DPSTA
		&& !dpsta_mode()
#endif
	)
	eval("rmmod", "wl");
#endif
#ifdef RTCONFIG_DHDAP
	eval("rmmod", "dhd");
#endif
}

void init_syspara(void)
{
	char *ptr;
	int model;

	nvram_set("firmver", rt_version);
	nvram_set("productid", rt_buildname);
	set_basic_fw_name();
	ptr = nvram_get("regulation_domain");

#ifdef HND_ROUTER
	refresh_cfe_nvram();
#endif

	model = get_model();
	switch(model) {
		case MODEL_RTN53:
		case MODEL_RTN16:
		case MODEL_RTN15U:
		case MODEL_RTN12:
		case MODEL_RTN12B1:
		case MODEL_RTN12C1:
		case MODEL_RTN12D1:
		case MODEL_RTN12VP:
		case MODEL_RTN12HP:
		case MODEL_RTN12HP_B1:
		case MODEL_APN12HP:
		case MODEL_RTN14UHP:
		case MODEL_RTN10U:
		case MODEL_RTN10P:
		case MODEL_RTN10D1:
		case MODEL_RTN10PV2:
			if (!nvram_get("et0macaddr"))	// eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("0:macaddr"))	// eth2(5G)
				nvram_set("0:macaddr", "00:22:15:A5:03:04");
			if (nvram_get("regulation_domain_5G")) {// by ate command from asuswrt, prior than ui 2.0
				nvram_set("wl1_country_code", nvram_get("regulation_domain_5G"));
				nvram_set("0:ccode", nvram_get("regulation_domain_5G"));
			} else if (nvram_get("regulation_domain_5g")) {	// by ate command from ui 2.0
				nvram_set("wl1_country_code", nvram_get("regulation_domain_5g"));
				nvram_set("0:ccode", nvram_get("regulation_domain_5g"));
			}
			else {
				nvram_set("wl1_country_code", "US");
				nvram_set("0:ccode", "US");
			}
			nvram_set("sb/1/macaddr", nvram_safe_get("et0macaddr"));
			if (ptr && *ptr) {
				if ((strlen(ptr) == 6) && /* legacy format */
					!strncasecmp(ptr, "0x", 2))
				{
					nvram_set("wl0_country_code", ptr+4);
					nvram_set("sb/1/ccode", ptr+4);
				}
				else
				{
					nvram_set("wl0_country_code", ptr);
					nvram_set("sb/1/ccode", ptr);
				}
			} else {
				nvram_set("wl0_country_code", "US");
				nvram_set("sb/1/ccode", "US");
			}
			break;

		case MODEL_RTN66U:
		case MODEL_RTAC66U:
			if (!nvram_get("et0macaddr"))		// eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("pci/2/1/macaddr"))	// eth2(5G)
				nvram_set("pci/2/1/macaddr", "00:22:15:A5:03:04");
			if (nvram_get("regulation_domain_5G")) {
				nvram_set("wl1_country_code", nvram_get("regulation_domain_5G"));
				nvram_set("pci/2/1/ccode", nvram_get("regulation_domain_5G"));
			} else {
				nvram_set("wl1_country_code", "US");
				nvram_set("pci/2/1/ccode", "US");
			}
			nvram_set("pci/1/1/macaddr", nvram_safe_get("et0macaddr"));
			if (ptr) {
				nvram_set("wl0_country_code", ptr);
				nvram_set("pci/1/1/ccode", ptr);
			} else {
				nvram_set("wl0_country_code", "US");
				nvram_set("pci/1/1/ccode", "US");
			}
			break;

		case MODEL_RTN18U:
			if (!nvram_get("et0macaddr"))	//eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			nvram_set("0:macaddr", nvram_safe_get("et0macaddr"));

			if (nvram_match("0:ccode", "0")) {
				nvram_set("0:ccode","US");
			}
			break;

		case MODEL_RTAC87U:
			if (!nvram_get("et1macaddr"))	//eth0, eth1
				nvram_set("et1macaddr", "00:22:15:A5:03:00");
			nvram_set("0:macaddr", nvram_safe_get("et1macaddr"));
			nvram_set("LANMACADDR", nvram_safe_get("et1macaddr"));
			break;

		case MODEL_DSLAC68U:
		case MODEL_RTAC68U:
		case MODEL_RTAC56S:
		case MODEL_RTAC56U:
			if (!nvram_get("et0macaddr"))	//eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("1:macaddr"))	//eth2(5G)
				nvram_set("1:macaddr", "00:22:15:A5:03:04");
			nvram_set("0:macaddr", nvram_safe_get("et0macaddr"));
			break;

		case MODEL_RTAC5300:
		case MODEL_RTAC88U:
			if (!nvram_get("lan_hwaddr"))
				nvram_set("lan_hwaddr", cfe_nvram_safe_get("et1macaddr"));

			break;

		case MODEL_RTAC3100:
		case MODEL_GTAC5300:
		case MODEL_RTAC86U:
			if (!nvram_get("lan_hwaddr"))
				nvram_set("lan_hwaddr", cfe_nvram_safe_get("et0macaddr"));
			break;

		case MODEL_RTAC3200:
			if (!nvram_get("et0macaddr"))				// eth0 (ethernet)
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			nvram_set("1:macaddr", nvram_safe_get("et0macaddr"));	// eth2 (2.4GHz)
			if (!nvram_get("0:macaddr"))				// eth1(5GHz)
				nvram_set("0:macaddr", "00:22:15:A5:03:04");
			if (!nvram_get("2:macaddr"))				// eth3(5GHz)
				nvram_set("2:macaddr", "00:22:15:A5:03:08");
			break;

		case MODEL_RTAC53U:
			if (!nvram_get("et0macaddr"))	//eth0, eth1
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("0:macaddr"))	//eth2(5G)
				nvram_set("0:macaddr", "00:22:15:A5:03:04");
			nvram_set("sb/1/macaddr", nvram_safe_get("et0macaddr"));
			break;

		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
			if (!nvram_get("et0macaddr"))
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
			if (!nvram_get("sb/1/macaddr"))	// (5GHz)
				nvram_set("sb/1/macaddr", "00:22:15:A5:03:04");
			nvram_set("0:macaddr", nvram_safe_get("et0macaddr")); // (2.4GHz)

		default:
#ifdef RTCONFIG_RGMII_BRCM5301X
			if (!nvram_get("lan_hwaddr"))
				nvram_set("lan_hwaddr", cfe_nvram_get("et1macaddr"));
#else
			if (!nvram_get("et0macaddr"))
				nvram_set("et0macaddr", "00:22:15:A5:03:00");
#endif
			break;
	}

#ifdef RTCONFIG_ODMPID
	if (nvram_match("odmpid", "ASUS") ||
		!is_valid_hostname(nvram_safe_get("odmpid")) ||
		!strcmp(RT_BUILD_NAME, nvram_safe_get("odmpid")))
		nvram_set("odmpid", "");
#endif

	if (nvram_get("secret_code"))
		nvram_set("wps_device_pin", nvram_get("secret_code"));
	else
		nvram_set("wps_device_pin", "12345670");
}

#ifdef RTCONFIG_BCMARM
#ifdef HND_ROUTER
void tweak_usb_affinity(int enable)
{
	char *usb_affinity = nvram_get("usb_affinity");

	if (usb_affinity) {
		if (enable) {
			f_write_string("/proc/irq/28/smp_affinity", usb_affinity, 0, 0);
			f_write_string("/proc/irq/29/smp_affinity", usb_affinity, 0, 0);
			f_write_string("/proc/irq/30/smp_affinity", usb_affinity, 0, 0);
		} else {
			f_write_string("/proc/irq/28/smp_affinity", "f", 0, 0);
			f_write_string("/proc/irq/29/smp_affinity", "f", 0, 0);
			f_write_string("/proc/irq/30/smp_affinity", "f", 0, 0);
		}
	}
}

void init_others(void)
{
	eval("insmod", "hnd");
}
#else

#define ASUS_TWEAK

#ifdef RTCONFIG_BCM_7114
#define SMP_AFFINITY_WL	"1"
#else
#define SMP_AFFINITY_WL "3"
#endif

void tweak_smp_affinity(int enable_samba)
{
#ifndef RTCONFIG_BCM7
	if (nvram_get_int("stop_tweak_wl") == 1)
#endif
		return;

#ifdef RTCONFIG_GMAC3
	if (nvram_match("gmac3_enable", "1"))
		return;
#endif

#ifdef RTCONFIG_BCM7114
	if (enable_samba) {
		f_write_string("/proc/irq/163/smp_affinity", SMP_AFFINITY_WL, 0, 0);
		f_write_string("/proc/irq/169/smp_affinity", SMP_AFFINITY_WL, 0, 0);
	}
	else
#endif
	{
		f_write_string("/proc/irq/163/smp_affinity", "2", 0, 0);
		f_write_string("/proc/irq/169/smp_affinity", "2", 0, 0);
	}
}

void init_others(void)
{
#ifdef SMP
	int fd;

	if ((fd = open("/proc/irq/163/smp_affinity", O_RDWR)) >= 0) {
		close(fd);
#ifdef RTCONFIG_GMAC3
		if (nvram_match("gmac3_enable", "1")) {
			if (nvram_match("asus_tweak_usb_disable", "1")) {
				char *fwd_cpumap;

				/* Place network interface vlan1/eth0 on CPU hosting 5G upper */
				fwd_cpumap = nvram_get("fwd_cpumap");

				if (fwd_cpumap == NULL) {
					/* BCM4709acdcrh: Network interface GMAC on Core#0
					 * [5G+2G:163 on Core#0] and [5G:169 on Core#1].
					 * Bind et2:vlan1:eth0:181 to Core#0
					 * Note, USB3 xhci_hcd's irq#112 binds Core#1
					 * bind eth0:181 to Core#1 impacts USB3 performance
					 */
					f_write_string("/proc/irq/181/smp_affinity", "1", 0, 0);
				} else {
					char cpumap[32], *next;

					foreach(cpumap, fwd_cpumap, next) {
						char mode, chan;
						int band, irq, cpu;

						/* Format: mode:chan:band#:irq#:cpu# */
						if (sscanf(cpumap, "%c:%c:%d:%d:%d",
							&mode, &chan, &band, &irq, &cpu) != 5) {
							break;
						}
						if (cpu > 1) {
							break;
						}
						/* Find the single 5G upper */
						if ((chan == 'u') || (chan == 'U')) {
							char command[128];
							snprintf(command, sizeof(command),
								"echo %d > /proc/irq/181/smp_affinity",
								1 << cpu);
							system(command);
							break;
						}
					}
				}
			}
			else
#ifdef RTCONFIG_BCM_7114
				f_write_string("/proc/irq/181/smp_affinity", "1", 0, 0);
#else
				f_write_string("/proc/irq/181/smp_affinity", "3", 0, 0);
#endif
		} else
#endif
		{
#ifdef ASUS_TWEAK
#ifdef RTCONFIG_BCM_7114
			f_write_string("/proc/irq/180/smp_affinity", "1", 0, 0);
#endif
#ifndef RTCONFIG_BCM7
			if (nvram_match("asus_tweak", "1")) {	/* dbg ref ? */
				f_write_string("/proc/irq/179/smp_affinity", "1", 0, 0);	// eth0
				f_write_string("/proc/irq/163/smp_affinity", "2", 0, 0);	// eth1 or eth1/eth2
				f_write_string("/proc/irq/169/smp_affinity", "2", 0, 0);	// eth2 or eth3
			} else
#endif	// RTCONFIG_BCM7
				tweak_smp_affinity(0);
#endif	// ASUS_TWEAK
		}

		if (!nvram_get_int("stop_tweak_usb")) {
			f_write_string("/proc/irq/111/smp_affinity", "2", 0, 0);		// ehci, ohci
			f_write_string("/proc/irq/112/smp_affinity", "2", 0, 0);		// xhci
		}
	}
#endif	// SMP

#ifdef ASUS_TWEAK
#ifdef RTCONFIG_BCM_7114
	nvram_unset("txworkq");
#else
	if (nvram_match("enable_samba", "1") &&
#ifdef RTCONFIG_USB_XHCI
		nvram_match("usb_usb3", "1") &&
#endif
		!factory_debug()) {
		nvram_set("txworkq", "1");
#if !defined(RTCONFIG_BCM9) && !defined(RTCONFIG_BCM7)
		nvram_set("txworkq_wl", "1");
#endif
	} else {
		nvram_unset("txworkq");
#if !defined(RTCONFIG_BCM9) && !defined(RTCONFIG_BCM7)
		nvram_unset("txworkq_wl");
#endif
	}
#endif
#endif // ASUS_TWEAK

#if defined(RTAC68U) && !defined(RTAC68A) && !defined(RT4GAC68U)
	update_cfe();
#endif
#ifdef RTAC3200
	update_cfe_ac3200();
	update_cfe_ac3200_128k();
#endif
#ifdef RTAC68U
	ac68u_cofs();
#endif
}
#endif	// HND_ROUTER
#endif	// RTCONFIG_BCMARM

void chanspec_fix_5g(int unit)
{
	char tmp[100], prefix[]="wlXXXXXXX_";
	int channel;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
	channel = nvram_get_int(strcat_r(prefix, "channel", tmp));

	if ((channel == 36) || (channel == 44) || (channel == 52) || (channel == 60) || (channel == 100) || (channel == 108) || (channel == 116) || (channel == 124) || (channel == 132) || (channel == 149) || (channel == 157))
	{
		dbG("fix nctrlsb of channel %d as %s\n", channel, "lower");
		nvram_set(strcat_r(prefix, "nctrlsb", tmp), "lower");
	}
	else if ((channel == 40) || (channel == 48) || (channel == 56) || (channel == 64) || (channel == 104) || (channel == 112) || (channel == 120) || (channel == 128) || (channel == 136) || (channel == 153) || (channel == 161))
	{
		dbG("fix nctrlsb of channel %d as %s\n", channel, "upper");
		nvram_set(strcat_r(prefix, "nctrlsb", tmp), "upper");
	}
}

// this function is used to jutisfy which band(unit) to be forced connected.
int is_ure(int unit)
{
	// forced to connect to which band
	// is it suitable
	if (sw_mode() == SW_MODE_REPEATER) {
		if (nvram_get_int("wlc_band") == unit) return 1;
	}
	return 0;
}

int wl_max_no_vifs(int unit)
{
	char nv_interface[NVRAM_MAX_PARAM_LEN];
	char cap[WLC_IOCTL_SMLEN];
	char caps[WLC_IOCTL_SMLEN];
	char *name = NULL;
	char *next = NULL;
	int max_no_vifs = 0;
#ifdef RTCONFIG_PSR_GUEST
	char tmp[100], prefix[]="wlXXXXXXX_";
#endif

#ifdef RTCONFIG_QTN
	if (unit == 1)
		return 4;
#endif

	snprintf(nv_interface, sizeof(nv_interface), "wl%d_ifname", unit);
#ifdef RTCONFIG_PSR_GUEST
	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#endif
	name = nvram_safe_get(nv_interface);
	if (!strlen(name))
	{
		sprintf(nv_interface, "eth%d", unit + 1);
		name = nv_interface;
	}

	if (!wl_iovar_get(name, "cap", (void *)caps, WLC_IOCTL_SMLEN)) {
		foreach(cap, caps, next) {
			if (!strcmp(cap, "mbss16"))
				max_no_vifs = 16;
			else if (!strcmp(cap, "mbss4"))
				max_no_vifs = 4;
		}
	}

#ifdef RTCONFIG_PSR_GUEST
	if ((!atoi (nvram_safe_get("ure_disable"))) ||
	    (!strcmp ("psr", nvram_safe_get(strcat_r(prefix, "mode", tmp))))) {
#if 1
		if (nvram_match(strcat_r(prefix, "psr_guest", tmp), "1"))
			max_no_vifs = 5;
		else
#endif
			max_no_vifs = 2;
	}
#endif

	return max_no_vifs;
}

#if (defined(RTCONFIG_BCM7) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)) && defined(BCM_BSD)

#define BSD_STA_SELECT_POLICY_NVRAM		"bsd_sta_select_policy"
#define BSD_STA_SELECT_POLICY_FLAG_NON_VHT	0x00000008	/* NON VHT STA */
#define BSD_IF_QUALIFY_POLICY_NVRAM		"bsd_if_qualify_policy"
#define BSD_QUALIFY_POLICY_FLAG_NON_VHT		0x00000004	/* NON VHT STA */
#if defined(RTAC5300) || defined(GTAC5300)
#define BSD_STA_SELECT_POLICY_NVRAM_X		"bsd_sta_select_policy_x"
#define BSD_IF_QUALIFY_POLICY_NVRAM_X		"bsd_if_qualify_policy_x"
#endif

int get_bsd_nonvht_status(int unit)
{
	char tmp[100], prefix[]="wlXXXXXXX_";
	char *str;
	int num;
	unsigned int i1,i2,i3,i4,i5,i6,i7,i8,i9,ia;
	unsigned int flags;

	snprintf(prefix, sizeof(prefix), "wl%d_", unit);
#if defined(RTAC5300) || defined(GTAC5300)
	if (nvram_get_int("smart_connect_x") == 2) // 5GHz Only
		str = nvram_get(strcat_r(prefix, BSD_STA_SELECT_POLICY_NVRAM_X, tmp));
	else
#endif
		str = nvram_get(strcat_r(prefix, BSD_STA_SELECT_POLICY_NVRAM, tmp));
	if (str) {
		num = sscanf(str, "%d %d %d %d %d %d %d %d %d %d %x",
			&i1, &i2, &i3, &i4, &i5, &i6, &i7, &i8, &i9, &ia, &flags);
		if ((num == 11) && (flags & BSD_STA_SELECT_POLICY_FLAG_NON_VHT))
			return 1;
	}
#if defined(RTAC5300) || defined(GTAC5300)
	if (nvram_get_int("smart_connect_x") == 2) // 5GHz Only
		str = nvram_get(strcat_r(prefix, BSD_IF_QUALIFY_POLICY_NVRAM_X, tmp));
	else
#endif
		str = nvram_get(strcat_r(prefix, BSD_IF_QUALIFY_POLICY_NVRAM, tmp));
	if (str) {
		num = sscanf(str, "%d %x", &i1, &flags);
		if ((num == 2) && (flags & BSD_QUALIFY_POLICY_FLAG_NON_VHT))
			return 1;
	}

	return 0;
}
#endif

void generate_wl_para(char *ifname, int unit, int subunit)
{
	dbG("unit %d subunit %d\n", unit, subunit);

	char tmp[100], prefix[]="wlXXXXXXX_";
	char tmp2[100], prefix2[]="wlXXXXXXX_";
	char *list, *list2;
	int list_size;
	char *nv, *nvp, *b;
	char word[256], *next;
#ifndef RTCONFIG_BCMWL6
	int match;
#endif
	int i, mcast_rate;
	char interface_list[NVRAM_MAX_VALUE_LEN];
	int interface_list_size = sizeof(interface_list);
	char nv_interface[NVRAM_MAX_PARAM_LEN];
	int if_unit, ure;
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
	int max_no_vifs = wl_max_no_vifs(unit);
	char lan_ifnames[NVRAM_MAX_PARAM_LEN] = "lan_ifnames";
	bool psta = 0, psr = 0;
#ifdef RTCONFIG_AMAS
	char prefix_local[]="wlXXXXXXX_";
#endif
#endif
#ifdef RTCONFIG_BCMWL6
	int phytype;
	char buf[8];
#endif

	if (subunit == -1)
	{
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

#ifdef RTCONFIG_BCMWL6
		if (!wl_ioctl(ifname, WLC_GET_PHYTYPE, &phytype, sizeof(phytype))) {
			memset(buf, 0, sizeof(buf));
			snprintf(buf, sizeof(buf), "%s", WLCONF_PHYTYPE2STR(phytype));
			nvram_set(strcat_r(prefix, "phytype", tmp), buf);
		}
#endif

		nvram_set(strcat_r(prefix, "wps_mode", tmp), (nvram_match("wps_enable", "1") && (nvram_match(strcat_r(prefix, "mode", tmp2), "ap")
#ifdef RTCONFIG_PROXYSTA
			|| (!unit && (is_dpsr(unit)
#ifdef RTCONFIG_DPSTA
				|| is_dpsta(unit)
#endif
			))
#endif
			))
				? "enabled" : "disabled");

#ifdef BCM_BSD
		if (((unit == 0) && nvram_get_int("smart_connect_x") == 1) ||
		    ((unit == 1) && nvram_get_int("smart_connect_x") == 2)) {
			int wlif_count = num_of_wl_if ();
			for (i = unit + 1; i < wlif_count; i++) {
				snprintf(prefix2, sizeof(prefix2), "wl%d_", i);
				nvram_set(strcat_r(prefix2, "ssid", tmp2), nvram_safe_get(strcat_r(prefix, "ssid", tmp)));
				nvram_set(strcat_r(prefix2, "auth_mode_x", tmp2), nvram_safe_get(strcat_r(prefix, "auth_mode_x", tmp)));
				nvram_set(strcat_r(prefix2, "wep_x", tmp2), nvram_safe_get(strcat_r(prefix, "wep_x", tmp)));
				nvram_set(strcat_r(prefix2, "key", tmp2), nvram_safe_get(strcat_r(prefix, "key", tmp)));
				nvram_set(strcat_r(prefix2, "key1", tmp2), nvram_safe_get(strcat_r(prefix, "key1", tmp)));
				nvram_set(strcat_r(prefix2, "key2", tmp2), nvram_safe_get(strcat_r(prefix, "key2", tmp)));
				nvram_set(strcat_r(prefix2, "key3", tmp2), nvram_safe_get(strcat_r(prefix, "key3", tmp)));
				nvram_set(strcat_r(prefix2, "key4", tmp2), nvram_safe_get(strcat_r(prefix, "key4", tmp)));
				nvram_set(strcat_r(prefix2, "phrase_x", tmp2), nvram_safe_get(strcat_r(prefix, "phrase_x", tmp)));
				nvram_set(strcat_r(prefix2, "crypto", tmp2), nvram_safe_get(strcat_r(prefix, "crypto", tmp)));
				nvram_set(strcat_r(prefix2, "wpa_psk", tmp2), nvram_safe_get(strcat_r(prefix, "wpa_psk", tmp)));
				nvram_set(strcat_r(prefix2, "radius_ipaddr", tmp2), nvram_safe_get(strcat_r(prefix, "radius_ipaddr", tmp)));
				nvram_set(strcat_r(prefix2, "radius_key", tmp2), nvram_safe_get(strcat_r(prefix, "radius_key", tmp)));
				nvram_set(strcat_r(prefix2, "radius_port", tmp2), nvram_safe_get(strcat_r(prefix, "radius_port", tmp)));
				nvram_set(strcat_r(prefix2, "closed", tmp2), nvram_safe_get(strcat_r(prefix, "closed", tmp)));
			}
		}

		int acs = 1;
#if 0
		if ((unit == 0) && (nvram_get_int("smart_connect_x") == 1))
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 1);
		else if ((unit == 1) && nvram_get_int("smart_connect_x"))
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 1);
		else if ((unit == 2) && nvram_get_int("smart_connect_x"))
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 1);
		else {
			nvram_set_int(strcat_r(prefix, "taf_enable", tmp), 0);
			acs = 0 ;
		}
#else
		if (!(((unit == 0) && (nvram_get_int("smart_connect_x") == 1)) ||
		      ((unit == 1) && nvram_get_int("smart_connect_x")) ||
		      ((unit == 2) && nvram_get_int("smart_connect_x"))))
			acs = 0;
#endif

		if (acs) {
			nvram_set(strcat_r(prefix, "nmode_x", tmp), "0");
			nvram_set(strcat_r(prefix, "bw", tmp), "0");
			nvram_set(strcat_r(prefix, "chanspec", tmp), "0");
		}
#endif

		if_unit = 0;
		foreach (word, nvram_safe_get("wl_ifnames"), next) {
			ure = is_ure(if_unit);
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
			psta = is_psta(if_unit);
			psr = is_psr(if_unit);
#endif

			if (ure
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
				|| psta || psr
#endif
			) {
				nvram_set(strcat_r(prefix, "nmode_x", tmp), "0");
#ifdef RTCONFIG_BCMWL6
				nvram_set(strcat_r(prefix, "bw", tmp), "0");
#else
				nvram_set(strcat_r(prefix, "bw", tmp), "1");
#endif
				nvram_set(strcat_r(prefix, "chanspec", tmp), "0");

				break;
			}

			if_unit++;
		}

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
		/* Disable all VIFS wlX.2 onwards */
		if (is_psta(unit) || is_psr(unit))
		{
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");

			/* Set the wl mode for the virtual interface */
			strncpy(interface_list, nvram_safe_get(lan_ifnames), interface_list_size);
			sprintf(nv_interface, "wl%d.1", unit);
			sprintf(tmp, "wl%d.1_mode", unit);
			if (is_psta(unit)) {
				/* For Proxy we need to remove our ap interface */
				remove_from_list(nv_interface, interface_list, interface_list_size);
				nvram_set(lan_ifnames, interface_list);

				/* Clear the ap mode */
				nvram_set(tmp, "");
			} else {
				/* For Repeater we need to add our ap interface to the bridged lan */
				add_to_list(nv_interface, interface_list, interface_list_size);
				nvram_set(lan_ifnames, interface_list);

				/* Set the ap mode */
				nvram_set(tmp, "ap");
			}

#ifdef RTCONFIG_PSR_GUEST
			for (i = max_no_vifs; i < WL_MAXBSSCFG; i++) {
#else
			for (i = 2; i < max_no_vifs; i++) {
#endif
				sprintf(tmp, "wl%d.%d_bss_enabled", unit, i);
				nvram_set(tmp, "0");
			}
		}

#ifdef RTCONFIG_PSR_GUEST
		nvram_set(strcat_r(prefix, "mbss_rmac", tmp), (nvram_match(strcat_r(prefix, "psr_guest", tmp2), "1") && (dpsr_mode()
#ifdef RTCONFIG_DPSTA
			|| dpsta_mode()
#endif
			)) ? "1" : "0");
#else
		nvram_unset(strcat_r(prefix, "mbss_rmac", tmp));
#endif
#endif

		memset(interface_list, 0, interface_list_size);
		for (i = 1; i < WL_MAXBSSCFG; i++) {
			sprintf(nv_interface, "wl%d.%d", unit, i);
			add_to_list(nv_interface, interface_list, interface_list_size);
			nvram_set(strcat_r(nv_interface, "_hwaddr", tmp), "");
		}

		reset_mssid_hwaddr(unit);
	}
	else
	{
		snprintf(prefix, sizeof(prefix), "wl%d.%d_", unit, subunit);
		snprintf(prefix2, sizeof(prefix2), "wl%d_", unit);
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
		if (is_psta(unit))
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
		else if (is_psr(unit)) {
			if (subunit == 1)
			{
#ifdef RTCONFIG_DPSTA
#ifdef RTCONFIG_AMAS
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), ((!dpsta_mode() && !dpsr_mode()) || is_dpsta(unit) || is_dpsr(unit) || dpsta_mode()) ? "1" : "0");

#else
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), ((!dpsta_mode() && !dpsr_mode()) || is_dpsta(unit) || is_dpsr(unit)) ? "1" : "0");
#endif
				if (!unit)
				nvram_set(strcat_r(prefix, "wps_mode", tmp), "enabled");
#else
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
#endif
			}
			else
#ifdef RTCONFIG_PSR_GUEST
			if (!nvram_match(strcat_r(prefix2, "psr_guest", tmp), "1"))
#endif
				nvram_set(strcat_r(prefix, "bss_enabled", tmp), "0");
		}
#endif
	}

	// convert wlc_xxx to wlX_ according to wlc_band == unit
	if (is_ure(unit)) {
		if (subunit == -1) {
			nvram_set("ure_disable", "0");
			nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ssid"));
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));
			nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));
			if (nvram_get_int("wlc_wep")) {
				nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));
				nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
				nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
				nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
				nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
			}
			nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
		} else if (subunit == 1) {
			nvram_set(strcat_r(prefix, "bss_enabled", tmp), "1");
/*
			nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ure_ssid"));
			nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));
			nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));
			nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));
			nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
			nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
			nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
			nvram_set(strcat_r(prefix, "bw", tmp), nvram_safe_get("wlc_nbw_cap"));
*/
		}
	}
	// TODO: recover nvram from repeater
	else
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
	if (is_psta(unit) || is_psr(unit)) {
		if (subunit == -1) {
			nvram_set("ure_disable", "1");
			if (
#ifdef RTCONFIG_DPSTA
#ifdef RTCONFIG_AMAS
				dpsta_mode() ||
#endif
				is_dpsta(unit) ||
#endif
				is_dpsr(unit)) {
#ifdef RTCONFIG_AMAS
				snprintf(prefix2, sizeof(prefix2), "wlc%d_", unit);
#else
				snprintf(prefix2, sizeof(prefix2), "wlc%d_", unit ? 1 : 0);
#endif
				nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get(strcat_r(prefix2, "ssid", tmp2)));
				nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get(strcat_r(prefix2, "auth_mode", tmp2)));
				nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get(strcat_r(prefix2, "wep", tmp2)));
				if (nvram_get_int(strcat_r(prefix2, "wep", tmp))) {
					nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get(strcat_r(prefix2, "key", tmp2)));
					nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
					nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
					nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
					nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
				}
				nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get(strcat_r(prefix2, "crypto", tmp2)));
				nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get(strcat_r(prefix2, "wpa_psk", tmp2)));
#ifdef RTCONFIG_AMAS
				snprintf(prefix_local, sizeof(prefix_local), "wl%d.%d_", unit, 1);
				nvram_set(strcat_r(prefix_local, "ssid", tmp), nvram_safe_get(strcat_r(prefix2, "ssid", tmp2)));
				nvram_set(strcat_r(prefix_local, "auth_mode_x", tmp), nvram_safe_get(strcat_r(prefix2, "auth_mode", tmp2)));
				nvram_set(strcat_r(prefix_local, "wep_x", tmp), nvram_safe_get(strcat_r(prefix2, "wep", tmp2)));
				if (nvram_get_int(strcat_r(prefix2, "wep", tmp))) {
					nvram_set(strcat_r(prefix_local, "key", tmp), nvram_safe_get(strcat_r(prefix2, "key", tmp2)));
					nvram_set(strcat_r(prefix_local, "key1", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
					nvram_set(strcat_r(prefix_local, "key2", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
					nvram_set(strcat_r(prefix_local, "key3", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
					nvram_set(strcat_r(prefix_local, "key4", tmp), nvram_safe_get(strcat_r(prefix2, "wep_key", tmp2)));
				}
				nvram_set(strcat_r(prefix_local, "crypto", tmp), nvram_safe_get(strcat_r(prefix2, "crypto", tmp2)));
				nvram_set(strcat_r(prefix_local, "wpa_psk", tmp), nvram_safe_get(strcat_r(prefix2, "wpa_psk", tmp2)));
#endif
			}
			else
			{
				nvram_set(strcat_r(prefix, "ssid", tmp), nvram_safe_get("wlc_ssid"));
				nvram_set(strcat_r(prefix, "auth_mode_x", tmp), nvram_safe_get("wlc_auth_mode"));
				nvram_set(strcat_r(prefix, "wep_x", tmp), nvram_safe_get("wlc_wep"));
				if (nvram_get_int("wlc_wep")) {
					nvram_set(strcat_r(prefix, "key", tmp), nvram_safe_get("wlc_key"));
					nvram_set(strcat_r(prefix, "key1", tmp), nvram_safe_get("wlc_wep_key"));
					nvram_set(strcat_r(prefix, "key2", tmp), nvram_safe_get("wlc_wep_key"));
					nvram_set(strcat_r(prefix, "key3", tmp), nvram_safe_get("wlc_wep_key"));
					nvram_set(strcat_r(prefix, "key4", tmp), nvram_safe_get("wlc_wep_key"));
				}
				nvram_set(strcat_r(prefix, "crypto", tmp), nvram_safe_get("wlc_crypto"));
				nvram_set(strcat_r(prefix, "wpa_psk", tmp), nvram_safe_get("wlc_wpa_psk"));
			}

			/* early set wlx_vifs for psr mode */
			if (is_psr(unit)
#ifdef RTCONFIG_PSR_GUEST
				&& !nvram_match(strcat_r(prefix, "psr_guest", tmp), "1")
#endif
			) {
				sprintf(tmp2, "wl%d.1", unit);
				nvram_set(strcat_r(prefix, "vifs", tmp), tmp2);
			}
		}

		if (is_psr(unit) && (subunit == 1)) {
			/* TODO: local AP profile */
		}
	}
#endif

	memset(tmp, 0, sizeof(tmp));
	memset(tmp2, 0, sizeof(tmp2));

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "shared"))
		nvram_set(strcat_r(prefix, "auth", tmp), "1");
	else nvram_set(strcat_r(prefix, "auth", tmp), "0");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk"))
		nvram_set(strcat_r(prefix, "akm", tmp), "psk");
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "psk2"))
	{
#if defined(HND_ROUTER) && defined(WLHOSTFBT)
		nvram_set(strcat_r(prefix, "akm", tmp), "psk2 psk2ft");
#else
		nvram_set(strcat_r(prefix, "akm", tmp), "psk2");
#endif
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "pskpsk2"))
#if defined(HND_ROUTER) && defined(WLHOSTFBT)
		nvram_set(strcat_r(prefix, "akm", tmp), "psk psk2 psk2ft");
#else
		nvram_set(strcat_r(prefix, "akm", tmp), "psk psk2");
#endif
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa2");
	}
	else if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "wpawpa2"))
	{
		nvram_set(strcat_r(prefix, "akm", tmp), "wpa wpa2");
	}
	else nvram_set(strcat_r(prefix, "akm", tmp), "");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
	{
		nvram_set(strcat_r(prefix, "auth_mode", tmp), "radius");
		nvram_set(strcat_r(prefix, "key", tmp), "2");
	}
	else nvram_set(strcat_r(prefix, "auth_mode", tmp), "none");

	if (nvram_match(strcat_r(prefix, "auth_mode_x", tmp), "radius"))
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else if (nvram_invmatch(strcat_r(prefix, "akm", tmp), ""))
		nvram_set(strcat_r(prefix, "wep", tmp), "disabled");
	else if (nvram_get_int(strcat_r(prefix, "wep_x", tmp)) != 0)
		nvram_set(strcat_r(prefix, "wep", tmp), "enabled");
	else nvram_set(strcat_r(prefix, "wep", tmp), "disabled");

	if (nvram_match(strcat_r(prefix, "mode", tmp), "ap") &&
	    strstr(nvram_safe_get(strcat_r(prefix, "akm", tmp2)), "wpa"))
		nvram_set(strcat_r(prefix, "preauth", tmp), "1");
	else
		nvram_set(strcat_r(prefix, "preauth", tmp), "");

	if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled")) {

		nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "maclist_x", tmp)));
		list_size = sizeof(char) * (strlen(nv)+1);
		list = (char*) malloc(list_size);
		list2 = (char*) malloc(list_size);
		list[0] = 0;

		if (nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if (strlen(b) == 0) continue;
				if (list[0] == 0)
					sprintf(list, "%s", b);
				else {
					sprintf(list2, "%s %s", list, b);
					strlcpy(list, list2, list_size);
				}
			}
			free(nv);
		}
		nvram_set(strcat_r(prefix, "maclist", tmp), list);
		free(list);
		free(list2);
	}
	else
		nvram_set(strcat_r(prefix, "maclist", tmp), "");

	if (subunit == -1)
	{
#ifdef RTCONFIG_BCM_7114
		/* for old fw(135x) compatibility, and don't use wlc_psta=3 afterwards */
		if (nvram_get_int("wlc_psta") == 3)
			nvram_set("wlc_psta", "1");
#endif
		// wds mode control
		if (is_ure(unit))
		{
			nvram_set(strcat_r(prefix, "mode", tmp), "wet");
		}
		else
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
		if (is_psta(unit))
		{
			nvram_set(strcat_r(prefix, "mode", tmp), "psta");
		}
		else if (is_psr(unit))
		{
			nvram_set(strcat_r(prefix, "mode", tmp), "psr");
		}
		else
#endif
		if (nvram_match(strcat_r(prefix, "mode_x", tmp), "1") &&	// wds only
			(is_router_mode() || access_point_mode()))
			nvram_set(strcat_r(prefix, "mode", tmp), "wds");
		else
			nvram_set(strcat_r(prefix, "mode", tmp), "ap");

#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
#ifdef RTCONFIG_BCMARM
		int dwds = 0;
		if (!nvram_get_int("dwds_ctrl")) {
			dwds = !is_ure(unit);
			nvram_set(strcat_r(prefix, "dwds", tmp), dwds ? "1" : "0");
		}
		dwds = nvram_get_int(strcat_r(prefix, "dwds", tmp));
		if (!nvram_get_int("psr_mrpt_ctrl"))
			nvram_set(strcat_r(prefix, "psr_mrpt", tmp), (is_psr(unit) && (!dwds)) ? "1" : "0");
#else
		if (!nvram_get_int("psr_mrpt_ctrl"))
			nvram_set(strcat_r(prefix, "psr_mrpt", tmp), is_psr(unit) ? "1" : "0");
#endif
#endif

		// TODO use lazwds directly
		//if (!nvram_match(strcat_r(prefix, "wdsmode_ex", tmp), "2"))
		//	nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
		//else nvram_set(strcat_r(prefix, "lazywds", tmp), "0");

		// TODO need sw_mode ?
		// handle wireless wds list
		if (!nvram_match(strcat_r(prefix, "mode_x", tmp), "0")) {
			if (nvram_match(strcat_r(prefix, "wdsapply_x", tmp), "1")) {
				nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "wdslist", tmp)));
				list_size = sizeof(char) * (strlen(nv)+1);
				list = (char*) malloc(list_size);
				list2 = (char*) malloc(list_size);
				list[0] = 0;

				if (nv) {
					while ((b = strsep(&nvp, "<")) != NULL) {
						if (strlen(b) == 0) continue;
						if (list[0] == 0)
							sprintf(list, "%s", b);
						else {
							sprintf(list2, "%s %s", list, b);
							strlcpy(list, list2, list_size);
						}
					}
					free(nv);
				}
				nvram_set(strcat_r(prefix, "wds", tmp), list);
				nvram_set(strcat_r(prefix, "lazywds", tmp), "0");
				free(list);
				free(list2);
			}
			else {
				nvram_set(strcat_r(prefix, "wds", tmp), "");
				nvram_set(strcat_r(prefix, "lazywds", tmp), "1");
			}
		} else {
			nvram_set(strcat_r(prefix, "wds", tmp), "");
			nvram_set(strcat_r(prefix, "lazywds", tmp), "0");
		}

		switch (nvram_get_int(strcat_r(prefix, "mrate_x", tmp))) {
		case 0: /* Auto */
			mcast_rate = 0;
			break;
		case 1: /* Legacy CCK 1Mbps */
			mcast_rate = 1000000;
			break;
		case 2: /* Legacy CCK 2Mbps */
			mcast_rate = 2000000;
			break;
		case 3: /* Legacy CCK 5.5Mbps */
			mcast_rate = 5500000;
			break;
		case 4: /* Legacy OFDM 6Mbps */
			mcast_rate = 6000000;
			break;
		case 5: /* Legacy OFDM 9Mbps */
			mcast_rate = 9000000;
			break;
		case 6: /* Legacy CCK 11Mbps */
			mcast_rate = 11000000;
			break;
		case 7: /* Legacy OFDM 12Mbps */
			mcast_rate = 12000000;
			break;
		case 8: /* Legacy OFDM 18Mbps */
			mcast_rate = 18000000;
			break;
		case 9: /* Legacy OFDM 24Mbps */
			mcast_rate = 24000000;
			break;
		case 10: /* Legacy OFDM 36Mbps */
			mcast_rate = 36000000;
			break;
		case 11: /* Legacy OFDM 48Mbps */
			mcast_rate = 48000000;
			break;
		case 12: /* Legacy OFDM 54Mbps */
			mcast_rate = 54000000;
			break;
		default: /* Auto */
			mcast_rate = 0;
			break;
		}
		nvram_set_int(strcat_r(prefix, "mrate", tmp), mcast_rate);

		if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "0"))		// auto => b/g/n mixed or a/n(/ac) mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "1");
#ifdef RTCONFIG_BCM_7114
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
#else
			nvram_set(strcat_r(prefix, "gmode", tmp), nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? "1" : "-1");	// 1: 54g Auto, 4: 4g Performance, 5: 54g LRS, 0: 802.11b Only
#endif
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
#if (defined(RTCONFIG_BCM7) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)) && defined(BCM_BSD)
			if (nvram_get_int("smart_connect_x") && get_bsd_nonvht_status(unit))
				nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "3");	// devices must advertise VHT (11ac) capabilities to be allowed to associate
			else
#endif
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "1"))	// n only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "1");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? "1" : "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "2");	// devices must advertise HT (11n) capabilities to be allowed to associate
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "4"))	// g/n mixed or a/n mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? "1" : "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "1");	// devices must advertise ERP (11g) capabilities to be allowed to associate
			else
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "5"))	// g only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "2");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "1");	// devices must advertise ERP (11g) capabilities to be allowed to associate
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "6"))	// b only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "0");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "7"))	// a only
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
#ifdef RTCONFIG_BCMWL6
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "2"))	// b/g mixed
#else
		else								// b/g mixed
#endif
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
			nvram_set(strcat_r(prefix, "vreqd", tmp), "0");
			nvram_set(strcat_r(prefix, "gmode", tmp), "1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
#ifdef RTCONFIG_BCMWL6
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
		}
#ifdef RTCONFIG_BCMWL6
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "3") && 	// ac only
			 nvram_match(strcat_r(prefix, "nband", tmp2), "1"))
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "vreqd", tmp), "1");
			nvram_set(strcat_r(prefix, "gmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "3");	// devices must advertise VHT (11ac) capabilities to be allowed to associate
		}
		else if (nvram_match(strcat_r(prefix, "nmode_x", tmp), "8"))	// n/ac mixed
		{
			nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-1");	// auto rate
			nvram_set(strcat_r(prefix, "nmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "vreqd", tmp), "1");
			nvram_set(strcat_r(prefix, "gmode", tmp), "-1");
			nvram_set(strcat_r(prefix, "rate", tmp), "0");
			nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "2");	// devices must advertise HT (11n) capabilities to be allowed to associate
		}
#endif
#if 0
		if (nvram_match(strcat_r(prefix, "nband", tmp), "2") &&
			nvram_match(strcat_r(prefix, "nmode", tmp2), "-1"))
			nvram_set(strcat_r(prefix, "gmode_protection", tmp), "auto");
#endif
#ifdef RTCONFIG_BCMWL6
		if (nvram_match(strcat_r(prefix, "bw", tmp), "0"))			// Auto
		{
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))		// 2.4G
			{
				if (nvram_match(strcat_r(prefix, "nmode", tmp), "-1"))
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");// 40M
				else
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");// 20M
			}
			else
			{
#if defined(RTCONFIG_BCMARM) || defined(RTAC66U)
				if (nvram_match(strcat_r(prefix, "vreqd", tmp2), "1")
					&& nvram_match(strcat_r(prefix, "phytype", tmp), "v")
				)
					nvram_set(strcat_r(prefix, "bw_cap", tmp), hw_vht_cap() ? "7" : "3");// 80M
				else
#endif
				if (nvram_match(strcat_r(prefix, "nmode", tmp), "-1"))
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");// 40M
				else
					nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");// 20M
			}

			nvram_set_int(strcat_r(prefix, "obss_coex", tmp),
				nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? 1 : 0);
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "1") ||	// 20M
			 nvram_match(strcat_r(prefix, "nmcsidx", tmp2), "-2"))
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "2") &&	// 40M
			 nvram_match(strcat_r(prefix, "nmode", tmp2), "-1"))
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
#if defined(RTCONFIG_BCMARM) || defined(RTAC66U)
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "3") &&	// 80M
			 nvram_match(strcat_r(prefix, "vreqd", tmp2), "1"))
		{
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))	// 2.4G
				nvram_set(strcat_r(prefix, "bw_cap", tmp), "3");
			else
				nvram_set(strcat_r(prefix, "bw_cap", tmp), hw_vht_cap() ? "7" : "3");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
#endif
		else
		{
			nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");
			nvram_set_int(strcat_r(prefix, "obss_coex", tmp),
				nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? 1 : 0);
		}

#if defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);
		if (nvram_match(strcat_r(prefix, "nmode", tmp), "0"))
		{
			dbG("set vhtmode -1\n");
			nvram_set(strcat_r(prefix, "vht_features", tmp), "-1");
			nvram_set(strcat_r(prefix, "vhtmode", tmp), "-1");
		}
		else if (hw_vht_cap() &&
			nvram_match(strcat_r(prefix, "vreqd", tmp), "1") &&
			nvram_get_int(strcat_r(prefix, "turbo_qam", tmp2)))
		{
			if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))
			{
				if (nvram_match(strcat_r(prefix, "turbo_qam", tmp), "1"))
					nvram_set(strcat_r(prefix, "vht_features", tmp), "3");
				else if (nvram_match(strcat_r(prefix, "turbo_qam", tmp), "2"))
					nvram_set(strcat_r(prefix, "vht_features", tmp), "7");
			}
			else if (nvram_match(strcat_r(prefix, "nband", tmp), "1")) {
				if (nvram_match(strcat_r(prefix, "turbo_qam", tmp), "2"))
#ifndef HND_ROUTER
					nvram_set(strcat_r(prefix, "vht_features", tmp), "4");
#else
					nvram_set(strcat_r(prefix, "vht_features", tmp), "6");
#endif
				else
					nvram_set(strcat_r(prefix, "vht_features", tmp), "2");
			}
			dbG("set vhtmode 1\n");
			nvram_set(strcat_r(prefix, "vhtmode", tmp), "1");
		}
		else
		{
			dbG("set vhtmode 0\n");
			nvram_set(strcat_r(prefix, "vht_features", tmp), nvram_match(strcat_r(prefix, "nband", tmp2), "2") ? "35": "34");
			nvram_set(strcat_r(prefix, "vhtmode", tmp), "0");
		}
#endif

		if (nvram_match(strcat_r(prefix, "txbf", tmp), "1"))
		{
#ifdef RTCONFIG_MUMIMO
			if (nvram_match(strcat_r(prefix, "mumimo", tmp), "1"))
			{
				nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), "2");
				nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), "2");
			} else {
#endif
				nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), "1");
				nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), "1");
#ifdef RTCONFIG_MUMIMO
			}
#endif
		}
		else
		{
			nvram_set(strcat_r(prefix, "txbf_bfr_cap", tmp), "0");
			nvram_set(strcat_r(prefix, "txbf_bfe_cap", tmp), "0");
		}

#ifdef RTCONFIG_MUMIMO
		/* mu_feature is not enabled for client modes. */
		if (nvram_match(strcat_r(prefix, "mumimo", tmp), "1")
#if 0
			&& !is_ure(unit)
#if defined(RTCONFIG_BCMWL6) && defined(RTCONFIG_PROXYSTA)
			&& !(is_psta(unit) || is_psr(unit))
#endif
#endif
		)
			nvram_set(strcat_r(prefix, "mu_features", tmp), "0x8000");
		else
			nvram_set(strcat_r(prefix, "mu_features", tmp), "0");
#endif

#ifdef RTCONFIG_BCMARM
		nvram_set(strcat_r(prefix, "txbf_imp", tmp), nvram_safe_get(strcat_r(prefix, "itxbf", tmp2)));
#endif
#else
		if (nvram_match(strcat_r(prefix, "bw", tmp), "1"))
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}
		else if (nvram_match(strcat_r(prefix, "bw", tmp), "2"))
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "1");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "0");
		}
		else
		{
			nvram_set(strcat_r(prefix, "nbw_cap", tmp), "0");
			nvram_set(strcat_r(prefix, "obss_coex", tmp), "1");
		}

		if (unit) chanspec_fix_5g(unit);
#ifdef RTCONFIG_WL_AUTO_CHANNEL
		else if (nvram_match("AUTO_CHANNEL", "1"))
			nvram_set("wl0_channel", "6");
#endif

		match = 0;
		foreach (word, "lower upper", next)
		{
			if (nvram_match(strcat_r(prefix, "nctrlsb", tmp), word))
			{
				match = 1;
				break;
			}
		}
/*
		if ((nvram_match(strcat_r(prefix, "channel", tmp), "0")))
			nvram_unset(strcat_r(prefix, "nctrlsb", tmp));
		else
*/
		if (!match)
			nvram_set(strcat_r(prefix, "nctrlsb", tmp), "lower");
#endif

#ifdef RTCONFIG_EMF
		/* Wireless IGMP Snooping */
		i = nvram_get_int(strcat_r(prefix, "igs", tmp));
		nvram_set_int(strcat_r(prefix, "wmf_bss_enable", tmp), i ? 1 : 0);
#ifdef RTCONFIG_BCMWL6
		nvram_set_int(strcat_r(prefix, "wmf_ucigmp_query", tmp), 1);
		nvram_set_int(strcat_r(prefix, "wmf_mdata_sendup", tmp), 1);
#ifdef RTCONFIG_BCMARM
#ifndef HND_ROUTER
		nvram_set_int(strcat_r(prefix, "wmf_ucast_upnp", tmp), 1);
#endif
		nvram_set_int(strcat_r(prefix, "wmf_igmpq_filter", tmp), 1);
#endif
		nvram_set_int(strcat_r(prefix, "acs_fcs_mode", tmp), i ? 1 : 0);
		nvram_set_int(strcat_r(prefix, "dcs_csa_unicast", tmp), i ? 1 : 0);
#endif
#else // RTCONFIG_EMF
		nvram_set_int(strcat_r(prefix, "wmf_bss_enable", tmp), 0);
#ifdef RTCONFIG_BCMWL6
		nvram_set_int(strcat_r(prefix, "wmf_ucigmp_query", tmp), 0);
		nvram_set_int(strcat_r(prefix, "wmf_mdata_sendup", tmp), 0);
#ifdef RTCONFIG_BCMARM
		nvram_set_int(strcat_r(prefix, "wmf_ucast_upnp", tmp), 0);
		nvram_set_int(strcat_r(prefix, "wmf_igmpq_filter", tmp), 0);
#endif
		nvram_set_int(strcat_r(prefix, "acs_fcs_mode", tmp), 0);
		nvram_set_int(strcat_r(prefix, "dcs_csa_unicast", tmp), 0);
#endif
#endif // RTCONFIG_EMF

		sprintf(tmp2, "%d", nvram_get_int(strcat_r(prefix, "pmk_cache", tmp)) * 60);
		nvram_set(strcat_r(prefix, "net_reauth", tmp), tmp2);

		wl_dfs_support(unit);
#if 0
#if defined(RTCONFIG_BCM_7114) || defined(GTAC5300)
		wl_CE_support(unit);
#endif
#endif

#if defined(RTCONFIG_BCM7) || defined(RTCONFIG_BCM_7114) || defined(HND_ROUTER)
		if (nvram_get_int("smart_connect_x"))
			nvram_set_int(strcat_r(prefix, "probresp_sw", tmp), 1);
		else
			nvram_set_int(strcat_r(prefix, "probresp_sw", tmp), 0);
#endif

		dbG("bw: %s\n", nvram_safe_get(strcat_r(prefix, "bw", tmp)));
#ifdef RTCONFIG_BCMWL6
		dbG("chanspec: %s\n", nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));
		dbG("bw_cap: %s\n", nvram_safe_get(strcat_r(prefix, "bw_cap", tmp)));
#else
		dbG("channel: %s\n", nvram_safe_get(strcat_r(prefix, "channel", tmp)));
		dbG("nbw_cap: %s\n", nvram_safe_get(strcat_r(prefix, "nbw_cap", tmp)));
		dbG("nctrlsb: %s\n", nvram_safe_get(strcat_r(prefix, "nctrlsb", tmp)));
#endif
		dbG("obss_coex: %s\n", nvram_safe_get(strcat_r(prefix, "obss_coex", tmp)));
	}
	else
	{
		if (nvram_match(strcat_r(prefix, "bss_enabled", tmp), "1"))
		{
			nvram_set(strcat_r(prefix, "preauth", tmp), nvram_safe_get(strcat_r(prefix2, "preauth", tmp2)));
			nvram_set(strcat_r(prefix, "bss_maxassoc", tmp), nvram_safe_get(strcat_r(prefix2, "bss_maxassoc", tmp2)));
			nvram_set(strcat_r(prefix, "closed", tmp), nvram_safe_get(strcat_r(prefix2, "closed", tmp2)));
			nvram_set(strcat_r(prefix, "ap_isolate", tmp), nvram_safe_get(strcat_r(prefix2, "ap_isolate", tmp2)));

			nvram_set(strcat_r(prefix, "net_reauth", tmp), nvram_safe_get(strcat_r(prefix2, "net_reauth", tmp2)));
			nvram_set(strcat_r(prefix, "radius_ipaddr", tmp), nvram_safe_get(strcat_r(prefix2, "radius_ipaddr", tmp2)));
			nvram_set(strcat_r(prefix, "radius_key", tmp), nvram_safe_get(strcat_r(prefix2, "radius_key", tmp2)));
			nvram_set(strcat_r(prefix, "radius_port", tmp), nvram_safe_get(strcat_r(prefix2, "radius_port", tmp2)));
			nvram_set(strcat_r(prefix, "wpa_gtk_rekey", tmp), nvram_safe_get(strcat_r(prefix2, "wpa_gtk_rekey", tmp2)));
			nvram_set(strcat_r(prefix, "wme", tmp), nvram_safe_get(strcat_r(prefix2, "wme", tmp2)));
			nvram_set(strcat_r(prefix, "wme_bss_disable", tmp), nvram_safe_get(strcat_r(prefix2, "wme_bss_disable", tmp2)));

			nvram_set(strcat_r(prefix, "wmf_bss_enable", tmp), nvram_safe_get(strcat_r(prefix2, "wmf_bss_enable", tmp2)));
			nvram_set(strcat_r(prefix, "wmf_psta_disable", tmp), nvram_safe_get(strcat_r(prefix2, "wmf_psta_disable", tmp2)));
			nvram_set(strcat_r(prefix, "mcast_regen_bss_enable", tmp), nvram_safe_get(strcat_r(prefix2, "mcast_regen_bss_enable", tmp2)));

			nvram_set(strcat_r(prefix, "rxchain_pwrsave_enable", tmp), nvram_safe_get(strcat_r(prefix2, "rxchain_pwrsave_enable", tmp2)));
			nvram_set(strcat_r(prefix, "rxchain_pwrsave_quiet_time", tmp), nvram_safe_get(strcat_r(prefix2, "rxchain_pwrsave_quiet_time", tmp2)));
			nvram_set(strcat_r(prefix, "rxchain_pwrsave_pps", tmp), nvram_safe_get(strcat_r(prefix2, "rxchain_pwrsave_pps", tmp2)));
			nvram_set(strcat_r(prefix, "rxchain_pwrsave_stas_assoc_check", tmp), nvram_safe_get(strcat_r(prefix2, "rxchain_pwrsave_stas_assoc_check", tmp2)));

			nvram_set(strcat_r(prefix, "radio_pwrsave_enable", tmp), nvram_safe_get(strcat_r(prefix2, "radio_pwrsave_enable", tmp2)));
			nvram_set(strcat_r(prefix, "radio_pwrsave_quiet_time", tmp), nvram_safe_get(strcat_r(prefix2, "radio_pwrsave_quiet_time", tmp2)));
			nvram_set(strcat_r(prefix, "radio_pwrsave_pps", tmp), nvram_safe_get(strcat_r(prefix2, "radio_pwrsave_pps", tmp2)));
			nvram_set(strcat_r(prefix, "radio_pwrsave_level", tmp), nvram_safe_get(strcat_r(prefix2, "radio_pwrsave_level", tmp2)));
			nvram_set(strcat_r(prefix, "radio_pwrsave_stas_assoc_check", tmp), nvram_safe_get(strcat_r(prefix2, "radio_pwrsave_stas_assoc_check", tmp2)));

			if (!nvram_match(strcat_r(prefix, "macmode", tmp), "disabled") &&
				nvram_match(strcat_r(prefix, "mode", tmp2), "ap")) {
				nv = nvp = strdup(nvram_safe_get(strcat_r(prefix, "maclist_x", tmp)));
				list_size = sizeof(char) * (strlen(nv)+1);
				list = (char*) malloc(list_size);
				list2 = (char*) malloc(list_size);
				list[0] = 0;

				if (nv) {
					while ((b = strsep(&nvp, "<")) != NULL) {
						if (strlen(b) == 0) continue;
						if (list[0] == 0)
							sprintf(list, "%s", b);
						else {
							sprintf(list2, "%s %s", list, b);
							strlcpy(list, list2, list_size);
						}
					}
					free(nv);
				}
				nvram_set(strcat_r(prefix, "maclist", tmp), list);
				free(list);
				free(list2);
			}
			else
				nvram_set(strcat_r(prefix, "maclist", tmp), "");

#ifdef RTCONFIG_BCMARM
			if (!nvram_get_int("dwds_ctrl"))
			nvram_set(strcat_r(prefix, "dwds", tmp), is_ure(unit) ? "0" : "1");
#endif
		}
		else
		{
			nvram_set(strcat_r(prefix, "macmode", tmp), "disabled");
			nvram_set(strcat_r(prefix, "maclist", tmp), "");
		}
	}

	/* Disable nmode for WEP and TKIP for TGN spec */
	if (nvram_match(strcat_r(prefix, "wep", tmp), "enabled") ||
		(nvram_invmatch(strcat_r(prefix, "akm", tmp), "") &&
		 nvram_match(strcat_r(prefix, "crypto", tmp2), "tkip")))
	{
#ifdef RTCONFIG_BCMWL6
		if (subunit == -1)
		{
			strcpy(tmp2, nvram_safe_get(strcat_r(prefix, "chanspec", tmp)));
			if ((nvp = strchr(tmp2, '/')) || (nvp = strchr(tmp2, 'l'))
				|| (nvp = strchr(tmp2, 'u')))
			{
				*nvp = '\0';
				nvram_set(strcat_r(prefix, "chanspec", tmp), tmp2);
			}
		}
#endif
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		nvram_set(strcat_r(prefix, "nmcsidx", tmp), "-2");	// legacy rate
		nvram_set(strcat_r(prefix, "nmode", tmp), "0");
#ifndef RTCONFIG_BCMWL6
		nvram_set(strcat_r(prefix, "nreqd", tmp), "0");
#endif
		if (nvram_match(strcat_r(prefix, "nband", tmp), "2"))
		nvram_set(strcat_r(prefix, "gmode", tmp), "1");
		nvram_set(strcat_r(prefix, "bw", tmp), "1");		// reset to default setting
#ifdef RTCONFIG_BCMWL6
		nvram_set(strcat_r(prefix, "bw_cap", tmp), "1");
#else
		nvram_set(strcat_r(prefix, "nbw_cap", tmp), "0");
#endif
#ifdef RTCONFIG_BCMWL6
		nvram_set(strcat_r(prefix, "bss_opmode_cap_reqd", tmp), "0");	// no requirements on joining devices
#endif
	}
#ifdef RTCONFIG_QTN
	if (nvram_get_int("qtn_ready") == 1) {
		if (unit == 1) runtime_config_qtn(unit, subunit);
	}
#endif
}

#define BCM5325_ventry(vid, inet_vid, iptv_vid, voip_vid) ( \
	0x01000000 | (vid << 12) | (1 << ports[SWPORT_WAN]) |		\
	((vid == inet_vid) ? (0x01 << ports[SWPORT_CPU]) : 0) |		\
	((vid == iptv_vid) ? (0x41 << ports[SWPORT_LAN4]) : 0) |	\
	((vid == voip_vid) ? (0x41 << ports[SWPORT_LAN3]) : 0)		\
)

void
set_wan_tag(char *interface) {
	int model, wan_vid, iptv_vid, voip_vid, iptv_prio, voip_prio, switch_stb;
	char wan_dev[sizeof("vlan4096")], port_id[7];
	char tag_register[sizeof("0xffffffff")], vlan_entry[sizeof("0xffffffff")];
	int gmac3_enable = 0;
#ifdef HND_ROUTER
	char wan_if[10], ethPort1[10], ethPort2[10], ethPort3[10], ethPort4[10];
	char vlanDev1[10], vlanDev2[10], vlanDev3[10], vlanDev4[10];
#endif

	model = get_model();
	wan_vid = nvram_get_int("switch_wan0tagid") & 0x0fff;
	iptv_vid = nvram_get_int("switch_wan1tagid") & 0x0fff;
	voip_vid = nvram_get_int("switch_wan2tagid") & 0x0fff;
	iptv_prio = nvram_get_int("switch_wan1prio") & 0x7;
	voip_prio = nvram_get_int("switch_wan2prio") & 0x7;
#ifdef RTCONFIG_MULTICAST_IPTV
	int mang_vid = nvram_get_int("switch_wan3tagid") & 0x0fff;
	int mang_prio = nvram_get_int("switch_wan3prio") & 0x7;
#endif
#ifdef RTCONFIG_GMAC3
	gmac3_enable = nvram_get_int("gmac3_enable");
#endif
	switch_stb = nvram_get_int("switch_stb_x");

	sprintf(wan_dev, "vlan%d", wan_vid);

	switch(model) {
				/* WAN L1 L2 L3 L4 CPU */
	case MODEL_RTN53:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN12:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN12B1:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN12C1:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN12D1:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN12VP:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN12HP:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN12HP_B1:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_APN12HP:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN10P:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN10D1:	/* P4  P3 P2 P1 P0 P5 */
	case MODEL_RTN10PV2:	/* P4  P3 P2 P1 P0 P5 */
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x0080");
		/* Config WAN port */
		if (wan_vid) {
			eval("vconfig", "rem", "vlan1");
			eval("et", "robowr", "0x34", "0x8", "0x01001000");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
		}
		/* Set Wan prio*/
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_wantag", "unifi_home")) {
			/* vlan0ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010003ae");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
			/* vlan600ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01258051");
			eval("et", "robowr", "0x34", "0x6", "0x3258");
			/* LAN4 vlan tag */
			eval("et", "robowr", "0x34", "0x10", "0x0258");
		} else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		} else if (nvram_match("switch_wantag", "singtel_mio")) {
			/* vlan0ports= 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100032c");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014051");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* vlan30ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0101e012");	/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x10", "0x8014");
		} else if (nvram_match("switch_wantag", "singtel_others")) {
			/* vlan0ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010003ae");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014051");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x10", "0x8014");
		} else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan0ports= 0 2 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x0100036d");	/*0011|0110|1101*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan1103ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f030");	/*0000|0011|0000*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01453012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		} else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan0 ports= 0 2 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x0100036d");	/*0011|0110|1101*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan621 ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d030");	/*0000|0011|0000*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01335012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		} else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan0ports= 0 2 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x0100036d");	/*0011|0110|1101*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan11ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b030");	/*0000|0011|0000*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 1 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e012");	/*0000|0001|0010*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
		} else {	/* manual */
							/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 3, 2, 1, 0, 5 };

			if (switch_stb != SWCFG_STB4 && switch_stb != SWCFG_STB34)
				iptv_vid = 0;
			if (switch_stb != SWCFG_STB3 && switch_stb != SWCFG_STB34)
				voip_vid = 0;
			if (wan_vid) {
				sprintf(vlan_entry, "0x%x", BCM5325_ventry(wan_vid, wan_vid, iptv_vid, voip_vid));
				eval("et", "robowr", "0x34", "0x8", vlan_entry);
				eval("et", "robowr", "0x34", "0x6", "0x3001");
			}
			if (iptv_vid) {
				if (iptv_vid != wan_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(iptv_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry);
					eval("et", "robowr", "0x34", "0x6", "0x3002");
				}
				sprintf(tag_register, "0x%x", (iptv_prio << 13) | iptv_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN4]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
			if (voip_vid) {
				if (voip_vid != wan_vid && voip_vid != iptv_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(voip_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry);
					eval("et", "robowr", "0x34", "0x6", "0x3003");
				}
				sprintf(tag_register, "0x%x", (voip_prio << 13) | voip_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN3]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
		}
		break;

				/* WAN L1 L2 L3 L4 CPU */
	case MODEL_RTN14UHP:	/* P4  P0 P1 P2 P3 P5 */
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x0080");
		/* Config WAN port */
		if (wan_vid) {
			eval("vconfig", "rem", "vlan1");
			eval("et", "robowr", "0x34", "0x8", "0x01001000");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
		}
		/* Set Wan prio*/
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_wantag", "unifi_home")) {
			/* vlan0ports= 0 1 2 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010001e7");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
			/* vlan600ports= 3 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01258218");
			eval("et", "robowr", "0x34", "0x6", "0x3258");
			/* LAN4 vlan tag */
			eval("et", "robowr", "0x34", "0x16", "0x0258");
		} else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4030");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		} else if (nvram_match("switch_wantag", "singtel_mio")) {
			/* vlan0ports= 0 1 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010000E3");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 3 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014218");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* vlan30ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0101e014");	/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x16", "0x8014");
		} else if (nvram_match("switch_wantag", "singtel_others")) {
			/* vlan0ports= 0 1 2 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010001e7");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a030");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 3 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014218");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x16", "0x8014");
		} else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan0ports= 0 1 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010002eb");	/*0010|1110|1011*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan1103ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f030");	/*0000|0011|0000*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01453014");	/*0000|0001|0100*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		} else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan0 ports= 0 1 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010002eb");	/*0010|1110|1011*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan621 ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d030");	/*0000|0011|0000*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01335014");	/*0000|0001|0100*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336014");	/*0000|0001|0100*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		} else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan0ports= 0 1 3 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010002eb");	/*0010|1110|1011*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan11ports= 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b030");	/*0000|0011|0000*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 2 4 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e014");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
		} else if (nvram_match("switch_wantag", "meo")) {
			/* vlan0ports= 0 1 2 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010001e7");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan12ports= 3 4 5 */				/* untag||forward */
			eval("et", "robowr", "0x34", "0x8", "0x0100c038");	/*0000|0011|1000*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300c");
		} else if (nvram_match("switch_wantag", "vodafone")) {
			/* vlan0ports= 0 1 2 5t */
			eval("et", "robowr", "0x34", "0x8", "0x010000E3");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan100ports= 3t 4t 5t */
			eval("et", "robowr", "0x34", "0x8", "0x01064038");
			eval("et", "robowr", "0x34", "0x6", "0x3064");
			/* vlan101ports= 3t 4t */
			eval("et", "robowr", "0x34", "0x8", "0x01065018");
			eval("et", "robowr", "0x34", "0x6", "0x3065");
			/* vlan105ports= 2 3t 4t */
			eval("et", "robowr", "0x34", "0x8", "0x0106961C");
			eval("et", "robowr", "0x34", "0x6", "0x3069");

			/* WAN port: tag=100 & prio=1 */
			eval("et", "robowr", "0x34", "0x10", "0x2064");
			/* LAN4: tag=105 & prio=1 */
			eval("et", "robowr", "0x34", "0x16", "0x2069");
		}

		else {	/* manual */
							/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 4, 0, 1, 2, 3, 5 };

			if (switch_stb != SWCFG_STB4 && switch_stb != SWCFG_STB34)
				iptv_vid = 0;
			if (switch_stb != SWCFG_STB3 && switch_stb != SWCFG_STB34)
				voip_vid = 0;
			if (wan_vid) {
				sprintf(vlan_entry, "0x%x", BCM5325_ventry(wan_vid, wan_vid, iptv_vid, voip_vid));
				eval("et", "robowr", "0x34", "0x8", vlan_entry);
				eval("et", "robowr", "0x34", "0x6", "0x3001");
			}
			if (iptv_vid) {
				if (iptv_vid != wan_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(iptv_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry);
					eval("et", "robowr", "0x34", "0x6", "0x3002");
				}
				sprintf(tag_register, "0x%x", (iptv_prio << 13) | iptv_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN4]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
			if (voip_vid) {
				if (voip_vid != wan_vid && voip_vid != iptv_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(voip_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry);
					eval("et", "robowr", "0x34", "0x6", "0x3003");
				}
				sprintf(tag_register, "0x%x", (voip_prio << 13) | voip_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN3]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
		}
		break;

				/* WAN L1 L2 L3 L4 CPU */
	case MODEL_RTN10U:	/* P0  P4 P3 P2 P1 P5 */
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x0080");
		/* Config WAN port */
		if (wan_vid) {
			eval("vconfig", "rem", "vlan1");
			eval("et", "robowr", "0x34", "0x8", "0x01001000");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
		}
		/* Set Wan prio*/
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_wantag", "unifi_home")) {
			/* vlan0ports= 2 3 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100073c");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan500ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4021");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
			/* vlan600ports= 0 1 */
			eval("et", "robowr", "0x34", "0x8", "0x01258083");
			eval("et", "robowr", "0x34", "0x6", "0x3258");
			/* LAN4 vlan tag */
			eval("et", "robowr", "0x34", "0x12", "0x0258");
		} else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4021");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		} else if (nvram_match("switch_wantag", "singtel_mio")) {
			/* vlan0ports= 3 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x01000638");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a021");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 1 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01014083");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* vlan30ports= 2 0 */
			eval("et", "robowr", "0x34", "0x8", "0x0101e005");	/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x12", "0x8014");
		} else if (nvram_match("switch_wantag", "singtel_others")) {
			/* vlan0ports= 2 3 4 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100073c");
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan10ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a021");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 0 1 */
			eval("et", "robowr", "0x34", "0x8", "0x01014083");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x12", "0x8014");
		} else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan0ports= 1 3 4 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010006ba");	/*0110|1011|1010*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan1103ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f021");	/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 2 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01453005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		} else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan0 ports= 1 3 4 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010006ba");	/*0110|1011|1010*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan621 ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d021");	/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 2 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01335005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		} else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan0ports= 1 3 4 5 */				/*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010006ba");	/*0110|1011|1010*/
			eval("et", "robowr", "0x34", "0x6", "0x3000");
			/* vlan11ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b021");	/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 2 0 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e005");	/*0000|0000|0101*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
		} else {	/* manual */
							/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 4, 3, 2, 1, 5 };

			if (switch_stb != SWCFG_STB4 && switch_stb != SWCFG_STB34)
				iptv_vid = 0;
			if (switch_stb != SWCFG_STB3 && switch_stb != SWCFG_STB34)
				voip_vid = 0;
			if (wan_vid) {
				sprintf(vlan_entry, "0x%x", BCM5325_ventry(wan_vid, wan_vid, iptv_vid, voip_vid));
				eval("et", "robowr", "0x34", "0x8", vlan_entry);
				eval("et", "robowr", "0x34", "0x6", "0x3001");
			}
			if (iptv_vid) {
				if (iptv_vid != wan_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(iptv_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry);
					eval("et", "robowr", "0x34", "0x6", "0x3002");
				}
				sprintf(tag_register, "0x%x", (iptv_prio << 13) | iptv_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN4]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
			if (voip_vid) {
				if (voip_vid != wan_vid && voip_vid != iptv_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(voip_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry);
					eval("et", "robowr", "0x34", "0x6", "0x3003");
				}
				sprintf(tag_register, "0x%x", (voip_prio << 13) | voip_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN3]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
		}
		break;

	case MODEL_RTN16:
		// config wan port
		if (wan_vid) {
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0101");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		// Set Wan port PRIO
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {
			// config LAN 3 = VoIP
			if (nvram_match("switch_wantag", "m1_fiber")) {
				// Just forward packets between port 0 & 3, without untag
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	// Nomo case, untag it.
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				// Set vlan table entry register
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) {
			// config LAN 4 = IPTV
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x12", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			// Set vlan table entry register
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x0403");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		} else if (nvram_match("switch_stb_x", "6")) {
			// config LAN 3 = VoIP
			if (nvram_match("switch_wantag", "singtel_mio")) {
				// Just forward packets between port 0 & 3, without untag
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	// Nomo case, untag it.
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "robowr", "0x34", "0x14", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					// Set vlan table entry register
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "robowr", "0x05", "0x83", "0x0C07");
					else
						eval("et", "robowr", "0x05", "0x83", "0x0805");
					eval("et", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			// config LAN 4 = IPTV
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				// Set vlan table entry register
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0C07");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0403");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;

				/* P0  P1 P2 P3 P4 P5 */
	case MODEL_RTAC3200: 	/* WAN L4 L3 L2 L1 CPU */
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0021");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) { // P3
			if (nvram_match("switch_wantag", "vodafone")) { //Config by robocfg
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);

				char vlan_cmd[64];
				sprintf(vlan_cmd, "robocfg vlan 1 ports \"2 3 4 5t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 2 ports \"0 5\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 1t 5t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 1t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 1t 2\"");
				system(vlan_cmd);
				break;
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", "0x0335"); /* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", "0x0336"); /* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) { // P4
			/* config LAN 4 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward packets between port 0 & 1, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("* vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0023");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				/* config LAN 4 = IPTV */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0403");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */ // P3
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "robowr", "0x34", "0x14", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* Set vlan table entry register */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "robowr", "0x05", "0x83", "0x0C07");
					else
						eval("et", "robowr", "0x05", "0x83", "0x0805");
					eval("et", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			/* config LAN 4 = IPTV */ // P4
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0C07");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0403");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) {
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config IPTV on wan port ***\n");
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", iptv_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0021");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (iptv_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
		if (switch_stb >= 8) {
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config VOIP on wan port ***\n");
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", voip_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0021");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (voip_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)voip_prio);
				}
			}
		}
		if (switch_stb >=9) {
			if (mang_vid) { /* config tr069 on wan port */
_dprintf("*** Multicast IPTV: config Singtel TR069 on wan port ***\n");
				sprintf(wan_dev, "vlan%d", mang_vid);
				nvram_set("wan12_ifname", wan_dev);
				sprintf(port_id, "%d", mang_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", mang_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0021");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (mang_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
#endif
		break;

				/* P0  P1 P2 P3 P4 P5 */
	case MODEL_RTAC68U:	/* WAN L1 L2 L3 L4 CPU */
	case MODEL_RTN18U:	/* WAN L1 L2 L3 L4 CPU */
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0021");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {	// P3
			if (nvram_match("switch_wantag", "vodafone")) { //Config by robocfg
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);

				char vlan_cmd[64];
				sprintf(vlan_cmd, "robocfg vlan 1 ports \"1 2 3 5t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 2 ports \"0 5\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 4t 5t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 4t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 3 4t\"");
				system(vlan_cmd);
				break;
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x1009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) { // P4
			/* config LAN 4 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward packets between port 0 & 4, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0031");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x18", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */ // P3
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "robowr", "0x34", "0x16", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* Set vlan table entry register */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "robowr", "0x05", "0x83", "0x3019");
					else
						eval("et", "robowr", "0x05", "0x83", "0x1009");
					eval("et", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			/* config LAN 4 = IPTV */ // P4
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x18", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x3019");
				else
					eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) {
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config IPTV on wan port ***\n");
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", iptv_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0021");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (iptv_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
		if (switch_stb >= 8) {
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config VOIP on wan port ***\n");
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", voip_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0021");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (voip_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)voip_prio);
				}
			}
		}
		if (switch_stb >=9) {
			if (mang_vid) { /* config tr069 on wan port */
_dprintf("*** Multicast IPTV: config Singtel TR069 on wan port ***\n");
				sprintf(wan_dev, "vlan%d", mang_vid);
				nvram_set("wan12_ifname", wan_dev);
				sprintf(port_id, "%d", mang_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", mang_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0021");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (mang_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
#endif
		break;

#ifdef HND_ROUTER
	case MODEL_GTAC5300:
				/*      P1   P0   x3 x2 P3   P2   x1 x0	[P4(P0) P5(P1&P2) P8(P3)] */
				/* eth0 eth2 eth1 eth5  eth4 eth3 eth5				  */
				/* WAN  L1   L2   L3 L4 L5   L6   L7 L8 CPU			  */
	case MODEL_RTAC86U:
				/*      P3   P2   P1   P0   [P4(P0) P5(P1&P2) P8(P3)]		  */
				/* eth0 eth4 eth3 eth2 eth1					  */
				/* WAN  L1   L2   L3   L4   CPU					  */
		if (nvram_match("iptv_port_settings", "12")) {
			sprintf(ethPort1, "eth2");
			sprintf(ethPort2, "eth1");
			sprintf(ethPort3, "eth2");
			sprintf(ethPort4, "eth1");
			sprintf(vlanDev1, "eth2.v0");
			sprintf(vlanDev2, "eth1.v0");
			sprintf(vlanDev3, "eth2.v0");
			sprintf(vlanDev4, "eth1.v0");
		} else {
			sprintf(ethPort1, "eth4");
			sprintf(ethPort2, "eth3");
			sprintf(ethPort3, "eth4");
			sprintf(ethPort4, "eth3");
			sprintf(vlanDev1, "eth4.v0");
			sprintf(vlanDev2, "eth3.v0");
			sprintf(vlanDev3, "eth4.v0");
			sprintf(vlanDev4, "eth3.v0");
		}

		/* RT-AC86U revert to 4 ports base IPTV profile */
		if (model == MODEL_RTAC86U) {
			sprintf(ethPort1, "eth1");
			sprintf(ethPort2, "eth2");
			sprintf(ethPort3, "eth3");
			sprintf(ethPort4, "eth4");
			sprintf(vlanDev1, "eth1.v0");
			sprintf(vlanDev2, "eth2.v0");
			sprintf(vlanDev3, "eth3.v0");
			sprintf(vlanDev4, "eth4.v0");
		}

		/* Using vlanctl to handle vlan forwarding */
		if (wan_vid || switch_stb > 0 || nvram_match("switch_wantag", "unifi_biz")) { /* config wan port or bridge hinet IPTV traffic */
#if 0
			/* Handle no vid traffic */
#endif
			sprintf(port_id, "%d", wan_vid);
			/* handle vlan + pppoe w/o bridge case. Using vconfig instead */
			if (nvram_match("switch_wantag", "movistar") || nvram_match("switch_wantag", "unifi_biz") || (nvram_match("switch_wantag", "none") && switch_stb == 0 && !wan_vid)) {
				eval("vconfig", "add", "eth0", port_id);
				sprintf(wan_dev, "vlan%d", wan_vid);
				eval("ifconfig", wan_dev, "allmulti", "up");
				set_wan_phy("");
				sprintf(wan_if, wan_dev);
				add_wan_phy(wan_if);
				nvram_set("wan0_ifname", wan_dev);
				nvram_set("wan0_gw_ifname", wan_dev);
			}
			else {
				/* Handle wan(internet) vlan traffic */
				eval("vlanctl", "--mcast", "--if-create", "eth0", "0");
				if ((!wan_vid || nvram_match("switch_wantag", "none")) && switch_stb > 0) { /* bridge IPTV traffic */
					/* set wan vlan interface */
					eval("vlanctl", "--if", "eth0", "--rx", "--tags", "0", "--set-rxif", "eth0.v0", "--rule-append");
					eval("vlanctl", "--if", "eth0", "--tx", "--tags", "0", "--filter-txif", "eth0.v0", "--rule-append");
				} else {
					/* pop tag for entering CPU */
					eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", port_id, "0", "--pop-tag", "--set-rxif", "eth0.v0", "--rule-append");
					/* tx push tag */
					/* Set Wan PRIO */
					if (nvram_invmatch("switch_wan0prio", "0")) {
						eval("vlanctl", "--if", "eth0", "--tx", "--tags", "0", "--filter-txif", "eth0.v0", "--push-tag", "--set-vid", port_id, "0", "--set-pbits", nvram_get("switch_wan0prio"), "0", "--rule-append");
					}
					else {
						eval("vlanctl", "--if", "eth0", "--tx", "--tags", "0", "--filter-txif", "eth0.v0", "--push-tag", "--set-vid", port_id, "0", "--rule-append");
					}
				}
				eval("ifconfig", "eth0.v0", "allmulti", "up");

				/*
					multicast iptv no need to forward vlan packets from wan to lan
				*/
				if (switch_stb <= 6) {
					/* Add wan bridge */
					eval("brctl", "addbr", "br1");
					eval("brctl", "stp", "br1", "on");
					eval("ifconfig", "br1", "up");
					eval("brctl", "addif", "br1", "eth0.v0");
					set_wan_phy("");
					sprintf(wan_if, "br1");
					add_wan_phy(wan_if);
					nvram_set("wan0_ifname", "br1");
					nvram_set("wan0_gw_ifname", "br1");
				} else {
					set_wan_phy("");
					sprintf(wan_if, "eth0.v0");
					add_wan_phy(wan_if);
					nvram_set("wan0_ifname", "eth0.v0");
					nvram_set("wan0_gw_ifname", "eth0.v0");
				}
			}
		}

		if (nvram_match("switch_stb_x", "1") && nvram_match("switch_wantag", "none")) {
			/* Just forward packets between wan & vlanDev1, no tag */
			eval("brctl", "delif", "br0", ethPort4);
			eval("vlanctl", "--mcast", "--if-create", ethPort4, "0");
			/* pop tag (lan bridge) */
			eval("vlanctl", "--if", ethPort4, "--rx", "--tags", "0", "--set-rxif", vlanDev4, "--rule-append");
			eval("vlanctl", "--if", ethPort4, "--tx", "--tags", "0", "--filter-txif", vlanDev4, "--rule-append");
			eval("ifconfig", vlanDev4, "allmulti", "up");
			eval("brctl", "addif", "br1", vlanDev4);
		} else if (nvram_match("switch_stb_x", "2") && nvram_match("switch_wantag", "none")) {
			/* Just forward packets between wan & vlanDev1, no tag */
			eval("brctl", "delif", "br0", ethPort3);
			eval("vlanctl", "--mcast", "--if-create", ethPort3, "0");
			/* pop tag (lan bridge) */
			eval("vlanctl", "--if", ethPort3, "--rx", "--tags", "0", "--set-rxif", vlanDev3, "--rule-append");
			eval("vlanctl", "--if", ethPort3, "--tx", "--tags", "0", "--filter-txif", vlanDev3, "--rule-append");
			eval("ifconfig", vlanDev3, "allmulti", "up");
			eval("brctl", "addif", "br1", vlanDev3);
		} else if (nvram_match("switch_stb_x", "3")) {  // w:eth0
			if (nvram_match("switch_wantag", "vodafone")) {
				/* Forward packets from wan:eth0 to vlanDev1 (leave tag) */
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", "101", "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("brctl", "delif", "br0", ethPort1);
				eval("vlanctl", "--mcast", "--if-create", ethPort1, "0");
				/* ---------- wan bridge lan leave tag case? ---------- */
				/* rx pop tag */
				eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "1", "--filter-vid", port_id, "0", "--pop-tag", "--set-rxif", vlanDev1, "--rule-append");
				/* tx push tag */
				/* Set Wan PRIO */
				if (nvram_invmatch("switch_wan0prio", "0"))
					eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "0", "--filter-txif", vlanDev1, "--push-tag", "--set-vid", port_id, "0", "--set-pbits", nvram_get("switch_wan0prio"), "0", "--rule-append");
				else
					eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "0", "--filter-txif", vlanDev1, "--push-tag", "--set-vid", port_id, "0", "--rule-append");
				/* bridge */
				eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "1", "--filter-vid", "101", "0", "--set-rxif", vlanDev1, "--rule-append");
				eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "1", "--filter-vid", "105", "0", "--set-rxif", vlanDev1, "--rule-append");
				eval("ifconfig", vlanDev1, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev1);
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between WAN & vlanDev2, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("brctl", "delif", "br0", ethPort2);
				eval("vlanctl", "--mcast", "--if-create", ethPort2, "0");
				eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", vlanDev2, "--rule-append");
				eval("ifconfig", vlanDev2, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev2);
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between WAN & vlanDev2, without untag */
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", "0x0335", "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", "0x0336", "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("brctl", "delif", "br0", ethPort2);
				eval("vlanctl", "--mcast", "--if-create", ethPort2, "0");
				eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "1", "--filter-vid", "0x0335", "0", "--set-rxif", vlanDev2, "--rule-append");
				eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "1", "--filter-vid", "0x0336", "0", "--set-rxif", vlanDev2, "--rule-append");
				eval("ifconfig", vlanDev2, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev2);
			}
			else if (nvram_match("switch_wantag", "none")) {
				if (model == MODEL_GTAC5300) {
					if (nvram_match("iptv_port_settings", "12")) {
						sprintf(ethPort2, "eth2");
						sprintf(vlanDev2, "eth2.v0");
					} else {
						sprintf(ethPort2, "eth4");
						sprintf(vlanDev2, "eth4.v0");
					}
				}
				/* Just forward packets between wan & vlanDev1, no tag */
				eval("brctl", "delif", "br0", ethPort2);
				eval("vlanctl", "--mcast", "--if-create", ethPort2, "0");
				/* pop tag (lan bridge) */
				eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "0", "--set-rxif", vlanDev2, "--rule-append");
				eval("vlanctl", "--if", ethPort2, "--tx", "--tags", "0", "--filter-txif", vlanDev2, "--rule-append");
				eval("ifconfig", vlanDev2, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev2);
			}
			else {  /* Nomo case. */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				/* Forward packets from wan:eth0 to vlanDev2 (untag) */
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("brctl", "delif", "br0", ethPort2);
				eval("vlanctl", "--mcast", "--if-create", ethPort2, "0");
				/* rx push tag */
				/* Set VOIP PRIO */
				if (nvram_invmatch("switch_wan2prio", "0"))
					eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-pbits", nvram_get("switch_wan2prio"), "0", "--set-rxif", vlanDev2, "--rule-append");
				else
					eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-rxif", vlanDev2, "--rule-append");
				/* tx pop tag */
				eval("vlanctl", "--if", ethPort2, "--tx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--filter-txif", vlanDev2, "--pop-tag", "--rule-append");
				eval("ifconfig", vlanDev2, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev2);
			}
		} else if (nvram_match("switch_stb_x", "4")) {
			/* config ethPort1 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward wan vid packets between wan & vlanDev1, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("brctl", "delif", "br0", ethPort1);
				eval("vlanctl", "--mcast", "--if-create", ethPort1, "0");
				/* pop tag (lan bridge) */
				eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--pop-tag", "--set-rxif", vlanDev1, "--rule-append");
				/* tx push tag */
				eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "0", "--filter-txif", vlanDev1, "--push-tag", "--set-vid", vlan_entry, "0", "--rule-append");
				eval("ifconfig", vlanDev1, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev1);
			}
			else if (nvram_match("switch_wantag", "hinet")) {
				/* Just forward packets between wan & vlanDev1, no tag */
				eval("brctl", "delif", "br0", ethPort1);
				eval("vlanctl", "--mcast", "--if-create", ethPort1, "0");
				/* no tag (lan bridge) */
				eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "0", "--set-rxif", vlanDev1, "--rule-append");
				eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "0", "--filter-txif", vlanDev1, "--rule-append");
				eval("ifconfig", vlanDev1, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev1);
			}
			else if (nvram_match("switch_wantag", "none")) {
				if (model == MODEL_GTAC5300) {
					if (nvram_match("iptv_port_settings", "12")) {
						sprintf(ethPort1, "eth1");
						sprintf(vlanDev1, "eth1.v0");
					} else {
						sprintf(ethPort1, "eth3");
						sprintf(vlanDev1, "eth3.v0");
					}
				}
				/* Just forward packets between wan & vlanDev1, no tag */
				eval("brctl", "delif", "br0", ethPort1);
				eval("vlanctl", "--mcast", "--if-create", ethPort1, "0");
				/* no tag (lan bridge) */
				eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "0", "--set-rxif", vlanDev1, "--rule-append");
				eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "0", "--filter-txif", vlanDev1, "--rule-append");
				eval("ifconfig", vlanDev1, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev1);
			}
			else {  /* Nomo case, untag it. */
				/* config ethPort1 = IPTV */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				/* Forward packets from wan:eth0 to vlanDev1 (untag) */
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("brctl", "delif", "br0", ethPort1);
				eval("vlanctl", "--mcast", "--if-create", ethPort1, "0");
				/* rx push tag */
				/* Set IPTV PRIO */
				if (nvram_invmatch("switch_wan1prio", "0"))
					eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-pbits", nvram_get("switch_wan1prio"), "0", "--set-rxif", vlanDev1, "--rule-append");
				else
					eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-rxif", vlanDev1, "--rule-append");
				/* tx pop tag */
				eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--filter-txif", vlanDev1, "--pop-tag", "--rule-append");
				eval("ifconfig", vlanDev1, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev1);
			}
		} else if (nvram_match("switch_stb_x", "5") && nvram_match("switch_wantag", "none")) {
			/* Just forward packets between wan & vlanDev1, no tag */
			eval("brctl", "delif", "br0", ethPort1);
			eval("brctl", "delif", "br0", ethPort2);
			eval("vlanctl", "--mcast", "--if-create", ethPort1, "0");
			eval("vlanctl", "--mcast", "--if-create", ethPort2, "0");
			/* pop tag (lan bridge) */
			eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "0", "--set-rxif", vlanDev1, "--rule-append");
			eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "0", "--filter-txif", vlanDev1, "--rule-append");
			eval("ifconfig", vlanDev1, "allmulti", "up");
			eval("brctl", "addif", "br1", vlanDev1);
			eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "0", "--set-rxif", vlanDev2, "--rule-append");
			eval("vlanctl", "--if", ethPort2, "--tx", "--tags", "0", "--filter-txif", vlanDev2, "--rule-append");
			eval("ifconfig", vlanDev2, "allmulti", "up");
			eval("brctl", "addif", "br1", vlanDev2);
		} else if (nvram_match("switch_stb_x", "6")) {
			/* config ethPort2 = VoIP */
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between WAN & vlanDev2, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("brctl", "delif", "br0", ethPort2);
				eval("vlanctl", "--mcast", "--if-create", ethPort2, "0");
				eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", vlanDev2, "--rule-append");
				eval("ifconfig", vlanDev2, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev2);
			}
			else if (nvram_match("switch_wantag", "none")) {
				/* Just forward packets between wan & vlanDev1, no tag */
				eval("brctl", "delif", "br0", ethPort3);
				eval("brctl", "delif", "br0", ethPort4);
				eval("vlanctl", "--mcast", "--if-create", ethPort3, "0");
				eval("vlanctl", "--mcast", "--if-create", ethPort4, "0");
				/* pop tag (lan bridge) */
				eval("vlanctl", "--if", ethPort3, "--rx", "--tags", "0", "--set-rxif", vlanDev3, "--rule-append");
				eval("vlanctl", "--if", ethPort3, "--tx", "--tags", "0", "--filter-txif", vlanDev3, "--rule-append");
				eval("ifconfig", vlanDev3, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev3);
				eval("vlanctl", "--if", ethPort4, "--rx", "--tags", "0", "--set-rxif", vlanDev4, "--rule-append");
				eval("vlanctl", "--if", ethPort4, "--tx", "--tags", "0", "--filter-txif", vlanDev4, "--rule-append");
				eval("ifconfig", vlanDev4, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev4);
			}
			else {
				if (voip_vid) {
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					/* Forward packets from wan:eth0 to vlanDev2 (untag) */
					eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", "eth0.v0", "--rule-append");
					eval("brctl", "delif", "br0", ethPort2);
					eval("vlanctl", "--mcast", "--if-create", ethPort2, "0");
					/* rx push tag */
					/* Set VOIP PRIO */
					if (nvram_invmatch("switch_wan2prio", "0"))
						eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-pbits", nvram_get("switch_wan2prio"), "0", "--set-rxif", vlanDev2, "--rule-append");
					else
						eval("vlanctl", "--if", ethPort2, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-rxif", vlanDev2, "--rule-append");
					/* tx pop tag */
					eval("vlanctl", "--if", ethPort2, "--tx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--filter-txif", vlanDev2, "--pop-tag", "--rule-append");
					eval("ifconfig", vlanDev2, "allmulti", "up");
					eval("brctl", "addif", "br1", vlanDev2);
				}
			}
			/* config ethPort1 = IPTV */
			if (iptv_vid) {
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				/* Forward packets from wan:eth0 to vlanDev1 (untag) */
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--set-rxif", "eth0.v0", "--rule-append");
				eval("brctl", "delif", "br0", ethPort1);
				eval("vlanctl", "--mcast", "--if-create", ethPort1, "0");
				/* rx push tag */
				/* Set IPTV PRIO */
				if (nvram_invmatch("switch_wan1prio", "0"))
					eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-pbits", nvram_get("switch_wan1prio"), "0", "--set-rxif", vlanDev1, "--rule-append");
				else
					eval("vlanctl", "--if", ethPort1, "--rx", "--tags", "0", "--push-tag", "--set-vid", vlan_entry, "0", "--set-rxif", vlanDev1, "--rule-append");
				/* tx pop tag */
				eval("vlanctl", "--if", ethPort1, "--tx", "--tags", "1", "--filter-vid", vlan_entry, "0", "--filter-txif", vlanDev1, "--pop-tag", "--rule-append");
				eval("ifconfig", vlanDev1, "allmulti", "up");
				eval("brctl", "addif", "br1", vlanDev1);
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) {
			/* handle vlan + pppoe w/o bridge case. Using vconfig instead */
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config IPTV on wan port ***\n");
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", "eth0", port_id);
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				if (nvram_invmatch("switch_wan1prio", "0"))
					eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan1prio"));
				eval("ifconfig", wan_dev, "allmulti", "up");
#if 0
				nvram_set("wan10_ifname", "eth0.v1");
				/* Handle wan(IPTV) vlan traffic */
				sprintf(port_id, "%d", iptv_vid);
				eval("vlanctl", "--mcast", "--if-create", "eth0", "1");
				/* pop tag for entering CPU */
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", port_id, "0", "--pop-tag", "--set-rxif", "eth0.v1", "--rule-append");
				/* tx push tag */
				/* Set Wan PRIO */
				if (nvram_invmatch("switch_wan1prio", "0"))
					eval("vlanctl", "--if", "eth0", "--tx", "--tags", "0", "--filter-txif", "eth0.v1", "--push-tag", "--set-vid", port_id, "0", "--set-pbits", nvram_get("switch_wan1prio"), "0", "--rule-append");
				else
					eval("vlanctl", "--if", "eth0", "--tx", "--tags", "0", "--filter-txif", "eth0.v1", "--push-tag", "--set-vid", port_id, "0", "--rule-append");
				eval("ifconfig", "eth0.v1", "allmulti", "up");
#endif
			}
		}
		if (switch_stb >= 8) {
			/* handle vlan + pppoe w/o bridge case. Using vconfig instead */
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config VOIP on wan port ***\n");
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", "eth0", port_id);
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				if (nvram_invmatch("switch_wan2prio", "0"))
					eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan2prio"));
				eval("ifconfig", wan_dev, "allmulti", "up");
#if 0
				nvram_set("wan11_ifname", "eth0.v2");
				/* Handle wan(IPTV) vlan traffic */
				sprintf(port_id, "%d", voip_vid);
				eval("vlanctl", "--mcast", "--if-create", "eth0", "2");
				/* pop tag for entering CPU */
				eval("vlanctl", "--if", "eth0", "--rx", "--tags", "1", "--filter-vid", port_id, "0", "--pop-tag", "--set-rxif", "eth0.v2", "--rule-append");
				/* tx push tag */
				/* Set Wan PRIO */
				if (nvram_invmatch("switch_wan2prio", "0"))
					eval("vlanctl", "--if", "eth0", "--tx", "--tags", "0", "--filter-txif", "eth0.v2", "--push-tag", "--set-vid", port_id, "0", "--set-pbits", nvram_get("switch_wan2prio"), "0", "--rule-append");
				else
					eval("vlanctl", "--if", "eth0", "--tx", "--tags", "0", "--filter-txif", "eth0.v2", "--push-tag", "--set-vid", port_id, "0", "--rule-append");
				eval("ifconfig", "eth0.v2", "allmulti", "up");
#endif
			}
		}
#if 0
		if (switch_stb >=9) {
		}
#endif
#endif
		break;
#endif

	case MODEL_RTAC5300:
				/* If enable gmac3, CPU port is 8 */
#ifdef RTCONFIG_RGMII_BRCM5301X
				/* P0  P1 P2 P3 P4 P5 		P7 */
				/* WAN L1 L2 L3 L4 L5 L6 L7 L8 	CPU*/
#else
				/* P0  P1 P2 P3 P4 P5 */
				/* WAN L1 L2 L3 L4 CPU*/
#endif
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);

			if (gmac3_enable) {
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0101");	/* 8, 0 */
			} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0081");	/* 7, 0 */
#else
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0021");	/* 5, 0 */
#endif
			}
			eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {  // L3:p3, w:p0, c:p7/p5
			if (nvram_match("switch_wantag", "vodafone")) { //Config by robocfg
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);

				char vlan_cmd[64];
				if (gmac3_enable) {
					sprintf(vlan_cmd, "robocfg vlan 1 ports \"1 2 3 5u 7 8t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 4t 8t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 4t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 3 4t\"");
					system(vlan_cmd);
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					sprintf(vlan_cmd, "robocfg vlan 1 ports \"1 2 3 7t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 4t 7t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 4t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 3 4t\"");
					system(vlan_cmd);
#else
					sprintf(vlan_cmd, "robocfg vlan 1 ports \"1 2 3 5t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 4t 5t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 4t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 3 4t\"");
					system(vlan_cmd);
#endif
				}
				break;
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between WAN & L3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between WAN & L3, without untag */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", "0x0335"); /* vlan id=821 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", "0x0336"); /* vlan id=822 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case. */
				voip_prio <<= 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "-i", "eth0", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan(3) tag register: %s\n", tag_register);
				/* untag port map */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x1009");	/* un:p3, f:p0 3*/
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) { // lan(4) = P4
			/* config LAN 4 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward packets between wan & L4, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);

				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x00111"); /* fwd: 0 4 8 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0091"); /* fwd: 0 4 7 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0031"); /* fwd: 0 4 5 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				/* config LAN 4 = IPTV */
				iptv_prio <<= 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "-i", "eth0", "robowr", "0x34", "0x18", tag_register);
				_dprintf("lan(4) tag register: %s\n", tag_register);
				/* untag port map */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "6")) {	// lan(3)=P3, lan(4)=P4
			/* config lan(3)/P3 = VoIP */
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between WAN & lan(3), without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0009");		/* f:30 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else {
				if (voip_vid) {
					voip_prio <<= 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "-i", "eth0", "robowr", "0x34", "0x16", tag_register);	/* p3 */
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* untag port map */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x3019"); /* un:43, f:430*/
					else
						eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x1009"); /* un:3 , f:30 */
					eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			/* config lan(4)/P4 = IPTV */
			if (iptv_vid) {
				iptv_prio <<= 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "-i", "eth0", "robowr", "0x34", "0x18", tag_register);	/* p4 */
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* untag port map */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x3019"); /* un:43, f:430*/
				else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x2011"); /* un:4, f:40 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) {
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config IPTV on wan port ***\n");
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", iptv_vid);
				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0101");	/* 8, 0 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0081");	/* 7, 0 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0021");	/* 5, 0 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");

				if (iptv_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
		if (switch_stb >= 8) {
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config VOIP on wan port ***\n");
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", voip_vid);
				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0101");	/* 8, 0 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0081");	/* 7, 0 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0021");	/* 5, 0 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");

				if (voip_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)voip_prio);
				}
			}
		}
		if (switch_stb >=9) {
			if (mang_vid) { /* config tr069 on wan port */
_dprintf("*** Multicast IPTV: config Singtel TR069 on wan port ***\n");
				sprintf(wan_dev, "vlan%d", mang_vid);
				nvram_set("wan12_ifname", wan_dev);
				sprintf(port_id, "%d", mang_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", mang_vid);
				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0101");	/* 8, 0 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0081");	/* 7, 0 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0021");	/* 5, 0 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");

				if (mang_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
#endif
		break;

	case MODEL_RTAC3100:
	case MODEL_RTAC88U:
				/* If enable gmac3, CPU port is 8 */
#ifdef RTCONFIG_RGMII_BRCM5301X
				/* P4  P3 P2 P1 P0 P5 		P7 */
				/* WAN L1 L2 L3 L4 L5 L6 L7 L8 	CPU*/
#else
				/* P4  P3 P2 P1 P0 P5 */
				/* WAN L1 L2 L3 L4 CPU*/
#endif
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);

			if (gmac3_enable) {
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0110");	/* f: 4, 8 */
			} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0090");	/* f: 4, 7 */
#else
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0030");	/* f: 4, 5 */
#endif
			}
			eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));
		if (nvram_match("switch_wantag", "stuff_fibre")) {
			eval("vconfig", "set_egress_map", wan_dev, "3", "5");
			break;
		}

		if (nvram_match("switch_stb_x", "3")) {  // L3:p1 w:p4 c:p7/p5
			if (nvram_match("switch_wantag", "vodafone")) { //Config by robocfg
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);

				char vlan_cmd[64];
				if (gmac3_enable) {
					sprintf(vlan_cmd, "robocfg vlan 1 ports \"1 2 3 5u 7 8t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 4t 8t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 4t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 1 4t\"");
					system(vlan_cmd);
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					sprintf(vlan_cmd, "robocfg vlan 1 ports \"1 2 3 7t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 4t 7t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 4t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 1 4t\"");
					system(vlan_cmd);
#else
					sprintf(vlan_cmd, "robocfg vlan 1 ports \"1 2 3 5t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 4t 5t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 4t\"");
					system(vlan_cmd);
					sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 1 4t\"");
					system(vlan_cmd);
#endif
				}
				break;
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between WAN & L3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0012");	/* f: 4 1 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between WAN & L3, without untag */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0012");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", "0x0335"); /* vlan id=821 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0012");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", "0x0336"); /* vlan id=822 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case. */
				voip_prio <<= 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "-i", "eth0", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan(3) tag register: %s\n", tag_register);
				/* untag port map */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0412");	/* un:1, f:41*/
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) { // L4:p0
			/* config LAN 4 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward packets between wan & L4, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);

				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0111");	/* fwd: 0 4 8 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0091");	/* fwd: 0 4 7 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0031");	/* fwd: 0 4 5 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				/* config LAN 4 = IPTV */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "-i", "eth0", "robowr", "0x34", "0x10", tag_register);
				_dprintf("lan(4) tag register: %s\n", tag_register);
				/* untag port map */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0211");	/* un:0 f:40 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "6")) {	// L3/p1, L4/p0
			/* config L3/p1 = VoIP */
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between WAN & L3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0012");		/* f:41 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
			else {
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "-i", "eth0", "robowr", "0x34", "0x12", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* untag port map */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0613");	/* un:10, f:410*/
					else
						eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0412");	/* un:1 , f:41 */
					eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			/* config L4/p0 = IPTV */
			if (iptv_vid) {
				iptv_prio <<= 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "-i", "eth0", "robowr", "0x34", "0x10", tag_register);	/* p0 */
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* untag port map */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0613"); /* un:10 , f:410 */
				else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0211"); /* un:0, f:40 */
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) {
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config IPTV on wan port ***\n");
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", iptv_vid);
				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0110");	/* f: 4, 8 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0090");	/* f: 4, 7 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0030");	/* f: 4, 5 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");

				if (iptv_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
		if (switch_stb >= 8) {
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config VOIP on wan port ***\n");
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", voip_vid);
				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0110");	/* f: 4, 8 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0090");	/* f: 4, 7 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0030");	/* f: 4, 5 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
							eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");

				if (voip_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)voip_prio);
				}
			}
		}
		if (switch_stb >=9) {
			if (mang_vid) { /* config tr069 on wan port */
_dprintf("*** Multicast IPTV: config Singtel TR069 on wan port ***\n");
				sprintf(wan_dev, "vlan%d", mang_vid);
				nvram_set("wan12_ifname", wan_dev);
				sprintf(port_id, "%d", mang_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", mang_vid);
				if (gmac3_enable) {
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0110");	/* f: 4, 8 */
				} else {
#ifdef RTCONFIG_RGMII_BRCM5301X
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0090");	/* f: 4, 7 */
#else
					eval("et", "-i", "eth0", "robowr", "0x05", "0x83", "0x0030");	/* f: 4, 5 */
#endif
				}
				eval("et", "-i", "eth0", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "-i", "eth0", "robowr", "0x05", "0x80", "0x0080");

				if (mang_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
#endif
		break;
				/* P0  P1 P2 P3 P5 P7 */
	case MODEL_RTAC87U:	/* WAN L4 L3 L2 L1 CPU */
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0081");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {  // P3
			if (nvram_match("switch_wantag", "vodafone")) { //Config by robocfg
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);

				char vlan_cmd[64];
				sprintf(vlan_cmd, "robocfg vlan 1 ports \"2 3 5u 7t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 2 ports \"0 7\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 100 ports \"0t 1t 7t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 101 ports \"0t 1t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 105 ports \"0t 1t 2\"");
				system(vlan_cmd);
				break;
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0805");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) { // P4
			/* config LAN 4 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward packets between port 0 & 1, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("* vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0083");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0403");	/* un: 1, fwd; 0,1*/
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */ // P3
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0005");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "robowr", "0x34", "0x14", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* Set vlan table entry register */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "robowr", "0x05", "0x83", "0x0C07");
					else
						eval("et", "robowr", "0x05", "0x83", "0x0805");
					eval("et", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			/* config LAN 4 = IPTV */ // P4
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0C07");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0403");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) {
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config IPTV on wan port ***\n");
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", iptv_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0081");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (iptv_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
		if (switch_stb >= 8) {
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config VOIP on wan port ***\n");
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", voip_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0081");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (voip_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)voip_prio);
				}
			}
		}
		if (switch_stb >=9) {
			if (mang_vid) { /* config tr069 on wan port */
_dprintf("*** Multicast IPTV: config Singtel TR069 on wan port ***\n");
				sprintf(wan_dev, "vlan%d", mang_vid);
				nvram_set("wan12_ifname", wan_dev);
				sprintf(port_id, "%d", mang_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", mang_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0081");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (mang_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
#endif
		break;
				/* P0 P1 P2 P3 P4 P5 */
	case MODEL_RTAC56S:	/* L1 L2 L3 L4 WAN CPU */
	case MODEL_RTAC56U:
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0030");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {	// P2
			if (nvram_match("switch_wantag", "vodafone")) { //Config by robocfg
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);

				char vlan_cmd[64];
				sprintf(vlan_cmd, "robocfg vlan 1 ports \"0 1 2 5t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 2 ports \"4 5\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 100 ports \"3t 4t 5t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 101 ports \"3t 4t\"");
				system(vlan_cmd);
				sprintf(vlan_cmd, "robocfg vlan 105 ports \"2 3t 4t\"");
				system(vlan_cmd);
				break;
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 4 & 2, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 4 & 2, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x14", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0814");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) { // P3
			/* config LAN 4 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward packets between port 0 & 4, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0038");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x1018");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */	// P2
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 2 & 4, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0014");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "robowr", "0x34", "0x14", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* Set vlan table entry register */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "robowr", "0x05", "0x83", "0x081C");
					else
						eval("et", "robowr", "0x05", "0x83", "0x0814");
					eval("et", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			/* config LAN 4 = IPTV */ //P3
			if (iptv_vid) {
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x081C");
				else
					eval("et", "robowr", "0x05", "0x83", "0x1018");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) {
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config IPTV on wan port ***\n");
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", iptv_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0030");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (iptv_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
		if (switch_stb >= 8) {
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config VOIP on wan port ***\n");
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", voip_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0030");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (voip_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)voip_prio);
				}
			}
		}
		if (switch_stb >=9) {
			if (mang_vid) { /* config tr069 on wan port */
_dprintf("*** Multicast IPTV: config Singtel TR069 on wan port ***\n");
				sprintf(wan_dev, "vlan%d", mang_vid);
				nvram_set("wan12_ifname", wan_dev);
				sprintf(port_id, "%d", mang_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", mang_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0030");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (mang_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
#endif
		break;

	case MODEL_RTN66U:
	case MODEL_RTAC66U:
	case MODEL_RTAC1200G:
	case MODEL_RTAC1200GP:
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0101");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {
			if (nvram_match("switch_wantag", "vodafone")) { //Config by robocfg
				iptv_vid = 105;
				iptv_prio = 1;
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				/* vlan 1 */
				eval("et", "robowr", "0x05", "0x83", "0x0D06");
				eval("et", "robowr", "0x05", "0x81", "0x0001");
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				/* vlan 100 */
				eval("et", "robowr", "0x05", "0x83", "0x0111");
				eval("et", "robowr", "0x05", "0x81", "0x0064");
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				/* vlan 101 */
				eval("et", "robowr", "0x05", "0x83", "0x0011");
				eval("et", "robowr", "0x05", "0x81", "0x0065");
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				/* vlan 105 */
				eval("et", "robowr", "0x05", "0x83", "0x1019");
				eval("et", "robowr", "0x05", "0x81", "0x0069");
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				break;
			}
			if (nvram_match("switch_wantag", "m1_fiber") ||
			   nvram_match("switch_wantag", "maxis_fiber_sp")
			) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber_iptv")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0335");	/* vlan id=821 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0336");	/* vlan id=822 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				/* Just forward packets between port 0 & 4 & untag */
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x0337");	/* vlan id=823 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x0338");	/* vlan id=824 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else if (nvram_match("switch_wantag", "maxis_fiber_sp_iptv")) {
				/* Just forward packets between port 0 & 3, without untag */
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x0006");	/* vlan id= 6  */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", "0x000E");	/* vlan id= 14 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				/* Just forward packets between port 0 & 4 & untag */
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x000F");	/* vlan id= 15 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", "0x0011");	/* vlan id= 17 */
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x16", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x1009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) {
			/* config LAN 4 = IPTV */
			if (nvram_match("switch_wantag", "meo")) {
				/* Just forward packets between port 0 & 1, without untag */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("* vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0111");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {  /* Nomo case, untag it. */
				/* config LAN 4 = IPTV */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x18", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between port 0 & 3, without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0009");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "robowr", "0x34", "0x16", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* Set vlan table entry register */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "robowr", "0x05", "0x83", "0x3019");
					else
						eval("et", "robowr", "0x05", "0x83", "0x1009");
					eval("et", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			if (iptv_vid) { /* config LAN 4 = IPTV */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x18", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x3019");
				else
					eval("et", "robowr", "0x05", "0x83", "0x2011");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
#ifdef RTCONFIG_MULTICAST_IPTV
		if (switch_stb >= 7) { //Maxis IPTV case
			if (iptv_vid) { /* config IPTV on wan port */
_dprintf("*** Multicast IPTV: config Maxis IPTV on wan port ***\n");
				sprintf(wan_dev, "vlan%d", iptv_vid);
				nvram_set("wan10_ifname", wan_dev);
				sprintf(port_id, "%d", iptv_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", iptv_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0101");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (iptv_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
		if (switch_stb >= 8) { //Singtel IPTV case
			if (voip_vid) { /* config voip on wan port */
_dprintf("*** Multicast IPTV: config Singtel VOIP on wan port ***\n");
				sprintf(wan_dev, "vlan%d", voip_vid);
				nvram_set("wan11_ifname", wan_dev);
				sprintf(port_id, "%d", voip_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", voip_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0101");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (voip_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)voip_prio);
				}
			}
		}
		if (switch_stb >=9) {
			if (mang_vid) { /* config tr069 on wan port */
_dprintf("*** Multicast IPTV: config Singtel TR069 on wan port ***\n");
				sprintf(wan_dev, "vlan%d", mang_vid);
				nvram_set("wan12_ifname", wan_dev);
				sprintf(port_id, "%d", mang_vid);
				eval("vconfig", "add", interface, port_id);
				sprintf(vlan_entry, "0x%x", mang_vid);
				eval("et", "robowr", "0x05", "0x83", "0x0101");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");

				if (mang_prio) { /* config priority */
					eval("vconfig", "set_egress_map", wan_dev, "0", (char *)iptv_prio);
				}
			}
		}
#endif
		break;

	case MODEL_RTN15U:
		if (wan_vid) { /* config wan port */
			eval("vconfig", "rem", "vlan2");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
			sprintf(vlan_entry, "0x%x", wan_vid);
			eval("et", "robowr", "0x05", "0x83", "0x0110");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		}
		/* Set Wan port PRIO */
		if (nvram_invmatch("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_stb_x", "3")) {
			if (nvram_match("switch_wantag", "m1_fiber")) {
				/* Just forward packets between LAN3 & WAN(port1 & 4), without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0012");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				voip_prio = voip_prio << 13;
				sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
				eval("et", "robowr", "0x34", "0x12", tag_register);
				_dprintf("lan 3 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0412");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		} else if (nvram_match("switch_stb_x", "4")) {
			/* config LAN 4 = IPTV */
			iptv_prio = iptv_prio << 13;
			sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
			eval("et", "robowr", "0x34", "0x10", tag_register);
			_dprintf("lan 4 tag register: %s\n", tag_register);
			/* Set vlan table entry register */
			sprintf(vlan_entry, "0x%x", iptv_vid);
			_dprintf("vlan entry: %s\n", vlan_entry);
			eval("et", "robowr", "0x05", "0x83", "0x0211");
			eval("et", "robowr", "0x05", "0x81", vlan_entry);
			eval("et", "robowr", "0x05", "0x80", "0x0000");
			eval("et", "robowr", "0x05", "0x80", "0x0080");
		} else if (nvram_match("switch_stb_x", "6")) {
			/* config LAN 3 = VoIP */
			if (nvram_match("switch_wantag", "singtel_mio")) {
				/* Just forward packets between LAN3 & WAN(port1 & 4), without untag */
				sprintf(vlan_entry, "0x%x", voip_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				eval("et", "robowr", "0x05", "0x83", "0x0012");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
			else {	/* Nomo case, untag it. */
				if (voip_vid) {
					voip_prio = voip_prio << 13;
					sprintf(tag_register, "0x%x", (voip_prio | voip_vid));
					eval("et", "robowr", "0x34", "0x12", tag_register);
					_dprintf("lan 3 tag register: %s\n", tag_register);
					/* Set vlan table entry register */
					sprintf(vlan_entry, "0x%x", voip_vid);
					_dprintf("vlan entry: %s\n", vlan_entry);
					if (voip_vid == iptv_vid)
						eval("et", "robowr", "0x05", "0x83", "0x0613");
					else
						eval("et", "robowr", "0x05", "0x83", "0x0412");
					eval("et", "robowr", "0x05", "0x81", vlan_entry);
					eval("et", "robowr", "0x05", "0x80", "0x0000");
					eval("et", "robowr", "0x05", "0x80", "0x0080");
				}
			}
			if (iptv_vid) { /* config LAN 4 = IPTV */
				iptv_prio = iptv_prio << 13;
				sprintf(tag_register, "0x%x", (iptv_prio | iptv_vid));
				eval("et", "robowr", "0x34", "0x10", tag_register);
				_dprintf("lan 4 tag register: %s\n", tag_register);
				/* Set vlan table entry register */
				sprintf(vlan_entry, "0x%x", iptv_vid);
				_dprintf("vlan entry: %s\n", vlan_entry);
				if (voip_vid == iptv_vid)
					eval("et", "robowr", "0x05", "0x83", "0x0613");
				else
					eval("et", "robowr", "0x05", "0x83", "0x0211");
				eval("et", "robowr", "0x05", "0x81", vlan_entry);
				eval("et", "robowr", "0x05", "0x80", "0x0000");
				eval("et", "robowr", "0x05", "0x80", "0x0080");
			}
		}
		break;

				/* WAN L1 L2 L3 L4 CPU */
	case MODEL_RTAC53U:	/* P0  P1 P2 P3 P4 P5 */
		/* Enable high bits check */
		eval("et", "robowr", "0x34", "0x3", "0x80", "0x1");
		/* Config WAN port */
		if (wan_vid) {
			eval("vconfig", "rem", "vlan2");
			eval("et", "robowr", "0x34", "0x8", "0x01002000", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3002");
			sprintf(port_id, "%d", wan_vid);
			eval("vconfig", "add", interface, port_id);
		}
		/* Set Wan prio*/
		if (!nvram_match("switch_wan0prio", "0"))
			eval("vconfig", "set_egress_map", wan_dev, "0", nvram_get("switch_wan0prio"));

		if (nvram_match("switch_wantag", "unifi_home")) {
			/* vlan1ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010013ae", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan500ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
			/* vlan600ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01258411", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3258");
			/* LAN4 vlan tag */
			eval("et", "robowr", "0x34", "0x18", "0x0258");
		} else if (nvram_match("switch_wantag", "unifi_biz")) {
			/* Modify vlan500ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x011f4021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x31f4");
		} else if (nvram_match("switch_wantag", "singtel_mio")) {
			/* vlan1ports= 1 2 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010011a6", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan10ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 4 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01014411", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* vlan30ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x0101e009", "0x4");/*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x301e");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x18", "0x8014");
		} else if (nvram_match("switch_wantag", "singtel_others")) {
			/* vlan1ports= 1 2 3 5 */
			eval("et", "robowr", "0x34", "0x8", "0x010013ae", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan10ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100a021", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x300a");
			/* vlan20ports= 0 4 */
			eval("et", "robowr", "0x34", "0x8", "0x01014411", "0x4");
			eval("et", "robowr", "0x34", "0x6", "0x3014");
			/* LAN4 vlan tag & prio */
			eval("et", "robowr", "0x34", "0x18", "0x8014");
		} else if (nvram_match("switch_wantag", "m1_fiber")) {
			/* vlan1ports= 1 2 4 5 */				 /*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010015b6", "0x4");/*0111|1011|0110*/
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan1103ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0144f021", "0x4");/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x344f");
			/* vlan1107ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01453009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3453");
		} else if (nvram_match("switch_wantag", "maxis_fiber")) {
			/* vlan1ports= 1 2 4 5 */				 /*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010015b6", "0x4");/*0111|1011|0110*/
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan621 ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0126d021", "0x4");/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x326d");
			/* vlan821/822 ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x01335009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3335");
			eval("et", "robowr", "0x34", "0x8", "0x01336009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x3336");
		} else if (nvram_match("switch_wantag", "maxis_fiber_sp")) {
			/* vlan1ports= 1 2 4 5 */				 /*5432 1054 3210*/
			eval("et", "robowr", "0x34", "0x8", "0x010015b6", "0x4");/*0111|1011|0110*/
			eval("et", "robowr", "0x34", "0x6", "0x3001");
			/* vlan11ports= 0 5 */
			eval("et", "robowr", "0x34", "0x8", "0x0100b021", "0x4");/*0000|0010|0001*/ /*Dont untag WAN port*/
			eval("et", "robowr", "0x34", "0x6", "0x300b");
			/* vlan14ports= 3 0 */
			eval("et", "robowr", "0x34", "0x8", "0x0100e009", "0x4");/*0000|0000|1001*/ /*Just forward without untag*/
			eval("et", "robowr", "0x34", "0x6", "0x300e");
		} else {	/* manual */
							/* WAN L1 L2 L3 L4 CPU */
			const int ports[SWPORT_COUNT] = { 0, 1, 2, 3, 4, 5 };

			if (switch_stb != SWCFG_STB4 && switch_stb != SWCFG_STB34)
				iptv_vid = 0;
			if (switch_stb != SWCFG_STB3 && switch_stb != SWCFG_STB34)
				voip_vid = 0;
			if (wan_vid) {
				sprintf(vlan_entry, "0x%x", BCM5325_ventry(wan_vid, wan_vid, iptv_vid, voip_vid));
				eval("et", "robowr", "0x34", "0x8", vlan_entry, "0x4");
				eval("et", "robowr", "0x34", "0x6", "0x3002");
			}
			if (iptv_vid) {
				if (iptv_vid != wan_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(iptv_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry, "0x4");
					eval("et", "robowr", "0x34", "0x6", "0x3003");
				}
				sprintf(tag_register, "0x%x", (iptv_prio << 13) | iptv_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN4]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
			if (voip_vid) {
				if (voip_vid != wan_vid && voip_vid != iptv_vid) {
					sprintf(vlan_entry, "0x%x", BCM5325_ventry(voip_vid, wan_vid, iptv_vid, voip_vid));
					eval("et", "robowr", "0x34", "0x8", vlan_entry, "0x4");
					eval("et", "robowr", "0x34", "0x6", "0x3004");
				}
				sprintf(tag_register, "0x%x", (voip_prio << 13) | voip_vid);
				sprintf(port_id, "0x%x", 0x10 + 2*ports[SWPORT_LAN3]);
				eval("et", "robowr", "0x34", port_id, tag_register);
			}
		}
		break;
	}
	return;
}

int
wl_exist(char *ifname, int band)
{
	int unit = -1;
	int bandtype;

#ifdef RTCONFIG_QTN
	if (band == 2) {
		if (nvram_get_int("qtn_ready") == 1)
			return 1;
		else
			return -1;
	}
#endif

	if (wl_probe(ifname) || wl_ioctl(ifname, WLC_GET_INSTANCE, &unit, sizeof(unit)))
		goto ERROR;

	wl_ioctl(ifname, WLC_GET_BAND, &bandtype, sizeof(bandtype));

	if ((bandtype != WLC_BAND_AUTO) &&
	    ((band == 1 && bandtype != WLC_BAND_2G) ||
	     (band >= 2 && bandtype != WLC_BAND_5G)))
		goto ERROR;
	else
		return 1;

ERROR:
	_dprintf("No wireless %s interface!!!\n", ifname);
	return 0;
}

#ifdef RTCONFIG_BCMWL6
#define WL_5G_BAND_2	1 << (2 - 1)
#define WL_5G_BAND_3	1 << (3 - 1)
#define WL_5G_BAND_4	1 << (4 - 1)

void set_acs_ifnames()
{
	char acs_ifnames[64];
	char acs_ifnames2[64];
	char word[256], *next;
	char tmp[128], tmp2[128], prefix[] = "wlXXXXXXXXXX_";
	int unit;
	int dfs_in_use = 0;
#if defined(RTAC3200) || defined(RTAC5300) || defined(GTAC5300)
	int dfs_in_use_5g_2 = 0;
#endif
#if defined(RTCONFIG_DPSTA) && defined(RTCONFIG_AMAS)
	char re_ap_ifname[32];
#endif

	wl_check_5g_band_group();

	unit = 0;
	memset(acs_ifnames, 0, sizeof(acs_ifnames));

	foreach (word, nvram_safe_get("wl_ifnames"), next) {
#ifdef RTCONFIG_QTN
		if (!strcmp(word, "wifi0")) break;
#endif
		snprintf(prefix, sizeof(prefix), "wl%d_", unit);

		if (nvram_match(strcat_r(prefix, "radio", tmp), "1") &&
			((nvram_match(strcat_r(prefix, "mode", tmp), "ap") &&
		       (nvram_match(strcat_r(prefix, "chanspec", tmp), "0") ||
		       (nvram_match(strcat_r(prefix, "bw", tmp), "0") &&
			nvram_match(strcat_r(prefix, "nband", tmp2), "2"))))
#if defined(RTCONFIG_AMAS) && defined(RTCONFIG_DPSTA)
			|| (dpsta_mode() && nvram_get_int("re_mode") == 1)
#endif
		))
		{
#if 0
			if (nvram_match(strcat_r(prefix, "bw", tmp), "0"))
				nvram_set(strcat_r(prefix, "chanspec", tmp), "0");
#endif

#if defined(RTCONFIG_AMAS) && defined(RTCONFIG_DPSTA)
			if (nvram_get_int("re_mode") == 1) {
				memset(re_ap_ifname, 0, sizeof(re_ap_ifname));
				snprintf(re_ap_ifname, sizeof(re_ap_ifname), "wl%d.1", unit);
			}
#endif

			if (strlen(acs_ifnames)) {
#if defined(RTCONFIG_AMAS) && defined(RTCONFIG_DPSTA)
				if (nvram_get_int("re_mode") == 1)
					sprintf(acs_ifnames2, "%s %s", acs_ifnames, re_ap_ifname);
				else
#endif
				sprintf(acs_ifnames2, "%s %s", acs_ifnames, word);
				strlcpy(acs_ifnames, acs_ifnames2, sizeof(acs_ifnames));
			} else
			{
#if defined(RTCONFIG_AMAS) && defined(RTCONFIG_AMAS)
				if (nvram_get_int("re_mode") == 1)
					sprintf(acs_ifnames, "%s", re_ap_ifname);
				else
#endif
				sprintf(acs_ifnames, "%s", word);
			}
		}

#ifndef RTCONFIG_BCM_7114
		nvram_set(strcat_r(prefix, "acs_pol", tmp), "-65 40 -1 -100 -100 -1 -100 50 -100 0 1 0");
#endif

		unit++;
	}

	nvram_set("acs_ifnames", acs_ifnames);

	/* exclude acsd from selecting chanspec 12, 12u, 13, 13u, 14, 14u */
	nvram_set("wl0_acs_excl_chans", nvram_match("acs_ch13", "1") ? "" : "0x100c,0x190a,0x100d,0x190b,0x100e,0x190c");

#if defined(RTAC3200) || defined(RTAC5300) || defined(GTAC5300)
	nvram_set("wl1_acs_excl_chans", "");
	dfs_in_use = nvram_get_int("wl1_band5grp") & WL_5G_BAND_2;
	dfs_in_use_5g_2 = nvram_get_int("wl1_band5grp") & WL_5G_BAND_3;

#if defined(RTAC5300) || defined(GTAC5300)
	if ((nvram_get_int("wl2_band5grp") & WL_5G_BAND_3) && nvram_match("wl2_country_code", "E0"))
		/* exclude non-certificated weather radar chanspec 116l, 116/80, 120, 120u, 120/80, 124, 124l, 124/80, 128, 128u, 128/80 */
		nvram_set("wl2_acs_excl_chans", "0xd876,0xe07a,0xd078,0xd976,0xe17a,0xd07c,0xd87e,0xe27a,0xd080,0xd97e,0xe37a");
	else
#endif
	/* exclude acsd from selecting chanspec 165 */
	nvram_set("wl2_acs_excl_chans", (nvram_get_int("wl2_band5grp") & WL_5G_BAND_4) ? "0xd0a5" : "");
#else
	if (nvram_match("wl1_band5grp", "7")) {		// EU, JP, UA
#ifdef RTAC66U
		if (!nvram_match("wl1_dfs", "1"))
			nvram_set("acs_dfs", "0");
#endif
		if (!strncmp(nvram_safe_get("territory_code"), "UA", 2))
			/* exclude acsd from selecting chanspec 100, 100l, 100/80, 104, 104u, 104/80, 108, 108l, 108/80, 112, 112u, 112/80, 116, 132, 132l, 136, 136u, 140 */
			nvram_set("wl1_acs_excl_chans", nvram_match("acs_band3", "1") ? "" : "0xd064,0xd866,0xe06a,0xd068,0xd966,0xe16a,0xd06c,0xd86e,0xe26a,0xd070,0xd96e,0xe36a,0xd074,0xd084,0xd886,0xd088,0xd986,0xd08c");
		else
#if defined(RTAC88U) || defined(RTAC3100)
			/* exclude non-certificated weather radar chanspec 116l, 116/80, 120, 120u, 120/80, 124, 124l, 124/80, 128, 128u, 128/80 */
			nvram_set("wl1_acs_excl_chans", nvram_match("acs_dfs", "1") ? (nvram_match("wl1_country_code", "E0") ? "0xd876,0xe07a,0xd078,0xd976,0xe17a,0xd07c,0xd87e,0xe27a,0xd080,0xd97e,0xe37a" : "") : "0xd034,0xe03a,0xd836,0xd038,0xe13a,0xd936,0xd03c,0xe23a,0xd83e,0xd040,0xe33a,0xd93e,0xd064,0xd866,0xe06a,0xd068,0xd966,0xe16a,0xd06c,0xd86e,0xe26a,0xd070,0xd96e,0xe36a,0xd074,0xd078,0xd976,0xe17a,0xd07c,0xd87e,0xe27a,0xd080,0xd97e,0xe37a,0xd084,0xd886,0xd088,0xd986,0xd08c");
#else
			/* exclude acsd from selecting chanspec 52, 52l, 52/80, 56, 56u, 56/80, 60, 60l, 60/80, 64, 64u, 64/80, 100, 100l, 100/80, 104, 104u, 104/80, 108, 108l, 108/80, 112, 112u, 112/80, 116, 132, 132l, 136, 136u, 140 */
			nvram_set("wl1_acs_excl_chans", nvram_match("acs_dfs", "1") ? "" : "0xd034,0xe03a,0xd836,0xd038,0xe13a,0xd936,0xd03c,0xe23a,0xd83e,0xd040,0xe33a,0xd93e,0xd064,0xd866,0xe06a,0xd068,0xd966,0xe16a,0xd06c,0xd86e,0xe26a,0xd070,0xd96e,0xe36a,0xd074,0xd084,0xd886,0xd088,0xd986,0xd08c");
#endif
		dfs_in_use = nvram_match("acs_dfs", "1");
	} else if (nvram_match("wl1_band5grp", "9")) {	// US, CA, TW, SG, KR, AU (FCC)
		if (nvram_match("wl1_country_code", "US") ||
		    nvram_match("wl1_country_code", "Q1") || nvram_match("wl1_country_code", "Q2") ||
		    nvram_match("wl1_country_code", "SG") ||
		    !strncmp(nvram_safe_get("territory_code"), "US", 2) ||
		    !strncmp(nvram_safe_get("territory_code"), "AU", 2))
			// enable band 1 for US region
			nvram_set("acs_band1", "1");

		/* exclude acsd from selecting chanspec 36, 36l, 36/80, 40, 40u, 40/80, 44, 44l, 44/80, 48, 48u, 48/80, 165 for non-US region by default */
		nvram_set("wl1_acs_excl_chans", nvram_match("acs_band1", "1") ? "0xd0a5" : "0xd024,0xd826,0xe02a,0xd028,0xd926,0xe12a,0xd02c,0xd82e,0xe22a,0xd030,0xd92e,0xe32a,0xd0a5");
	} else if (nvram_match("wl1_band5grp", "f")) {	// AU (NEW)
		/* exclude acsd from selecting chanspec 52, 52l, 52/80, 56, 56u, 56/80, 60, 60l, 60/80, 64, 64u, 64/80, 100, 100l, 100/80, 104, 104u, 104/80, 108, 108l, 108/80, 112, 112u, 112/80, 116, 132, 132l, 136, 136u, 140, 165 */
		nvram_set("wl1_acs_excl_chans", nvram_match("acs_dfs", "1") ? "0xd0a5" : "0xd034,0xe03a,0xd836,0xd038,0xe13a,0xd936,0xd03c,0xe23a,0xd83e,0xd040,0xe33a,0xd93e,0xd064,0xd866,0xe06a,0xd068,0xd966,0xe16a,0xd06c,0xd86e,0xe26a,0xd070,0xd96e,0xe36a,0xd074,0xd084,0xd886,0xd088,0xd986,0xd08c,0xd0a5");
		dfs_in_use = nvram_match("acs_dfs", "1");
	} else {					// CN, AU (FCC + CE)
		/* exclude acsd from selecting chanspec 165 */
		nvram_set("wl1_acs_excl_chans", "0xd0a5");
	}
#endif

	nvram_set_int("wl0_acs_dfs", 0);
	nvram_set_int("wl1_acs_dfs", dfs_in_use ? 2 : 0);
#if defined(RTAC3200) || defined(RTAC5300) || defined(GTAC5300)
	nvram_set_int("wl2_acs_dfs", dfs_in_use_5g_2 ? 2 : 0);
#endif
}
#endif

#ifdef RTCONFIG_PORT_BASED_VLAN
int check_used_stb_voip_port(int lan)
{
	int used = 0;
	int used_port = 0;

	/* L4 L3 L2 L1 */
	/* 8  4  2  1 */
	if (!nvram_match("switch_wantag", "none") && !nvram_match("switch_wantag", "")) {
		if (!strcmp(nvram_safe_get("switch_wantag"), "unifi_home"))
			used_port = 0x8;	/* LAN4 for stb */
		if (!strcmp(nvram_safe_get("switch_wantag"), "unifi_biz"))
			used_port = 0x0;
		else if (!strcmp(nvram_safe_get("switch_wantag"), "singtel_mio"))
			used_port = 0xc;	/* LAN4 for stb, LAN3 for voip */
		else if (!strcmp(nvram_safe_get("switch_wantag"), "singtel_others"))
			used_port = 0x8;	/* LAN4 for stb */
		else if (!strcmp(nvram_safe_get("switch_wantag"), "m1_fiber"))
			used_port = 0x4;	/* LAN3 for voip */
		else if (!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber"))
			used_port = 0x4;	/* LAN3 for voip */
		else if (!strcmp(nvram_safe_get("switch_wantag"), "maxis_fiber_sp"))
			used_port = 0x4;	/* LAN3 for voip */
		else	/* For manual */
		{
			if (strcmp(nvram_safe_get("switch_wan1tagid"), ""))
				used_port += 0x8;	/* LAN4 for stb */

			if (strcmp(nvram_safe_get("switch_wan2tagid"), ""))
				used_port += 0x4;	/* LAN3 for voip */
		}
	}
	else	/* For none */
	{
		int stbport = 0;

		stbport = nvram_get_int("switch_stb_x");
		if (stbport < 0 || stbport > 6)
			stbport = 0;

		if (stbport >= 1 && stbport <= 4)
			used_port = 1 << (stbport - 1);
		else if (stbport == 5)	/* LAN1 & LAN2 */
			used_port = 0x3;
		else if (stbport == 6)	/* LAN3 & LAN4 */
			used_port = 0xc;
	}

	if (lan & used_port)
		used = 1;

	return used;
}

unsigned int convert_vlan_entry(int tag_enable, int portset, char *tag_reg_val)
{
	int real_portset = 0;
	int model, i;
	int port_shift_bit[] = { 0, 0, 0, 0};	/* shift bit for LAN X */
	char *port_tag_reg[] = { "0x10", "0x12", "0x14", "0x16", "0x18", "0x1a" };
	unsigned int vlan_entry = 0;

	model = get_model();

	/* P0  P1 P2 P3 P4 */
	/* WAN L4 L3 L2 L1 */
	if (model == MODEL_RTN16) {
		port_shift_bit[0] = 4;
		port_shift_bit[1] = 3;
		port_shift_bit[2] = 2;
		port_shift_bit[3] = 1;
	}
	/* P0  P1 P2 P3 P5 */
	/* WAN L4 L3 L2 L1 */
	else if (model == MODEL_RTAC87U) {
		port_shift_bit[0] = 5;
		port_shift_bit[1] = 3;
		port_shift_bit[2] = 2;
		port_shift_bit[3] = 1;
	}
	/* P0  P1 P2 P3 P4 */
	/* WAN L1 L2 L3 L4 */
	else if (model == MODEL_RTAC68U || model == MODEL_RTN18U ||
		model == MODEL_RTN66U || model == MODEL_RTAC66U ||
		model == MODEL_DSLAC68U) {
		port_shift_bit[0] = 1;
		port_shift_bit[1] = 2;
		port_shift_bit[2] = 3;
		port_shift_bit[3] = 4;
	}
	/* P0 P1 P2 P3 P4 */
	/* L1 L2 L3 L4 WAN */
	else if (model == MODEL_RTAC56S || model == MODEL_RTAC56U) {
		port_shift_bit[0] = 0;
		port_shift_bit[1] = 1;
		port_shift_bit[2] = 2;
		port_shift_bit[3] = 3;
	}
	/* P0 P1 P2 P3 P4 */
	/* L4 L3 L2 L1 WAN */
	else if (model == MODEL_RTN15U) {
		port_shift_bit[0] = 3;
		port_shift_bit[1] = 2;
		port_shift_bit[2] = 1;
		port_shift_bit[3] = 0;
	}

	/* Convert the port set of web to real port set of switch */
	_dprintf("%s: temp portset=0x%x\n", __FUNCTION__, portset);
	for (i = 0; i < sizeof(port_shift_bit)/sizeof(int); i++) {
#ifdef RTCONFIG_DUALWAN
		int wancfg = nvram_get_int("wans_lanport");
		if ((get_wans_dualwan() & WANSCAP_LAN) && wancfg >= 1 && wancfg <= 4) {
			/* Filter lan port as wan */
			if ((i == (wancfg -1)) && (portset & (1 << (wancfg - 1))))
				continue;
		}
#endif
		if (portset & (1 << i) && !check_used_stb_voip_port(1 << i))
			real_portset |= (1 << port_shift_bit[i]);
	}
	_dprintf("%s: real portset=0x%x\n", __FUNCTION__, real_portset);

	/* Set the temporary value of vlan entry for port 0 ~ port 4 */
	for (i = 0; i < sizeof(port_shift_bit)/sizeof(int); i++) {
		int port_val = real_portset & (1 << port_shift_bit[i]);

		if (port_val) {
#ifndef HND_ROUTER
			eval("et", "robowr", "0x34", port_tag_reg[port_shift_bit[i]], tag_reg_val);//
			_dprintf("%s: port tag reg=%s\n", __FUNCTION__, port_tag_reg[port_shift_bit[i]]);
#endif

			if (tag_enable)
				vlan_entry |= (1 << port_shift_bit[i]);
			else
				vlan_entry |= ((1 << (9 + port_shift_bit[i])) | (1 << port_shift_bit[i]));
		}
	}

	return vlan_entry;
}

unsigned int convert_vlan_entry_bcm5325(int tag_enable, int portset, char *tag_reg_val)
{
	int real_portset = 0;
	int model, i;
	int port_shift_bit[] = { 0, 0, 0, 0};	/* shift bit for LAN X */
	char *port_tag_reg[] = { "0x10", "0x12", "0x14", "0x16", "0x18", "0x1a" };
	unsigned int vlan_entry = 0;

	model = get_model();

	/* P0 P1 P2 P3 P4 */
	/* L4 L3 L2 L1 WAN */
	if (model == MODEL_APN12HP || model == MODEL_RTN53 || model == MODEL_RTN12 ||
		model == MODEL_RTN12B1 || model == MODEL_RTN12C1 || model == MODEL_RTN12D1 ||
		model == MODEL_RTN12VP || model == MODEL_RTN12HP || model == MODEL_RTN12HP_B1 ||
		model == MODEL_RTN10P || model == MODEL_RTN10D1 || model == MODEL_RTN10PV2) {
		port_shift_bit[0] = 3;
		port_shift_bit[1] = 2;
		port_shift_bit[2] = 1;
		port_shift_bit[3] = 0;
	}
	/* P0 P1 P2 P3 P4 */
	/* L1 L2 L3 L4 WAN */
	else if (model == MODEL_RTN14UHP || model == MODEL_RTN10U) {
		port_shift_bit[0] = 0;
		port_shift_bit[1] = 1;
		port_shift_bit[2] = 2;
		port_shift_bit[3] = 3;
	}
	/* P0  P1 P2 P3 P4 */
	/* WAN L1 L2 L3 L4 */
	else if (model == MODEL_RTAC53U) {
		port_shift_bit[0] = 1;
		port_shift_bit[1] = 2;
		port_shift_bit[2] = 3;
		port_shift_bit[3] = 4;
	}

	/* Convert the port set of web to real port set of switch */
	_dprintf("%s: temp portset=0x%x\n", __FUNCTION__, portset);
	for (i = 0; i < sizeof(port_shift_bit)/sizeof(int); i++) {
#ifdef RTCONFIG_DUALWAN
		int wancfg = nvram_get_int("wans_lanport");
		if ((get_wans_dualwan() & WANSCAP_LAN) && wancfg >= 1 && wancfg <= 4) {
			/* Filter lan port as wan */
			if ((i == (wancfg -1)) && (portset & (1 << (wancfg - 1))))
				continue;
		}
#endif
		if (portset & (1 << i) && !check_used_stb_voip_port(1 << i))
			real_portset |= (1 << port_shift_bit[i]);
	}
	_dprintf("%s: real portset=0x%x\n", __FUNCTION__, real_portset);

	/* Set the temporary value of vlan entry for port 0 ~ port 4 */
	for (i = 0; i < sizeof(port_shift_bit)/sizeof(int); i++) {
		int port_val = real_portset & (1 << port_shift_bit[i]);

		if (port_val) {
#ifndef HND_ROUTER
			eval("et", "robowr", "0x34", port_tag_reg[port_shift_bit[i]], tag_reg_val);//
			_dprintf("%s: port tag reg=0x%s\n", __FUNCTION__, port_tag_reg[port_shift_bit[i]]);
#endif

			if (tag_enable)
				vlan_entry |= (1 << port_shift_bit[i]);
			else
				vlan_entry |= ((1 << (6 + port_shift_bit[i])) | (1 << port_shift_bit[i]));
		}
	}

	return vlan_entry;
}

void set_port_based_vlan_config(char *interface)
{
	char *nv, *nvp, *b;
	//char *enable, *vid, *priority, *portset, *wlmap, *subnet_name;
	//char *portset, *wlmap, *subnet_name;
	char *enable, *desc, *portset, *wlmap, *subnet_name, *intranet;
	int set_flag = (interface != NULL) ? 1 : 0;

	/* Clean some parameters for vlan */
	//clean_vlan_ifnames();

	if (vlan_enable()) {
		nv = nvp = strdup(nvram_safe_get("vlan_rulelist"));

		if (nv) {
			int vlan_tag = 4;
			int model;
			int br_index = 1;

			model = get_model();

			if (model == MODEL_DSLAC68U)
				vlan_tag = 5;

			while ((b = strsep(&nvp, "<")) != NULL) {
				//int real_portset = 0;
				char tag_reg_val[7], vlan_id[9], vlan_entry[12];
				unsigned int vlan_entry_tmp = 0, tag_reg_val_tmp = 0;
				int i = 0, vlan_id_tmp = 0;
				int cpu_port = 0;

				//if ((vstrsep(b, ">", &enable, &vid, &priority, &portset, &wlmap, &subnet_name) != 6))
				//if ((vstrsep(b, ">", &portset, &wlmap, &subnet_name) != 3))
				if ((vstrsep(b, ">", &enable, &desc, &portset, &wlmap, &subnet_name, &intranet) != 6))
					continue;

				//_dprintf("%s: %s %s %s %s %s %s\n", __FUNCTION__, enable, vid, priority, portset, wlmap, subnet_name);
				_dprintf("%s: %s %s %s %s %s %s\n", __FUNCTION__, enable, desc, portset, wlmap, subnet_name, intranet);

				if (!strcmp(enable, "0") || strlen(enable) == 0)
					continue;

				//real_portset = atoi(portset);
				//real_portset = convert_portset(atoi(portset));
				//_dprintf("%s: real port set=0x%x\n", __FUNCTION__, real_portset);

				/* Port mapping of the switch for MODEL_RTN16 */
				/* P0  P1 P2 P3 P4 P8 */
				/* WAN L4 L3 L2 L1 CPU */
				/* Port mapping of the switch for MODEL_RTAC68U & MODEL_RTN18U */
				/* P0  P1 P2 P3 P4 P5 */
				/* WAN L1 L2 L3 L4 CPU */
				/* Port mapping of the switch for MODEL_RTAC87U */
				/* P0  P1 P2 P3 P5 P7 */
				/* WAN L4 L3 L2 L1 CPU */
				/* Port mapping of the switch for MODEL_RTAC56S & MODEL_RTAC56U */
				/* P0 P1 P2 P3 P4  P5 */
				/* L1 L2 L3 L4 WAN CPU */
				/* Port mapping of the switch for MODEL_RTN66U & MODEL_RTAC66U */
				/* P0  P1 P2 P3 P4 P8 */
				/* WAN L1 L2 L3 L4 CPU */
				/* Port mapping of the switch for MODEL_RTN15U */
				/* P0 P1 P2 P3 P4  P8 */
				/* L1 L2 L3 L4 WAN CPU */
				/* Port mapping of the switch for MODEL_DSLAC68U */
				/* P0  P1 P2 P3 P4 P5 */
				/* WAN L1 L2 L3 L4 CPU */
				if (model == MODEL_RTN16 ||
					model == MODEL_RTAC68U || model == MODEL_RTN18U ||
					model == MODEL_RTAC87U ||
					model == MODEL_RTAC56S || model == MODEL_RTAC56U ||
					model == MODEL_RTN66U || model == MODEL_RTAC66U ||
					model == MODEL_RTN15U ||
					model == MODEL_DSLAC68U) {
					/*char tag_reg_val[7], vlan_id[9], vlan_entry[7];
					unsigned int vlan_entry_tmp = 0, tag_reg_val_tmp = 0;
					int i = 0, vlan_id_tmp = 0;
					//char *port_tag_reg[] = { "0x12", "0x14", "0x16", "0x18" };
					int cpu_port = 0;*/

					/* Decide cpu port by model */
					if (model == MODEL_RTAC68U || model == MODEL_RTN18U ||
						model == MODEL_RTAC56S || model == MODEL_RTAC56U ||
						model == MODEL_DSLAC68U)
						cpu_port = 5;
					else if (model == MODEL_RTAC87U)
						cpu_port = 7;
					else if (model == MODEL_RTN16 ||
						model == MODEL_RTN66U || model == MODEL_RTAC66U ||
						MODEL_RTN15U)
						cpu_port = 8;

					if (atoi(portset) != 0) {
						vlan_id_tmp = vlan_tag;
						tag_reg_val_tmp = vlan_tag;
						vlan_tag++;

						snprintf(tag_reg_val, sizeof(tag_reg_val), "0x%x", tag_reg_val_tmp);
						_dprintf("%s: tag register value=%s\n", __FUNCTION__, tag_reg_val);

						if (set_flag) {
							/* Set vlan entry for port 0 ~ port 4 */
							//vlan_entry_tmp = convert_vlan_entry(atoi(enable), atoi(portset), tag_reg_val);
							vlan_entry_tmp = convert_vlan_entry(0, atoi(portset), tag_reg_val);

							/* Set vlan entry for cpu port */
							vlan_entry_tmp |= (1 << cpu_port);
						}

						/* Set vlan table entry register */
						snprintf(vlan_id, sizeof(vlan_id), "0x%x", vlan_id_tmp);
						_dprintf("%s: vlan id=%s\n", __FUNCTION__, vlan_id);
						snprintf(vlan_entry, sizeof(vlan_entry), "0x%x", vlan_entry_tmp);
						_dprintf("%s: vlan entry=%s\n", __FUNCTION__, vlan_entry);
						if (set_flag) {
#ifndef HND_ROUTER
							eval("et", "robowr", "0x05", "0x83", vlan_entry);
							eval("et", "robowr", "0x05", "0x81", vlan_id);
#if !defined(RTAC87U)
							eval("et", "robowr", "0x05", "0x80", "0x0000");
#endif
							eval("et", "robowr", "0x05", "0x80", "0x0080");
#endif

							/* Create the VLAN interface */
							snprintf(vlan_id, sizeof(vlan_id), "%d", vlan_id_tmp);
							eval("vconfig", "add", interface, vlan_id);

							/* Setup ingress map (vlan->priority => skb->priority) */
							snprintf(vlan_id, sizeof(vlan_id), "vlan%d", vlan_id_tmp);
							for (i = 0; i < VLAN_NUMPRIS; i++) {
								char prio[8];

								snprintf(prio, sizeof(prio), "%d", i);
								eval("vconfig", "set_ingress_map", vlan_id, prio, prio);
							}
						}

						snprintf(vlan_id, sizeof(vlan_id), "vlan%d", vlan_id_tmp);
						set_vlan_ifnames(br_index, atoi(wlmap), subnet_name, vlan_id);
					}
					else	/* portset is 0, only for wireless */
					{
						set_vlan_ifnames(br_index, atoi(wlmap), subnet_name, NULL);
					}
				}
				/* Port mapping of the switch for MODEL_APN12HP, MODEL_RTN53, MODEL_RTN12
				   MODEL_RTN12B1, MODEL_RTN12C1, MODEL_RTN12D1, MODEL_RTN12VP, MODEL_RTN12HP
				   MODEL_RTN12HP_B1, MODEL_RTN10P, MODEL_RTN10D1 and MODEL_RTN10PV2 */
				/* P0 P1 P2 P3 P4  P5 */
				/* L4 L3 L2 L1 WAN CPU */
				/* Port mapping of the switch for MODEL_RTN14UHP and MODEL_RTN10U */
				/* P0 P1 P2 P3 P4  P5 */
				/* L1 L2 L3 L4 WAN CPU */
				/* Port mapping of the switch for MODEL_RTAC53U */
				/* P0  P1 P2 P3 P4 P5 */
				/* WAN L1 L2 L3 L4 CPU */
				else
				{
					/*char tag_reg_val[7], vlan_id[9], vlan_entry[12];
					unsigned int vlan_entry_tmp = 0, tag_reg_val_tmp = 0;
					int i = 0, vlan_id_tmp = 0;*/
					cpu_port = 5;

					/* Enable high bits check */
#ifndef HND_ROUTER
					eval("et", "robowr", "0x34", "0x3", "0x0080");
#endif

					if (atoi(portset) != 0) {
						vlan_id_tmp = vlan_tag;
						tag_reg_val_tmp = vlan_tag;
						vlan_tag++;

						snprintf(tag_reg_val, sizeof(tag_reg_val), "0x%x", tag_reg_val_tmp);
						_dprintf("%s: tag register value=%s\n", __FUNCTION__, tag_reg_val);

						if (set_flag) {
							/* Set vlan entry for port 0 ~ port 4 */
							//vlan_entry_tmp = convert_vlan_entry_bcm5325(atoi(enable), atoi(portset), tag_reg_val);
							vlan_entry_tmp = convert_vlan_entry_bcm5325(0, atoi(portset), tag_reg_val);

							/* Set vlan entry for cpu port */
							vlan_entry_tmp |= (1 << cpu_port);
						}

						/* Set vlan table entry register */
						snprintf(vlan_id, sizeof(vlan_id), "0x%x", ((1 << 13) |	(1 << 12) | vlan_id_tmp));
						_dprintf("%s: vlan id=%s\n", __FUNCTION__, vlan_id);
						snprintf(vlan_entry, sizeof(vlan_entry), "0x%x", ((1 << 24) | (vlan_id_tmp << 12) | vlan_entry_tmp));
						_dprintf("%s: vlan entry=%s\n", __FUNCTION__, vlan_entry);

						if (set_flag) {
							eval("et", "robowr", "0x34", "0x8", vlan_entry);
							eval("et", "robowr", "0x34", "0x6", vlan_id);

							/* Create the VLAN interface */
							snprintf(vlan_id, sizeof(vlan_id), "%d", vlan_id_tmp);
							eval("vconfig", "add", interface, vlan_id);

							/* Setup ingress map (vlan->priority => skb->priority) */
							snprintf(vlan_id, sizeof(vlan_id), "vlan%d", vlan_id_tmp);
							for (i = 0; i < VLAN_NUMPRIS; i++) {
								char prio[8];

								snprintf(prio, sizeof(prio), "%d", i);
								eval("vconfig", "set_ingress_map", vlan_id, prio, prio);
							}
						}

						snprintf(vlan_id, sizeof(vlan_id), "vlan%d", vlan_id_tmp);
						set_vlan_ifnames(br_index, atoi(wlmap), subnet_name, vlan_id);
					}
					else	/* portset is 0, only for wireless */
					{
						set_vlan_ifnames(br_index, atoi(wlmap), subnet_name, NULL);
					}
				}

				br_index++;
			}
			free(nv);
		}
	}
	return;
}
#endif

#ifdef HND_ROUTER
void fc_init()
{
	int fc_enable = !nvram_get_int("fc_disable");
#if 0
	int unit;

	for (unit = WAN_UNIT_FIRST; unit < WAN_UNIT_MAX; unit++) {
		if (dualwan_unit__usbif (unit)) {
			fc_enable = 0;
			break;
		}
	}
#endif

	eval("fc", fc_enable ? "enable" : "disable");
}

void fc_fini()
{
	eval("fc", "disable");
	eval("fc", "flush");
}

void hnd_nat_ac_init(int bootup)
{
	int qos_en = nvram_match("qos_enable", "1");
	int routing_mode = is_routing_enabled();

	// traditional qos / bandwidth limter: disable fc
	nvram_set_int("fc_disable", nvram_get_int("fc_disable_force") || (routing_mode && qos_en && (nvram_get_int("qos_type") != 1)) ? 1 : 0);
	nvram_set_int("runner_disable", nvram_get_int("runner_disable_force") || (routing_mode && qos_en) ? 1 : 0);

	if (nvram_match("fc_disable", "1"))
		fc_fini();
	else if (!bootup)
		fc_init();

	if (nvram_match("runner_disable", "1"))
		eval("runner", "disable");
	else if (!bootup)
		eval("runner", "enable");
}
#endif
