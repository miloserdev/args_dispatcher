#ifndef __POWER_H__
#define __POWER_H__

#include <stddef.h>

#include "../args.h"

static int power_on ( args_context_t *ctx ) {
	(void) ctx;
	log_info ( "[fn] power on" );
	return 0;
}

static int power_off ( args_context_t *ctx ) {
	(void) ctx;
	log_info ( "[fn] power off" );
	return 0;
}

static int power_status ( args_context_t *ctx ) {
	(void) ctx;
	log_info ( "[fn] power status: on" );
	return 0;
}

static args_command_t power_table[] = {
	{ "status", power_status, NULL, 0, "Show power status"     },
	{ "on",     power_on,     NULL, 0, "Power on the chassis"  },
	{ "off",    power_off,    NULL, 0, "Power off the chassis" },
	{ NULL,     NULL,         NULL, 0, NULL                    },
};

const args_command_t power = { .name = "power", .fn = NULL, .next = power_table };

#endif
