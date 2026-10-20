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

#define PROMPT "> "

static void trim_newline ( char *s ) {
	size_t len = strlen ( s );
	if ( len > 0 && s[len - 1] == '\n' )
	{
		s[len - 1] = '\0';
	}
}

int main ( void ) {
	char line[MAX_LINE];

	printf ( PROMPT );
	fflush ( stdout );

	while ( fgets ( line, sizeof ( line ), stdin ) != NULL )
	{
		trim_newline ( line );

		/* пустая строка — просто новый промпт, не гоняем парсер вхолостую */
		if ( line[0] == '\0' )
		{
			printf ( PROMPT );
			fflush ( stdout );
			continue;
		}

		if ( strcmp ( line, "exit" ) == 0 || strcmp ( line, "quit" ) == 0 )
		{
			break;
		}

		argv_t    av;
		context_t ctx = {
			.av      = &av,
			.line    = line,
			.command = cmd_table,
			.idx     = 0,
			.depth   = 0,
		};

		if ( args_tokenize ( &ctx ) == 0 )
		{
			fprintf ( stderr, "usage: <object> <action> [args...]\n" );
			printf ( PROMPT );
			fflush ( stdout );
			continue;
		}

		int ret = args_dispatch ( &ctx );
		if ( ret < 0 )
		{
			fprintf ( stderr, "error: command failed\n" );
		}

		printf ( PROMPT );
		fflush ( stdout );
	}

	printf ( "\n" );
	return 0;
}
