#ifndef _CMDLINE_H
#define _CMDLINE_H
#include<stdbool.h>
#include"boot.h"

// handle kernel cmdline option parse
typedef int option_handle(char*,char*);

// store kernel cmdline summary
struct boot_options{

	// end parse general options
	bool end;

	// verbose log
	bool verbose;

	// logfs block path
	char logfs_block[64];

	// log file name template
	char logfs_file[64];

	// boot config
	boot_config*config;
};

// option value type
enum value_type{
	NO_VALUE,       // accept AAA
	OPTIONAL_VALUE, // accept AAA=BBB, AAA
	REQUIRED_VALUE  // accept AAA=BBB
};

// 
struct cmdline_option{
	// handled key
	char*name;

	// always parse when end=true
	bool always;

	// option value type
	enum value_type type;

	// parser handler function
	option_handle*handler;
};

// src/cmdline/options.c: options to handle
extern struct cmdline_option*cmdline_options[];

// src/cmdline/options.c: boot options storage
extern struct boot_options boot_options;

// src/cmdline/options.c: default init list
extern char*init_list[];

// src/cmdline/options.c: firmware search path
extern char*firmware_list[32];

// src/cmdline/cmdline.c: parse converted params
extern void parse_params(keyval**params);

// src/cmdline/cmdline.c: convert cmdline to keyval array
extern int parse_cmdline(int fd);

// src/cmdline/cmdline.c: auto parse cmdline from /proc/cmdline
extern int load_cmdline();
#endif
