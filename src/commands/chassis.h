/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 miloserdev */
#ifndef __CHASSIS_H__
#define __CHASSIS_H__

#include <stddef.h>

#include "../args.h"

typedef struct
{
	int scanned;
} chassis_data_t;

static chassis_data_t cd;

static int chassis_status ( args_context_t *ctx ) {
	(void) ctx;
	log_info ( "[fn] chassis status: ON" );
	return 0;
}

static int chassis_identify ( args_context_t *ctx ) {
	log_info ( "[fn] chassis identify=%s", ctx->av->tokens[ctx->idx] );
	return 0;
}

static int chassis_bootdev ( args_context_t *ctx ) {
	log_info ( "[fn] chassis bootdev=%s", ctx->av->tokens[ctx->idx] );
	return 0;
}

static int chassis_policy ( args_context_t *ctx ) {
	log_info ( "[fn] chassis policy=%s", ctx->av->tokens[ctx->idx] );
	return 0;
}

static args_command_t chassis_table[] = {
	{ "status",   chassis_status,   NULL, 0, "Show chassis power status", NULL },
	{ "identify", chassis_identify, NULL, 1, "Blink identify LED [on|off|<sec>]", NULL },
	{ "bootdev",  chassis_bootdev,  NULL, 1, "Set boot device <pxe|disk|cdrom|bios>", NULL },
	{ "policy",   chassis_policy,   NULL, 1, "Power restore policy", NULL },
	{ "asd",      NULL,             NULL, 1, NULL, NULL },
	{ NULL,       NULL,             NULL, 0, NULL, NULL },
};

static int chassis_fn ( args_context_t *ctx ) {
	log_info ( "chassis scan" );

	ctx->data  = &cd;
	cd.scanned = 111;

	return 0;
}

const args_command_t chassis = { .name = "chassis", .fn = chassis_fn, .next = chassis_table };

#endif
