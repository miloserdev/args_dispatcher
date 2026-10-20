
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "args.h"

#include "commands/chassis.h"
#include "commands/info.h"
#include "commands/power.h"

/* main input */
command_t cmd_table[] = {
	chassis,
	power,
	info,
	NULL,
};

int main (
	int   argc,
	char *argv[]
) {

	printf ( "┌─<" );
	for ( size_t i = 0; i < argc; i++ )
	{
		printf ( "%s ", argv[i] );
	}
	printf ( ">\n" );

	// printf("𜲐\n");

	// context_data_t data = {
	// 	.fn = logger,
	// };

	argv_t    av;
	context_t ctx = {
		.av      = &av,
		.line    = args_parse ( argc, argv ),
		.command = cmd_table,
		// .data    = data,
	};

	if ( args_tokenize ( &ctx ) == 0 )
	{
		fprintf ( stderr, "usage: %s <object> <action> [args...]\n", argv[0] );
		fprintf ( stderr, "\navailable commands:\n" );
		args_help ( stderr, cmd_table, 1 );
		return 1;
	}

	args_dispatch ( &ctx );

	printf ( "\n" );
}
