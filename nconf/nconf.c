/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2021-2024
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

/* types.h must be before stat.h on some platforms! */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STYLE_RESET "\033[0m"
#define STYLE_BOLD "\033[1m"

#define STR_EMPTY(str) (NULL == str ? "" : str)
#define FREE(ptr) do {\
		free(ptr);\
		ptr = NULL;\
	} while (0)
#define LENGTH(arr) (sizeof arr / sizeof *arr)
#define ZERO(x) memset(&x, 0, sizeof x)

enum {
	VAR_CPPFLAGS = 0,
	VAR_CFLAGS,
	VAR_LDFLAGS,
	VAR_LIBS,
	VAR_SRCS,
	VAR_NUM
};
struct VarArray {
	size_t	size;
	size_t	num;
	char	**array;
};

static void		append(char **, char const *);
static void		err_(char const *, ...);
static char const	*extract_args(struct VarArray *, char const *);
static void		free_vars(struct VarArray *);
static void		log_(char const *, ...);
static void		merge_vars(struct VarArray *, struct VarArray const *,
    struct VarArray const *);
static void		mkdirs(char const *);
static void		resolve_env(struct VarArray *, char const *);
static int		run(char **);
static char		*strdup_(char const *);
static char		*strndup_(char const *, size_t);
static void		try(void);
static void		try_done(void);
static int		try_module_branch(char const *);
static int		try_var(char const *);
static void		usage(int);
static void		var_array_grow(struct VarArray *, size_t);
static void		write_files(int);

static char const *g_arg0;
static struct VarArray g_env_vars[VAR_NUM];
static struct VarArray g_prev_vars[VAR_NUM];
static char *g_compiler;
static char *g_input_path;
static char *g_output_dir;
static char *g_dst_path;
static char *g_nconfing_path;
static char *g_nconfing_nconf_path;
static char *g_log_path;
static int g_line_no;
static char *g_module;
static char *g_branch;
static struct VarArray g_vars[VAR_NUM];
static int g_do_cflags;
static int g_do_link;
static int g_do_exec;
static struct {
	char	*module;
	char	*branch;
} g_done_array[20];
static unsigned g_done_num;

void
append(char **a_dst, char const *a_app)
{
	char *new;
	size_t old_len, app_len;

	old_len = NULL == *a_dst ? 0 : strlen(*a_dst);
	app_len = strlen(a_app);
	new = malloc(old_len + app_len + 1);
	memcpy(new, *a_dst, old_len);
	memcpy(new + old_len, a_app, app_len);
	new[old_len + app_len] = '\0';
	free(*a_dst);
	*a_dst = new;
}

void
err_(char const *a_fmt, ...)
{
	va_list args;
	int errno_;

	errno_ = errno;
	va_start(args, a_fmt);
	vfprintf(stderr, a_fmt, args);
	va_end(args);
	fprintf(stderr, ": %s.\n", strerror(errno_));
	exit(EXIT_FAILURE);
}

char const *
extract_args(struct VarArray *a_array, char const *a_str)
{
	char const *p, *start;

	/* Extract words and resolve $.*. */
	for (p = a_str;;) {
		char *value;
		size_t len;
		int is_shell;

		for (; isspace(*p); ++p)
			;
#define IS_END '\0' == *p || ('*' == p[0] && '/' == p[1])
		if (IS_END) {
			return p;
		}
		/* TODO: Shell invocation could be recursive. */
		is_shell = 0;
		for (start = p; ; ++p) {
			if (IS_END) {
				break;
			}
			if (is_shell) {
				if (')' == *p) {
					is_shell = 0;
				}
			} else if ('$' == p[0] && '(' == p[1]) {
				is_shell = 1;
			} else if (isspace(*p)) {
				break;
			}
		}
#undef IS_END
		len = p - start;
		value = strndup_(start, len);
		resolve_env(a_array, value);
		free(value);
	}
}

void
free_vars(struct VarArray *a_vars)
{
	size_t i;

	for (i = 0; i < a_vars->num; ++i) {
		FREE(a_vars->array[i]);
	}
	a_vars->num = 0;
}

void
log_(char const *a_fmt, ...)
{
	FILE *file;
	va_list args;

	file = fopen(g_log_path, "a");
	if (NULL == file) {
		err_("fopen(%s)", g_log_path);
	}
	va_start(args, a_fmt);
	vfprintf(file, a_fmt, args);
	va_end(args);
	if (0 != fclose(file)) {
		err_("fclose(%s)", g_log_path);
	}
}

void
merge_vars(struct VarArray *a_dst, struct VarArray const *a_left, struct
    VarArray const *a_right)
{
	size_t i;

	var_array_grow(a_dst, a_left->num + a_right->num);
	a_dst->num = 0;
	for (i = 0; i < a_left->num; ++i) {
		a_dst->array[i] = strdup_(a_left->array[i]);
		++a_dst->num;
	}
	for (i = 0; i < a_right->num; ++i) {
		size_t j;

		for (j = 0; j < a_left->num; ++j) {
			/* Don't add duplicates. */
			if (0 == strcmp(a_left->array[j], a_right->array[i]))
			{
				break;
			}
		}
		if (a_left->num == j) {
			a_dst->array[a_dst->num] = strdup_(a_right->array[i]);
			++a_dst->num;
		}
	}
}

void
mkdirs(char const *a_path)
{
	char *tmp, *p;

	tmp = strdup_(a_path);
	for (p = tmp; '\0' != *p; ++p) {
		if ('/' == *p) {
			struct stat st;

			*p = '\0';
			if (-1 == stat(tmp, &st)) {
				if (ENOENT != errno) {
					err_("stat(%s)", tmp);
				}
				if (0 != mkdir(tmp, 0700)) {
					err_("mkdir(%s)", tmp);
				}
			}
			*p = '/';
		}
	}
	free(tmp);
}

/*
 * Look up $ENVVAR and ${ENVVAR}, $(...) is resolved recursively and run in a
 * shell.
 */
void
resolve_env(struct VarArray *a_array, char const *a_str)
{
	char *dst = NULL;
	char const *p;

	for (p = a_str; '\0' != *p;) {
		char *tmp;
		size_t i;

		if ('$' == *p) {
			size_t j;
			int do_shell;

			do_shell = 0;
			if ('(' == p[1]) {
				p += 2;
				for (i = 0; ')' != p[i]; ++i) {
					if ('\0' == p[i]) {
						/* TODO: FILE:LINE. */
						fprintf(stderr, "Unclosed "
						    "shell execution.\n");
						exit(EXIT_FAILURE);
					}
				}
				j = i + 1;
				do_shell = 1;
			} else if ('{' == p[1]) {
				p += 2;
				for (i = 0; '}' != p[i]; ++i) {
					if ('\0' == p[i]) {
						/* TODO: FILE:LINE. */
						fprintf(stderr, "Unclosed "
						    "env-var bracket.\n");
						exit(EXIT_FAILURE);
					}
				}
				j = i + 1;
			} else {
				++p;
				for (i = 0; '_' == p[i] || isalnum(p[i]); ++i)
					;
				j = i;
			}
			tmp = strndup_(p, i);
			if (do_shell) {
				/* Execute $(...). */
				FILE *pip;

				append(&tmp, " 2>/dev/null");
				pip = popen(tmp, "r");
				if (NULL == pip) {
					err_("pipe(%s)", tmp);
				}
				for (;;) {
					char line[80];
					char *q;

					if (NULL == fgets(line, sizeof line,
					    pip)) {
						break;
					}
					q = strchr(line, '\n');
					if (NULL != q) {
						*q = '\0';
					}
					append(&dst, line);
				}
				if (-1 == pclose(pip)) {
					err_("pclose(%s)", tmp);
				}
			} else {
				char const *value;

				value = getenv(tmp);
				if (NULL != value) {
					append(&dst, value);
				}
			}
			free(tmp);
			p += j;
		}
		for (i = 0; '\0' != p[i] && '$' != p[i]; ++i) {
			if ('\\' == p[i] && '$' == p[i + 1]) {
				++i;
			}
		}
		tmp = strndup_(p, i);
		append(&dst, tmp);
		free(tmp);
		p += i;
	}

	/* Extract words. */
	for (p = dst;;) {
		char const *start;
		char const *end;

		for (; isspace(*p); ++p)
			;
		if ('\0' == *p) {
			break;
		}
		start = p;
		for (; !isspace(*p) && '\0' != *p; ++p)
			;
		end = p;
		if (end != start) {
			char *word;

			word = strndup_(start, end - start);
			var_array_grow(a_array, 10);
			a_array->array[a_array->num++] = word;
		}
	}
	free(dst);
}

int
run(char **a_argv)
{
	char buf[256];
	int pip[2];
	pid_t pid;
	int fd, i, ok, status;

	log_("run: ");
	for (i = 0; NULL != a_argv[i]; ++i) {
		log_("%s ", a_argv[i]);
	}
	log_("\n");
	if (-1 == pipe(pip)) {
		err_("pipe");
	}
	pid = fork();
	if (-1 == pid) {
		err_("pid");
	}
	if (0 == pid) {
		if (-1 == dup2(pip[1], STDOUT_FILENO) ||
		    -1 == dup2(pip[1], STDERR_FILENO)) {
			err_("dup2");
		}
		if (0 != close(pip[0])) {
			err_("close");
		}
		if (0 != close(pip[1])) {
			err_("close");
		}
		execvp(a_argv[0], a_argv);
		err_("execvp(%s)", a_argv[0]);
	}
	if (0 != close(pip[1])) {
		err_("close");
	}
	fd = open(g_log_path, O_RDWR | O_APPEND);
	if (-1 == fd) {
		err_("open(%s)", g_log_path);
	}
	ok = 1;
	for (;;) {
		ssize_t ret, ret2;

		ret = read(pip[0], buf, sizeof buf);
		if (-1 == ret) {
			if (EINTR == errno) {
				continue;
			}
			err_("read");
		}
		if (0 == ret) {
			break;
		}
		ok = 0;
		ret2 = write(fd, buf, ret);
		if (ret2 != ret) {
			err_("write");
		}
	}
	if (0 != close(fd)) {
		err_("close(%s)", g_log_path);
	}
	if (-1 == wait(&status)) {
		err_("wait");
	}
	ok &= EXIT_SUCCESS == WEXITSTATUS(status);
	return ok;
}

char *
strdup_(char const *a_str)
{
	char *p;
	size_t len;

	len = strlen(a_str);
	p = malloc(len + 1);
	strcpy(p, a_str);
	return p;
}

char *
strndup_(char const *a_str, size_t a_len)
{
	char *p;

	p = malloc(a_len + 1);
	memcpy(p, a_str, a_len);
	p[a_len] = '\0';
	return p;
}

void
try()
{
	struct VarArray local_vars[VAR_NUM]; /* Without env vars. */
	struct VarArray full_vars[VAR_NUM];
	FILE *file;
	char *argv[1000];
	char *bin = NULL;
	char *obj = NULL;
	char *c = NULL;
	unsigned var_i, j;
	int argc, success;

	ZERO(local_vars);
	ZERO(full_vars);
	if (NULL == g_module) {
		return;
	}
	for (j = 0; j < g_done_num; ++j) {
		if (0 == strcmp(g_module, g_done_array[j].module)) {
			return;
		}
	}

	log_("=== %s:%s ===\n", g_module, g_branch);

	append(&bin, g_nconfing_nconf_path);
	append(&bin, ".bin");

	append(&obj, g_nconfing_nconf_path);
	append(&obj, ".o");

	append(&c, g_nconfing_nconf_path);
	append(&c, ".c");
	mkdirs(c);
	file = fopen(c, "w");
	if (NULL == file) {
		err_("fopen(%s)", c);
	}
	fprintf(file, "#define NCONFING 1\n");
	fprintf(file, "#define NCONFING_m%s 1\n", g_module);
	fprintf(file, "#include <%s>\n", g_input_path);
	fprintf(file, "int main(int argc, char **argv) {\n");
	fprintf(file, "\t(void)argc; (void)argv;\n");
	fprintf(file, "#ifdef NCONF_TEST\n");
	fprintf(file, "\treturn !(NCONF_TEST);\n");
	fprintf(file, "#else\n");
	fprintf(file, "#ifdef NCONF_TEST_VOID\n");
	fprintf(file, "\tNCONF_TEST_VOID;\n");
	fprintf(file, "#endif\n");
	fprintf(file, "\treturn 0;\n");
	fprintf(file, "#endif\n");
	fprintf(file, "}\n");
	if (0 != fclose(file)) {
		err_("fclose(%s)", c);
	}

	write_files(0);

	success = 1;

	/*
	 * Merge previous, current, and env variables, in very specific
	 * orders!
	 *  The new file:
	 *   CPPFLAGS: append  -> doesn't break old nconf search paths.
	 *   CFLAGS  : append  -> overrides compilation switches.
	 *   LDFLAGS : append  -> doesn't break old nconf search paths.
	 *   LIBS    : prepend -> rely on old nconf libraries.
	 *  Environment variables:
	 *   CPPFLAGS: prepend -> user can override search paths.
	 *   CFLAGS  : append  -> user can override compilation switches.
	 *   LDFLAGS : prepend -> user can override search paths.
	 *   LIBS    : append  -> user can add missing libraries.
	 */
	var_i = VAR_CPPFLAGS;
	merge_vars(&local_vars[var_i], &g_prev_vars[var_i], &g_vars[var_i]);
	++var_i;
	merge_vars(&local_vars[var_i], &g_prev_vars[var_i], &g_vars[var_i]);
	++var_i;
	merge_vars(&local_vars[var_i], &g_prev_vars[var_i], &g_vars[var_i]);
	++var_i;
	merge_vars(&local_vars[var_i], &g_vars[var_i], &g_prev_vars[var_i]);

	var_i = VAR_CPPFLAGS;
	merge_vars(&full_vars[var_i], &g_env_vars[var_i], &local_vars[var_i]);
	++var_i;
	merge_vars(&full_vars[var_i], &local_vars[var_i], &g_env_vars[var_i]);
	++var_i;
	merge_vars(&full_vars[var_i], &g_env_vars[var_i], &local_vars[var_i]);
	++var_i;
	merge_vars(&full_vars[var_i], &local_vars[var_i], &g_env_vars[var_i]);

	/* Compile. */
	argc = 0;
	argv[argc++] = g_compiler;
	argv[argc++] = "-o";
	argv[argc++] = obj;
	argv[argc++] = c;
	argv[argc++] = "-I.";
	argv[argc++] = "-I";
	argv[argc++] = g_nconfing_path;
	argv[argc++] = "-c";
	for (var_i = VAR_CPPFLAGS; var_i <= VAR_CFLAGS; ++var_i) {
		if (!g_do_cflags && var_i == VAR_CFLAGS) {
			continue;
		}
		for (j = 0; j < full_vars[var_i].num; ++j) {
			argv[argc++] = full_vars[var_i].array[j];
		}
	}
	argv[argc++] = NULL;
	if (!run(argv)) {
		success = 0;
	} else if (g_do_link) {
		/* Link, reuse previous args. */
		argv[2] = bin;
		argv[3] = obj;
		argc = 4;
		/* Keep compilation flags for extra sources. */
		for (var_i = VAR_CFLAGS; var_i <= VAR_LIBS; ++var_i) {
			for (j = 0; j < full_vars[var_i].num; ++j) {
				argv[argc++] = full_vars[var_i].array[j];
			}
		}
		for (j = 0; j < g_vars[VAR_SRCS].num; ++j) {
			argv[argc++] = g_vars[VAR_SRCS].array[j];
		}
		argv[argc++] = NULL;
		if (!run(argv)) {
			success = 0;
		} else if (g_do_exec) {
			/* Run. */
			argc = 0;
			argv[argc++] = bin;
			argv[argc++] = NULL;
			success = run(argv);
		}
	}

	free(c);
	free(obj);
	free(bin);

	for (var_i = VAR_CPPFLAGS; var_i <= VAR_LIBS; ++var_i) {
		free_vars(&full_vars[var_i]);
	}
	if (success) {
		g_done_array[g_done_num].module = strdup_(g_module);
		g_done_array[g_done_num].branch = strdup_(g_branch);
		/* Save without env vars. */
		for (var_i = VAR_CPPFLAGS; var_i <= VAR_LIBS; ++var_i) {
			free_vars(&g_prev_vars[var_i]);
			memcpy(&g_prev_vars[var_i], &local_vars[var_i], sizeof
			    local_vars[var_i]);
		}
		++g_done_num;
		log_("Success!\n");
		printf(STYLE_BOLD "%s:%s\n" STYLE_RESET, g_module, g_branch);
	} else {
		for (var_i = VAR_CPPFLAGS; var_i <= VAR_LIBS; ++var_i) {
			free_vars(&local_vars[var_i]);
		}
		log_("Failed.\n");
	}
}

void
try_done()
{
	FILE *file;
	unsigned i;

	for (i = 0; i < g_done_num; ++i) {
		if (0 == strcmp(g_module, g_done_array[i].module)) {
			return;
		}
	}
	fprintf(stderr, "%s: Could not configure module %s, %s:\n",
	    g_input_path, g_module, g_log_path);
	/* Dump log file. */
	file = fopen(g_log_path, "r");
	if (NULL == file) {
		err_("fopen(%s)", g_log_path);
	}
	for (;;) {
		char buf[256];

		if (NULL == fgets(buf, sizeof buf, file)) {
			break;
		}
		fprintf(stderr, "%s", buf);
	}
	if (0 != fclose(file)) {
		err_("fclose(%s)", g_log_path);
	}
	exit(EXIT_FAILURE);
}

int
try_module_branch(char const *a_line)
{
	char const *p, *start;
	char *module;
	size_t len, var_i;

	/* Find NCONF_mMODULE_bBRANCH. */
	p = a_line;
	if ('#' != *p) {
		return 0;
	}
	p = strstr(p + 1, "NCONF_m");
	if (NULL == p) {
		return 0;
	}
	p += 7;

	/* We're knee-deep now, try the previous option if available. */
	try();

	for (start = p;; ++p) {
		if ('\0' == *p) {
			fprintf(stderr, "%s:%d: Missing branch.\n",
			    g_input_path, g_line_no);
			exit(EXIT_FAILURE);
		}
		if ('_' == p[0] && 'b' == p[1]) {
			break;
		}
	}
	len = p - start;
	module = strndup_(start, len);
	if (NULL != g_module && 0 != strcmp(module, g_module)) {
		try_done();
	}
	free(g_module);
	g_module = module;
	p += 2;
	for (start = p; '_' == *p || isalnum(*p); ++p)
		;
	len = p - start;
	g_branch = strndup_(start, len);

	g_do_cflags = 1;
	g_do_link = 1;
	g_do_exec = 1;

	for (var_i = 0; var_i < LENGTH(g_vars); ++var_i) {
		free_vars(&g_vars[var_i]);
	}

	return 1;
}

int
try_var(char const *a_line)
{
	char const *p;
	size_t var_i;

	/*
	 * Find commented NCONF_{CPPFLAGS,CFLAGS,LDFLAGS,LIBS,NOLINK,NOEXEC}.
	 */
	p = a_line;
	if ('/' != p[0] && '*' != p[1]) {
		return 0;
	}
	for (p += 2; isspace(*p); ++p)
		;
	if (0 != strncmp(p, "NCONF_", 6)) {
		return 0;
	}
	p += 6;
	if (0 == strncmp(p, "CPPFLAGS", 8)) {
		var_i = VAR_CPPFLAGS;
		p += 8;
	} else if (0 == strncmp(p, "CFLAGS", 6)) {
		var_i = VAR_CFLAGS;
		p += 6;
	} else if (0 == strncmp(p, "LDFLAGS", 7)) {
		var_i = VAR_LDFLAGS;
		p += 7;
	} else if (0 == strncmp(p, "LIBS", 4)) {
		var_i = VAR_LIBS;
		p += 4;
	} else if (0 == strncmp(p, "SRCS", 4)) {
		var_i = VAR_SRCS;
		p += 4;
	} else if (0 == strncmp(p, "NOCFLAGS", 8)) {
		g_do_cflags = 0;
		return 1;
	} else if (0 == strncmp(p, "NOLINK", 6)) {
		g_do_link = 0;
		return 1;
	} else if (0 == strncmp(p, "NOEXEC", 6)) {
		g_do_exec = 0;
		return 1;
	} else {
		fprintf(stderr, "%s:%d: Invalid NCONF_var.\n", g_input_path,
		    g_line_no);
		exit(EXIT_FAILURE);
	}
	for (; isspace(*p); ++p)
		;
	if ('=' != *p) {
		fprintf(stderr, "%s:%d: Missing '='.\n", g_input_path,
		    g_line_no);
		exit(EXIT_FAILURE);
	}
	for (++p; isspace(*p); ++p)
		;
	p = extract_args(&g_vars[var_i], p);
	if ('\0' == *p) {
		fprintf(stderr, "%s:%d: EOL.\n", g_input_path, g_line_no);
		exit(EXIT_FAILURE);
	}
	return 1;
}

void
usage(int a_code)
{
	FILE *str;

	str = EXIT_SUCCESS == a_code ? stdout : stderr;
	fprintf(str, "Usage: %s [-h] -i: -o: [-c:] [-a:]\n", g_arg0);
	fprintf(str, " -h  Help!\n");
	fprintf(str, " -c  Compiler command.\n");
	fprintf(str, " -i  Input file path.\n");
	fprintf(str, " -o  Output directory.\n");
	fprintf(str, " -a  Config to use and update.\n");
	exit(a_code);
}

void
var_array_grow(struct VarArray *a_array, size_t a_size0)
{
	void *p;
	size_t size;

	if (0 == a_array->size) {
		size = a_size0;
	} else if (a_array->num >= a_array->size) {
		size = a_array->size * 2;
	} else {
		return;
	}
	p = malloc(size * sizeof *a_array->array);
	memcpy(p, a_array->array, a_array->num * sizeof *a_array->array);
	free(a_array->array);
	a_array->size = size;
	a_array->array = p;
}

void
write_files(int a_is_final)
{
	FILE *file;
	char *path;
	char *guard_name = NULL;
	char *p;
	unsigned i;

	path = strdup_(a_is_final ? g_dst_path : g_nconfing_nconf_path);

	append(&guard_name, "NCONF_");
	append(&guard_name, g_input_path);
	for (p = guard_name + 6; '\0' != *p; ++p) {
		if (islower(*p)) {
			*p = toupper(*p);
		} else if (!isalnum(*p)) {
			*p = '_';
		}
	}

	mkdirs(path);
	file = fopen(path, "w");
	if (NULL == file) {
		err_("fopen(%s)", path);
	}
	fprintf(file, "#ifndef %s\n", guard_name);
	fprintf(file, "#define %s\n", guard_name);
	for (i = 0; i < g_done_num; ++i) {
		fprintf(file, "\t#define NCONF_m%s_b%s 1\n",
		    g_done_array[i].module, g_done_array[i].branch);
	}
	if (!a_is_final) {
		fprintf(file, "\t#define NCONF_m%s_b%s 1\n",
		    g_module, g_branch);
	}
	fprintf(file, "#endif\n");
	if (0 != fclose(file)) {
		err_("fclose(%s)", path);
	}
	free(guard_name);

	if (a_is_final) {
		unsigned j;

		append(&path, ".args");
		file = fopen(path, "w");
		if (NULL == file) {
			err_("fopen(%s)", path);
		}
		for (j = 0; j <= VAR_LIBS; ++j) {
			unsigned k;

			for (k = 0; k < g_prev_vars[j].num; ++k) {
				fprintf(file, "%s ", g_prev_vars[j].array[k]);
			}
			fprintf(file, "\n");
		}
		if (0 != fclose(file)) {
			err_("fclose(%s)", path);
		}
	}
	free(path);
}

int
main(int argc, char **argv)
{
	FILE *file;
	char *prev_args_path = NULL;
	unsigned i;
	int ok;

	g_arg0 = argv[0];

	for (i = 1; i < argc; ++i) {
		char const *arg;
		char **var_ptr;
		int opt;

		arg = argv[i];
		if ('-' != arg[0]) {
			fprintf(stderr, "Missing switch.\n");
			usage(EXIT_FAILURE);
		}
		opt = arg[1];
		var_ptr = NULL;
		switch (opt) {
		case 'c':
			var_ptr = &g_compiler;
			break;
		case 'a':
			var_ptr = &prev_args_path;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
		case 'i':
			var_ptr = &g_input_path;
			break;
		case 'o':
			var_ptr = &g_output_dir;
			break;
		default:
			fprintf(stderr, "Unknown argument -%c.\n", opt);
			usage(EXIT_FAILURE);
		}
		if (NULL != var_ptr) {
			if ('\0' == arg[2]) {
				++i;
				arg = argc == i ? "" : argv[i];
			} else {
				arg += 2;
			}
			*var_ptr = strdup_(arg);
		}
	}
	ok = 1;
	if (NULL == g_input_path) {
		fprintf(stderr, "No input file.\n");
		ok = 0;
	}
	if (NULL == g_output_dir) {
		fprintf(stderr, "No output directory.\n");
		ok = 0;
	}
	if (!ok) {
		fprintf(stderr, "Missing mandatory arguments.\n");
		usage(EXIT_FAILURE);
	}

	/* Get env vars. */
	{
		char const *p;

		p = getenv("CPPFLAGS");
		if (NULL != p) {
			extract_args(&g_env_vars[VAR_CPPFLAGS], p);
		}
		p = getenv("CFLAGS");
		if (NULL != p) {
			extract_args(&g_env_vars[VAR_CFLAGS], p);
		}
		p = getenv("LDFLAGS");
		if (NULL != p) {
			extract_args(&g_env_vars[VAR_LDFLAGS], p);
		}
		p = getenv("LIBS");
		if (NULL != p) {
			extract_args(&g_env_vars[VAR_LIBS], p);
		}
	}

	/* Get flags from previous nconf, if given. */
	if (NULL != prev_args_path) {
		file = fopen(prev_args_path, "r");
		if (NULL == file) {
			err_("fopen(%s)", prev_args_path);
		}
		for (i = 0; i <= VAR_LIBS; ++i) {
			char buf[BUFSIZ];
			size_t len;

			if (NULL == fgets(buf, sizeof buf, file)) {
				fprintf(stderr, "%s:%u: Unexpected EOF.\n",
				    prev_args_path, 1 + i);
				exit(EXIT_FAILURE);
			}
			len = strlen(buf);
			if (len + 1 >= sizeof buf || '\n' != buf[len - 1]) {
				fprintf(stderr, "%s:%u: Crazy long line.\n",
				    prev_args_path, 1 + i);
				exit(EXIT_FAILURE);
			}
			extract_args(&g_prev_vars[i], buf);
		}
		if (0 != fclose(file)) {
			err_("fclose(%s)", prev_args_path);
		}
	}

	/* Tidy up output files. */
	append(&g_dst_path, g_output_dir);
	append(&g_dst_path, "/nconf/");
	append(&g_dst_path, g_input_path);
	remove(g_dst_path);

	append(&g_nconfing_path, g_output_dir);
	append(&g_nconfing_path, "/nconfing/");
	remove(g_nconfing_path);

	append(&g_nconfing_nconf_path, g_output_dir);
	append(&g_nconfing_nconf_path, "/nconfing/nconf/");
	append(&g_nconfing_nconf_path, g_input_path);

	append(&g_log_path, g_dst_path);
	append(&g_log_path, ".log");
	remove(g_log_path);
	mkdirs(g_log_path);

	/* Parse input file. */
	file = fopen(g_input_path, "r");
	if (NULL == file) {
		err_("fopen(%s)", g_input_path);
	}
	for (g_line_no = 1;; ++g_line_no) {
		char line[256];
		char const *p;

		if (NULL == fgets(line, sizeof line, file)) {
			if (ferror(file)) {
				err_("%s:%d: fgets", g_input_path, g_line_no);
			}
			break;
		}
		for (p = line; isspace(*p); ++p)
			;
		if (try_module_branch(p)) {
			continue;
		}
		if (try_var(p)) {
			continue;
		}
	}
	if (0 != fclose(file)) {
		err_("fclose(%s)", g_input_path);
	}
	/* Try the last option. */
	try();
	if (NULL == g_module) {
		fprintf(stderr, "%s: Nothing to configure.\n", g_input_path);
		exit(EXIT_FAILURE);
	}
	try_done();
	write_files(1);
	return 0;
}
