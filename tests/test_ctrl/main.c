/*
 * nurdlib, NUstar ReaDout LIBrary
 *
 * Copyright (C) 2023-2025
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

#define _POSIX_C_SOURCE 199309L

#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <nurdlib.h>
#include <ctrl/ctrl.h>
#include <module/genlist.h>
#include <module/map/map.h>
#include <nurdlib/base.h>

#if HAS_MESYTEC_MDPP16SCP && HAS_CAEN_V1725

#define SYSCALL(err, func, args) do { \
	if (err == func args) { \
		log_err(LOGL, #func); \
	} \
} while (0)

static void sighandler(int);

static int g_is_serving;

void
sighandler(int a_signum)
{
	if (SIGINT == a_signum) {
		g_is_serving = 0;
	}
}

int
main(void)
{
	pid_t server_pid;
	pid_t client_pid;
	int pip[2];
	int status;
	uint8_t u8;

	SYSCALL(-1, pipe, (pip));

	server_pid = fork();
	if (-1 == server_pid) {
		log_err(LOGL, "fork");
	}
	if (0 == server_pid) {
		/* The server part. */
		char *buf[3];
		struct Crate *crate;

		SYSCALL(-1, close, (pip[0]));
		/* TODO: Some platforms have SIG_ERR with (), not (void)! */
		signal(SIGINT, sighandler);

#define SIZE 0x10000
		buf[0] = calloc(1, SIZE);
		map_user_add(0x01000000, buf[0], SIZE);

		buf[1] = calloc(1, SIZE);
		map_user_add(0x02000000, buf[1], SIZE);

		buf[2] = calloc(1, SIZE);
		map_user_add(0x03000000, buf[2], SIZE);
		{
			/* Put a reasonable family-code for the v1725. */
			uint32_t *p32;

			p32 = (void *)&buf[2][0x8140];
			*p32 = 0x0008010b;
		}

		crate = nurdlib_setup(NULL, "tests/test_ctrl/server.cfg");

		/* Let client know we're ready. */
		u8 = 0;
		SYSCALL(-1, write, (pip[1], &u8, 1));

		printf("Server entering main loop.\n");
		for (g_is_serving = 1; g_is_serving;) {
			sleep(1);
		}
		printf("Server exited main loop.\n");

		nurdlib_shutdown(&crate);
		free(buf[0]);
		free(buf[1]);
		free(buf[2]);
		SYSCALL(-1, close, (pip[1]));
		return 0;
	}

	SYSCALL(-1, close, (pip[1]));
	/* Wait until server is set up. */
	SYSCALL(-1, read, (pip[0], &u8, 1));
	SYSCALL(-1, close, (pip[0]));

	client_pid = fork();
	if (-1 == client_pid) {
		log_err(LOGL, "fork");
	}
	if (0 == client_pid) {
		/* The client part. */
		struct CtrlCrateInfo crate_info;
		struct CtrlConfigList crate_cfg;
		struct CtrlRegisterArray register_array;
		struct CtrlClient *client;
		int ok;

		printf("Client connecting...\n");
		client = ctrl_client_create("localhost", 10000);

		ok = 1;

		printf("Client asking for crate info... ");
		if (ctrl_client_crate_info_get(client, &crate_info, 0)) {
			printf("Success!\n");
		} else {
			printf("Failed.\n");
			ok = 0;
		}

		printf("Client asking for crate config... ");
		TAILQ_INIT(&crate_cfg);
		if (ctrl_client_config_dump(client, &crate_cfg)) {
			printf("Success!\n");
		} else {
			printf("Failed.\n");
			ok = 0;
		}
		ctrl_config_dump_print(&crate_cfg, 0);
		ctrl_config_dump_free(&crate_cfg);

		printf("Client asking for dummy regs... ");
		if (ctrl_client_register_array_get(client,
		    &register_array, 0, 0, 0)) {
			printf("Success!\n");
		} else {
			printf("Failed.\n");
			ok = 0;
		}
		ctrl_client_register_array_print(&register_array);
		ctrl_client_register_array_free(&register_array);

		printf("Client asking for mdpp16scp regs... ");
		if (ctrl_client_register_array_get(client,
		    &register_array, 0, 2, 0)) {
			printf("Success!\n");
		} else {
			printf("Failed.\n");
			ok = 0;
		}
		ctrl_client_register_array_print(&register_array);
		ctrl_client_register_array_free(&register_array);

		{
			struct CtrlModuleAccess arr[] = {
				{0x6004, 16, 0, 100},
				{0x6004, 16, 1, 0}
			};
			printf("Client asking for mdpp16scp offsets... ");
			if (ctrl_client_module_access_get(client, 0, 2, -1,
			    arr, LENGTH(arr)) &&
			    100 == arr[1].value) {
				printf("Success!\n");
			} else {
				printf("Failed.\n");
				ok = 0;
			}
		}

		printf("Client disconnecting.\n");
		ctrl_client_free(&client);

		return ok ? 0 : EXIT_FAILURE;
	}
	SYSCALL(-1, waitpid, (client_pid, &status, 0));
	SYSCALL(-1, kill, (server_pid, SIGINT));

	return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 :
	    EXIT_FAILURE;
}

#else
int
main(void)
{
	return 0;
}
#endif
