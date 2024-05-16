/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2016, 2023-2024
 * Hans Toshihide Törnqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <module/reggen2/reggen2.h>
#include <module/reggen2/reggen2.tab.h>

extern int yyparse(void);

void usage(const char *);

void
usage(const char *s)
{
	fprintf(stderr, "Usage: %s <--offsets|--functions> module_name\n", s);
	abort();
}

int
main(int argc, char ** argv)
{
	char* module_name;
	char* dump_opt;

	(void) argc;

	if (argv[1] == NULL) { usage(argv[0]); }
	if (argv[2] == NULL) { usage(argv[0]); }

	dump_opt = argv[1];
	module_name = argv[2];

	register_init();

	if (0 != yyparse()) {
		abort();
	}

	register_dump(module_name, dump_opt);

	return 0;
}
