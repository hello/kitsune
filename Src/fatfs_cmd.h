#ifndef __FATFS_CMD_H__
#define __FATFS_CMD_H__

int Cmd_mnt(int argc, char *argv[]);
int Cmd_umnt(int argc, char *argv[]);
int Cmd_ls(int argc, char *argv[]);
int Cmd_cd(int argc, char *argv[]);
int Cmd_rm(int argc, char *argv[]);
int Cmd_write(int argc, char *argv[]);
int Cmd_mkfs(int argc, char *argv[]);
int Cmd_mkdir(int argc, char *argv[]);
int Cmd_pwd(int argc, char *argv[]);
int Cmd_cat(int argc, char *argv[]);

#endif
