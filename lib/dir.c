// implement directory operation

#include <inc/string.h>
#include <inc/lib.h>

// get current directory
// directly read from env->workpath
// and copy into *buffer
char *getcwd(char *buffer, int maxlen)
{
	if (!buffer || maxlen < 0)
		return (char *)thisenv->workpath;
	return strncpy((char *)buffer, (const char *)thisenv->workpath, maxlen);
}

// change workpath
// Return 0 on success,
// Return < 0 on error. Errors are:
//	-E_INVAL *path not exist or not a path
int chdir(const char *path)
{
	int r;
	struct Stat st;
	if ((r = stat(path, &st)) < 0)
		return r;
	if (!st.st_isdir)
		return -E_INVAL;
	if (path[strlen(path)-1] != '/')
		strcat((char *)path, "/");
	return sys_env_set_workpath(thisenv->env_id, path);
}

// make directory
// Return 0 on success,
// Return < 0 on error. Errors are:
//	-E_FILE_EXISTS directory already exist
int mkdir(const char *dirname)
{
	char cur_path[MAXPATH];
	int r;
	getcwd(cur_path, MAXPATH);
	strcat(cur_path, dirname);
	if ((r = open(cur_path, O_MKDIR)) < 0)
		return r;
	close(r);
	return 0;
}