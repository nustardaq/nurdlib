/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015, 2019, 2023-2024
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


%{
	#include <module/reggen2/reggen2.h>
	#include <stdio.h>

	extern int yylineno;
	extern int yylex(void);
	void yyerror(char*);
	int yyparse(void);
	int g_arg1;
	int g_arg2;
	int g_arg3;
	int g_arg4;
%}

%union{
	unsigned int val_int;
	char* val_str;
};

%token OPEN_GROUP
%token CLOSE_GROUP
%token OPEN_BRACE
%token CLOSE_BRACE
%token COMMA
%token SEPARATOR
%token <val_int> HEX
%token <val_int> INTEGER
%token <val_str> ID
%token <val_int> ONE

%%

input:
	block
	| block input

block:
	block_header OPEN_GROUP CLOSE_GROUP {
		register_block_end();
	}
	| block_header OPEN_GROUP block_body CLOSE_GROUP {
		register_block_end();
	}

block_header:
	ID OPEN_BRACE block_header_args CLOSE_BRACE {
		register_block_start($1, g_arg1, g_arg2, g_arg3, g_arg4);
		free($1);
	}
	| ID OPEN_BRACE CLOSE_BRACE {
		register_block_start($1, 1, 0, 0, 0);
		free($1);
	}
	| ID {
		register_block_start($1, 1, 0, 0, 0);
		free($1);
	}

block_header_args:
	INTEGER COMMA HEX {
		g_arg1 = $1;
		g_arg2 = $3;
		g_arg3 = 0;
		g_arg4 = 0;
	}
	| INTEGER COMMA HEX COMMA HEX {
		g_arg1 = $1;
		g_arg2 = $3;
		g_arg3 = $5;
		g_arg4 = 0;
	}
	| INTEGER COMMA HEX COMMA HEX COMMA HEX {
		g_arg1 = $1;
		g_arg2 = $3;
		g_arg3 = $5;
		g_arg4 = $7;
	}

block_body:
	register_definition SEPARATOR
	| register_definition SEPARATOR block_body

register_definition:
	ID COMMA HEX COMMA INTEGER COMMA ID {
		register_add($1, $3, $5, $7);
		free($1);
		free($7);
	}


%%

void
yyerror(char* s)
{
	fprintf(stderr, "ERROR on line %d: %s\n", yylineno, s);
}
