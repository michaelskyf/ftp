#pragma once

#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum conn_mode
{
	CONN_IDLE,
	CONN_ACTV,
	CONN_PASV,
};

struct conn_info
{
	enum conn_mode mode;
	int cmd_conn_fd;
	int data_fd;

	int logged_in;
	int quit;

	char username[64];
	char password[64];
};

typedef int (cmd_func_t)(struct conn_info *conn, const char *arg, size_t arg_len);

struct cmd_entry
{
	const char *cmd_str;
	cmd_func_t *cmd_func;
};


cmd_func_t *cmd_get_cmd(const char *msg, const char **arg, size_t msg_len);

cmd_func_t cmd_ftp_user;
cmd_func_t cmd_ftp_pass;
cmd_func_t cmd_ftp_type;
cmd_func_t cmd_ftp_stor;
cmd_func_t cmd_ftp_retr;

cmd_func_t cmd_ftp_syst;
cmd_func_t cmd_ftp_pasv;
cmd_func_t cmd_ftp_list;
cmd_func_t cmd_ftp_quit;

static const struct cmd_entry commands[] =
{
	/* Add space after command if requires arguments */
	{"USER", cmd_ftp_user},
	{"PASS", cmd_ftp_pass},
	{"SYST", cmd_ftp_syst},
	{"PASV", cmd_ftp_pasv},
	{"LIST", cmd_ftp_list},
	{"TYPE", cmd_ftp_type},
	{"STOR", cmd_ftp_stor},
	{"PUT", cmd_ftp_stor}, /* Behaves the same as STOR */
	{"QUIT", cmd_ftp_quit},
	{"RETR", cmd_ftp_retr},
};
