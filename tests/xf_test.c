/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 miloserdev */
/* Test harness: a CLI driver over a synthetic command table that covers
 * cases the real commands do not (exact vs prefix match, handler return
 * values, self-loop chains). Prints RET=<dispatch result>. */
#include <stdio.h>
#include <stdlib.h>

#include "args.h"

static int h_ab ( args_context_t *ctx ) {
	(void) ctx;
	puts ( "H_AB" );
	return 0;
}

static int h_abc ( args_context_t *ctx ) {
	(void) ctx;
	puts ( "H_ABC" );
	return 0;
}

static int h_ok ( args_context_t *ctx ) {
	(void) ctx;
	puts ( "H_OK" );
	return 0;
}

static int h_neg ( args_context_t *ctx ) {
	(void) ctx;
	puts ( "H_NEG" );
	return -1;
}

static int h_pos ( args_context_t *ctx ) {
	(void) ctx;
	puts ( "H_POS" );
	return 1;
}

/* keyword parser with overlapping names (src/scope) and an exact-vs-prefix pair (dev/device) */
static args_keyword_t kw_tbl[] = {
	{ "src", 0, 0, NULL },
	{ "scope", 0, 0, NULL },
	{ "dev", 0, 0, NULL },
	{ "device", 0, 0, NULL },
	{ "up", 1, 0, NULL },
	{ NULL },
};

static int h_kw ( args_context_t *ctx ) {
	args_keyword_t *kw = ctx->command->kw;
	if ( args_parse_keyword ( ctx ) != 0 )
	{
		return -1;
	}
	for ( int i = 0; i < 5; i++ )
	{
		if ( kw[i].val != NULL )
		{
			printf ( "KW %s=%s\n", kw[i].key, kw[i].val );
		}
	}
	return 0;
}

static args_command_t chain[] = {
	{ "one", h_ok, chain, 0, "one" },
	{ "two", h_ok, chain, 0, "two" },
	{ NULL,  NULL, NULL,  0, NULL  },
};

static args_command_t tbl[] = {
	{ "ab", h_ab, NULL, 0, "ab" },
	{ "abc", h_abc, NULL, 0, "abc" },
	{ "neg", h_neg, NULL, 0, "neg" },
	{ "negchain", h_neg, chain, 0, "negchain" },
	{ "poschain", h_pos, chain, 0, "poschain" },
	{ "chain", NULL, chain, 0, "chain" },
	{ "kw", h_kw, NULL, 0, "kw", kw_tbl },
	{ NULL, NULL, NULL, 0, NULL },
};

int main ( int   argc,
		   char *argv[] ) {
	char  buf[MAX_LINE];
	char *line = args_parse ( buf, sizeof ( buf ), argc, argv );
	if ( line == NULL )
	{
		fputs ( "too long\n", stderr );
		return 2;
	}

	args_argv_t    av;
	args_context_t ctx = { .av = &av, .line = line, .command = tbl };

	if ( args_tokenize ( &ctx ) == 0 )
	{
		puts ( "NOTOKENS" );
		return 1;
	}

	int r = args_dispatch ( &ctx );
	printf ( "RET=%d\n", r );
	return r < 0;
}
