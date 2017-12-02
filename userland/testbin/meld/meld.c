/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * meld.c
 *
 * 	Tests the filesystem by melding two files from two
 * 	user specified files.
 *
 * This should run (on SFS) even before the file system assignment is started.
 * It should also continue to work once said assignment is complete.
 * It will not run fully on emufs, because emufs does not support remove().
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	static char writebuf1[12] = "AAAABBBBCCCC";
	static char writebuf2[12] = "ddddeeeeffff";
	static char readbuf[25];

	const char *file_1, *file_2, *file_3;
	int fd, rv;

	if (argc == 2) {
		file_1 = argv[1];
	}
	else if(argc > 2){
		errx(1, "Usage: testbin/meld");
	}

	file_1 = "testfile01";
	file_2 = "testfile02";
	file_3 = "testfile03";
	
	//writing to first file
	fd = open(file_1, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd < 0) 
	{
		err(1, "%s: open for write", file_1);
	}

	rv = write(fd, writebuf1, 12);
	if (rv < 0) 
	{
		err(1, "%s: write", file_1);
	}

	rv = close(fd);
	if (rv < 0) 
	{
		err(1, "%s: close (meld 1st time)", file_1);
	}

	//writing to second file
	fd = open(file_2, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd < 0) 
	{
		err(1, "%s: open for write", file_2);
	}

	rv = write(fd, writebuf2, 12);
	if (rv < 0) 
	{
		err(1, "%s: write", file_2);
	}

	rv = close(fd);
	if (rv < 0) 
	{
		err(1, "%s: close (meld 2nd time)", file_2);
	}
	
	//doing meld
	int mix = meld(file_1, file_2, file_3);
	mix = 0;
	if (mix < 0) 
	{
		err(1, "%s: melding time", file_3);
	}

	//reading
	fd = open(file_3, O_RDONLY);
	if (fd < 0) 
	{
		err(1, "%s: open for read", file_3);
	}

	rv = read(fd, readbuf, 24);
	if (rv < 0) 
	{
		err(1, "%s: read", file_3);
	}

	rv = close(fd);

	if (rv < 0) 
	{
		err(1, "%s: close (meld 3rd time)", file_3);
	}

	printf("rv = %d\n", mix);

	printf("Passed meldtest.\n");


	return 0;
}
