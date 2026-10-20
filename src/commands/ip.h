#ifndef __IP_H__
#define __IP_H__

#include <arpa/inet.h>
#include <net/if.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../args.h"

/*
 * iproute2-like commands:
 *
 *   ip addr  add|del <addr>[/plen] dev <if> [label <s>] [broadcast <addr>]
 *   ip addr  show [dev <if>]
 *   ip addr  flush dev <if>
 *   ip route add|del|replace <dest|default> [via <gw>] [dev <if>] [src <addr>] [metric <n>] [onlink]
 *   ip route show [dev <if>]
 *   ip route get <addr>
 *   ip link  set <if> [up|down] [mtu <n>] [name <new>]
 *   ip link  show [dev <if>]
 *   ip neigh add|del <addr> [lladdr <mac>] dev <if>
 *   ip neigh show [dev <if>]
 *
 * The dispatcher consumes the fixed positional arguments (args_command_t.args);
 * everything after them is a list of "keyword [value]" pairs which the
 * handler parses with args_parse_keyword().
 */

/* ---- value validators ---------------------------------------------------- */

/* "a.b.c.d", "a.b.c.d/len", IPv6 likewise. Without "/len" the prefix is host-wide. */
static int ip_parse_prefix (
	const char *s,
	int        *family,
	int        *plen
) {

	char          buf[64];
	unsigned char bin[16];
	const char   *slash = strchr ( s, '/' );
	size_t        alen  = slash ? (size_t) ( slash - s ) : strlen ( s );

	if ( alen == 0 || alen >= sizeof ( buf ) )
	{
		return -1;
	}
	memcpy ( buf, s, alen );
	buf[alen] = '\0';

	if ( inet_pton ( AF_INET, buf, bin ) == 1 )
	{
		*family = AF_INET;
	} else if ( inet_pton ( AF_INET6, buf, bin ) == 1 )
	{
		*family = AF_INET6;
	} else
	{
		return -1;
	}

	int max = *family == AF_INET ? 32 : 128;

	if ( slash == NULL )
	{
		*plen = max;
		return 0;
	}

	char *end;
	long  v = strtol ( slash + 1, &end, 10 );
	if ( slash[1] == '\0' || *end != '\0' || v < 0 || v > max )
	{
		return -1;
	}

	*plen = (int) v;
	return 0;
}

/* plain address, no "/len" */
static int ip_parse_addr (
	const char *s,
	int        *family
) {

	int plen;
	return strchr ( s, '/' ) == NULL ? ip_parse_prefix ( s, family, &plen ) : -1;
}

static int ip_valid_dev ( const char *s ) {
	size_t len = strlen ( s );
	return len > 0 && len < IF_NAMESIZE && strpbrk ( s, "/ \t" ) == NULL;
}

/* aa:bb:cc:dd:ee:ff */
static int ip_valid_mac ( const char *s ) {
	if ( strlen ( s ) != 17 )
	{
		return 0;
	}
	for ( int i = 0; i < 17; i++ )
	{
		char c = s[i];
		if ( i % 3 == 2 )
		{
			if ( c != ':' )
			{
				return 0;
			}
		} else if ( !( ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' ) ) )
		{
			return 0;
		}
	}
	return 1;
}

static const char *ip_family_name ( int family ) {
	return family == AF_INET ? "ipv4" : "ipv6";
}

__attribute__ ( ( format ( printf,
						   3,
						   4 ) ) ) static void
ip_cat (
	char       *buf,
	size_t      size,
	const char *fmt,
	...
) {

	size_t len = strlen ( buf );
	if ( len + 1 >= size )
	{
		return;
	}

	va_list ap;
	va_start ( ap, fmt );
	vsnprintf ( buf + len, size - len, fmt, ap );
	va_end ( ap );
}

/* ---- keyword tables shared by several commands --------------------------- */

static args_keyword_t ip_dev_kw[] = {
	{ "dev", 0, 0, NULL },
	{ NULL }
}; /* dev optional */
static args_keyword_t ip_dev_req_kw[] = {
	{ "dev", 0, 1, NULL },
	{ NULL }
}; /* dev required */

/* ---- ip addr ------------------------------------------------------------- */

enum
{
	ADDR_DEV,
	ADDR_LABEL,
	ADDR_BRD,
	ADDR_N
};

static args_keyword_t ip_addr_add_kw[] = {
	[ADDR_DEV]   = { "dev", 0, 1, NULL },
	[ADDR_LABEL] = { "label", 0, 0, NULL },
	[ADDR_BRD]   = { "broadcast", 0, 0, NULL },
	[ADDR_N]     = { NULL },
};

/* label and broadcast make sense only for "add" */
static args_keyword_t ip_addr_del_kw[] = {
	{ "dev", 0, 1, NULL },
	{ NULL }
};

static int ip_addr_modify (
	args_context_t *ctx,
	int             add
) {

	const char *addr = ctx->av->tokens[ctx->idx];
	int         family, plen;

	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}

	/* the del table has no label/broadcast entries */
	const char *label = add ? kw[ADDR_LABEL].val : NULL;
	const char *brd   = add ? kw[ADDR_BRD].val : NULL;

	if ( ip_parse_prefix ( addr, &family, &plen ) != 0 )
	{
		log_info ( "error: invalid address <%s>", addr );
		return -1;
	}

	if ( !ip_valid_dev ( kw[ADDR_DEV].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[ADDR_DEV].val );
		return -1;
	}

	if ( brd != NULL )
	{
		int bfam;
		if ( ip_parse_addr ( brd, &bfam ) != 0 || bfam != family )
		{
			log_info ( "error: invalid broadcast <%s>", brd );
			return -1;
		}
	}

	char msg[192] = "";
	ip_cat ( msg, sizeof ( msg ), "%s address %s (%s, /%d) %s device %s", add ? "add" : "delete", addr, ip_family_name ( family ), plen, add ? "to" : "from", kw[ADDR_DEV].val );
	if ( label != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), ", label %s", label );
	}
	if ( brd != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), ", broadcast %s", brd );
	}

	log_info ( "[ip addr] %s", msg );
	return 0;
}

static int ip_addr_add ( args_context_t *ctx ) { return ip_addr_modify ( ctx, 1 ); }

static int ip_addr_del ( args_context_t *ctx ) { return ip_addr_modify ( ctx, 0 ); }

static int ip_addr_show ( args_context_t *ctx ) {
	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}
	if ( kw[0].val != NULL && !ip_valid_dev ( kw[0].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[0].val );
		return -1;
	}

	if ( kw[0].val != NULL )
	{
		log_info ( "[ip addr] show addresses of device %s", kw[0].val );
	} else
	{
		log_info ( "[ip addr] show addresses of all devices" );
	}
	return 0;
}

static int ip_addr_flush ( args_context_t *ctx ) {
	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}
	if ( !ip_valid_dev ( kw[0].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[0].val );
		return -1;
	}

	log_info ( "[ip addr] flush all addresses of device %s", kw[0].val );
	return 0;
}

static args_command_t ip_addr_table[] = {
	{ "add", ip_addr_add, NULL, 1, "add <addr>[/plen] dev <if> [label <s>] [broadcast <addr>]", ip_addr_add_kw },
	{ "del", ip_addr_del, NULL, 1, "del <addr>[/plen] dev <if>", ip_addr_del_kw },
	{ "show", ip_addr_show, NULL, 0, "show [dev <if>]", ip_dev_kw },
	{ "flush", ip_addr_flush, NULL, 0, "flush dev <if>", ip_dev_req_kw },
	{ NULL, NULL, NULL, 0, NULL },
};

/* ---- ip route ------------------------------------------------------------ */

enum
{
	RT_VIA,
	RT_DEV,
	RT_SRC,
	RT_METRIC,
	RT_ONLINK,
	RT_N
};

static args_keyword_t ip_route_kw[] = {
	[RT_VIA]    = { "via", 0, 0, NULL },
	[RT_DEV]    = { "dev", 0, 0, NULL },
	[RT_SRC]    = { "src", 0, 0, NULL },
	[RT_METRIC] = { "metric", 0, 0, NULL },
	[RT_ONLINK] = { "onlink", 1, 0, NULL },
	[RT_N]      = { NULL },
};

static int ip_route_modify (
	args_context_t *ctx,
	const char     *verb
) {

	const char   *dest       = ctx->av->tokens[ctx->idx];
	int           is_default = strcmp ( dest, "default" ) == 0;
	int           family = AF_INET, plen = 0, dummy;
	unsigned long metric;

	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}

	if ( !is_default && ip_parse_prefix ( dest, &family, &plen ) != 0 )
	{
		log_info ( "error: invalid destination <%s>", dest );
		return -1;
	}

	if ( kw[RT_VIA].val == NULL && kw[RT_DEV].val == NULL )
	{
		log_info ( "error: route needs at least <via> or <dev>" );
		return -1;
	}

	if ( kw[RT_VIA].val != NULL )
	{
		int gfam;
		if ( ip_parse_addr ( kw[RT_VIA].val, &gfam ) != 0 )
		{
			log_info ( "error: invalid gateway <%s>", kw[RT_VIA].val );
			return -1;
		}
		if ( is_default )
		{
			family = gfam; /* "default" takes the family of its gateway */
		} else if ( gfam != family )
		{
			log_info ( "error: gateway <%s> and destination <%s> have different families", kw[RT_VIA].val, dest );
			return -1;
		}
	}

	if ( kw[RT_DEV].val != NULL && !ip_valid_dev ( kw[RT_DEV].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[RT_DEV].val );
		return -1;
	}

	if ( kw[RT_SRC].val != NULL
		 && ( ip_parse_addr ( kw[RT_SRC].val, &dummy ) != 0 || dummy != family ) )
	{
		log_info ( "error: invalid source address <%s>", kw[RT_SRC].val );
		return -1;
	}

	if ( kw[RT_METRIC].val != NULL && args_parse_uint ( kw[RT_METRIC].val, 0, 4294967295UL, &metric ) != 0 )
	{
		log_info ( "error: invalid metric <%s>", kw[RT_METRIC].val );
		return -1;
	}

	if ( kw[RT_ONLINK].val != NULL && kw[RT_VIA].val == NULL )
	{
		log_info ( "error: <onlink> needs <via>" );
		return -1;
	}

	char msg[256] = "";
	if ( is_default )
	{
		ip_cat ( msg, sizeof ( msg ), "%s default route (%s)", verb, ip_family_name ( family ) );
	} else
	{
		ip_cat ( msg, sizeof ( msg ), "%s route to %s %s (%s)", verb, plen == ( family == AF_INET ? 32 : 128 ) ? "host" : "network", dest, ip_family_name ( family ) );
	}
	if ( kw[RT_VIA].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " via %s", kw[RT_VIA].val );
	}
	if ( kw[RT_DEV].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " dev %s", kw[RT_DEV].val );
	}
	if ( kw[RT_SRC].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " src %s", kw[RT_SRC].val );
	}
	if ( kw[RT_METRIC].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " metric %s", kw[RT_METRIC].val );
	}
	if ( kw[RT_ONLINK].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " onlink" );
	}

	log_info ( "[ip route] %s", msg );
	return 0;
}

static int ip_route_add ( args_context_t *ctx ) { return ip_route_modify ( ctx, "add" ); }

static int ip_route_del ( args_context_t *ctx ) { return ip_route_modify ( ctx, "delete" ); }

static int ip_route_replace ( args_context_t *ctx ) { return ip_route_modify ( ctx, "replace" ); }

static int ip_route_show ( args_context_t *ctx ) {
	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}
	if ( kw[0].val != NULL && !ip_valid_dev ( kw[0].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[0].val );
		return -1;
	}

	if ( kw[0].val != NULL )
	{
		log_info ( "[ip route] show routes of device %s", kw[0].val );
	} else
	{
		log_info ( "[ip route] show all routes" );
	}
	return 0;
}

static int ip_route_get ( args_context_t *ctx ) {
	const char *addr = ctx->av->tokens[ctx->idx];
	int         family;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}
	if ( ip_parse_addr ( addr, &family ) != 0 )
	{
		log_info ( "error: invalid address <%s>", addr );
		return -1;
	}

	log_info ( "[ip route] look up the route to %s (%s)", addr, ip_family_name ( family ) );
	return 0;
}

static args_command_t ip_route_table[] = {
	{ "add", ip_route_add, NULL, 1, "add <dest|default> [via <gw>] [dev <if>] [src <addr>] [metric <n>] [onlink]", ip_route_kw },
	{ "del", ip_route_del, NULL, 1, "del <dest|default> [via <gw>] [dev <if>]", ip_route_kw },
	{ "replace", ip_route_replace, NULL, 1, "replace <dest|default> [via <gw>] [dev <if>] [metric <n>]", ip_route_kw },
	{ "show", ip_route_show, NULL, 0, "show [dev <if>]", ip_dev_kw },
	{ "get", ip_route_get, NULL, 1, "get <addr>" },
	{ NULL, NULL, NULL, 0, NULL },
};

/* ---- ip link ------------------------------------------------------------- */

enum
{
	LINK_UP,
	LINK_DOWN,
	LINK_MTU,
	LINK_NAME,
	LINK_N
};

static args_keyword_t ip_link_set_kw[] = {
	[LINK_UP]   = { "up", 1, 0, NULL },
	[LINK_DOWN] = { "down", 1, 0, NULL },
	[LINK_MTU]  = { "mtu", 0, 0, NULL },
	[LINK_NAME] = { "name", 0, 0, NULL },
	[LINK_N]    = { NULL },
};

static int ip_link_set ( args_context_t *ctx ) {
	const char   *dev = ctx->av->tokens[ctx->idx];
	unsigned long mtu = 0;

	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}

	if ( !ip_valid_dev ( dev ) )
	{
		log_info ( "error: invalid device name <%s>", dev );
		return -1;
	}

	if ( kw[LINK_UP].val == NULL && kw[LINK_DOWN].val == NULL
		 && kw[LINK_MTU].val == NULL && kw[LINK_NAME].val == NULL )
	{
		log_info ( "error: nothing to set, expected up|down|mtu|name" );
		return -1;
	}

	if ( kw[LINK_UP].val != NULL && kw[LINK_DOWN].val != NULL )
	{
		log_info ( "error: <up> and <down> are mutually exclusive" );
		return -1;
	}

	if ( kw[LINK_MTU].val != NULL && args_parse_uint ( kw[LINK_MTU].val, 68, 65535, &mtu ) != 0 )
	{
		log_info ( "error: invalid mtu <%s> (68..65535)", kw[LINK_MTU].val );
		return -1;
	}

	if ( kw[LINK_NAME].val != NULL && !ip_valid_dev ( kw[LINK_NAME].val ) )
	{
		log_info ( "error: invalid new name <%s>", kw[LINK_NAME].val );
		return -1;
	}

	char msg[192] = "";
	ip_cat ( msg, sizeof ( msg ), "set device %s:", dev );
	if ( kw[LINK_UP].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " state up" );
	}
	if ( kw[LINK_DOWN].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " state down" );
	}
	if ( kw[LINK_MTU].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " mtu %lu", mtu );
	}
	if ( kw[LINK_NAME].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " new name %s", kw[LINK_NAME].val );
	}

	log_info ( "[ip link] %s", msg );
	return 0;
}

static int ip_link_show ( args_context_t *ctx ) {
	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}
	if ( kw[0].val != NULL && !ip_valid_dev ( kw[0].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[0].val );
		return -1;
	}

	if ( kw[0].val != NULL )
	{
		log_info ( "[ip link] show device %s", kw[0].val );
	} else
	{
		log_info ( "[ip link] show all devices" );
	}
	return 0;
}

static args_command_t ip_link_table[] = {
	{ "set", ip_link_set, NULL, 1, "set <if> [up|down] [mtu <n>] [name <new>]", ip_link_set_kw },
	{ "show", ip_link_show, NULL, 0, "show [dev <if>]", ip_dev_kw },
	{ NULL, NULL, NULL, 0, NULL },
};

/* ---- ip neigh ------------------------------------------------------------ */

enum
{
	NB_DEV,
	NB_LLADDR,
	NB_N
};

static args_keyword_t ip_neigh_add_kw[] = {
	[NB_DEV]    = { "dev", 0, 1, NULL },
	[NB_LLADDR] = { "lladdr", 0, 1, NULL },
	[NB_N]      = { NULL },
};

static args_keyword_t ip_neigh_del_kw[] = {
	[NB_DEV]    = { "dev", 0, 1, NULL },
	[NB_LLADDR] = { "lladdr", 0, 0, NULL },
	[NB_N]      = { NULL },
};

static int ip_neigh_modify (
	args_context_t *ctx,
	int             add
) {

	const char *addr = ctx->av->tokens[ctx->idx];
	int         family;

	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}

	if ( ip_parse_addr ( addr, &family ) != 0 )
	{
		log_info ( "error: invalid address <%s>", addr );
		return -1;
	}

	if ( !ip_valid_dev ( kw[NB_DEV].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[NB_DEV].val );
		return -1;
	}

	if ( kw[NB_LLADDR].val != NULL && !ip_valid_mac ( kw[NB_LLADDR].val ) )
	{
		log_info ( "error: invalid lladdr <%s>", kw[NB_LLADDR].val );
		return -1;
	}

	char msg[192] = "";
	ip_cat ( msg, sizeof ( msg ), "%s neighbour %s (%s)", add ? "add" : "delete", addr, ip_family_name ( family ) );
	if ( kw[NB_LLADDR].val != NULL )
	{
		ip_cat ( msg, sizeof ( msg ), " lladdr %s", kw[NB_LLADDR].val );
	}
	ip_cat ( msg, sizeof ( msg ), " on device %s", kw[NB_DEV].val );

	log_info ( "[ip neigh] %s", msg );
	return 0;
}

static int ip_neigh_add ( args_context_t *ctx ) { return ip_neigh_modify ( ctx, 1 ); }

static int ip_neigh_del ( args_context_t *ctx ) { return ip_neigh_modify ( ctx, 0 ); }

static int ip_neigh_show ( args_context_t *ctx ) {
	args_keyword_t *kw = ctx->command->kw;

	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}
	if ( kw[0].val != NULL && !ip_valid_dev ( kw[0].val ) )
	{
		log_info ( "error: invalid device name <%s>", kw[0].val );
		return -1;
	}

	if ( kw[0].val != NULL )
	{
		log_info ( "[ip neigh] show neighbours of device %s", kw[0].val );
	} else
	{
		log_info ( "[ip neigh] show all neighbours" );
	}
	return 0;
}

static args_command_t ip_neigh_table[] = {
	{ "add", ip_neigh_add, NULL, 1, "add <addr> lladdr <mac> dev <if>", ip_neigh_add_kw },
	{ "del", ip_neigh_del, NULL, 1, "del <addr> [lladdr <mac>] dev <if>", ip_neigh_del_kw },
	{ "show", ip_neigh_show, NULL, 0, "show [dev <if>]", ip_dev_kw },
	{ NULL, NULL, NULL, 0, NULL },
};

/* ---- ip ------------------------------------------------------------------ */

static args_command_t ip_table[] = {
	{ "addr",  NULL, ip_addr_table,  0, "Protocol addresses"          },
	{ "route", NULL, ip_route_table, 0, "Routing table"               },
	{ "link",  NULL, ip_link_table,  0, "Network devices"             },
	{ "neigh", NULL, ip_neigh_table, 0, "ARP / NDISC neighbour cache" },
	{ NULL,    NULL, NULL,           0, NULL                          },
};

const args_command_t ip = { .name = "ip", .fn = NULL, .next = ip_table };

#endif
