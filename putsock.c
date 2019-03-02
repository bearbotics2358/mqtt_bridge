/* putsock.c - Send a string to a socket

	 created 3/27/02 BD
	 updated 9/14/15 - add string.h
	 
	 BUILD: cc -c -o putsock.o putsock.c

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "putsock.h"

void putsock(int sock, char *s)
{
	write(sock, s, strlen(s));
}

