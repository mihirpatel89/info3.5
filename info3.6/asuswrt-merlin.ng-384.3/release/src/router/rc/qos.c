 /*
 * Copyright 2017, ASUSTeK Inc.
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND ASUS GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */

/*
	feature implement:
	1. traditaional qos
	2. bandwdith limiter (also for guest network)
	3. facebook wifi

	NOTE:
	qos mark bit 8~31 : TrendMicro adaptive qos usage, so ASUS only can use bit 0~7 for different applications
	ex. Traditional qos / bandwidth limiter / Facebook wifi
*/

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "rc.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#ifdef RTCONFIG_FBWIFI
#include <fbwifi.h>
#endif

#ifdef HND_ROUTER
#define TCPATH "/bin/tc"
#else
#define TCPATH "/usr/sbin/tc"
#endif

/*
	DEBUG DEFINE
	QOSDBG  : normal debug
	QOSLOG  : qos logmessage, we can get more information to trace memory leakage or crash issue
*/
#define QOS_DEBUG           "/tmp/QOS_DEBUG"
#define QOSDBG(fmt,args...) \
	if(f_exists(QOS_DEBUG) > 0) { \
		_dprintf("[QOS][%s:(%d)]"fmt, __FUNCTION__, __LINE__, ##args); \
	}

#define QOS_LOG             "/tmp/QOS_LOG"
#define QOSLOG(fmt,args...) \
	if(f_exists(QOS_LOG) > 0) { \
		char info[1024]; \
		snprintf(info, sizeof(info), "echo \"[QOS][%s:(%d)]"fmt"\" >> /tmp/QOS_LOG.log", __FUNCTION__, __LINE__, ##args); \
		system(info); \
	}

static const char *qosfn = "/tmp/qos";
static const char *mangle_fn = "/tmp/mangle_rules";
#ifdef RTCONFIG_IPV6
static const char *mangle_fn_ipv6 = "/tmp/mangle_rules_ipv6";
#endif

int etable_flag = 0;
int manual_return = 0;
#define GUEST_INIT_MARKNUM 10 /*10 ~ 30 for Guest Network. */
#define INITIAL_MARKNUM    30 /*30 ~ X  for LAN . */

/*
	ip / mac / ip-range status
*/
enum {
	TYPE_UNKNOWN = -1,
	TYPE_IP = 0,
	TYPE_MAC,
	TYPE_IPRANGE
};

/*
	isIPnum : 0~255
*/
static int isIPnum(char *ip)
{
	int sum = -1;
	int finish = 0;

	if (ip == NULL || *ip == '\0')
		goto end;

	while (*ip != '\0')
	{
		/* character : 0~9 */
		if (*ip < '0' || *ip > '9')
			goto end;

		/* sum : 0~255 */
		sum = (sum * 10 + (*ip - '0'));
		if (sum > 255)
			goto end;

		ip++;
	}

	finish = 1;

end:
	if (finish == 0) sum = -1;
	return sum;
}

/*
	isSubnet : x.x.x.0/24 ~ x.x.x.0/32
*/
static int isSubnet(char *sub)
{
	int mask = safe_atoi(sub);

	if (mask < 24 || mask > 32)
		return 0;
	else
		return mask;
}

/*
	ip_range_checker:
	1. 192.168.1.*    = 192.168.1.1-254 (subnet)
	2. 192.168.1.0/24 = 192.168.1.1-254 (subnet)
	3. 192.168.1.10-20                  (short)
*/
static int ip_range_checker(char *old, char *new, int len)
{
	int ret = 0;
	int len_to_dot = 0, len_total = 0;
	int len_to_line = 0;
	char *p = NULL, *g = NULL, *buf = NULL;
	char a[4];
	char head[16];
	char host[16];
	int mask = 0;
	int mask_t = 0, mask_addr = 0;
	int host_start = 0, host_end = 0;
	struct in_addr inet_src, inet_dst;
	char *start = NULL, *end = NULL;

	memset(new, 0, len);
	memset(head, 0, sizeof(head));
	QOSLOG("old=%s", old);

	/* check fail case */
	g = buf = strdup(old);
	while ((p = strchr(g, '.')) != NULL) {
		len_to_dot = p - g;
		len_total += len_to_dot + 1;
		memset(a, 0, sizeof(a));
		strncpy(a, g, len_to_dot);

		// validate value is valid IP num
		if (isIPnum(a) == -1) {
			QOSLOG("fail case : a=%s, len_total=%d", a, len_total);
			goto end;
		}
		g += len_to_dot + 1;
	}

	/* copy head */
	strncpy(head, old, len_total);

	/* case1 : x.x.x.0/24 subnet */
	if (*g == '*') {
		snprintf(new, len, "%s1-%s254", head, head);
		ret = 1;
		goto end;
	}

	/* case2 : IP subnetting */
	p = NULL;
	p = strchr(g, '/');
	if (p != NULL) {
		len_to_line = p - g;
		memset(a, 0, sizeof(a));
		strncpy(a, g, len_to_line);

		// validate value is valid IP num
		if (isIPnum(a) == -1) {
			QOSLOG("case 2: p+=%s, g=%s, a=%s", p+1, g, a);
			goto end;
		}

		// get mask and mask_addr
		g += len_to_dot + 1;
		mask = isSubnet(p+1);
		if (mask == 0) goto end;
		snprintf(host, sizeof(host), "%s%s", head, a);
		mask_t = ntohl(inet_addr(host));
		mask_addr = mask_t & (0xffffffff & (0xffffffff << (32 - mask)));
		host_start = mask_addr + 1;
		host_end = mask_addr + (0xffffffff & ~(0xffffffff << (32 - mask))) - 1;
		inet_src.s_addr = htonl(host_start);
		inet_dst.s_addr = htonl(host_end);

		QOSLOG("case 2: mask=%d, mask_addr=%x, host_start=%x, host_end=%x", mask, mask_addr, host_start, host_end);

		start = inet_ntoa(inet_src);
		strncpy(new, start, strlen(start));
		strncpy(new + strlen(start), "-", 1);
		end = inet_ntoa(inet_dst);
		strncpy(new + strlen(start) + 1, end, strlen(end));
		ret = 1;
		goto end;
	}

	/* case3 : find minus in tail */
	p = NULL;
	p = strchr(g, '-');
	if (p != NULL) {
		len_to_line = p - g;
		memset(a, 0, sizeof(a));
		strncpy(a, g, len_to_line);

		// validate value is valid IP num
		if (isIPnum(a) == -1) {
			QOSLOG("case 3: p+=%s, g=%s, a=%s", p+1, g, a);
			goto end;
		}

		snprintf(new, len, "%s%s-%s%s", head, a, head, (p+1));
		ret = 1;
		goto end;
	}

end:
	if (buf) free(buf);

	QOSLOG("new=%s", new);
	return ret;
}

/*
	address_format_checker:
	1. unknown
	2. ip
	3. mac
	4. ip-range
*/
static void address_format_checker(int *type, char *old, char *new, int len)
{
	char *g = NULL, *buf = NULL;
	int s[6]; // strip mac address
	int is_ip = 0;
	int is_mac = 0;
	int is_range = 0;

	memset(s, 0, sizeof(s));

	// mac format
	g = buf = strdup(old);
	if (sscanf(g, "%02X:%02X:%02X:%02X:%02X:%02X",&s[0],&s[1],&s[2],&s[3],&s[4],&s[5]) == 6) {
		is_mac = 1;
		goto end;
	}

	// ip format
	g = buf;
	if (illegal_ipv4_address(g) == 0) {
		is_ip = 1;
		goto end;
	}

	// ip-range format
	g = buf;
	if (ip_range_checker(g, new, len) == 1) {
		is_range = 1;
		goto end;
	}

end:
	if (buf) free(buf);
	if (is_ip == 1) {
		*type = TYPE_IP;
		strncpy(new, old, len);
	}
	else if (is_mac == 1) {
		*type = TYPE_MAC;
		strncpy(new, old, len);
	}
	else if (is_range == 1) {
		*type = TYPE_IPRANGE;
	}
	else {
		*type = TYPE_UNKNOWN;
		strncpy(new, "", len);
	}
	QOSLOG("is_ip=%d, is_mac=%d, is_range=%d, type=%d, new=%s", is_ip, is_mac, is_range, *type, new);
}

static unsigned calc(unsigned bw, unsigned pct)
{
	unsigned n = ((unsigned long)bw * pct) / 100;
	return (n < 2) ? 2 : n;
}

void del_EbtablesRules(void)
{
	/* Flush all rules in nat table of ebtable*/
	eval("ebtables", "-t", "nat", "-F");
	etable_flag = 0;
}

#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan

#ifdef RTCONFIG_FBWIFI
static void set_fbwifi_mark(void)
{
	int band, j, max_mssid;
	char mark[16], inv_mask[16];	/* for ebtables mark, inverse mask */
	char wl_ifname[IFNAMSIZ] = "", *wl_if = wl_ifname;
	char *fbwifi_iface[3] = { "fbwifi_2g", "fbwifi_5g", "fbwifi_5g_2" };

	if (!nvram_match("fbwifi_enable","on"))
		return;

	snprintf(mark, sizeof(mark), "0x%x", FBWIFI_MARK_SET(1));
	snprintf(inv_mask, sizeof(inv_mask), "0x%x", FBWIFI_MARK_INV_MASK);
	for (band = 0; band < ARRAYSIZE(fbwifi_iface); ++band) {
#if !defined(RTAC3200)
		/* Skip band 2, 5G-2, if DUT not support 2-nd 5G band. */
		if (band == 2)
			continue;
#endif

		if (nvram_match(fbwifi_iface[band], "off"))
			continue;

		max_mssid = num_of_mssid_support(band);
		for (j = 1; j <= max_mssid; ++j) {
			get_wlxy_ifname(band, j, wl_if);
			eval("ebtables", "-D", "INPUT", "-i", wl_if, "-j", "mark", "--mark-and", inv_mask, "--mark-target", "CONTINUE");
			eval("ebtables", "-D", "INPUT", "-i", wl_if, "-j", "mark", "--mark-or", mark, "--mark-target", "ACCEPT");
		}

		if (sscanf(nvram_safe_get(fbwifi_iface[band]), "wl%*d.%d", &j) != 1)
			continue;

		get_wlxy_ifname(band, j, wl_if);
		eval("ebtables", "-A", "INPUT", "-i", wl_if, "-j", "mark", "--mark-and", inv_mask, "--mark-target", "CONTINUE");
		eval("ebtables", "-A", "INPUT", "-i", wl_if, "-j", "mark", "--mark-or", mark, "--mark-target", "ACCEPT");
	}
}
#endif

void add_EbtablesRules(void)
{
	if(etable_flag == 1) return;
	char *nv, *p, *g;
	nv = g = strdup(nvram_safe_get("wl_ifnames"));
	if(nv){
		while ((p = strsep(&g, " ")) != NULL){
			QOSLOG("p=%s", p);
			eval("ebtables", "-t", "nat", "-A", "PREROUTING", "-i", p, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
			eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", p, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
		}
		free(nv);
	}

	// for MultiSSID
	int UnitNum = 2; 	// wl0.x, wl1.x
	int GuestNum = 3;	// wlx.0, wlx.1, wlx.2
	char mssid_if[32];
	char mssid_enable[32];
	int i, j;
	for( i = 0; i < UnitNum; i++){
		for( j = 1; j <= GuestNum; j++ ){
			snprintf(mssid_if, sizeof(mssid_if), "wl%d.%d", i, j);
			snprintf(mssid_enable, sizeof(mssid_enable), "%s_bss_enabled", mssid_if);
			QOSLOG("mssid_enable=%s", mssid_enable);
			if(!strcmp(nvram_safe_get(mssid_enable), "1")){
				eval("ebtables", "-t", "nat", "-A", "PREROUTING", "-i", mssid_if, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
				eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", mssid_if, "-j", "mark", "--mark-or", "6", "--mark-target", "ACCEPT");
			}
		}
	}

#ifdef RTCONFIG_FBWIFI
	if(sw_mode() == SW_MODE_ROUTER){
		set_fbwifi_mark();
	}
#endif

	etable_flag = 1;
}
#endif

void del_iQosRules(void)
{
#ifdef CLS_ACT
	eval("ip", "link", "set", "imq0", "down");
#endif

	del_EbtablesRules(); // flush ebtables nat table

	/* Flush all rules in mangle table */
	eval("iptables", "-t", "mangle", "-F");
#ifdef RTCONFIG_IPV6
	eval("ip6tables", "-t", "mangle", "-F");
#endif

#ifdef RTCONFIG_BCMARM
	remove_codel_patch();
#endif
}

static int add_qos_rules(char *pcWANIF)
{
	FILE *fn;
#ifdef RTCONFIG_IPV6
	FILE *fn_ipv6 = NULL;
#endif
	char *buf;
	char *g;
	char *p;
	char *desc, *addr, *port, *prio, *transferred, *proto;
	int class_num;
	int down_class_num=6; 	// for download class_num = 0x6 / 0x106
	int i, inuse;
	char q_inuse[32]; 	// for inuse
	char dport[256], saddr_1[192], proto_1[8], proto_2[8],conn[256], end[256], end2[256];
	//int method;
	int gum;
	int sticky_enable;
	const char *chain;
	int v4v6_ok;

	if((fn = fopen(mangle_fn, "w")) == NULL) return -2;
#ifdef RTCONFIG_IPV6
	if(ipv6_enabled() && (fn_ipv6 = fopen(mangle_fn_ipv6, "w")) == NULL){
		fclose(fn);
		return -3;
	}
#endif

	inuse = sticky_enable = 0;

	/* action and manual_return */
	char *action = NULL;
	int model = get_model();

	switch (model) {
		case MODEL_RTAC56S:
		case MODEL_RTAC68U:
		case MODEL_RTAC87U:
		case MODEL_RTAC3200:
		case MODEL_DSLAC68U:
		case MODEL_RTAC88U:
		case MODEL_RTAC5300:
		case MODEL_RTAC3100:
		case MODEL_GTAC5300:
		case MODEL_RTAC86U:
		case MODEL_RTAC1200G:
		case MODEL_RTAC1200GP:
#if defined(RTCONFIG_LANTIQ)
		case MODEL_BLUECAVE:
#endif
			action = "--set-mark";
			manual_return = 1;
			break;
		default:
			action = "--set-return";
			manual_return = 0;
			break;
	}

	if(nvram_match("qos_sticky", "0"))
		sticky_enable = 1;

	del_iQosRules(); // flush all rules in mangle table

#ifdef CLS_ACT
	eval("ip", "link", "set", "imq0", "up");
#endif
	QOSDBG("[qos] iptables START\n");

	fprintf(fn,
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		":QOSO - [0:0]\n"
		"-A QOSO -j CONNMARK --restore-mark --mask 0x7\n"
		"-A QOSO -m connmark ! --mark 0/0xff00 -j RETURN\n"
		);
#ifdef RTCONFIG_IPV6
	if (fn_ipv6 && ipv6_enabled())
	fprintf(fn_ipv6,
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		":QOSO - [0:0]\n"
		"-A QOSO -j CONNMARK --restore-mark --mask 0x7\n"
		"-A QOSO -m connmark ! --mark 0/0xff00 -j RETURN\n"
		);
#endif
	g = buf = strdup(nvram_safe_get("qos_rulelist"));
	while (g) {

		/* ASUSWRT
		qos_rulelist :
			desc>addr>port>proto>transferred>prio

			addr  : (source) IP or MAC or IP-range
			port  : dest port
			proto : tcp, udp, tcp/udp, any , (icmp, igmp)
			transferred : min:max
			prio  : 0-4, 0 is the highest
  		*/

		if ((p = strsep(&g, "<")) == NULL) break;
		if((vstrsep(p, ">", &desc, &addr, &port, &proto, &transferred, &prio)) != 6) continue;
		class_num = safe_atoi(prio);
		if ((class_num < 0) || (class_num > 4)) continue;

		i = 1 << class_num;
		++class_num;

		if ((inuse & i) == 0) {
			inuse |= i;
			QOSDBG("[qos] iptable creates, inuse=%d\n", inuse);
		}

		v4v6_ok = IPT_V4;
#ifdef RTCONFIG_IPV6
		if (fn_ipv6 && ipv6_enabled())
			v4v6_ok |= IPT_V6;
#endif

		/* Beginning of the Rule */
		/*
 			if transferred != NULL, class_num must bt 0x1~0x6, not 0x101~0x106
			0x1~0x6		: keep tracing this connection.
			0x101~0x106 	: connection will be considered as marked connection, won't detect again.
		*/
		gum = 0;
		class_num |= gum;
		down_class_num |= gum;	// for download

		chain = "QOSO";		// chain name
		snprintf(end , sizeof(end), " -j CONNMARK %s 0x%x/0x7\n", action, class_num);	// CONNMARK string
		snprintf(end2, sizeof(end2), " -j RETURN\n");

		/*************************************************/
		/*                        addr                   */
		/*           src mac or src ip or IP range       */
		/*************************************************/
		char addr_new[40];
		int addr_type;
		memset(addr_new, 0, sizeof(addr_new));

		if(strcmp(addr, "")) {
			/* if addr != "", it needs to check the addr_type */
			address_format_checker(&addr_type, addr, addr_new, sizeof(addr_new));

			if (addr_type == TYPE_IP) {
				snprintf(saddr_1, sizeof(saddr_1), "-s %s", addr_new);
			}
			else if (addr_type == TYPE_MAC) {
				snprintf(saddr_1, sizeof(saddr_1), "-m mac --mac-source %s", addr_new);
			}
			else if (addr_type == TYPE_IPRANGE) {
				snprintf(saddr_1, sizeof(saddr_1), "-m iprange --src-range %s", addr_new);
			}
			else if (addr_type == TYPE_UNKNOWN) {
				QOSDBG("[qos] addr is TYPE_UKNOWN!\n");
				continue;
			}
			QOSLOG("[qos] addr_type=%d, saddr_1=%s", addr_type, saddr_1);
		}
		else {
			strncpy(saddr_1, "", sizeof(saddr_1));
		}

		/*************************************************/
		/*                      port                     */
		/*            single port or multi-ports         */
		/*************************************************/
		if(strcmp(port, "") == 0 ) {
			strncpy(dport, "", sizeof(dport));
		}
		else{
			/* note : max number of multiple port in iptables is 15 */
			snprintf(dport, sizeof(dport), "-m multiport --dport %s", port);
		}
		QOSLOG("[qos] port=%s, dport=%s", port, dport);

		/*************************************************/
		/*                   transferred                 */
		/*   --connbytes min:max                         */
 		/*   --connbytes-dir (original/reply/both)       */
 		/*   --connbytes-mode (packets/bytes/avgpkt)     */
		/*************************************************/
		char tmp[40];
		char *tmp_trans, *q_min, *q_max;
		long min = 0, max =0;

		snprintf(tmp, sizeof(tmp), "%s", transferred);
		tmp_trans = tmp;
		q_min = strsep(&tmp_trans, "~");
		q_max = tmp_trans;

		if (strcmp(transferred,"") == 0){
			snprintf(conn, sizeof(conn), "%s", "");
		}
		else{
			snprintf(tmp, sizeof(tmp), "%s", q_min);
			min = atol(tmp);

			if(strcmp(q_max,"") == 0) // q_max == NULL
				snprintf(conn, sizeof(conn), "-m connbytes --connbytes %ld:%s --connbytes-dir both --connbytes-mode bytes", min*1024, q_max);
			else{// q_max != NULL
				snprintf(tmp, sizeof(tmp), "%s", q_max);
				max = atol(tmp);
				snprintf(conn, sizeof(conn), "-m connbytes --connbytes %ld:%ld --connbytes-dir both --connbytes-mode bytes", min*1024, max*1024-1);
			}
		}
		QOSLOG("[qos] tmp=%s, transferred=%s, min=%ld, max=%ld, q_max=%s, conn=%s", tmp, transferred, min*1024, max*1024-1, q_max, conn);

		/*************************************************/
		/*                      proto                    */
		/*        tcp, udp, tcp/udp, any, (icmp, igmp)   */
		/*************************************************/
		memset(proto_1, 0, sizeof(proto_1));
		memset(proto_2, 0, sizeof(proto_2));
		if(!strcmp(proto, "tcp"))
		{
			snprintf(proto_1, sizeof(proto_1), "-p tcp");
			snprintf(proto_2, sizeof(proto_2), "NO");
		}
		else if(!strcmp(proto, "udp"))
		{
			snprintf(proto_1, sizeof(proto_1), "-p udp");
			snprintf(proto_2, sizeof(proto_2), "NO");
		}
		else if(!strcmp(proto, "any"))
		{
			snprintf(proto_1, sizeof(proto_1), "%s", "");
			snprintf(proto_2, sizeof(proto_2), "NO");
		}
		else if(!strcmp(proto, "tcp/udp"))
		{
			snprintf(proto_1, sizeof(proto_1), "-p tcp");
			snprintf(proto_2, sizeof(proto_1), "-p udp");
		}
		else{
			snprintf(proto_1, sizeof(proto_1), "NO");
			snprintf(proto_2, sizeof(proto_2), "NO");
		}
		QOSLOG("[qos] proto_1=%s, proto_2=%s, proto=%s", proto_1, proto_2, proto);

		/*******************************************************************/
		/*                                                                 */
		/*  build final rule for check proto_1, proto_2, saddr_1           */
		/*                                                                 */
		/*******************************************************************/
		// step1. check proto != "NO"
		// step2. if proto = any, no proto / dport
		// step3. check saddr for ip-range; saddr_1 could be empty, dport only

		if (v4v6_ok & IPT_V4){
			// step1. check proto != "NO"
			if(strcmp(proto_1, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_1, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
						fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end2);
				}
				else{
						fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end2);
				}
			}

			// step1. check proto != "NO"
			if(strcmp(proto_2, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_2, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
						fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end2);
				}
				else{
						fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn, "-A %s %s %s %s", chain, saddr_1, conn, end2);
				}
			}
		}

#ifdef RTCONFIG_IPV6
		if (fn_ipv6 && ipv6_enabled() && (v4v6_ok & IPT_V6)){
			// step1. check proto != "NO"
			if(strcmp(proto_1, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_1, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
						fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_1, dport, saddr_1, conn, end2);
				}
				else{
						fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end2);
				}
			}

			// step1. check proto != "NO"
			if(strcmp(proto_2, "NO")){
				// step2. if proto = any, no proto / dport
				if(strcmp(proto_2, "")){
					// step3. check saddr for ip-range;saddr_1 could be empty, dport only
						fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn_ipv6, "-A %s %s %s %s %s %s", chain, proto_2, dport, saddr_1, conn, end2);
				}
				else{
						fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end);
						if(manual_return)
						fprintf(fn_ipv6, "-A %s %s %s %s", chain, saddr_1, conn, end2);
				}
			}
		}
#endif
	}
	free(buf);

	/* lan_addr for iptables use (LAN download) */
	char *a, *b, *c, *d;
	char lan_addr[20];
	g = buf = strdup(nvram_safe_get("lan_ipaddr"));
	if((vstrsep(g, ".", &a, &b, &c, &d)) != 4){
		QOSDBG("[qos] lan_ipaddr doesn't exist!!\n");
	}
	else{
		snprintf(lan_addr, sizeof(lan_addr), "%s.%s.%s.0/24", a, b, c);
		QOSDBG("[qos] lan_addr=%s\n", lan_addr);
	}
	free(buf);

	/* The default class */
	i = nvram_get_int("qos_default");
	if ((i < 0) || (i > 4)) i = 3;  // "lowest"
	class_num = i + 1;

#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	if(strncmp(pcWANIF, "ppp", 3)==0){
		// ppp related interface doesn't need physdev
		// do nothing
	}
	else{
		/* for WLAN to LAN bridge packet */
		// ebtables : identify bridge packet
		add_EbtablesRules();

		// for multicast
		fprintf(fn, "-A QOSO -d 224.0.0.0/4 -j CONNMARK %s 0x%x/0x7\n", action, down_class_num);
		if(manual_return)
			fprintf(fn , "-A QOSO -d 224.0.0.0/4 -j RETURN\n");
		// for download (LAN or wireless)
		fprintf(fn, "-A QOSO -d %s -j CONNMARK %s 0x%x/0x7\n", lan_addr, action, down_class_num);
		if(manual_return)
			fprintf(fn , "-A QOSO -d %s -j RETURN\n", lan_addr);
/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
		// for WLAN to LAN bridge issue
		fprintf(fn, "-A POSTROUTING -d %s -m physdev --physdev-is-in -j CONNMARK --set-return 0x6/0x7\n", lan_addr);
*/
		// for download, interface br0
		fprintf(fn, "-A POSTROUTING -o br0 -j QOSO\n");
	}
#endif
		fprintf(fn,
			"-A QOSO -j CONNMARK %s 0x%x/0x7\n"
			"-A FORWARD -o %s -j QOSO\n"
			"-A OUTPUT -o %s -j QOSO\n",
				action, class_num, pcWANIF, pcWANIF);
		if(manual_return)
			fprintf(fn , "-A QOSO -j RETURN\n");

#ifdef RTCONFIG_IPV6
	if (fn_ipv6 && ipv6_enabled() && *wan6face) {
#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
		if(strncmp(wan6face, "ppp", 3)==0){
			// ppp related interface doesn't need physdev
			// do nothing
		}
		else{
			/* for WLAN to LAN bridge packet */
			// ebtables : identify bridge packet
			add_EbtablesRules();

			// for multicast
			fprintf(fn_ipv6, "-A QOSO -d 224.0.0.0/4 -j CONNMARK %s 0x%x/0x7\n", action, down_class_num);
			if(manual_return)
				fprintf(fn_ipv6, "-A QOSO -d 224.0.0.0/4 -j RETURN\n");
			// for download (LAN or wireless)
			fprintf(fn_ipv6, "-A QOSO -d %s -j CONNMARK %s 0x%x/0x7\n", lan_addr, action, down_class_num);
			if(manual_return)
				fprintf(fn_ipv6, "-A QOSO -d %s -j RETURN\n", lan_addr);
/* Requires bridge netfilter, but slows down and breaks EMF/IGS IGMP IPTV Snooping
			// for WLAN to LAN bridge issue
			fprintf(fn_ipv6, "-A POSTROUTING -d %s -m physdev --physdev-is-in -j CONNMARK --set-return 0x6/0x7\n", lan_addr);
*/
			// for download, interface br0
			fprintf(fn_ipv6, "-A POSTROUTING -o br0 -j QOSO\n");
		}
#endif
		fprintf(fn_ipv6,
			"-A QOSO -j CONNMARK %s 0x%x/0x7\n"
			"-A FORWARD -o %s -j QOSO\n"
			"-A OUTPUT -o %s -j QOSO\n",
				action, class_num, wan6face, wan6face);
		if(manual_return)
			fprintf(fn_ipv6, "-A QOSO -j RETURN\n");
	}
#endif

	inuse |= (1 << i) | 1;  // default and highest are always built
	snprintf(q_inuse, sizeof(q_inuse), "%d", inuse);
	nvram_set("qos_inuse", q_inuse);
	QOSDBG("[qos] qos_inuse=%d\n", inuse);

	/* Ingress rules */
	g = buf = strdup(nvram_safe_get("qos_irates"));
	for (i = 0; i < 10; ++i) {
		if ((!g) || ((p = strsep(&g, ",")) == NULL)) continue;
		if ((inuse & (1 << i)) == 0) continue;
		if (safe_atoi(p) > 0) {
			fprintf(fn, "-A PREROUTING -i %s -j CONNMARK --restore-mark --mask 0x7\n", pcWANIF);
#ifdef CLS_ACT
			fprintf(fn, "-A PREROUTING -i %s -j IMQ --todev 0\n", pcWANIF);
#endif
#ifdef RTCONFIG_IPV6
			if (fn_ipv6 && ipv6_enabled() && *wan6face) {
				fprintf(fn_ipv6, "-A PREROUTING -i %s -j CONNMARK --restore-mark --mask 0x7\n", wan6face);
#ifdef CLS_ACT
				fprintf(fn_ipv6, "-A PREROUTING -i %s -j IMQ --todev 0\n", wan6face);
#endif
			}
#endif
			break;
		}
	}
	free(buf);

	fprintf(fn, "COMMIT\n");
	fclose(fn);

	chmod(mangle_fn, 0700);
	eval("iptables-restore", (char*)mangle_fn);
#ifdef RTCONFIG_IPV6
	if (fn_ipv6 && ipv6_enabled())
	{
		fprintf(fn_ipv6, "COMMIT\n");
		fclose(fn_ipv6);
		chmod(mangle_fn_ipv6, 0700);
//		eval("ip6tables-restore", (char*)mangle_fn_ipv6);
	}
#endif
	run_custom_script("qos-start", "rules");
	QOSDBG("[qos] iptables DONE!\n");

	return 0;
}


/*******************************************************************/
// The definations of all partations
// eth0 : WAN
// 1:1  : upload
// 1:2  : download   (1000000Kbits)
// 1:10 : highest
// 1:20 : high
// 1:30 : middle
// 1:40 : low        (default)
// 1:50 : lowest
// 1:60 : ALL Download (WAN to LAN and LAN to LAN) (1000000kbits)
/*******************************************************************/

/* Tc */
static int start_tqos(void)
{
	int i;
	char *buf, *g, *p;
	unsigned int rate;
	unsigned int ceil;
	unsigned int ibw, obw, bw;
	unsigned int mtu;
	FILE *f;
	int x;
	int inuse;
	char s[256];
	int first;
	char burst_root[32];
	char burst_leaf[32];
#ifdef CONFIG_BCMWL5
	char *protocol="802.1q";
#endif
	char *qsched;
	int overhead = 0;
	char overheadstr[sizeof("overhead 128 linklayer ethernet")];

	// judge interface by get_wan_ifname
	// add Qos iptable rules in mangle table,
	// move it to firewall - mangle_setting
	// add_iQosRules(get_wan_ifname(wan_primary_ifunit())); // iptables start

	ibw = strtoul(nvram_safe_get("qos_ibw"), NULL, 10);
	obw = strtoul(nvram_safe_get("qos_obw"), NULL, 10);
	if(ibw==0||obw==0) return -1;

	if((f = fopen(qosfn, "w")) == NULL) return -2;

	QOSDBG("[qos] tc START!\n");

	/* qos_burst */
	i = nvram_get_int("qos_burst0");
	if(i > 0) snprintf(burst_root, sizeof(burst_root), "burst %dk", i);
		else burst_root[0] = 0;
	i = nvram_get_int("qos_burst1");

	if(i > 0) snprintf(burst_leaf, sizeof(burst_leaf), "burst %dk", i);
		else burst_leaf[0] = 0;

	/* Egress OBW  -- set the HTB shaper (Classful Qdisc)
	* the BW is set here for each class
	*/

	mtu = strtoul(nvram_safe_get("wan_mtu"), NULL, 10);
	bw = obw;

#ifdef RTCONFIG_BCMARM
		switch(nvram_get_int("qos_sched")){
			case 1:
				qsched = "codel";
				break;
			case 2:
				if (bw < 51200)
					qsched = "fq_codel quantum 300 noecn";
				else
					qsched = "fq_codel noecn";
				break;
			default:
				qsched = "sfq perturb 10";
				break;
		}

		overhead = nvram_get_int("qos_overhead");
#else
		qsched = "sfq perturb 10";
#endif

		if (overhead > 0)
			snprintf(overheadstr, sizeof(overheadstr),"overhead %d %s",
			         overhead, nvram_get_int("qos_atm") ? "linklayer atm" : "linklayer ethernet");
		else
			strcpy(overheadstr, "");

	/* WAN */
	fprintf(f,
		"#!/bin/sh\n"
		"#LAN/WAN\n"
		"I=%s\n"
		"SFQ=\"sfq perturb 10\"\n"
		"TQA=\"tc qdisc add dev $I\"\n"
		"TCA=\"tc class add dev $I\"\n"
		"TFA=\"tc filter add dev $I\"\n"
#ifdef CLS_ACT
		"DLIF=imq0\n"
		"TQADL=\"tc qdisc add dev $DLIF\"\n"
		"TCADL=\"tc class add dev $DLIF\"\n"
		"TFADL=\"tc filter add dev $DLIF\"\n"
#endif
		"case \"$1\" in\n"
		"start)\n"
		"#LAN/WAN\n"
		"\ttc qdisc del dev $I root 2>/dev/null\n"
		"\t$TQA root handle 1: htb default %u\n"
#ifdef CLS_ACT
		"\ttc qdisc del dev $DLIF root 2>/dev/null\n"
		"\t$TQADL root handle 2: htb default %u\n"
#endif
		"# upload 1:1\n"
		"\t$TCA parent 1: classid 1:1 htb rate %ukbit ceil %ukbit %s %s\n" ,
			get_wan_ifname(wan_primary_ifunit()), // judge WAN interface
			(nvram_get_int("qos_default") + 1) * 10,
#ifdef CLS_ACT
			(nvram_get_int("qos_default") + 1) * 10,
#endif
			bw, bw, burst_root, overheadstr);

	/* LAN protocol: 802.1q */
#ifdef CONFIG_BCMWL5 // TODO: it is only for the case, eth0 as wan, vlanx as lan
	protocol = "802.1q";
	fprintf(f,
		"# download 1:2\n"
		"\t$TCA parent 1: classid 1:2 htb rate 1000000kbit ceil 1000000kbit burst 10000 cburst 10000\n"
		"# 1:60 ALL Download for BCM\n"
		"\t$TCA parent 1:2 classid 1:60 htb rate 1000000kbit ceil 1000000kbit burst 10000 cburst 10000 prio 6\n"
		"\t$TQA parent 1:60 handle 60: pfifo\n"
		"\t$TFA parent 1: prio 6 protocol %s handle 6 fw flowid 1:60\n", protocol
		);
#endif

	inuse = nvram_get_int("qos_inuse");

	g = buf = strdup(nvram_safe_get("qos_orates"));
	for (i = 0; i < 5; ++i) { // 0~4 , 0:highest, 4:lowest

		if ((!g) || ((p = strsep(&g, ",")) == NULL)) break;

		if ((inuse & (1 << i)) == 0){
			QOSDBG("[qos] egress %d doesn't create, inuse=%d\n", i, inuse);
			continue;
		}
		else {
			QOSDBG("[qos] egress %d creates\n", i);
		}

		if ((sscanf(p, "%u-%u", &rate, &ceil) != 2) || (rate < 1)) continue;

		if (ceil > 0) snprintf(s, sizeof(s), "ceil %ukbit ", calc(bw, ceil));
			else s[0] = 0;
		x = (i + 1) * 10;

		fprintf(f,
			"# egress %d: %u-%u%%\n"
			"\t$TCA parent 1:1 classid 1:%d htb rate %ukbit %s %s prio %d quantum %u %s\n"
			"\t$TQA parent 1:%d handle %d: %s\n"
			"\t$TFA parent 1: prio %d protocol ip handle %d fw flowid 1:%d\n",
				i, rate, ceil,
				x, calc(bw, rate), s, burst_leaf, (i >= 6) ? 7 : (i + 1), mtu, overheadstr,
				x, x, qsched,
				x, i + 1, x);
	}
	free(buf);

	/*
		10000 = ACK
		00100 = RST
		00010 = SYN
		00001 = FIN
	*/

	if (nvram_match("qos_ack", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 14 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x10 0xff at 33 "			// ACK only
			"flowid 1:10\n");
	}
	if (nvram_match("qos_syn", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 15 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x02 0x02 at 33 "			// SYN,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_fin", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 17 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x01 0x01 at 33 "			// FIN,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_rst", "on")) {
		fprintf(f,
			"\n"
			"\t$TFA parent 1: prio 19 protocol ip u32 "
			"match ip protocol 6 0xff "			// TCP
			"match u8 0x05 0x0f at 0 "			// IP header length
			"match u16 0x0000 0xffc0 at 2 "			// total length (0-63)
			"match u8 0x04 0x04 at 33 "			// RST,*
			"flowid 1:10\n");
	}
	if (nvram_match("qos_icmp", "on")) {
		fputs("\n\t$TFA parent 1: prio 13 protocol ip u32 match ip protocol 1 0xff flowid 1:10\n", f);
	}

	// ingress
	first = 1;
	bw = ibw;

	if (bw > 0) {
		g = buf = strdup(nvram_safe_get("qos_irates"));
		for (i = 0; i < 5; ++i) { // 0~4 , 0:highest, 4:lowest
			if ((!g) || ((p = strsep(&g, ",")) == NULL)) break;
			if ((inuse & (1 << i)) == 0) continue;
			if ((rate = safe_atoi(p)) < 1) continue;	// 0 = off

			if (first) {
				first = 0;
				fprintf(f,
					"\n"
#if !defined(CLS_ACT)
					"\ttc qdisc del dev $I ingress 2>/dev/null\n"
					"\t$TQA handle ffff: ingress\n"
#endif
					);
			}

			// rate in kb/s
			unsigned int u = calc(bw, rate);

			// burst rate
			unsigned int v = u / 2;
			if (v < 50) v = 50;

#ifdef CLS_ACT
			x = (i + 1) * 10;
			fprintf(f,
				"# ingress %d: %u%%\n"
				"\t$TCADL parent 2:1 classid 2:%d htb rate %ukbit %s prio %d quantum %u %s\n"
				"\t$TQADL parent 2:%d handle %d: %s\n"
				"\t$TFADL parent 2: prio %d protocol ip handle %d fw flowid 2:%d\n",
					i, rate,
					x, calc(bw, rate), burst_leaf, (i >= 6) ? 7 : (i + 1), mtu, overheadstr,
					x, x, qsched,
					x, i + 1, x);
#else
			x = i + 1;
			fprintf(f,
				"# ingress %d: %u%%\n"
				"\t$TFA parent ffff: prio %d protocol ip handle %d"
					" fw police rate %ukbit burst %ukbit drop flowid ffff:%d\n",
					i, rate, x, x, u, v, x);
#endif
		}
		free(buf);
	}

	fputs(
		"\t;;\n"
		"stop)\n"
		"\ttc qdisc del dev $I root 2>/dev/null\n"
#ifdef CLS_ACT
		"\ttc qdisc del dev $DLIF root 2>/dev/null\n"
#else
		"\ttc qdisc del dev $I ingress 2>/dev/null\n"
#endif
		"\t;;\n"
		"*)\n"
		"\t#---------- Upload ----------\n"
		"\ttc -s -d class ls dev $I\n"
		"\ttc -s -d qdisc ls dev $I\n"
		"\techo\n"
#ifdef CLS_ACT
		"\t#--------- Download ---------\n"
		"\ttc -s -d class ls dev $DLIF\n"
		"\ttc -s -d qdisc ls dev $DLIF\n"
		"\techo\n"
#endif
		"esac\n",
		f);

	fclose(f);
	chmod(qosfn, 0700);
	run_custom_script("qos-start", "init");
	eval((char *)qosfn, "start");
	QOSDBG("[qos] tc done!\n");

	return 0;
}


void stop_iQos(void)
{
	eval((char *)qosfn, "stop");
}

static int add_bandwidth_limiter_rules(char *pcWANIF)
{
	FILE *fn = NULL;
	char *buf, *g, *p;
	char *enable, *addr, *dlc, *upc, *prio;
	char lan_addr[32];
	char addr_new[40], wl_ifname[IFNAMSIZ];
	int addr_type;
	char *action = NULL;

	if ((fn = fopen(mangle_fn, "w")) == NULL) return -2;
	del_iQosRules(); // flush all rules in mangle table

	switch (get_model()){
		case MODEL_DSLN55U:
		case MODEL_RTN13U:
		case MODEL_RTN56U:
			action = "CONNMARK --set-return";
			manual_return = 0;
			break;
		default:
			action = "MARK --set-mark";
			manual_return = 1;
			break;
	}

	/* ASUSWRT
	qos_bw_rulelist :
		enable>addr>DL-Ceil>UL-Ceil>prio
		enable : enable or disable this rule
		addr : (source) IP or MAC or IP-range or wireless interface(wl0.1, wl0.2, etc.)
		DL-Ceil : the max download bandwidth
		UL-Ceil : the max upload bandwidth
		prio : priority for client
	*/

	snprintf(lan_addr, sizeof(lan_addr), "%s/%s", nvram_safe_get("lan_ipaddr"), nvram_safe_get("lan_netmask"));

	fprintf(fn,
		"*mangle\n"
		":PREROUTING ACCEPT [0:0]\n"
		":OUTPUT ACCEPT [0:0]\n"
		);

	// access router : mark 9
	fprintf(fn,
		"-A POSTROUTING -s %s -d %s -j %s 9\n"
		"-A PREROUTING -s %s -d %s -j %s 9\n"
		, nvram_safe_get("lan_ipaddr"), lan_addr, action
		, lan_addr, nvram_safe_get("lan_ipaddr"), action
		);
	
	if(manual_return){
	fprintf(fn,
		"-A POSTROUTING -s %s -d %s -j RETURN\n"
		"-A PREROUTING -s %s -d %s -j RETURN\n"
		, nvram_safe_get("lan_ipaddr"), lan_addr
		, lan_addr, nvram_safe_get("lan_ipaddr")
		);
	}

	g = buf = strdup(nvram_safe_get("qos_bw_rulelist"));
	while (g) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if ((vstrsep(p, ">", &enable, &addr, &dlc, &upc, &prio)) != 5) continue;
		if (!strcmp(enable, "0")) continue;
		memset(addr_new, 0, sizeof(addr_new));
		address_format_checker(&addr_type, addr, addr_new, sizeof(addr_new));
		QOSDBG("[BWLIT] addr_type=%d, addr=%s, add_new=%s, lan_addr=%s\n", addr_type, addr, addr_new, lan_addr);

		if (addr_type == TYPE_IP){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -d %s -j %s %d\n"
				"-A PREROUTING -s %s ! -d %s -j %s %d\n"
				, lan_addr, addr_new, action, safe_atoi(prio)+INITIAL_MARKNUM
				, addr_new, lan_addr, action, safe_atoi(prio)+INITIAL_MARKNUM
				);
			if(manual_return){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -d %s -j RETURN\n"
				"-A PREROUTING -s %s ! -d %s -j RETURN\n"
				, lan_addr, addr_new, addr_new, lan_addr
				);
			}
		}
		else if (addr_type == TYPE_MAC){
			fprintf(fn,
				"-A PREROUTING -m mac --mac-source %s ! -d %s  -j %s %d\n"
				, addr_new, lan_addr, action, safe_atoi(prio)+INITIAL_MARKNUM
				);
			if(manual_return){
			fprintf(fn,
				"-A PREROUTING -m mac --mac-source %s ! -d %s  -j RETURN\n"
				, addr_new, lan_addr
				);
			}
		}
		else if (addr_type == TYPE_IPRANGE){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -m iprange --dst-range %s -j %s %d\n"
				"-A PREROUTING -m iprange --src-range %s ! -d %s -j %s %d\n"
				, lan_addr, addr_new, action, safe_atoi(prio)+INITIAL_MARKNUM
				, addr_new, lan_addr, action, safe_atoi(prio)+INITIAL_MARKNUM
				);
			if(manual_return){
			fprintf(fn,
				"-A POSTROUTING ! -s %s -m iprange --dst-range %s -j RETURN\n"
				"-A PREROUTING -m iprange --src-range %s ! -d %s -j RETURN\n"
				, lan_addr, addr_new, addr_new, lan_addr
				);
			}
		}
	}
	free(buf);

	fprintf(fn, "COMMIT\n");
	fclose(fn);
	chmod(mangle_fn, 0700);
	eval("iptables-restore", (char*)mangle_fn);
	QOSDBG("[BWLIT] Create iptables rules done.\n");

	/* Setup guest network's ebtables rules */
	int  guest_mark = GUEST_INIT_MARKNUM;
	char wl[128], wlv[128], tmp[128], *next, *next2;
	char prefix[32];
	char mssid_mark[4];
	char *wl_if = wl_ifname;
	int  i = 0;
	int  j = 1;
	foreach(wl, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		foreach(wlv, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next2) {

			if(nvram_get_int(strcat_r(wlv, "_bss_enabled", tmp)) && 
			   nvram_get_int(strcat_r(wlv, "_bw_enabled" , tmp))) {
				
				get_wlxy_ifname(i, j, wl_if);
				if(get_model()==MODEL_RTAC87U && (i == 1)) {
					if(j == 1) wl_if = "vlan4000";
					if(j == 2) wl_if = "vlan4001";
					if(j == 3) wl_if = "vlan4002";
				}

				snprintf(mssid_mark, sizeof(mssid_mark), "%d", guest_mark);
				eval("ebtables", "-t", "nat", "-A", "PREROUTING",  "-i", wl_if, "-j", "mark", "--set-mark", mssid_mark, "--mark-target", "ACCEPT");
				eval("ebtables", "-t", "nat", "-A", "POSTROUTING", "-o", wl_if, "-j", "mark", "--set-mark", mssid_mark, "--mark-target", "ACCEPT");
				guest_mark++;
			} //bss_enabled
			j++;
		}
		i++; j = 1;
	}

	QOSDBG("[BWLIT_GUEST] Create ebtables rules done.\n");
	return 0;
}

static int guest; // qdisc root only 3: ~ 14: (12 guestnetwork)

static int start_bandwidth_limiter(void)
{
	FILE *f = NULL;
	char *buf, *g, *p;
	char *enable, *addr, *dlc, *upc, *prio;
	int class = 0;
	int s[6]; // strip mac address
	int addr_type;
	char addr_new[40];
	char wl_ifname[IFNAMSIZ];
	char *qsched;

#ifdef RTCONFIG_BCMARM
	switch(nvram_get_int("qos_sched")){
		case 1:
			qsched = "codel";
			break;
		case 2:
			qsched = "fq_codel quantum 300";
			break;
		default:
			qsched = "sfq perturb 10";
			break;
	}
#else
	qsched = "sfq perturb 10";
#endif

	if ((f = fopen(qosfn, "w")) == NULL) return -2;
	fprintf(f,
		"#!/bin/sh\n"
		"WAN=%s\n"
		"tc qdisc del dev $WAN root 2>/dev/null\n"
		"tc qdisc del dev $WAN ingress 2>/dev/null\n"
		"tc qdisc del dev br0 root 2>/dev/null\n"
		"tc qdisc del dev br0 ingress 2>/dev/null\n"
		"\n"
		"TQAU=\"tc qdisc add dev $WAN\"\n"
		"TCAU=\"tc class add dev $WAN\"\n"
		"TFAU=\"tc filter add dev $WAN\"\n"
		"SFQ=\"sfq perturb 10\"\n"
		"TQA=\"tc qdisc add dev br0\"\n"
		"TCA=\"tc class add dev br0\"\n"
		"TFA=\"tc filter add dev br0\"\n"
		"\n"

		"start()\n"
		"{\n"
		"\t$TQA root handle 1: htb\n"
		"\t$TCA parent 1: classid 1:1 htb rate 1024000kbit\n"
		"\n"
		"\t$TQAU root handle 2: htb\n"
		"\t$TCAU parent 2: classid 2:1 htb rate 1024000kbit\n"
		, get_wan_ifname(wan_primary_ifunit())
	);
	// access router : mark 9
	// default : 10Gbps
	fprintf(f,
		"\n"
		"\t$TCA parent 1:1 classid 1:9 htb rate 10240000kbit ceil 10240000kbit prio 1\n"
		"\t$TQA parent 1:9 handle 9: %s\n"
		"\t$TFA parent 1: prio 1 protocol ip handle 9 fw flowid 1:9\n"
		"\n"
		"\t$TCAU parent 2:1 classid 2:9 htb rate 10240000kbit ceil 10240000kbit prio 1\n"
		"\t$TQAU parent 2:9 handle 9: %s\n"
		"\t$TFAU parent 2: prio 1 protocol ip handle 9 fw flowid 2:9\n",
		qsched,
		qsched
	);

	/* ASUSWRT
	qos_bw_rulelist :
		enable>addr>DL-Ceil>UL-Ceil>prio
		enable : enable or disable this rule
		addr : (source) IP or MAC or IP-range
		DL-Ceil : the max download bandwidth
		UL-Ceil : the max upload bandwidth
		prio : priority for client
	*/

	g = buf = strdup(nvram_safe_get("qos_bw_rulelist"));

	while (g) {
		if ((p = strsep(&g, "<")) == NULL) break;
		if ((vstrsep(p, ">", &enable, &addr, &dlc, &upc, &prio)) != 5) continue;
		if (!strcmp(enable, "0")) continue;

		address_format_checker(&addr_type, addr, addr_new, sizeof(addr_new));
		class = safe_atoi(prio) + INITIAL_MARKNUM;
		if (addr_type == TYPE_MAC)
		{
			sscanf(addr_new, "%02X:%02X:%02X:%02X:%02X:%02X",&s[0],&s[1],&s[2],&s[3],&s[4],&s[5]);
			fprintf(f,
				"\n"
				"\t$TCA parent 1:1 classid 1:%d htb rate %skbit ceil %skbit prio %d\n"
				"\t$TQA parent 1:%d handle %d: %s\n"
				"\t$TFA parent 1: protocol ip prio %d u32 match u16 0x0800 0xFFFF at -2 match u32 0x%02X%02X%02X%02X 0xFFFFFFFF at -12 match u16 0x%02X%02X 0xFFFF at -14 flowid 1:%d"
				"\n"
				"\t$TCAU parent 2:1 classid 2:%d htb rate %skbit ceil %skbit prio %d\n"
				"\t$TQAU parent 2:%d handle %d: %s\n"
				"\t$TFAU parent 2: prio %d protocol ip handle %d fw flowid 2:%d\n"
				, class, dlc, dlc, class
				, class, class, qsched
				, class, s[2], s[3], s[4], s[5], s[0], s[1], class
				, class, upc, upc, class
				, class, class, qsched
				, class, class, class
			);
		}
		else if (addr_type == TYPE_IP || addr_type == TYPE_IPRANGE)
		{
			fprintf(f,
				"\n"
				"\t$TCA parent 1:1 classid 1:%d htb rate %skbit ceil %skbit prio %d\n"
				"\t$TQA parent 1:%d handle %d: %s\n"
				"\t$TFA parent 1: prio %d protocol ip handle %d fw flowid 1:%d\n"
				"\n"
				"\t$TCAU parent 2:1 classid 2:%d htb rate %skbit ceil %skbit prio %d\n"
				"\t$TQAU parent 2:%d handle %d: %s\n"
				"\t$TFAU parent 2: prio %d protocol ip handle %d fw flowid 2:%d\n"
				, class, dlc, dlc, class
				, class, class, qsched
				, class, class, class
				, class, upc, upc, class
				, class, class, qsched
				, class, class, class
			);
		}
	}

	if (buf) free(buf);

	// init guest 3: ~ 14: (12 guestnetwork), start number = 3
	guest = 3;
	int  guest_mark = GUEST_INIT_MARKNUM;
	char wl[128], wlv[128], tmp[128], *next, *next2;
	char prefix[32];
	char *wl_if = wl_ifname;
	int  i = 0;
	int  j = 1;
	
	/* Setup guest network's bandwidth limiter */
	foreach(wl, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		foreach(wlv, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next2) {

			if(nvram_get_int(strcat_r(wlv, "_bss_enabled", tmp)) && 
			   nvram_get_int(strcat_r(wlv, "_bw_enabled" , tmp))) {
				
				get_wlxy_ifname(i, j, wl_if);
				if(get_model()==MODEL_RTAC87U && (i == 1)) {
					if(j == 1) wl_if = "vlan4000";
					if(j == 2) wl_if = "vlan4001";
					if(j == 3) wl_if = "vlan4002";
				}

				QOSDBG("[BWLIT_GUEST] Processor [%s] Interface \n", wl_if);

				fprintf(f,
					"\n"
					"\ttc qdisc del dev %s root 2>/dev/null\n"
					"\tGUEST%d%d=%s\n"
					"\tTQA%d%d=\"tc qdisc add dev $GUEST%d%d\"\n"
					"\tTCA%d%d=\"tc class add dev $GUEST%d%d\"\n"
					"\tTFA%d%d=\"tc filter add dev $GUEST%d%d\"\n" // 5
					"\n"
					"\t$TQA%d%d root handle %d: htb\n"
					"\t$TCA%d%d parent %d: classid %d:1 htb rate %skbit\n" // 7
					"\n"
					"\t$TCA%d%d parent %d:1 classid %d:%d htb rate 1kbit ceil %skbit prio %d\n"
					"\t$TQA%d%d parent %d:%d handle %d: %s\n"
					"\t$TFA%d%d parent %d: prio %d protocol ip handle %d fw flowid %d:%d\n" // 10
					"\n"
					"\t$TCAU parent 2:1 classid 2:%d htb rate 1kbit ceil %skbit prio %d\n"
					"\t$TQAU parent 2:%d handle %d: %s\n"
					"\t$TFAU parent 2: prio %d protocol ip handle %d fw flowid 2:%d\n" // 13
					, wl_if
					, i, j, wl_if
					, i, j, i, j
					, i, j, i, j
					, i, j, i, j // 5
					, i, j, guest
					, i, j, guest, guest, nvram_safe_get(strcat_r(wlv, "_bw_dl", tmp)) //7
					, i, j, guest, guest, guest_mark, nvram_safe_get(strcat_r(wlv, "_bw_dl", tmp)), guest_mark
					, i, j, guest, guest_mark, guest_mark, qsched
					, i, j, guest, guest_mark, guest_mark, guest, guest_mark // 10
					, guest_mark, nvram_safe_get(strcat_r(wlv, "_bw_ul", tmp)), guest_mark
					, guest_mark, guest_mark, qsched
					, guest_mark, guest_mark, guest_mark //13
				);
				QOSDBG("[BWLIT_GUEST] create %s bandwidth limiter, qdisc=%d, class=%d\n", wl_if, guest, guest_mark);
				guest++; // add guest 3: ~ 14: (12 guestnetwork)
				guest_mark++;
			} //bss_enabled
			j++;
		}
		i++; j = 1;
	}
	
	/* Stop Function */
	fprintf(f,
		"}\n\n"
		"stop()\n"
		"{\n"
		/* Flush ebtables */
		"\t#ebtables -t nat -F\n\n"
		//WAN/LAN
		"\ttc qdisc del dev $WAN root 2>/dev/null\n"
		"\ttc qdisc del dev br0 root 2>/dev/null\n"
		);
	i = 0; j = 1;
	foreach(wl, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		foreach(wlv, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next2) {
			
			if(nvram_get_int(strcat_r(wlv, "_bss_enabled", tmp)) && 
			   nvram_get_int(strcat_r(wlv, "_bw_enabled" , tmp))) {
				
				wl_if = wl_ifname;
				get_wlxy_ifname(i, j, wl_ifname);
				if(get_model()==MODEL_RTAC87U && (i == 1)) {
					if(j == 1) wl_if = "vlan4000";
					if(j == 2) wl_if = "vlan4001";
					if(j == 3) wl_if = "vlan4002";
				}
				fprintf(f, "\ttc qdisc del dev %s root 2>/dev/null\n", wl_if);
			}
			j++;
		}
		i++; j = 1;
	}
	
	/* Show Function */
	fprintf(f,
		"}\n\n"
		"show()\n"
		"{\n"
		"\ttc -s -d class ls dev $WAN\n"
		"\ttc -s -d class ls dev br0\n"
		);
	
	/* Main Funtion */
	fprintf(f,
		"}\n\n"
		"if [ $# != 1 ]; then\n"
		"\techo \"Usage: $0 start/stop/restart\"\n"
		"else\n"
		"\tif [ $1 = \"start\" ]; then\n"
		"\t\tstart\n"
		"\telif [ $1 = \"stop\" ]; then\n"
		"\t\tstop\n"
		"\telif [ $1 = \"restart\" ]; then\n"
		"\t\tstop\n"
		"\t\tstart\n"
		"\tfi\n"
		"fi\n"
		);

	fclose(f);
	chmod(qosfn, 0700);
	eval((char *)qosfn, "start");
	QOSDBG("[BWLIT] Execute Bandwidth Limiter Done.\n");

	return 0;
}

int add_iQosRules(char *pcWANIF)
{
	int status = 0;

	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 1)
		set_codel_patch();

	if (pcWANIF == NULL || nvram_get_int("qos_enable") != 1 || nvram_get_int("qos_type") == 1) return -1;
	
	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0)
		status = add_qos_rules(pcWANIF);
	else if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 2)
		status = add_bandwidth_limiter_rules(pcWANIF);
	
	if (status < 0) _dprintf("[%s] status = %d\n", __FUNCTION__, status);

	return status;
}

int start_iQos(void)
{
	int status = 0;
	if (nvram_get_int("qos_enable") != 1 || nvram_get_int("qos_type") == 1) return -1;

	if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 0)
		status = start_tqos();
	else if (nvram_get_int("qos_enable") == 1 && nvram_get_int("qos_type") == 2)
		status = start_bandwidth_limiter();

	if (status < 0) _dprintf("[%s] status = %d\n", __FUNCTION__, status);

	return status;
}

int check_wl_guest_bw_enable()
{
	char wl[128], wlv[128], tmp[128], *next, *next2;
	char prefix[32];
	int  i = 0;

	foreach(wl, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		foreach(wlv, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next2) {
			
			if ( nvram_get_int(strcat_r(wlv, "_bss_enabled", tmp)) && 
			     nvram_get_int(strcat_r(wlv, "_bw_enabled" , tmp))) {
				return 1;
			}
		}
		i++;
	}
	return 0;
}

void ForceDisableWLan_bw(void)
{
	char wl[128], wlv[128], tmp[128], *next, *next2;
	char prefix[32];
	int  i = 0;

	foreach(wl, nvram_safe_get("wl_ifnames"), next) {
		snprintf(prefix, sizeof(prefix), "wl%d_", i);
		foreach(wlv, nvram_safe_get(strcat_r(prefix, "vifnames", tmp)), next2) {
			nvram_set_int(strcat_r(wlv, "_bw_enabled" , tmp), 0);
		}
		i++;
	}
	QOSDBG("[BWLIT] ALL Guest Netwok of Bandwidth Limiter has been Didabled.\n");
}

#ifdef RTCONFIG_BCMARM
void set_codel_patch(void)
{
	int sched;

	if (nvram_get_int("qos_type") != 1)
		return;

	sched = nvram_get_int("qos_sched");

	if (!f_exists("/var/lock/qostc") &&
	    ((sched == 1) || (sched == 2))) {
		eval("touch", "/var/lock/qostc");
		mount("/usr/sbin/faketc", TCPATH, NULL, MS_BIND, NULL);
		logmessage("qos", "Applying codel patch");
	}
}

void remove_codel_patch(void)
{
#if 0
	if (f_exists("/var/lock/qostc")) {
		umount2(TCPATH,MNT_DETACH);
		unlink("/var/lock/qostc");
		logmessage("qos", "Removing codel patch");
	}
#else
	return;
#endif

}

#endif

