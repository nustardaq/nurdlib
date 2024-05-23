/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2015-2022, 2024
 * Bastian Löher
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

/*
 * Custom parser.
 * LL1, the idea is to keep a small look-ahead circular buffer, and to
 * determine what to do by simply looking at the next few characters.
 *
 * On entry for every "parse" function, the requested object should follow
 * immediately, i.e. all whitespace should have been parsed, except (of course
 * there are exceptions) for units.
 *
 * Minimal grammar enforced here, data (in)sanity should be checked by user
 * code.
 */

#include <config/parser.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nurdlib/base.h>
#include <nurdlib/config.h>
#include <nurdlib/log.h>
#include <util/fmtmod.h>
#include <util/string.h>

#define STR_BYTES 256

struct File {
	char	const *filename;
	FILE	*file;
	char	const *snippet;
	size_t	snippet_i;
	int	line_no;
	int	col_no;
	/* Exclude one char, user buffers need a terminator. */
	char	buf[STR_BYTES - 1];
	size_t	buf_ofs;
	size_t	buf_len;
	/*
	 * 0 = No comment.
	 * 1 = Single-line comment (#,//)
	 * 2 = Block comment (/ * ... * /)
	 */
	int	comment_state;
};

static void		advancec(struct File *, size_t);
static void		fatal(struct File *, char const *);
static int		file_getc(struct File *) FUNC_RETURNS;
static void		file_ungetc(struct File *, int);
static int		is_exactly_identifier(struct File *, char const *)
	FUNC_RETURNS;
static void		parse_block(struct File *, int);
static enum Keyword	parse_identifier(struct File *) FUNC_RETURNS;
static int32_t		parse_int32(struct File *, unsigned) FUNC_RETURNS;
static void		parse_literal_string(struct File *, char *);
static void		parse_scalar(struct File *, struct ScalarList *,
    unsigned);
static int		parse_sign(struct File *) FUNC_RETURNS;
static void		parse_unit(struct File *, struct ScalarList *,
    unsigned);
static void		parse_value(struct File *, struct ScalarList *,
    unsigned);
static void		parse_vector(struct File *, struct ScalarList *);
static int		peekc(struct File *, size_t) FUNC_RETURNS;
static int		skip_whitespace(struct File *);

/* Advances the look-ahead buffer. */
void
advancec(struct File *a_file, size_t a_num)
{
	size_t i;

	if (a_num > a_file->buf_len) {
		log_die(LOGL, "Tried to advance too much (request=%"PRIz" > "
		    "buf_len=%"PRIz").", a_num, a_file->buf_len);
	}
	for (i = 0; i < a_num; ++i) {
		++a_file->col_no;
		if ('\n' == a_file->buf[a_file->buf_ofs]) {
			++a_file->line_no;
			a_file->col_no = 1;
		}
		a_file->buf_ofs = (a_file->buf_ofs + 1) % LENGTH(a_file->buf);
	}
	a_file->buf_len -= a_num;
}

/* Prints a verbose parsing message and aborts. */
void
fatal(struct File *a_file, char const *a_msg)
{
	char buf[STR_BYTES];
	size_t i;

	for (i = 0; a_file->buf_len > i; ++i) {
		buf[i] = peekc(a_file, i);
	}
	buf[i] = '\0';
	log_die(LOGL, "%s:%d:%d:buf=\"%s\": %s.", a_file->filename,
	    a_file->line_no, a_file->col_no,
	    0 == a_file->buf_len ? "EOF" : buf,
	    a_msg);
}

int
file_getc(struct File *a_file)
{
	int c;

	if (NULL != a_file->file) {
		return fgetc(a_file->file);
	}
	assert(NULL != a_file->snippet);
	c = a_file->snippet[a_file->snippet_i];
	if ('\0' == c) {
		return EOF;
	}
	++a_file->snippet_i;
	return c;
}

void
file_ungetc(struct File *a_file, int a_ch)
{
	if (NULL != a_file->file) {
		if (EOF == ungetc(a_ch, a_file->file)) {
			fatal(a_file, "ungetc returned EOF");
		}
	} else {
		assert(NULL != a_file->snippet);
		if (0 == a_file->snippet_i) {
			fatal(a_file, "file_ungetc underflow");
		}
		--a_file->snippet_i;
		if (a_file->snippet[a_file->snippet_i] != a_ch) {
			fatal(a_file, "file_ungetc different char");
		}
	}
}

/* Compares the look-ahead buffer to given identifier. */
int
is_exactly_identifier(struct File *a_file, char const *a_name)
{
	size_t i;

	for (i = 0;; ++i) {
		int c;

		c = peekc(a_file, i);
		if ('\0' == a_name[i]) {
			return '_' != c && !isalnum(c);
		}
		if (a_name[i] != c) {
			return 0;
		}
	}
}

/* Parses at block level. */
void
parse_block(struct File *a_file, int a_is_root)
{
	for (;;) {
		struct ScalarList *scalar_list;
		enum Keyword identifier;
		int vector_index;
		int src_line_no, src_col_no;
		int block_has_params, c;

		/* Block-level parsing. */
		skip_whitespace(a_file);
		if ('\0' == peekc(a_file, 0)) {
			if (!a_is_root) {
				fatal(a_file, "Unexpected EOF");
			}
			break;
		}
		if ('}' == peekc(a_file, 0)) {
			if (a_is_root) {
				fatal(a_file, "Unexpected '}'");
			}
			parser_pop_block();
			advancec(a_file, 1);
			break;
		}

		/* Save location for later. */
		src_line_no = a_file->line_no;
		src_col_no = a_file->col_no;

		identifier = parse_identifier(a_file);
		if (KW_NONE == identifier) {
			fatal(a_file, "Expected identifier");
		}
		if (KW_INCLUDE == identifier) {
			char path[STR_BYTES];

			skip_whitespace(a_file);
			if ('"' != peekc(a_file, 0)) {
				fatal(a_file, "Expected '\"'");
			}
			parse_literal_string(a_file, path);
			parser_include_file(path, 1);
			continue;
		}
		if (KW_ID_SKIP == identifier) {
			scalar_list = parser_prepare_block(KW_ID_SKIP,
			    a_file->filename, src_line_no, src_col_no);
			skip_whitespace(a_file);
			c = peekc(a_file, 0);
			if ('(' == c) {
				advancec(a_file, 1);
				parse_vector(a_file, scalar_list);
			}
			parser_push_simple_block();
			continue;
		}
		if (KW_BARRIER == identifier) {
			parser_prepare_block(KW_BARRIER, a_file->filename,
			    src_line_no, src_col_no);
			parser_push_simple_block();
			continue;
		}
		skip_whitespace(a_file);
		c = peekc(a_file, 0);
		advancec(a_file, 1);
		block_has_params = 0;
		vector_index = -1;
		switch (c) {
		case '[':
			/* Parse vector index. */
			skip_whitespace(a_file);
			vector_index = parse_int32(a_file, 10);
			skip_whitespace(a_file);
			if (']' != peekc(a_file, 0)) {
				fatal(a_file, "Expected ']'");
			}
			advancec(a_file, 1);
			skip_whitespace(a_file);
			if ('=' != peekc(a_file, 0)) {
				fatal(a_file, "Expected '='");
			}
			advancec(a_file, 1);
			/* FALLTHROUGH */
		case '=':
			scalar_list = parser_push_config(identifier,
			    vector_index, a_file->filename, src_line_no,
			    src_col_no);
			skip_whitespace(a_file);
			vector_index = MAX(vector_index, 0);
			parse_value(a_file, scalar_list, vector_index);
			parser_check_log_level();
			break;
		case '(':
			block_has_params = 1;
			/* FALLTHROUGH */
		case '{':
			scalar_list = parser_prepare_block(identifier,
			    a_file->filename, src_line_no, src_col_no);
			skip_whitespace(a_file);
			if (block_has_params) {
				parse_vector(a_file, scalar_list);
				skip_whitespace(a_file);
				if ('{' != peekc(a_file, 0)) {
					if (KW_TAGS == identifier) {
						parser_push_block();
						parser_pop_block();
						break;
					}
					fatal(a_file, "Expected '{'");
				}
				advancec(a_file, 1);
			}
			parser_push_block();
			parse_block(a_file, 0);
			break;
		default:
			fatal(a_file, "Expected one of \"=[({\"");
		}
	}
}

/* Parses a new file. */
int
parse_file(char const *a_path)
{
	struct File cur;
	FILE *file;

	file = fopen(a_path, "rb");
	if (NULL == file) {
		return 0;
	}
	LOGF(info)(LOGL, "Opened '%s' {", a_path);
	cur.filename = a_path;
	cur.file = file;
	cur.snippet = NULL;
	cur.snippet_i = 0;
	cur.line_no = 1;
	cur.col_no = 1;
	cur.buf_ofs = 0;
	cur.buf_len = 0;
	cur.comment_state = 0;
	parse_block(&cur, 1);
	fclose(file);
	LOGF(info)(LOGL, "Closed '%s' }", a_path);
	return 1;
}

/* Parses an identifier into given string, if available. */
enum Keyword
parse_identifier(struct File *a_file)
{
	char token[STR_BYTES];
	size_t i;
	int c;

	c = peekc(a_file, 0);
	if ('_' != c && !isalpha(c)) {
		return KW_NONE;
	}
	i = 0;
	do {
		if (STR_BYTES - 1 == i) {
			fatal(a_file, "Over-sized identifier");
		}
		token[i++] = c;
		c = peekc(a_file, i);
	} while ('_' == c || isalnum(c));
	token[i] = '\0';
	advancec(a_file, i);
	return keyword_get_id(token);
}

/* Parses a 32-bit int in given base (only 10 or 16 make sense). */
int32_t
parse_int32(struct File *a_file, unsigned a_base)
{
	char token[STR_BYTES];
	unsigned i;

	assert(10 == a_base || 16 == a_base);
	for (i = 0;; ++i) {
		int c;

		c = peekc(a_file, i);
		if ((10 == a_base && !isdigit(c)) ||
		    (16 == a_base && !isxdigit(c))) {
			if (0 == i) {
				fatal(a_file, "Expected digit");
			}
			break;
		}
		token[i] = c;
	}
	token[i] = '\0';
	advancec(a_file, i);
	return strtoi32(token, NULL, a_base);
}

/* Parses a literal string. */
void
parse_literal_string(struct File *a_file, char *a_str)
{
	size_t i;

	assert('"' == peekc(a_file, 0));
	advancec(a_file, 1);
	for (i = 0;;) {
		int c;

		c = peekc(a_file, i);
		if ('"' == c) {
			break;
		}
		a_str[i++] = c;
	}
	a_str[i] = '\0';
	advancec(a_file, i + 1);
}

/* Parses a scalar. */
void
parse_scalar(struct File *a_file, struct ScalarList *a_scalar_list, unsigned
    a_vector_index)
{
	char token[STR_BYTES];
	double d;
	enum Keyword keyword;
	size_t i, j;
	unsigned digit_num;
	int is_integer;

	/* Keyword? */
	keyword = parse_identifier(a_file);
	if (KW_NONE != keyword) {
		parser_push_keyword(a_scalar_list, a_vector_index, keyword);
		return;
	}
	/* Literal string? */
	if ('"' == peekc(a_file, 0)) {
		parse_literal_string(a_file, token);
		parser_push_string(a_scalar_list, a_vector_index, token);
		return;
	}
	/* Hex? */
	if ('0' == peekc(a_file, 0) && 'x' == peekc(a_file, 1)) {
		advancec(a_file, 2);
		parser_push_integer(a_scalar_list, a_vector_index,
		    parse_int32(a_file, 16));
		return;
	}
	if ('-' == peekc(a_file, 0) && '0' == peekc(a_file, 1) &&
	     'x' == peekc(a_file, 2)) {
		advancec(a_file, 3);
		parser_push_integer(a_scalar_list, a_vector_index,
		    -parse_int32(a_file, 16));
		return;
	}

	/*
	 * Hmm, either a double or an integer...
	 * A double is recognized by a single decimal sign, whereas an integer
	 * may be followed by a range specifier (..).
	 */
	i = 0;
	digit_num = 0;
	is_integer = 0;
	if ('+' == peekc(a_file, 0) || '-' == peekc(a_file, 0)) {
		++i;
	}
	while (isdigit(peekc(a_file, i))) {
		++i;
		++digit_num;
	}
	if ('.' != peekc(a_file, i)) {
		is_integer = 1;
	} else {
		if ('.' == peekc(a_file, i + 1)) {
			is_integer = 1;
		} else {
			++i;
		}
	}
	if (is_integer) {
		int sign, value;

		sign = parse_sign(a_file);
		value = sign * parse_int32(a_file, 10);
		skip_whitespace(a_file);
		/* If there's a range specifier next, parse a range. */
		if ('.' == peekc(a_file, 0) && '.' == peekc(a_file, 1)) {
			int value2;

			advancec(a_file, 2);
			skip_whitespace(a_file);
			value2 = parse_int32(a_file, 10);
			if (0 > value) {
				fatal(a_file, "Range limits must be "
				    "non-negative");
			}
			parser_push_range(a_scalar_list, a_vector_index,
			    value, value2);
			return;
		}
		/* No range, integer + possible unit then. */
		parser_push_integer(a_scalar_list, a_vector_index, value);
		parse_unit(a_file, a_scalar_list, a_vector_index);
		return;
	}
	/* So let's cover the full double. */
	while (isdigit(peekc(a_file, i))) {
		++i;
		++digit_num;
	}
	if (0 == digit_num) {
		fatal(a_file, "Syntax error");
	}
	if ('e' == peekc(a_file, i) || 'E' == peekc(a_file, i)) {
		j = ('+' == peekc(a_file, i + 1) ||
		    '-' == peekc(a_file, i + 1)) ? i + 1 : i;
		if (isdigit(peekc(a_file, j))) {
			i = j;
			while (isdigit(peekc(a_file, i))) {
				++i;
			}
		}
	}
	for (j = 0; i > j; ++j) {
		token[j] = peekc(a_file, j);
	}
	token[i] = '\0';
	d = strtod(token, NULL);
	advancec(a_file, i);
	parser_push_double(a_scalar_list, a_vector_index, d);
	parse_unit(a_file, a_scalar_list, a_vector_index);
}

int
parse_sign(struct File *a_file)
{
	int c;

	c = peekc(a_file, 0);
	if ('-' == c) {
		advancec(a_file, 1);
		return -1;
	}
	if ('+' == c) {
		advancec(a_file, 1);
	}
	return 1;
}

/* Parses a config snippet in string format. */
void
parse_snippet(char const *a_str)
{
	struct File cur;

	LOGF(verbose)(LOGL, "Parsing snippet.");
	cur.filename = "<snippet>";
	cur.file = NULL;
	cur.snippet = a_str;
	cur.snippet_i = 0;
	cur.line_no = 1;
	cur.col_no = 1;
	cur.buf_ofs = 0;
	cur.buf_len = 0;
	cur.comment_state = 0;
	parse_block(&cur, 1);
	LOGF(verbose)(LOGL, "Parsed snippet.");
}

/* Parses a unit. */
void
parse_unit(struct File *a_file, struct ScalarList *a_scalar_list, unsigned
    a_vector_index)
{
	skip_whitespace(a_file);
	if (is_exactly_identifier(a_file, "ns")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_NS);
		advancec(a_file, 2);
	} else if (is_exactly_identifier(a_file, "ps")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_PS);
		advancec(a_file, 2);
	} else if (is_exactly_identifier(a_file, "us")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_US);
		advancec(a_file, 2);
	} else if (is_exactly_identifier(a_file, "ms")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_MS);
		advancec(a_file, 2);
	} else if (is_exactly_identifier(a_file, "s")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_S);
		advancec(a_file, 1);
	} else if (is_exactly_identifier(a_file, "V")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_V);
		advancec(a_file, 1);
	} else if (is_exactly_identifier(a_file, "mV")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_MV);
		advancec(a_file, 2);
	} else if (is_exactly_identifier(a_file, "kHz")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_KHZ);
		advancec(a_file, 3);
	} else if (is_exactly_identifier(a_file, "MHz")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_MHZ);
		advancec(a_file, 3);
	} else if (is_exactly_identifier(a_file, "B")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_B);
		advancec(a_file, 1);
	} else if (is_exactly_identifier(a_file, "KiB")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_KIB);
		advancec(a_file, 3);
	} else if (is_exactly_identifier(a_file, "MiB")) {
		parser_push_unit(a_scalar_list, a_vector_index,
		    CONFIG_UNIT_MIB);
		advancec(a_file, 3);
	}
}

/*
 * Parses a value, which is either a scalar or a vector of scalars, and
 * potentially creates a bitmask.
 */
void
parse_value(struct File *a_file, struct ScalarList *a_scalar_list, unsigned
    a_vector_index)
{
	if ('(' == peekc(a_file, 0)) {
		advancec(a_file, 1);
		skip_whitespace(a_file);
		parse_vector(a_file, a_scalar_list);
	} else {
		parse_scalar(a_file, a_scalar_list, a_vector_index);
	}
	parser_create_bitmask(a_scalar_list);
}

/* Parse a vector = grouped scalars. */
void
parse_vector(struct File *a_file, struct ScalarList *a_scalar_list)
{
	unsigned vector_index;

	if (')' == peekc(a_file, 0)) {
		advancec(a_file, 1);
		/* An empty vector should empty everything old. */
		parser_scalar_list_clear(a_scalar_list);
		return;
	}
	for (vector_index = 0;; ++vector_index) {
		int c;

		parse_scalar(a_file, a_scalar_list, vector_index);
		skip_whitespace(a_file);
		c = peekc(a_file, 0);
		if ('{' == c) {
			char token[10];
			unsigned i, num;

			advancec(a_file, 1);
			skip_whitespace(a_file);
			for (i = 0;; ++i) {
				c = peekc(a_file, 0);
				if (!isdigit(c)) {
					break;
				} else if (sizeof token == i) {
					fatal(a_file, "Element count too "
					    "large");
				}
				token[i] = c;
				advancec(a_file, 1);
			}
			skip_whitespace(a_file);
			c = peekc(a_file, 0);
			if ('}' != c) {
				fatal(a_file, "Expected '}'");
			}
			token[i] = '\0';
			num = strtol(token, NULL, 10);
			parser_copy_scalar(a_scalar_list, vector_index, num -
			    1);
			vector_index += num - 1;
			advancec(a_file, 1);
			skip_whitespace(a_file);
			c = peekc(a_file, 0);
		}
		if (')' == c) {
			advancec(a_file, 1);
			return;
		} else if (',' != c) {
			/* Do not call 'advancec' before this message! */
			fatal(a_file, "Expected ')' or ','");
		}
		advancec(a_file, 1);
		skip_whitespace(a_file);
	}
}

/* Peek at the character at "a_ofs", comments skipped already here. */
int
peekc(struct File *a_file, size_t a_ofs)
{
	int c;

	if (LENGTH(a_file->buf) <= a_ofs) {
		fatal(a_file, "Peeking too far");
	}
	if (a_ofs >= a_file->buf_len) {
		size_t i;

		c = 0;
		i = (a_file->buf_ofs + a_file->buf_len) % LENGTH(a_file->buf);
		while (a_file->buf_len <= a_ofs) {
			do {
				int c2;

sneaky_continue:
				c = file_getc(a_file);
				if (EOF == c) {
					if (2 == a_file->comment_state) {
						fatal(a_file, "Block comment "
						    "not closed");
					}
					/*
					 * EOF -> nothing will ever show up
					 * here, and we'd rather not return
					 * -1 = \377.
					 */
					return 0;
				}
				if (0 == a_file->comment_state) {
					if ('#' == c) {
						a_file->comment_state = 1;
					} else if ('/' == c) {
						c2 = file_getc(a_file);
						if ('/' == c2) {
							a_file->comment_state
							    = 1;
						} else if ('*' == c2) {
							a_file->comment_state
							    = 2;
						} else {
							file_ungetc(a_file,
							    c2);
						}
					}
				} else if (1 == a_file->comment_state) {
					if ('\n' == c) {
						a_file->comment_state = 0;
					}
				} else {
					if ('*' == c) {
						c2 = file_getc(a_file);
						if ('/' == c2) {
							a_file->comment_state
							    = 0;
							/*
							 * This bypasses the
							 * do...while
							 * condition.
							 */
							goto sneaky_continue;
						}
						file_ungetc(a_file, c2);
					} else if ('\n' == c) {
						/*
						 * TODO: Counting newlines
						 * in the peek-ahead buffer
						 * could overflow with lots of
						 * commented modules configs,
						 * not nice...
						 */
						break;
					}
				}
			} while (0 != a_file->comment_state);
			a_file->buf[i] = c;
			i = (i + 1) % LENGTH(a_file->buf);
			++a_file->buf_len;
		}
	}
	c = a_file->buf[(a_file->buf_ofs + a_ofs) % LENGTH(a_file->buf)];
	/*
	 * Not exactly house trained, but if the user uses this char... That
	 * person is not house trained :p (This is why we have unit testing.)
	 */
	return 255 == c ? 0 : c;
}

/* Skip whitespaces. */
int
skip_whitespace(struct File *a_file)
{
	int hit;

	hit = 0;
	while (isspace(peekc(a_file, 0))) {
		advancec(a_file, 1);
		hit = 1;
	}
	return hit;
}
