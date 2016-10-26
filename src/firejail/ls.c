/*
 * Copyright (C) 2014-2016 Firejail Authors
 *
 * This file is part of firejail project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
	
#include "firejail.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
//#include <dirent.h>
//#include <stdio.h>
//#include <stdlib.h>

// uid/gid cache
static uid_t c_uid = 0;
static char *c_uid_name = NULL;

static void print_file_or_dir(const char *path, const char *fname, int separator) {
	assert(fname);
	
	char *name;
	if (separator) {
		if (asprintf(&name, "%s/%s", path, fname) == -1)
			errExit("asprintf");
	}
	else {
		if (asprintf(&name, "%s%s", path, fname) == -1)
			errExit("asprintf");
	}
	
	struct stat s;
	if (stat(name, &s) == -1) {
		if (lstat(name, &s) == -1) {
			printf("Error: cannot access %s\n", name);
			return;
		}
	}

	// permissions
	if (S_ISLNK(s.st_mode))
		printf("l");
	else if (S_ISDIR(s.st_mode))
		printf("d");
	else if (S_ISCHR(s.st_mode))
		printf("c");
	else if (S_ISBLK(s.st_mode))
		printf("b");
	else if (S_ISSOCK(s.st_mode))
		printf("s");
	else
		printf("-");
	printf( (s.st_mode & S_IRUSR) ? "r" : "-");
	printf( (s.st_mode & S_IWUSR) ? "w" : "-");
	printf( (s.st_mode & S_IXUSR) ? "x" : "-");
	printf( (s.st_mode & S_IRGRP) ? "r" : "-");
	printf( (s.st_mode & S_IWGRP) ? "w" : "-");
	printf( (s.st_mode & S_IXGRP) ? "x" : "-");
	printf( (s.st_mode & S_IROTH) ? "r" : "-");
	printf( (s.st_mode & S_IWOTH) ? "w" : "-");
	printf( (s.st_mode & S_IXOTH) ? "x" : "-");
	printf(" ");
		
	// user name
	char *username;
	int allocated = 0;
	if (s.st_uid == 0)
		username = "root";
	else if (s.st_uid == c_uid) {
		assert(c_uid_name);
		username = c_uid_name;
	}
	else {
		struct passwd *pw = getpwuid(s.st_uid);
		allocated = 1;
		if (!pw) {
			if (asprintf(&username, "%d", s.st_uid) == -1)
				errExit("asprintf");
		}
		else {
			username = strdup(pw->pw_name);
			if (!username)
				errExit("asprintf");
		}
		
		if (c_uid == 0) {
			c_uid = s.st_uid;
			c_uid_name = strdup(username);
			if (!c_uid_name)
				errExit("asprintf");
		}
	}
	
	// print user name, 8 chars maximum
	int len = strlen(username);
	if (len > 8) {
		username[8] = '\0';
		len = 8;
	}
	printf("%s ", username);
	int i;
	for (i = len; i < 8; i++)
		printf(" ");
	if (allocated)
		free(username);
	

	// group name
	char *groupname;
	allocated = 0;
	if (s.st_uid == 0)
		groupname = "root";
	else {
		struct group *g = getgrgid(s.st_gid);
		allocated = 1;
		if (!g) {
			if (asprintf(&groupname, "%d", s.st_gid) == -1)
				errExit("asprintf");
		}
		else {
			groupname = strdup(g->gr_name);
			if (!groupname)
				errExit("asprintf");
		}
	}
	
	// print grup name, 8 chars maximum
	len = strlen(groupname);
	if (len > 8) {
		groupname[8] = '\0';
		len = 8;
	}
	printf("%s ", groupname);
	for (i = len; i < 8; i++)
		printf(" ");
	if (allocated)
		free(groupname);

	char *sz;
	if (asprintf(&sz, "%d", (int) s.st_size) == -1)
		errExit("asprintf");
	printf("%11.10s %s\n", sz, fname);
	free(sz);
	
}

static void print_directory(const char *path) {
	assert(path);
	struct stat s;
	if (stat(path, &s) == -1)
		return;
	assert(S_ISDIR(s.st_mode));
	
	struct dirent **namelist;
	int i;
	int n;

	n = scandir(path, &namelist, 0, alphasort);
	if (n < 0)
        		errExit("scandir");
	else {
		for (i = 0; i < n; i++) {
			print_file_or_dir(path, namelist[i]->d_name, 0);
			free(namelist[i]);
		}
	}
	free(namelist);
}

void sandboxfs_name(int op, const char *name, const char *path1, const char *path2) {
	EUID_ASSERT();
	
	if (!name || strlen(name) == 0) {
		fprintf(stderr, "Error: invalid sandbox name\n");
		exit(1);
	}
	pid_t pid;
	if (name2pid(name, &pid)) {
		fprintf(stderr, "Error: cannot find sandbox %s\n", name);
		exit(1);
	}

	sandboxfs(op, pid, path1, path2);
}

char *expand_path(const char *path) {
	char *fname = NULL;
	if (*path == '/') {
		fname = strdup(path);
		if (!fname)
			errExit("strdup");
	}
	else if (*path == '~') {
		if (asprintf(&fname, "%s%s", cfg.homedir, path + 1) == -1)
			errExit("asprintf");
	}
	else {
		// assume the file is in current working directory
		if (asprintf(&fname, "%s/%s", cfg.cwd, path) == -1)
			errExit("asprintf");
	}		
	return fname;
}

void sandboxfs(int op, pid_t pid, const char *path1, const char *path2) {
	EUID_ASSERT();

	// if the pid is that of a firejail  process, use the pid of the first child process
	EUID_ROOT();
	char *comm = pid_proc_comm(pid);
	EUID_USER();
	if (comm) {
		if (strcmp(comm, "firejail") == 0) {
			pid_t child;
			if (find_child(pid, &child) == 0) {
				pid = child;
			}
		}
		free(comm);
	}

	// check privileges for non-root users
	uid_t uid = getuid();
	if (uid != 0) {
		uid_t sandbox_uid = pid_get_uid(pid);
		if (uid != sandbox_uid) {
			fprintf(stderr, "Error: permission denied.\n");
			exit(1);
		}
	}

	// expand paths
	char *fname1 = expand_path(path1);;
	char *fname2 = NULL;
	if (path2 != NULL) {
		fname2 = expand_path(path2);
	}
	if (arg_debug) {
		printf("file1 %s\n", fname1);
		printf("file2 %s\n", fname2);
	}
		
	// sandbox root directory
	char *rootdir;
	if (asprintf(&rootdir, "/proc/%d/root", pid) == -1)
		errExit("asprintf");

	if (op == SANDBOX_FS_LS) {
		EUID_ROOT();
		// chroot
		if (chroot(rootdir) < 0)
			errExit("chroot");
		if (chdir("/") < 0)
			errExit("chdir");
		
		// drop privileges
		drop_privs(0);

		if (access(fname1, R_OK) == -1) {
			fprintf(stderr, "Error: Cannot access %s\n", fname1);
			exit(1);
		}
	
		// list directory contents
		struct stat s;
		if (stat(fname1, &s) == -1) {
			fprintf(stderr, "Error: Cannot access %s\n", fname1);
			exit(1);
		}
		if (S_ISDIR(s.st_mode)) {
			char *rp = realpath(fname1, NULL);
			if (!rp) {
				fprintf(stderr, "Error: Cannot access %s\n", fname1);
				exit(1);
			}
			if (arg_debug)
				printf("realpath %s\n", rp);
	
			char *dir;
			if (asprintf(&dir, "%s/", rp) == -1)
				errExit("asprintf");
			
			print_directory(dir);
			free(rp);
			free(dir);
		}
		else {
			char *rp = realpath(fname1, NULL);
			if (!rp) {
				fprintf(stderr, "Error: Cannot access %s\n", fname1);
				exit(1);
			}
			if (arg_debug)
				printf("realpath %s\n", rp);
			char *split = strrchr(rp, '/');
			if (split) {
				*split = '\0';
				char *rp2 = split + 1;
				if (arg_debug)
					printf("path %s, file %s\n", rp, rp2);
				print_file_or_dir(rp, rp2, 1);
			}
			free(rp);
		}
	}
	
	// get file from sandbox and store it in the current directory
	else if (op == SANDBOX_FS_GET) {
		// check source file (sandbox)
		char *src_fname;
		if (asprintf(&src_fname, "%s%s", rootdir, fname1) == -1)
			errExit("asprintf");
		EUID_ROOT();
		struct stat s;
		if (stat(src_fname, &s) == -1) {
			fprintf(stderr, "Error: Cannot access %s\n", fname1);
			exit(1);
		}
		if (is_dir(src_fname)) {
			fprintf(stderr, "Error: source file name is a directory\n");
			exit(1);
		}
		
		// try to open the source file - we need to chroot
		pid_t child = fork();
		if (child < 0)
			errExit("fork");
		if (child == 0) {
			// chroot
			if (chroot(rootdir) < 0)
				errExit("chroot");
			if (chdir("/") < 0)
				errExit("chdir");
			
			// drop privileges
			drop_privs(0);
			
			// try to read the file
			if (access(fname1, R_OK) == -1) {
				fprintf(stderr, "Error: Cannot read %s\n", fname1);
				exit(1);
			}
			_exit(0);
		}

		// wait for the child to finish
		int status = 0;
		waitpid(child, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0);
		else
			exit(1);
		EUID_USER();
		
		// check destination file (host)
		char *dest_fname = strrchr(fname1, '/');
		if (!dest_fname || *(++dest_fname) == '\0') {
			fprintf(stderr, "Error: invalid file name %s\n", fname1);
			exit(1);
		}
		
		if (access(dest_fname, F_OK) == -1) {
			// try to create the file as a regular user
			pid_t child = fork();
			if (child < 0)
				errExit("fork");
			if (child == 0) {
				// drop privileges
				drop_privs(0);
				
				FILE *fp = fopen(dest_fname, "w");
				if (!fp) {
					fprintf(stderr, "Error: cannot create %s\n", dest_fname);
					exit(1);
				}
				fclose(fp);
				_exit(0);
			}

			// wait for the child to finish
			int status = 0;
			waitpid(child, &status, 0);
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0);
			else
				exit(1);
		}
		else {
			if (access(dest_fname, W_OK) == -1) {
				fprintf(stderr, "Error: cannot write %s\n", dest_fname);
				exit(1);
			}
		}
		
		// copy file
		if (arg_debug)
			printf("copy %s to %s\n", src_fname, dest_fname);
		EUID_ROOT();
		if (copy_file(src_fname, dest_fname, getuid(), getgid(), 0644))
			fprintf(stderr, "Error: transfer failed\n");
		else
			printf("Transfer complete\n");
		EUID_USER();
	}
	// get file from host and store it in the sandbox
	else if (op == SANDBOX_FS_PUT && path2) {
		// verify the source file
		const char *src_fname = path1;
		struct stat s;
		if (stat(src_fname, &s) == -1) {
			fprintf(stderr, "Error: Cannot access %s\n", fname1);
			exit(1);
		}
		if (is_dir(src_fname)) {
			fprintf(stderr, "Error: source file name is a directory\n");
			exit(1);
		}
		
		// try to open the source file 
		pid_t child = fork();
		if (child < 0)
			errExit("fork");
		if (child == 0) {
			// drop privileges
			drop_privs(0);
			
			// try to read the file
			if (access(src_fname, R_OK) == -1) {
				fprintf(stderr, "Error: Cannot read %s\n", src_fname);
				exit(1);
			}
			_exit(0);
		}

		// wait for the child to finish
		int status = 0;
		waitpid(child, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0);
		else
			exit(1);
		
		// check destination file (sandbox)
		char *dest_fname;
		if (asprintf(&dest_fname, "%s%s", rootdir, fname2) == -1)
			errExit("asprintf");
		EUID_ROOT();
		if (is_dir(dest_fname)) {
			fprintf(stderr, "Error: destination file name is a directory inside the sandbox\n");
			exit(1);
		}
		
		// check write access on destination
		child = fork();
		if (child < 0)
			errExit("fork");
		if (child == 0) {
			// chroot
			if (chroot(rootdir) < 0)
				errExit("chroot");
			if (chdir("/") < 0)
				errExit("chdir");

			// drop privileges
			drop_privs(0);

			if (access(path2, F_OK) == -1) {
				FILE *fp = fopen(path2, "w");
				if (!fp) {
					fprintf(stderr, "Error: cannot create %s\n", path2);
					exit(1);
				}
				fclose(fp);
			}
			else {
				if (access(path2, W_OK) == -1) {
					fprintf(stderr, "Error: cannot write %s\n", path2);
					exit(1);
				}
			}

			_exit(0);
		}

		// wait for the child to finish
		status = 0;
		waitpid(child, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0);
		else
			exit(1);
			
		// copy file
		if (arg_debug)
			printf("copy %s to %s\n", src_fname, dest_fname);
		EUID_ROOT();
		if (copy_file(src_fname, dest_fname, getuid(), getgid(), 0644))
			fprintf(stderr, "Error: transfer failed\n");
		else
			printf("Transfer complete\n");
		EUID_USER();
	}

	if (fname2)
		free(fname2);
	free(fname1);
	free(rootdir);

	exit(0);
}
