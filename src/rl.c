

#include <stdio.h>
#include <stdlib.h>
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
#include <readline/history.h>
#include <readline/readline.h>

command_t *g_leaf_found_in = NULL;

static int is_terminal_leaf ( command_t *leaf,
							  command_t *found_in ) {
	if ( leaf->next == NULL )
	{
		return 1; /* обычный тупик */
	}
	if ( leaf->next == found_in )
	{
		return 1; /* self-loop: команда сама уже завершена,
					  next нужен только для СЛЕДУЮЩЕЙ команды,
					  а не для продолжения ЭТОЙ */
	}
	return 0;
}

/* Находит таблицу текущего уровня по уже введённым (завершённым) токенам.
 * table_out — куда писать таблицу для completion следующего слова.
 * leaf_out  — если весь ввод уже указывает на конкретный лист (команду
 *             с .fn != NULL), сюда пишется указатель на неё — пригодится для help.
 * Возвращает 0 при успехе, -1 если путь по дереву не совпал ни с чем.
 */
static int walk_tree ( const char *completed_part,
					   command_t  *root,
					   command_t **table_out,
					   command_t **leaf_out ) {
	char buf[MAX_LINE];
	strncpy ( buf, completed_part, sizeof ( buf ) - 1 );
	buf[sizeof ( buf ) - 1] = '\0';

	command_t *table      = root;
	command_t *found_in   = root; /* таблица, где был найден ПОСЛЕДНИЙ токен */
	command_t *last_exact = NULL;

	char *tok = strtok ( buf, " \t" );
	while ( tok != NULL )
	{
		command_t *found = NULL;
		int        count = 0;
		for ( int i = 0; table[i].name != NULL; i++ )
		{
			if ( strcmp ( tok, table[i].name ) == 0 )
			{
				found = &table[i];
				count = 1;
				break;
			}
			if ( matches ( tok, table[i].name ) )
			{
				found = &table[i];
				count++;
			}
		}
		if ( found == NULL || count > 1 )
		{
			return -1;
		}

		last_exact = found;
		found_in   = table; /* запоминаем, ИЗ какой таблицы взяли этот токен */

		if ( found->next != NULL )
		{
			table = found->next;
		}

		tok = strtok ( NULL, " \t" );
	}

	*table_out      = table;
	*leaf_out       = last_exact;
	g_leaf_found_in = found_in; /* или верни третьим out-параметром вместо глобала */
	return 0;
}

static command_t *g_completion_table = NULL;

static char *command_generator ( const char *text,
								 int         state ) {
	static int index;

	if ( state == 0 )
	{
		index = 0;
	}

	if ( g_completion_table == NULL )
	{
		return NULL;
	}

	size_t len = strlen ( text );
	while ( g_completion_table[index].name != NULL )
	{
		const char *name = g_completion_table[index++].name;
		if ( strncmp ( name, text, len ) == 0 )
		{
			return strdup ( name );
		}
	}
	return NULL;
}

/* Заглушка — сюда позже добавишь реальный вывод help (описание, usage, примеры) */
static void print_help ( command_t *cmd ) {
	printf ( "\n%s\n", cmd->name );

	if ( cmd->help != NULL )
	{
		printf ( "  -> %s\n", cmd->help );
	} else
	{
		printf ( "  (help not written yet)\n" );
	}
}

static char **args_completion ( const char *text,
								int         start,
								int         end ) {
	(void) end;
	rl_attempted_completion_over = 1;

	char completed_part[MAX_LINE];
	int  len = start < (int) sizeof ( completed_part ) - 1 ? start : (int) sizeof ( completed_part ) - 1;
	memcpy ( completed_part, rl_line_buffer, len );
	completed_part[len] = '\0';

	command_t *table = NULL, *leaf = NULL;
	int        ok = walk_tree ( completed_part, cmd_table, &table, &leaf );

	// fprintf ( stderr, "\n[dbg] completed_part=\"%s\" text=\"%s\" ok=%d leaf=%s fn=%p table=%p found_in=%p\n", completed_part, text, ok, leaf ? leaf->name : "(null)", leaf ? (void *) leaf->fn : NULL, (void *) table, (void *) g_leaf_found_in );

	if ( ok != 0 )
	{
		return NULL;
	}

	if ( text[0] == '\0' && leaf != NULL && leaf->fn != NULL
		 && is_terminal_leaf ( leaf, g_leaf_found_in ) )
	{
		print_help ( leaf );
		rl_forced_update_display( );
		return NULL;
	}

	g_completion_table = table;
	return rl_completion_matches ( text, command_generator );
}

int main ( void ) {
	rl_attempted_completion_function = args_completion;

	char *line;
	while ( ( line = readline ( "> " ) ) != NULL )
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

		argv_t    av;
		context_t ctx = {
			.av      = &av,
			.line    = line,
			.command = cmd_table,
			.idx     = 0,
			.depth   = 0,
		};

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
