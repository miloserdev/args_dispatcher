#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARGS_REPL
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

/* readline callbacks carry no user data, so the context lives here and is
 * reused for every line */
static args_context_repl_t repl = { .root = cmd_table };
static args_argv_t         av;
static args_context_t      ctx = { .av = &av, .repl = &repl };

static char **complete (
	const char *text,
	int         start,
	int         end
) {
	(void) end;
	return args_complete ( &ctx, rl_line_buffer, text, start );
}

#define PROMPT "> "

int main ( void ) {
	rl_attempted_completion_function = complete;

	char *line;
	while ( ( line = readline ( PROMPT ) ) != NULL )
	{
		if ( line[0] == '\0' )
		{
			free ( line );
			continue;
		}
		if ( strcmp ( line, "exit" ) == 0 )
		{
			free ( line );
			break;
		}

		add_history ( line );

		ctx.line    = line;
		ctx.command = cmd_table;
		ctx.idx     = 0;
		ctx.depth   = 0;
		ctx.data    = NULL;

		if ( args_tokenize ( &ctx ) > 0 )
		{
			int ret = args_dispatch ( &ctx );
			if ( ret < 0 )
			{
				fprintf ( stderr, "error: command failed\n" );
			}
		}

		free ( line );
	}
	printf ( "\n" );
	return 0;
}
