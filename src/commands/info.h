#ifndef __INFO_H__
#define __INFO_H__

#include <stdio.h>

#include "../args.h"

const char splash[] = "                                                         ███    \n"
					  "                                                        █████   \n"
					  "                                                        █████   \n"
					  "                                                     ██ █████   \n"
					  "    ███          ██              ███   ██            ████████   \n"
					  " ████ ████ ██  █████ ████       ███████                █████████\n"
					  "████  ██████████████  ████   ███████████                ████████\n"
					  "████  ██████   █████  ████  ████████████                █████  █\n"
					  "   ██ ████     █████ ████   ██████ █████                █████   \n"
					  "███████████    █████         █████ █████                █████   \n"
					  "████  ████     █████         █████ █████                █████   \n"
					  "████  ████     █████         █████ █████                █████   \n"
					  "████  ████     █████         █████ █████                █████   \n"
					  "████  ████     █████         █████ █████                █████   \n"
					  "█████ ████     ██████        ██████████                 █████   \n"
					  "█████ ████ ██████████████     ███████                   █████   \n"
					  " ████████████  █████        ███  ████    ██████████      █████  \n"
					  "    ███ ███     ████       ██████████████ ██████████       ███  \n"
					  "                            █████    ███                    ███ \n"
					  "                            ████████ ██                      ██ \n"
					  "                                █████                           \n"
					  "                                 ███                            \n";

static int info_splash ( args_context_t *ctx ) {
	printf ( "%s", splash );
	return 0;
}

static int info_set ( args_context_t *ctx ) {
	log_info ( "[fn] info set %s=%s", ctx->av->tokens[ctx->idx], ctx->av->tokens[ctx->idx + 1] );
	return 0;
}

static int info_show ( args_context_t *ctx ) {
	(void) ctx;
	log_info ( "[fn] info show" );
	return 0;
}

static args_command_t info_table[] = {
	{ "set",    info_set,    info_table, 2, "set <what> <to>" },
	{ "show",   info_show,   info_table, 1, "Show info"       },
	{ "splash", info_splash, info_table, 0, "Show splash"     },
	{ NULL,     NULL,        NULL,       0, NULL              },
};

const args_command_t info = { .name = "info", .fn = NULL, .next = info_table };

#endif
