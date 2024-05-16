/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015, 2019, 2021, 2024
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


%option nounput
%option noyywrap
%option yylineno

%{
	#include <module/reggen2/reggen2.tab.h>
	#include <util/string.h>

	#if 1
	#define PRINTD(x)
	#define PRINTD2(x,y)
	#else
	#define PRINTD(x) printf(x);
	#define PRINTD2(x,y) printf(x,y);
	#endif

	void yyerror(char*);
	int yylex(void);
%}

D [0-9]
H [0-9a-fA-F]
ID [a-z][a-z0-9_]*
HEX "0x"{H}*
INTEGER {D}+
ACCESS [rw]+


%%

 /* Grammar */
"("	{ PRINTD("OBRACE "); return OPEN_BRACE; }
")"	{ PRINTD("EBRACE "); return CLOSE_BRACE; }
"{"	{ PRINTD("OGROUP "); return OPEN_GROUP; }
"}"	{ PRINTD("EGROUP "); return CLOSE_GROUP; }
","	{ PRINTD("COMMA "); return COMMA; }
";"	{ PRINTD("SEP "); return SEPARATOR; }

{HEX} { sscanf(yytext,"0x%x",&yylval.val_int);
	PRINTD("HEX "); return HEX; }
{INTEGER} { yylval.val_int = atoi(yytext);
	PRINTD("INT "); return INTEGER; }
{ID} { yylval.val_str = strdup(yytext);
	PRINTD2("ID(%s) ",yytext); return ID; }


 /* Whitespace */
\n      { PRINTD("WHITESPACE\n"); /*next();*/ }
[ \t]+ { }

 /* Handle comments c-style */
"/*" {
	for (;;) {
		char c = 0;
		while ((c = input()) != '*' && c != '\0')
			; /* eat up text of comment */
		if (c == '*') {
			while ((c = input()) == '*')
				;
			if (c == '/')
				break; /* found the end */
		}
		if (c == '\0') {
			yyerror ("EOF in comment");
			break;
		}
	}
}

 /*Anything else is invalid */
.       { yyerror("invalid token"); return 1;}

%%
