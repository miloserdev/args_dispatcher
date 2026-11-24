/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (c) 2026 miloserdev */
#ifndef __ARGS_H__
#define __ARGS_H__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"

#define MAX_TOKENS 32
#define MAX_LINE   256

typedef struct
{
	char *tokens[MAX_TOKENS];
	int   count;
} args_argv_t;

/* A "keyword [value]" option of a command; tables end with { NULL } */
typedef struct
{
	const char *key;
	int         flag;     /* 1: keyword without a value (up, onlink, ...) */
	int         required; /* 1: command fails when the keyword is missing  */
	const char *val;      /* out: value, "" for a present flag, NULL if absent */
} args_keyword_t;

typedef struct args_command_t args_command_t;
typedef struct args_context_t args_context_t;

struct args_command_t
{
	const char     *name;
	int             ( *fn ) ( args_context_t *ctx );
	args_command_t *next;
	size_t          args;
	const char     *help;
	args_keyword_t *kw; /* optional keywords following the positional arguments */
};

/* REPL part of the context; args_context_t.repl is NULL outside of a REPL */
typedef struct
{
	args_command_t *root; /* root command table (args_context_t.command moves while dispatching) */
} args_context_repl_t;

struct args_context_t
{
	args_command_t *command;
	void           *data;
	char           *line;
	args_argv_t    *av;
	int             idx;
	int             depth;

	args_context_repl_t *repl; /* NULL: not a REPL */
};

static int __matches (
	const char *cmd,
	const char *pattern
) {

	size_t len = strlen ( cmd );
	return len > 0 && strncmp ( pattern, cmd, len ) == 0;
}

#define ARGS_NONE      -1              /* args_find: nothing matches            */
#define ARGS_AMBIGUOUS -2              /* args_find: more than one prefix match */
#define ARGS_NT        ( (size_t) -1 ) /* args_find: array ends with a NULL name */

/* Looks up a token among the names of an array of structs: an exact match
 * wins, otherwise a unique prefix is accepted. Returns the element index,
 * ARGS_NONE or ARGS_AMBIGUOUS. n is the element count or ARGS_NT. */
static int args_find_name (
	const char *token,
	const void *base,
	size_t      stride,
	size_t      name_off,
	size_t      n
) {

	int hit  = ARGS_NONE;
	int hits = 0;

	for ( size_t i = 0; n == ARGS_NT || i < n; i++ )
	{
		const char *name = *(const char *const *) ( (const char *) base + i * stride + name_off );

		if ( name == NULL )
		{
			if ( n == ARGS_NT )
			{
				break;
			}
			continue;
		}

		if ( strcmp ( token, name ) == 0 )
		{
			return (int) i; /* exact match */
		}

		if ( __matches ( token, name ) )
		{
			hit = (int) i;
			hits++;
		}
	}

	return hits > 1 ? ARGS_AMBIGUOUS : hit;
}

#define args_find( token, arr, type, field, n ) \
	args_find_name ( token, arr, sizeof ( type ), offsetof ( type, field ), n )

/* input  : argc, argv, caller-owned buffer buf of `size` bytes
 * return : buf with the arguments joined by spaces,
 *          or NULL if the result does not fit into the buffer
 */
static inline char *args_parse (
	char  *buf,
	size_t size,
	int    argc,
	char  *argv[]
) {
	size_t pos = 0;

	buf[0] = '\0';
	for ( int i = 1; i < argc; i++ )
	{
		size_t len = strlen ( argv[i] );
		size_t sep = ( i != argc - 1 ) ? 1 : 0;

		/* len + sep + terminating '\0' must fit into the buffer */
		if ( len + sep >= size - pos )
		{
			return NULL;
		}

		memcpy ( buf + pos, argv[i], len );
		pos += len;
		if ( sep )
		{
			buf[pos++] = ' ';
		}
		buf[pos] = '\0';
	}
	return buf;
}

static int args_tokenize (
	args_context_t *ctx
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

static args_command_t *__args_lookup (
	args_context_t *ctx
) {

	args_command_t *table = ctx->command;
	char           *token = ctx->av->tokens[ctx->idx];

	int idx = args_find ( token, table, args_command_t, name, ARGS_NT );

	if ( idx == ARGS_AMBIGUOUS )
	{
		log ( "error: ambiguous command %s ", token );
		return NULL;
	}

	return idx == ARGS_NONE ? NULL : &table[idx];
}

/* prints the table's commands and, recursively, their subcommands */
static void args_help (
	FILE                 *out,
	const args_command_t *table,
	int                   depth
) {

	for ( int i = 0; table[i].name != NULL; i++ )
	{
		const args_command_t *c = &table[i];

		fprintf ( out, "%*s%-*s %s\n", depth * 2, "", 16 - depth * 2, c->name, c->help ? c->help : "" );

		/* .next pointing to the same table is a loop, do not recurse */
		if ( c->next != NULL && c->next != table )
		{
			args_help ( out, c->next, depth + 1 );
		}
	}
}

static int args_dispatch ( args_context_t *ctx ) {

	if ( ctx->idx >= ctx->av->count )
	{
		return 0;
	}

	if ( ctx->av->tokens[ctx->idx] == NULL )
	{
		return 0;
	}

	args_command_t *table = ctx->command; /* table of the CURRENT level - capture BEFORE lookup */

	ctx->command = __args_lookup ( ctx );
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

	if ( ctx->command->next == NULL )
	{
		return 0; /* leaf without continuation - command fully handled */
	}

	int self_loop = ( ctx->command->next == table ); /* .next points to the SAME table the command was looked up in */

	ctx->command = ctx->command->next;

	/* command without a subcommand - show what can follow */
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

	if ( !self_loop )
	{
		ctx->depth--;
	}

	return ret;
}

/* Parses the tokens following the command's positional arguments into the
 * command's keyword table (ctx->command->kw). Keywords may be abbreviated to
 * a unique prefix, like in iproute2.
 * Returns 0 on success, -1 (after logging the reason) on any error. */
static int args_parse_keyword (
	args_context_t *ctx
) {

	args_keyword_t *kw = ctx->command->kw;

	for ( int k = 0; kw != NULL && kw[k].key != NULL; k++ )
	{
		kw[k].val = NULL;
	}

	int i = ctx->idx + (int) ctx->command->args;

	while ( i < ctx->av->count )
	{
		const char *tok = ctx->av->tokens[i];

		/* exact match wins, otherwise a unique prefix is accepted: "d" -> "dev" */
		int k = kw == NULL ? ARGS_NONE : args_find ( tok, kw, args_keyword_t, key, ARGS_NT );

		if ( k == ARGS_AMBIGUOUS )
		{
			log_info ( "error: ambiguous keyword <%s>", tok );
			return -1;
		}

		if ( k == ARGS_NONE )
		{
			log_info ( "error: unknown keyword <%s>", tok );
			return -1;
		}

		args_keyword_t *hit = &kw[k];

		if ( hit->val != NULL )
		{
			log_info ( "error: keyword <%s> given twice", tok );
			return -1;
		}

		if ( hit->flag )
		{
			hit->val = "";
			i++;
			continue;
		}

		if ( i + 1 >= ctx->av->count )
		{
			log_info ( "error: keyword <%s> needs a value", tok );
			return -1;
		}

		hit->val  = ctx->av->tokens[i + 1];
		i        += 2;
	}

	for ( int k = 0; kw != NULL && kw[k].key != NULL; k++ )
	{
		if ( kw[k].required && kw[k].val == NULL )
		{
			log_info ( "error: missing required keyword <%s>", kw[k].key );
			return -1;
		}
	}

	return 0;
}

static int args_parse_uint (
	const char    *s,
	unsigned long  min,
	unsigned long  max,
	unsigned long *out
) {

	char *end;
	if ( *s < '0' || *s > '9' )
	{
		return -1;
	}
	unsigned long v = strtoul ( s, &end, 10 );
	if ( *end != '\0' || v < min || v > max )
	{
		return -1;
	}
	*out = v;
	return 0;
}

#ifdef ARGS_REPL

	#include <readline/history.h>
	#include <readline/readline.h>

static int __is_terminal_leaf ( args_command_t *leaf,
								args_command_t *found_in ) {
	if ( leaf->next == NULL )
	{
		return 1; /* ordinary dead end */
	}

	if ( leaf->next == found_in )
	{
		return 1; /* self-loop: the command is already complete,
					  next is only for the FOLLOWING command,
					  not a continuation of THIS one */
	}

	return 0;
}

typedef struct
{
	args_command_t *table;        /* table to complete command names from (NULL: none) */
	args_command_t *leaf;         /* last command found, if any                        */
	args_command_t *found_in;     /* table the last command was taken from             */
	args_keyword_t *kw;           /* set while the keywords of `leaf` are completed    */
	unsigned long   used;         /* bit k: keyword kw[k] is already on the line       */
	int             expect_value; /* last token is a keyword that still needs a value  */
} args_walk_t;

/* Walks the already typed (completed) tokens through the command tree:
 * command names, then the command's positional arguments, then its
 * "keyword [value]" pairs, and tells what can come next.
 * Also sets w->found_in to the table the last command was taken from.
 * Returns 0 on success, -1 if the input does not fit the tree.
 */
static int __args_walk_tree ( args_command_t *root,
							  const char     *completed_part,
							  args_walk_t    *w ) {
	char buf[MAX_LINE];
	strncpy ( buf, completed_part, sizeof ( buf ) - 1 );
	buf[sizeof ( buf ) - 1] = '\0';

	memset ( w, 0, sizeof ( *w ) );

	args_command_t *table    = root;
	args_command_t *found_in = table; /* table where the LAST command was found */

	char *tok = strtok ( buf, " \t" );
	while ( tok != NULL )
	{
		int idx = args_find ( tok, table, args_command_t, name, ARGS_NT );

		if ( idx < 0 )
		{
			return -1; /* nothing matched, or ambiguous */
		}

		args_command_t *found = &table[idx];

		w->leaf  = found;
		found_in = table; /* remember WHICH table this token was taken from */
		tok      = strtok ( NULL, " \t" );

		/* skip the positional arguments of the command */
		size_t need = found->fn != NULL ? found->args : 0;
		while ( need > 0 && tok != NULL )
		{
			tok = strtok ( NULL, " \t" );
			need--;
		}

		if ( need > 0 )
		{
			/* the cursor is inside a positional argument: nothing to complete */
			w->found_in = found_in;
			return 0;
		}

		if ( found->kw != NULL )
		{
			w->kw = found->kw;

			while ( tok != NULL )
			{
				int k = args_find ( tok, found->kw, args_keyword_t, key, ARGS_NT );

				if ( k < 0 )
				{
					return -1;
				}

				w->used |= 1UL << k;
				tok      = strtok ( NULL, " \t" );

				if ( !found->kw[k].flag )
				{
					if ( tok == NULL )
					{
						w->expect_value = 1;
						break;
					}
					tok = strtok ( NULL, " \t" ); /* skip the value */
				}
			}

			w->found_in = found_in;
			return 0;
		}

		if ( found->next != NULL )
		{
			table = found->next;
		} else if ( tok != NULL )
		{
			return -1; /* tokens after a command that takes no more input */
		}
	}

	w->table    = table;
	w->found_in = found_in;

	return 0;
}

/* readline-style match list: [0] is the common prefix (the single match when
 * there is only one), [1..] are the matches, NULL-terminated. Frees `v` and
 * returns NULL when there are no matches. n is the number of matches. */
static char **__match_list (
	char **v,
	size_t n
) {

	if ( n == 0 )
	{
		free ( v );
		return NULL;
	}

	if ( n == 1 )
	{
		v[0] = v[1];
		v[1] = NULL;
		return v;
	}

	size_t common = strlen ( v[1] );
	for ( size_t i = 2; i <= n; i++ )
	{
		size_t j = 0;
		while ( j < common && v[i][j] == v[1][j] )
		{
			j++;
		}
		common = j;
	}

	v[0] = malloc ( common + 1 );
	if ( v[0] == NULL )
	{
		for ( size_t i = 1; i <= n; i++ )
		{
			free ( v[i] );
		}
		free ( v );
		return NULL;
	}
	memcpy ( v[0], v[1], common );
	v[0][common] = '\0';
	v[n + 1]     = NULL;

	return v;
}

/* strdup() into v[++*n]; on failure frees everything collected so far */
static int __match_add (
	char       **v,
	size_t      *n,
	const char  *s
) {

	char *dup = strdup ( s );
	if ( dup == NULL )
	{
		for ( size_t i = 1; i <= *n; i++ )
		{
			free ( v[i] );
		}
		free ( v );
		return -1;
	}

	v[++*n] = dup;
	return 0;
}

/* names of `table` starting with `text` */
static char **__complete_commands (
	const args_command_t *table,
	const char           *text
) {

	size_t count = 0;
	while ( table[count].name != NULL )
	{
		count++;
	}

	char **v = calloc ( count + 2, sizeof ( *v ) );
	if ( v == NULL )
	{
		return NULL;
	}

	size_t n   = 0;
	size_t len = strlen ( text );
	for ( size_t i = 0; i < count; i++ )
	{
		if ( strncmp ( table[i].name, text, len ) == 0 )
		{
			if ( __match_add ( v, &n, table[i].name ) != 0 )
			{
				return NULL;
			}
		}
	}

	return __match_list ( v, n );
}

/* keywords of `kw` starting with `text` that are not marked in `used` yet */
static char **__complete_keywords (
	const args_keyword_t *kw,
	unsigned long         used,
	const char           *text
) {

	size_t count = 0;
	while ( kw[count].key != NULL )
	{
		count++;
	}

	char **v = calloc ( count + 2, sizeof ( *v ) );
	if ( v == NULL )
	{
		return NULL;
	}

	size_t n   = 0;
	size_t len = strlen ( text );
	for ( size_t k = 0; k < count; k++ )
	{
		if ( ( used >> k ) & 1UL )
		{
			continue;
		}
		if ( strncmp ( kw[k].key, text, len ) == 0 )
		{
			if ( __match_add ( v, &n, kw[k].key ) != 0 )
			{
				return NULL;
			}
		}
	}

	return __match_list ( v, n );
}

/* Stub - real help output (description, usage, examples) goes here later */
static void __print_help ( args_command_t *cmd ) {
	printf ( "\n%s\n", cmd->name );

	if ( cmd->help != NULL )
	{
		printf ( "  -> %s\n", cmd->help );
	} else
	{
		printf ( "  (help not written yet)\n" );
	}
}

/* Completion entry point for the application's readline callback:
 *   ctx    - context owned by the application, ctx->repl must be set
 *   line   - the whole line typed so far (rl_line_buffer)
 *   text   - the word being completed
 *   start  - offset of `text` in `line`
 * Returns a match list for readline, or NULL. */
static char **args_complete (
	args_context_t *ctx,
	const char     *line,
	const char     *text,
	int             start
) {
	rl_attempted_completion_over = 1;

	if ( ctx->repl == NULL )
	{
		return NULL;
	}

	char completed_part[MAX_LINE];
	int  len = start < (int) sizeof ( completed_part ) - 1 ? start : (int) sizeof ( completed_part ) - 1;
	memcpy ( completed_part, line, len );
	completed_part[len] = '\0';

	args_walk_t w;
	if ( __args_walk_tree ( ctx->repl->root, completed_part, &w ) != 0 )
	{
		return NULL;
	}

	/* the cursor is on a keyword (or on its value) of the last command */
	if ( w.kw != NULL )
	{
		if ( w.expect_value )
		{
			if ( text[0] == '\0' )
			{
				__print_help ( w.leaf );
				rl_forced_update_display( );
			}
			return NULL;
		}

		return __complete_keywords ( w.kw, w.used, text );
	}

	if ( text[0] == '\0' && w.leaf != NULL && w.leaf->fn != NULL
		 && __is_terminal_leaf ( w.leaf, w.found_in ) )
	{
		__print_help ( w.leaf );
		rl_forced_update_display( );
		return NULL;
	}

	if ( w.table == NULL )
	{
		return NULL;
	}

	return __complete_commands ( w.table, text );
}

#endif /* ARGS_REPL */

#endif
