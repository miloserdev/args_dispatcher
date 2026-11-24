/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 miloserdev */
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "args.h"

#include "commands/chassis.h"
#include "commands/info.h"
#include "commands/ip.h"
#include "commands/power.h"

/* main input */
args_command_t cmd_table[] = {
	chassis,
	power,
	info,
	ip,
	{ .name = NULL },
};

int main (
	int   argc,
	char *argv[]
) {

	printf ( "┌─<" );
	for ( int i = 0; i < argc; i++ )
	{
		printf ( "%s ", argv[i] );
	}
	printf ( ">\n" );

	char  buf[MAX_LINE];
	char *line = args_parse ( buf, sizeof ( buf ), argc, argv );
	if ( line == NULL )
	{
		fprintf ( stderr, "error: command line too long (max %d)\n", MAX_LINE - 1 );
		return 1;
	}

	args_argv_t    av;
	args_context_t ctx = {
		.av      = &av,
		.line    = line,
		.command = cmd_table,
	};

	if ( args_tokenize ( &ctx ) == 0 )
	{
		fprintf ( stderr, "usage: %s <object> <action> [args...]\n", argv[0] );
		fprintf ( stderr, "\navailable commands:\n" );
		args_help ( stderr, cmd_table, 1 );
		return 1;
	}

	int ret = args_dispatch ( &ctx );

	printf ( "\n" );
	return ret < 0;
}
