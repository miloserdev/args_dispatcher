#ifndef __ARGS_H__
#define __ARGS_H__

#include <stdio.h>
#include <string.h>

#include "debug.h"

#define MAX_TOKENS 32
#define MAX_LINE   256

typedef struct
{
	char *tokens[MAX_TOKENS];
	int   count;
} argv_t;

typedef struct command_t command_t;
typedef struct context_t context_t;

// typedef int ( *handler_fn ) ( context_t *ctx );

struct command_t
{
	const char *name;
	int         ( *fn ) ( context_t *ctx );
	command_t  *next;
	size_t      args;
	const char *help;
};

struct context_t
{
	command_t *command;
	void      *data;
	char      *line;
	argv_t    *av;
	int        idx;
	int        depth;
};

static int matches (
	const char *cmd,
	const char *pattern
) {

	size_t len = strlen ( cmd );
	return len > 0 && strncmp ( pattern, cmd, len ) == 0;
}

/* input : argc, argv
 * return : cat'ed buffer
 */
static char *args_parse (
	int   argc,
	char *argv[]
) {
	static char buf[MAX_LINE];
	buf[0] = '\0';
	for ( int i = 1; i < argc; i++ )
	{
		strcat ( buf, argv[i] );
		if ( i != argc - 1 )
		{
			strcat ( buf, " " );
		}
	}
	return buf;
}

static int args_tokenize (
	context_t *ctx
) {

	ctx->av->count = 0;
	char *tok      = strtok ( ctx->line, " \t\r\n" );

	while ( tok && ctx->av->count < MAX_TOKENS )
	{
		ctx->av->tokens[ctx->av->count++] = tok;
		tok                               = strtok ( NULL, " \t\r\n" );
	}

	return ctx->av->count;
}

static command_t *args_lookup (
	context_t *ctx
) {

	command_t *found = NULL;
	int        count = 0;

	command_t *table = ctx->command;
	char      *token = ctx->av->tokens[ctx->idx];

	for ( int i = 0; table[i].name != NULL; i++ )
	{

		// printf("look[%d]: %s =? %s (addr=%p)\n", i, token, table[i].name, (void*)&table[i]);

		if ( strcmp ( token, table[i].name ) == 0 )
		{
			return &table[i]; /* точное совпадение */
		}

		if ( matches ( token, table[i].name ) )
		{
			found = &table[i];
			count++;
		}
	}

	if ( count > 1 )
	{
		log ( "error: ambiguous command %s ", token );
		return NULL;
	}

	return found;
}

/* печатает команды таблицы и, рекурсивно, их подкоманды */
static void args_help (
	FILE            *out,
	const command_t *table,
	int              depth
) {

	for ( int i = 0; table[i].name != NULL; i++ )
	{
		const command_t *c = &table[i];

		fprintf ( out, "%*s%-*s %s\n", depth * 2, "", 16 - depth * 2, c->name, c->help ? c->help : "" );

		/* .next на ту же таблицу — цикл, не уходим в рекурсию */
		if ( c->next != NULL && c->next != table )
		{
			args_help ( out, c->next, depth + 1 );
		}
	}
}

static int args_dispatch ( context_t *ctx ) {

	// printf ( "ctx->idx=%d, ctx->av->count=%d \n", ctx->idx, ctx->av->count );

	if ( ctx->idx >= ctx->av->count )
	{
		// log_info ( "done " );
		return 0;
	}

	if ( ctx->av->tokens[ctx->idx] == NULL )
	{
		return 0;
	}

	command_t *table = ctx->command; /* таблица ТЕКУЩЕГО уровня — фиксируем ДО lookup */

	ctx->command = args_lookup ( ctx );
	if ( ctx->command == NULL )
	{
		log_info ( "error: unknown command <%s> ", ctx->av->tokens[ctx->idx] );
		return -1;
	}

	log ( "└─<%s>, args=%ld ", ctx->command->name, ctx->command->args );

	if ( ctx->command->fn == NULL && ctx->command->next == NULL )
	{
		log_info ( "error: <%s> has no handler and no subcommands (misconfigured table)", ctx->command->name );
		return -1;
	}

	if ( ctx->command->fn != NULL )
	{

		if ( ctx->idx + (int) ctx->command->args >= ctx->av->count )
		{
			log_info ( "usage: %s ", ctx->command->help );
			return -1;
		}

		log_info ( "[__call] <%p> ", ctx->command->fn );

		size_t saved_idx = ctx->idx;
		ctx->idx         = saved_idx + 1;

		/* return should be greater than 0 */
		int ret = ctx->command->fn ( ctx );

		if ( 0 > ret )
		{
			log_info ( "error: command execution <%s> ", ctx->command->name );
			return -1;
		}

		ctx->idx = saved_idx + ctx->command->args;
	}

	ctx->idx++;

	// ctx->idx += 1 + ( ctx->command->args != 0 ? ctx->command->args : 0 );

	if ( ctx->command->next == NULL )
	{
		return 0; /* лист без продолжения — команда полностью обработана */
	}

	// need to save! ;
	int self_loop = ( ctx->command->next == table ); /* .next указывает на ТУ ЖЕ таблицу, из которой искали c */

	ctx->command = ctx->command->next;

	/* команда без подкоманды — показываем, что можно дальше */
	if ( ctx->idx >= ctx->av->count && !self_loop )
	{
		printf ( "\navailable commands:\n" );
		args_help ( stdout, ctx->command, 1 );
		return 0;
	}

	if ( !self_loop )
	{
		ctx->depth++;
	}

	int ret = args_dispatch ( ctx );

	// post_call

	if ( !self_loop )
	{
		ctx->depth--;
	}

	return ret;
}

#endif
