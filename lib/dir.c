// implement dir operation

#include <inc/string.h>
#include <inc/lib.h>

char *getcwd(char *buffer, int maxlen)
{
	if(!buffer || maxlen < 0)
		return NULL;
	return strncpy((char *)buffer,(const char*)thisenv->workpath,maxlen);
}

// change work path
// Return 0 on success, 
// Return < 0 on error. Errors are:
//	-E_INVAL *path not exist or not a path
int chdir(const char *path)
{
	int r;
	struct Stat st;
	if ((r = stat(path, &st)) < 0)
		return r;
	if(!st.st_isdir)
		return -E_INVAL;
	return sys_chdir(path);
}