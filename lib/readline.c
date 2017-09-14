#include <inc/stdio.h>
#include <inc/error.h>

#define BUFLEN 1024
static char buf[BUFLEN];

char *
readline(const char *prompt)
{
	int i, c, echoing;

#if JOS_KERNEL
	if (prompt != NULL)
		cprintf("%s", prompt);
#else
	if (prompt != NULL)
		printf("%s", prompt);
#endif

	i = 0;
	echoing = 1;

	while (1)
	{
		c = getchar();

		if (c < 0)
		{
			if (c != -E_EOF)
#if JOS_KERNEL
				cprintf("read error: %e\n", c);
#else
				printf("read error: %e\n", c);
#endif
			return NULL;
		}
		else if ((c == '\b' || c == '\x7f') && i > 0)
		{
			if (echoing)
#if JOS_KERNEL
				cputchar('\b');
#else
				printf("\b");
#endif
			i--;
		}
		else if (c >= ' ' && i < BUFLEN - 1)
		{
			if (echoing)
#if JOS_KERNEL
				cputchar(c);
#else
				printf("%c", c);
#endif

			buf[i++] = c;
		}
		else if (c == '\n' || c == '\r')
		{
			if (echoing)
#if JOS_KERNEL
				cputchar('\n');
#else
				printf("\n");
#endif
			buf[i] = 0;
			return buf;
		}
	}
}
