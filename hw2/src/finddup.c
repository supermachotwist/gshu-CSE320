/****************************************************************\
|  finddup.c - the one true find duplicate files program
|----------------------------------------------------------------
|  Bill Davidsen, last hacked 2/22/91
|  Copyright (c) 1991 by Bill Davidsen, all rights reserved. This
|  program may be freely used and distributed in its original
|  form. Modified versions must be distributed with the original
|  unmodified source code, and redistribution of the original code
|  or any derivative program may not be restricted.
|----------------------------------------------------------------
|  Calling sequence:
|   finddup [-l] checklist
|
|  where checklist is the name of a file containing filenames to 
|  be checked, such as produced by "find . -type f -print >file"
|  returns a list of linked and duplicated files.
|
|  If the -l option is used the hard links will not be displayed.
\***************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>

/* parameters */
#define MAXFN	120             /* max filename length */

/* constants */
#define EOS		((char) '\0')	/* end of string */
#define FL_CRC	0x0001			/* flag if CRC valid */
#define FL_DUP	0x0002			/* files are duplicates */
#define FL_LNK	0x0004			/* file is a link */

/* macros */
#ifdef DEBUG
#define debug(X) if (DebugFlg) printf X
#define OPTSTR	"lhd"
#else
#define debug(X)
#define OPTSTR	"lh"
#endif
#define SORT qsort((char *)filelist, n_files, sizeof(filedesc *), comp1);
#define GetFlag(x,f) ((filelist[x].flags & (f)) != 0)
#define SetFlag(x,f) (filelist[x].flags |= (f))

typedef struct {
	off_t length;				/* file length */
	unsigned long crc32;		/* CRC for same length */
	dev_t device;				/* physical device # */
	ino_t inode;				/* inode for link detect */
	off_t nameloc;				/* name loc in names file */
	char flags;					/* flags for compare */
} filedesc;

filedesc *filelist;				/* master sorted list of files */
long n_files = 0;				/* # files in the array */
long max_files = 0;				/* entries allocated in the array */
int linkflag = 1;				/* show links */
int DebugFlg = 0;				/* inline debug flag */
FILE *namefd;					/* file for names */
extern int
	opterr,						/* error control flag */
	optind;						/* index for next arg */

/* help message, in a table format */
static char *HelpMsg[] = {
	"Calling sequence:",
    "",
	"  finddup [options] list",
	"",
	"where list is a list of files to check, such as generated",
	"by \"find . -type f -print > file\"",
	"",
	"Options:",
	"  -l - don't list hard links",
#ifdef DEBUG
	"  -d - debug (must compile with DEBUG)"
#endif /* ?DEBUG */
};
static HelpLen = sizeof(HelpMsg)/sizeof(char *);

#ifndef	lint
static char *SCCSid[] = {
	"@(#)finddup.c v1.13 - 2/22/91",
	"Copyright (c) 1991 by Bill Davidsen, all rights reserved"
};
#endif

int comp1();					/* compare two filedesc's */
void scan1();					/* make the CRC scan */
void scan2();					/* do full compare if needed */
void scan3();					/* print the results */
unsigned long get_crc();		/* get crc32 on a file */
char *getfn();					/* get a filename by index */

int finddup_main(argc, argv)
int argc;
char *argv[];
{
	char curfile[MAXFN];
	struct stat statbuf;
	int ch;
	int firsterr = 0;			/* flag on 1st error for format */
	int firsttrace = 0;			/* flag for 1st trace output */
	off_t loc;            		/* location of name in the file */
	int zl_hdr = 1;				/* need header for zero-length files list */
	filedesc *curptr;			/* pointer to current storage loc */

	/* parse options, if any */
	opterr = 0;
	while ((ch = att_getopt(argc, argv, OPTSTR)) != EOF) {
		switch (ch) {
		case 'l': /* set link flag */
			linkflag = 0;
			break;
#ifdef DEBUG
		case 'd': /* debug */
			++DebugFlg;
			break;
#endif /* ?DEBUG */
		case 'h': /* help */
		case '?':
			for (ch = 0; ch < HelpLen; ++ch) {
				printf("%s\n", HelpMsg[ch]);
			}
			exit(1);
		}
	}

	/* correct for the options */
	argc -= (optind-1);
	argv += (optind-1);

	/* check for filename given, and open it */
	if (argc != 2) {
		fprintf(stderr, "Needs name of file with filenames\n");
		exit(1);
	}
	namefd = fopen(argv[1], "r");
	if (namefd == NULL) {
		perror("Can't open names file");
		exit(1);
	}

	/* start the list of name info's */
	filelist = (filedesc *) malloc(50 * sizeof(filedesc));
	if (filelist == NULL) {
		perror("Can't start files vector");
		exit(1);
	}
	/* finish the pointers */
	max_files = 50;
	debug(("First vector allocated @ %08lx, size %d bytes\n",
		(long) filelist, 50*sizeof(filedesc)
	));
	fprintf(stderr, "build list...");

	/* this is the build loop */
	while (loc = ftell(namefd), fgets(curfile, MAXFN, namefd) != NULL) {
		/* check for room in the buffer */
		if (n_files == max_files) {
			/* allocate more space */
			max_files += 50;
			filelist =
				(filedesc *) realloc(filelist, (max_files)*sizeof(filedesc));
			if (filelist == NULL) {
				perror("Out of memory!");
				exit(1);
			}
			debug(("Got more memory!\n"));
		}
		curfile[strlen(curfile)-1] = EOS;

		/* add the data for this one */
		if (stat(curfile, &statbuf)) {
			fprintf(stderr, "%c  %s - ", 
				(firsterr++ == 0 ? '\n' : '\r'), curfile
			);
			perror("ignored");
			continue;
		}

		/* check for zero length files */
		if ( statbuf.st_size == 0) {
			if (zl_hdr) {
				zl_hdr = 0;
				printf("Zero length files:\n\n");
			}
			printf("%s\n", curfile);
			continue;
		}

		curptr = filelist + n_files++;
		curptr->nameloc = loc;
		curptr->length = statbuf.st_size;
		curptr->device = statbuf.st_dev;
		curptr->inode = statbuf.st_ino;
		curptr->flags = 0;
		debug(("%cName[%d] %s, size %ld, inode %d\n",
			(firsttrace++ == 0 ? '\n' : '\r'), n_files, curfile,
			(long) statbuf.st_size, statbuf.st_ino
		));
	}

	/* sort the list by size, device, and inode */
	fprintf(stderr, "sort...");
	SORT;

	/* make the first scan for equal lengths */
	fprintf(stderr, "scan1...");
	scan1();

	/* make the second scan for dup CRC also */
	fprintf(stderr, "scan2...");
	scan2();

	fprintf(stderr, "done\n");

#ifdef DEBUG
	for (loc = 0; DebugFlg > 1 && loc < n_files; ++loc) {
		curptr = filelist + loc;
		printf("%8ld %08lx %6d %6d %02x\n",
			curptr->length, curptr->crc32,
			curptr->device, curptr->inode,
			curptr->flags
		);
	}
#endif

	/* now scan and output dups */
	scan3();

	exit(0);
}

/* comp1 - compare two values */
int
comp1(p1, p2)
char *p1, *p2;
{
	register filedesc *p1a = (filedesc *)p1, *p2a = (filedesc *)p2;
	register int retval;

	(retval = p1a->length - p2a->length) ||
	(retval = p1a->crc32 - p2a->crc32) ||
	(retval = p1a->device - p2a->device) ||
	(retval = p1a->inode - p2a->inode);
	
	return retval;
}

/* scan1 - get a CRC32 for files of equal length */

void
scan1() {
	FILE *fp;
	int ix, needsort = 0;

	for (ix = 1; ix <= n_files; ++ix) {
		if (filelist[ix-1].length == filelist[ix].length) {
			/* get a CRC for each */
			if (! GetFlag(ix-1, FL_CRC)) {
				filelist[ix-1].crc32 = get_crc(ix-1);
				SetFlag(ix-1, FL_CRC);
			}
			if (! GetFlag(ix, FL_CRC)) {
				filelist[ix].crc32 = get_crc(ix);
				SetFlag(ix, FL_CRC);
			}
			needsort = 1;
		}
	}

	if (needsort) SORT;
}

/* scan2 - full compare if CRC is equal */

void
scan2() {
	int ix, ix2, lastix;
	int inmatch;				/* 1st filename has been printed */
	int need_hdr = 1;			/* Need a hdr for the hard link list */
	int lnkmatch;				/* flag for matching links */
	register filedesc *p1, *p2;
	filedesc wkdesc;

	/* mark links and output before dup check */
	for (ix = 0; ix < n_files; ix = ix2) {
		p1 = filelist + ix;
		for (ix2 = ix+1, p2 = p1+1, inmatch = 0;
			ix2 < n_files
				&& p1->device == p2->device
				&& p1->inode == p2->inode;
			++ix2, ++p2
		) {
			SetFlag(ix2, FL_LNK);
			if (linkflag) {
				if (need_hdr) {
					need_hdr = 0;
					printf("\n\nHard link summary:\n\n");
				}

				if (!inmatch) {
					inmatch = 1;
					printf("\nFILE: %s\n", getfn(ix));
				}
				printf("LINK: %s\n", getfn(ix2));
			}
		}
	}
	debug(("\nStart dupscan"));

	/* now really scan for duplicates */
	for (ix = 0; ix < n_files; ix = lastix) {
		p1 = filelist + ix;
		for (lastix = ix2 = ix+1, p2 = p1+1, lnkmatch = 1;
			ix2 < n_files
				&& p1->length == p2->length
				&& p1->crc32 == p2->crc32;
			++ix2, ++p2
		) {
			if ((GetFlag(ix2, FL_LNK) && lnkmatch)
				|| fullcmp(ix, ix2) == 0
			) {
				SetFlag(ix2, FL_DUP);
				/* move if needed */
				if (lastix != ix2) {
					int n1, n2;

					debug(("\n  swap %d and %d", lastix, ix2));
					wkdesc = filelist[ix2];
					for (n1 = ix2; n1 > lastix; --n1) {
						filelist[n1] = filelist[n1-1];
					}
					filelist[lastix++] = wkdesc;
				}
				lnkmatch = 1;
			}
			else {
				/* other links don't match */
				lnkmatch = 0;
			}
		}
	}
}

/* scan3 - output dups */

void
scan3()
{
	register filedesc *p1, *p2;
	int ix, ix2, inmatch, need_hdr = 1;
	char *headfn;				/* pointer to the filename for sups */

	/* now repeat for duplicates, links or not */
	for (ix = 0; ix < n_files; ++ix) {
		if (GetFlag(ix, FL_DUP)) {
			/* put out a header if you haven't */
			if (!inmatch)
				strcpy(headfn, getfn(ix-1));
			inmatch = 1;
			if (linkflag || !GetFlag(ix, FL_LNK)) {
				/* header on the very first */
				if (need_hdr) {
					need_hdr = 0;
					printf("\n\nList of files with duplicate contents");
					if (linkflag) printf(" (includes hard links)");
					putchar('\n');
				}

				/* 1st filename if any dups */
				if (headfn != NULL) {
					printf("\nFILE: %s\n", headfn);
					headfn = NULL;
				}
				printf("DUP:  %s\n", getfn(ix));
			}
		}
	}
}

/* get_crc - get a CRC32 for a file */

unsigned long
get_crc(ix)
int ix;
{
	FILE *fp;
	register unsigned long val1 = 0x90909090, val2 = 0xeaeaeaea;
	register int carry;
	char ch;
	char fname[MAXFN];

	/* open the file */
	fseek(namefd, filelist[ix].nameloc, 0);
	fgets(fname, MAXFN, namefd);
	fname[strlen(fname)-1] = EOS;
	debug(("\nCRC start - %s ", fname));
	if ((fp = fopen(fname, "r")) == NULL) {
		fprintf(stderr, "Can't read file %s\n", fname);
		exit(1);
	}

	/* build the CRC values */
	while ((ch = fgetc(fp)) != EOF) {
		carry = (val1 & 0x8000000) != 0;
		val1 = (val1 << 1) ^ ch + carry;
		val2 += ch << (ch & 003);
	}
	debug(("v1: %08lx v2: %08lx ", val1, val2));

	return ((val1 & 0xffff) << 12) ^ (val2 && 0xffffff);
}

/* getfn - get filename from index */

char *
getfn(ix)
off_t ix;
{
	static char fnbuf[MAXFN];

	fseek(namefd, filelist[ix].nameloc, 0);
	fgets(fnbuf, MAXFN, namefd);
	fnbuf[strlen(fnbuf)-1] = EOS;

	return fnbuf;
}

/* fullcmp - compare two files, bit for bit */

int
fullcmp(v1, v2)
int v1, v2;
{
	FILE *fp1, *fp2;
	char filename[MAXFN];
	register char ch;

	/* open the files */
	strcpy(filename, getfn(v1));
	fp1 = fopen(filename, "r");
	if (fp1 == NULL) {
		fprintf(stderr, "%s: ", filename);
		perror("can't access for read");
		exit(1);
	}
	debug(("\nFull compare %s\n         and", filename));

	strcpy(filename, getfn(v2));
	fp2 = fopen(filename, "r");
	if (fp2 == NULL) {
		fprintf(stderr, "%s: ", filename);
		perror("can't access for read");
		exit(1);
	}
	debug(("%s", filename));

	/* now do the compare */
	while ((ch = getc(fp1)) != EOF) {
		if (ch - getc(fp2)) break;
	}

	/* close files and return value */
	fclose(fp1);
	fclose(fp2);
	debug(("\n      return %d", !(ch == EOF)));
	return (!(ch == EOF));
}
