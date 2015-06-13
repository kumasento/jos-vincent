#include <inc/lib.h>

void 
umain(int argc, char *argv[]) {
	binaryname = "testexec";
	int r;
	cprintf("textexec...\n");
	if ((r = execl("/init", "init", "initarg1", "initarg2", (char*)0)) < 0) 
		panic("execl: %e", r);
	cprintf("testexec existing ...\n");
}