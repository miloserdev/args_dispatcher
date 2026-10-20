#ifndef __DEBUG_H__
#define __DEBUG_H__

#define dbg( fmt, ... ) \
	printf ( "[dbg] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__ )

#define log( fmt, ... )                                            \
	for ( size_t i = 0; i < ctx->depth + 0; i++, printf ( "  " ) ) \
		;                                                          \
	printf ( "" fmt " \n", ##__VA_ARGS__ )

#ifdef __NCURSES_H

	#define log_info printw

#else

	#define log_info( fmt, ... )                                       \
		for ( size_t i = 0; i < ctx->depth + 1; i++, printf ( "  " ) ) \
			;                                                          \
		printf ( "" fmt " \n", ##__VA_ARGS__ )

#endif

#endif
