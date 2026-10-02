#define	__MODULE__	"SRCHFIL"
#define	__IDENT__	"X01-04"
#define	__REV__		"1.4.0"

/*
**++
**
**  FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
**
**  MODULE:	srchfil.c
**
**  ABSTRACT:	The file selection of the SEARCH utility: expansion of a
**		file specification, the /RECURSIVE walk of a directory
**		tree and the /EXCLUDE, /SINCE and /BEFORE filters.
**
**  DESCRIPTION: The utility expands a file specification itself rather
**		than leaving it to the shell.  Two reasons:
**
**		  - a pattern quoted against the shell - 'SEARCH "*.c"' -
**		    still has to be expanded, and the diagnostics of an
**		    unmatched pattern are then ours, not the shell's;
**
**		  - /RECURSIVE plays the part of the [...] of an OpenVMS
**		    directory specification, which no shell offers.
**
**		A specification is split at the last slash: what precedes
**		it is a directory, what follows it is the pattern applied
**		to the names within.  Wildcards in the directory part are
**		NOT expanded - use /RECURSIVE from the top of the tree
**		instead.  A specification carrying no wildcard at all is
**		taken literally: a file is searched as it is, a directory
**		is searched as if the pattern "*" had been applied within it.
**
**		A name is matched against a pattern by UTIL$PATTERN_MATCH
**		of the STARLET library; '?' is normalized to '%' when a
**		pattern is taken in, since the library knows the OpenVMS
**		spelling alone.
**
**		The walk sorts the names of every directory, so that the
**		order of the output does not depend on the order READDIR
**		happens to deliver.  A symbolic link is never followed -
**		neither to a file nor, above all, to a directory: a link
**		pointing at an ancestor would otherwise turn the walk into
**		an endless one.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:   1-AUG-2026
**
**  MODIFICATION HISTORY:
**
**	X01-04		21-AUG-2026	RRL
**		The matching of a name is UTIL$PATTERN_MATCH of the STARLET
**		library, which carries the same algorithm since V.01-26;
**		the '?' of the shell is turned into '%' when a pattern is
**		taken in.
**
**	X01-03		 3-AUG-2026	RRL
**		The FAO parameters of a message are cast and counted by
**		$SRCHMSG rather than by hand.
**
**	X01-02		 1-AUG-2026	RRL
**		An action routine that returns STS$K_FATAL stops the walk:
**		the reader who has answered Q at the pager of /PAGE is not
**		to be made to wait for the rest of the tree.
**
**	X01-01		 1-AUG-2026	RRL
**		Initial version.
**
**--
*/

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<dirent.h>
#include	<sys/stat.h>
#include	<unistd.h>

#include	"srchdef.h"

/*
**  A directory being read: the names are collected first and sorted, and
**  only then are they acted upon.  The array grows by SEARCH$K_DIRGROW
**  entries at a time.
*/
#define	SEARCH$K_DIRGROW	64

typedef	struct	srch_dir_t
{
	unsigned	cnt,			/* Names collected			*/
			max;			/* Capacity of the array		*/
	char **		name;			/* The names themselves, ASCIZ		*/
} SEARCH$DIR;


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Match a name against a pattern.
**
**	The matching itself is UTIL$PATTERN_MATCH of the STARLET library,
**	which since V.01-26 is the iterative matcher with a single
**	backtrack point - the very algorithm this module used to carry, and
**	one copy of it in the system is enough.  It knows '*' and '%'; the
**	'?' this utility accepts as well has been turned into '%' by
**	S_SRCH$NORMPAT before a pattern ever gets here, so nothing is lost.
**
**	The routine of the library takes its arguments as CHAR * and does
**	not touch them.  The const is therefore cast away, which is the
**	price of an interface that predates the qualifier.
**
**  FORMAL PARAMETERS:
**
**	a_pat		The pattern, ASCIZ, normalized
**	a_str		The name to be matched, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the name matches the pattern;
**	STS$K_WARN	- it does not.
**--
*/
static	int	s_srch$wildmat	(
	const	char *		a_pat,
	const	char *		a_str
			)
{
	return	__util$pattern_match((char *) a_str, (char *) a_pat) ? STS$K_SUCCESS : STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Normalize a pattern in place: '?', which every Linux user has in
**	his fingers, is the '%' of OpenVMS, and the matcher of the library
**	knows the latter only.  Done once, when a pattern is taken in,
**	rather than at every name it is matched against.
**
**  FORMAL PARAMETERS:
**
**	a_pat		The pattern, ASCIZ; modified in place
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS always.
**--
*/
static	int	s_srch$normpat	(
		char *		a_pat
			)
{
	for ( char *l_ptr = a_pat; *l_ptr; l_ptr++ )
		if ( *l_ptr == '?' )
			*l_ptr	= '%';

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tell whether a string carries a wildcard character.
**
**  FORMAL PARAMETERS:
**
**	a_str		The string to be examined, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- a wildcard is present;
**	STS$K_WARN	- the string is a literal one.
**--
*/
static	int	s_srch$iswild	(
	const	char *		a_str
			)
{
	for ( const char *l_ptr = a_str; *l_ptr; l_ptr++ )
		if ( (*l_ptr == '*') || (*l_ptr == '%') || (*l_ptr == '?') )
			return	STS$K_SUCCESS;

	return	STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Decide whether a file passes the selection criteria that do not
**	depend on its name: /SINCE, /BEFORE and the time stamp they are
**	applied to (/MODIFIED, the default, or /CREATED).
**
**  FORMAL PARAMETERS:
**
**	a_sel		The selection criteria
**	a_st		The status block of the file
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the file is to be searched;
**	STS$K_WARN	- it has been filtered out.
**--
*/
static	int	s_srch$intime	(
		SEARCH$SEL *	a_sel,
	const	struct stat *	a_st
			)
{
fao_time_t	l_tim;

	if ( !a_sel->hassince && !a_sel->hasbefore )
		return	STS$K_SUCCESS;

	/*
	**  Linux keeps no creation time a portable program may rely upon, so
	**  /CREATED is served by the inode change time - the closest analogue
	**  the system has to offer.
	*/
	l_tim	= (fao_time_t) ((a_sel->timsrc == SEARCH$K_TIME_CREATED) ? a_st->st_ctime : a_st->st_mtime);

	if ( a_sel->hassince && (l_tim < a_sel->since) )
		return	STS$K_WARN;

	if ( a_sel->hasbefore && (l_tim >= a_sel->before) )
		return	STS$K_WARN;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Decide whether a file has been excluded by /EXCLUDE.  A pattern
**	that carries a slash is matched against the whole specification of
**	the file, one that does not - against its name alone, so that
**
**	    /EXCLUDE=*.o		drops every object file of the tree,
**	    /EXCLUDE=build/%*		drops one directory of it - a pattern
**					carrying a slash is a path pattern.
**
**  FORMAL PARAMETERS:
**
**	a_sel		The selection criteria
**	a_fspec		The specification of the file, ASCIZ
**	a_name		The name of the file within its directory, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the file has been excluded;
**	STS$K_WARN	- it has not.
**--
*/
static	int	s_srch$excluded	(
		SEARCH$SEL *	a_sel,
	const	char *		a_fspec,
	const	char *		a_name
			)
{
	for ( unsigned i = 0; i < a_sel->exclcnt; i++ )
		{
		if ( strchr(a_sel->excl [i], '/') )
			{
			if ( 1 & s_srch$wildmat(a_sel->excl [i], a_fspec) )
				return	STS$K_SUCCESS;
			}
		else if ( 1 & s_srch$wildmat(a_sel->excl [i], a_name) )
			return	STS$K_SUCCESS;
		}

	return	STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Append a name to the collection of a directory, growing the array
**	when it is full.
**
**  FORMAL PARAMETERS:
**
**	a_dir		The collection
**	a_name		The name to be appended, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Allocates memory.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the name has been appended;
**	STS$K_FATAL	- out of memory, the condition has been signalled.
**--
*/
static	int	s_srch$dir_add	(
		SEARCH$DIR *	a_dir,
	const	char *		a_name
			)
{
char **		l_new;
unsigned	l_max;

	if ( a_dir->cnt == a_dir->max )
		{
		l_max	= a_dir->max + SEARCH$K_DIRGROW;

		if ( !(l_new = realloc(a_dir->name, l_max * sizeof(char *))) )
			return	$SRCHMSG(SEARCH$_NOMEM, (l_max * sizeof(char *)), errno, strerror(errno));

		a_dir->name	= l_new;
		a_dir->max	= l_max;
		}

	if ( !(a_dir->name [a_dir->cnt] = strdup(a_name)) )
		return	$SRCHMSG(SEARCH$_NOMEM, (strlen(a_name) + 1), errno, strerror(errno));

	a_dir->cnt++;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Release the collection of a directory.
**
**  FORMAL PARAMETERS:
**
**	a_dir		The collection
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Releases memory.
**
**  RETURN VALUE:
**	STS$K_SUCCESS always.
**--
*/
static	int	s_srch$dir_free	(
		SEARCH$DIR *	a_dir
			)
{
	for ( unsigned i = 0; i < a_dir->cnt; i++ )
		free(a_dir->name [i]);

	free(a_dir->name);

	a_dir->name	= NULL;
	a_dir->cnt	= a_dir->max = 0;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The comparison routine of the sort of a directory: plain byte
**	order, so that the output of a run is reproducible.
**
**  FORMAL PARAMETERS:
**
**	a_1, a_2	Addresses of the two names to be compared
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	Negative, zero or positive, as STRCMP has it.
**--
*/
static	int	s_srch$namecmp	(
	const	void *		a_1,
	const	void *		a_2
			)
{
	return	strcmp(*(const char **) a_1, *(const char **) a_2);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Compose a file specification of a directory and a name, keeping the
**	result within SEARCH$K_SZ_PATH.  A directory of "." contributes
**	nothing, so that the specifications the utility displays look
**	exactly like the ones it has been given.
**
**  FORMAL PARAMETERS:
**
**	a_dir		The directory, ASCIZ
**	a_name		The name within it, ASCIZ
**	a_bufsz		Capacity of the buffer
**	a_buf		Receives the specification, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the specification has been composed;
**	STS$K_WARN	- it would not fit and has been truncated.
**--
*/
static	int	s_srch$mkspec	(
	const	char *		a_dir,
	const	char *		a_name,
		unsigned	a_bufsz,
		char *		a_buf
			)
{
unsigned	l_dirlen, l_namlen;

	if ( !strcmp(a_dir, ".") )
		return	srch$strcpy(a_bufsz, a_buf, a_name);

	l_dirlen	= (unsigned) strlen(a_dir);
	l_namlen	= (unsigned) strlen(a_name);

	/* Directory + '/' + name + the terminator */
	if ( (l_dirlen + 1 + l_namlen + 1) > a_bufsz )
		{
		srch$strcpy(a_bufsz, a_buf, a_dir);

		return	STS$K_WARN;
		}

	memcpy(a_buf, a_dir, l_dirlen);

	/* A directory already ending in a slash - "/" itself - does not get a second one */
	if ( l_dirlen && (a_dir [l_dirlen - 1] != '/') )
		a_buf [l_dirlen++] = '/';

	memcpy(a_buf + l_dirlen, a_name, l_namlen + 1);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Walk one directory: hand every file whose name matches the pattern
**	and which passes the filters over to the action routine and, when
**	/RECURSIVE is in force, descend into the subdirectories.
**
**  FORMAL PARAMETERS:
**
**	a_dir		The directory to be read, ASCIZ
**	a_pat		The pattern applied to the names within, ASCIZ
**	a_sel		The selection criteria
**	a_depth		Nesting of this call, 0 - the top
**	a_rtn		The action routine, called per selected file
**	a_arg		Opaque argument of the action routine
**	a_cnt		Incremented once per selected file
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the directory, calls the action routine.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the directory has been walked;
**	STS$K_FATAL	- the action routine has asked for the walk to stop;
**	STS$K_ERROR	- it could not be read, the condition has been signalled.
**--
*/
static	int	s_srch$walk	(
	const	char *		a_dir,
	const	char *		a_pat,
		SEARCH$SEL *	a_sel,
		int		a_depth,
		int		(*a_rtn) (const char *a_fspec, void *a_arg),
		void *		a_arg,
		unsigned *	a_cnt
			)
{
DIR *		l_dirp;
struct dirent *	l_dp;
struct stat	l_st = {0};
SEARCH$DIR	l_names = {0};
char		l_fspec [SEARCH$K_SZ_PATH];
int		l_status = STS$K_SUCCESS;

	if ( !(l_dirp = opendir(a_dir)) )
		return	$SRCHMSG(SEARCH$_OPENDIR, a_dir, errno, strerror(errno));

	/*
	**  The names are collected and sorted before anything is done with
	**  them: READDIR delivers them in the order of the directory, which
	**  is an implementation detail of the file system and would make the
	**  output of two identical runs differ.
	*/
	while ( (l_dp = readdir(l_dirp)) )
		{
		if ( !strcmp(l_dp->d_name, ".") || !strcmp(l_dp->d_name, "..") )
			continue;

		if ( !(1 & (l_status = s_srch$dir_add(&l_names, l_dp->d_name))) )
			break;
		}

	closedir(l_dirp);

	if ( !(1 & l_status) )
		{
		s_srch$dir_free(&l_names);

		return	l_status;
		}

	if ( l_names.cnt > 1 )
		qsort(l_names.name, l_names.cnt, sizeof(char *), s_srch$namecmp);

	for ( unsigned i = 0; i < l_names.cnt; i++ )
		{
		if ( !(1 & s_srch$mkspec(a_dir, l_names.name [i], sizeof(l_fspec), l_fspec)) )
			continue;

		/*
		**  LSTAT, not STAT: a symbolic link is looked at as a link.  A
		**  link to a directory is therefore never descended into, which
		**  is what keeps a link pointing at an ancestor from turning the
		**  walk into an endless one.
		*/
		if ( lstat(l_fspec, &l_st) )
			continue;

		if ( S_ISDIR(l_st.st_mode) )
			{
			if ( !a_sel->recurse )
				continue;

			if ( a_depth >= SEARCH$K_MAXDEPTH )
				{
				$SRCHMSG(SEARCH$_TOODEEP, l_fspec, SEARCH$K_MAXDEPTH);
				continue;
				}

			/* An excluded directory is not descended into at all */
			if ( 1 & s_srch$excluded(a_sel, l_fspec, l_names.name [i]) )
				continue;

			if ( STS$K_FATAL == s_srch$walk(l_fspec, a_pat, a_sel, a_depth + 1, a_rtn, a_arg, a_cnt) )
				{
				s_srch$dir_free(&l_names);

				return	STS$K_FATAL;
				}

			continue;
			}

		if ( !S_ISREG(l_st.st_mode) )
			continue;

		if ( !(1 & s_srch$wildmat(a_pat, l_names.name [i])) )
			continue;

		if ( 1 & s_srch$excluded(a_sel, l_fspec, l_names.name [i]) )
			continue;

		if ( !(1 & s_srch$intime(a_sel, &l_st)) )
			continue;

		(*a_cnt)++;

		/*
		**  STS$K_FATAL from the action routine means that nothing more
		**  is wanted at all - the reader has abandoned the display -
		**  and it is carried up rather than merely noted: opening the
		**  rest of the tree to display nothing would be waste.
		*/
		if ( STS$K_FATAL == a_rtn(l_fspec, a_arg) )
			{
			s_srch$dir_free(&l_names);

			return	STS$K_FATAL;
			}
		}

	s_srch$dir_free(&l_names);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Expand one file specification and call the action routine for every
**	file it selects.
**
**	The specification is split at the last slash into a directory and a
**	pattern.  A specification carrying no wildcard is taken literally:
**	a directory is searched as if the pattern "*" had been applied within it, a file
**	is handed over as it is - it still has to pass /EXCLUDE, /SINCE and
**	/BEFORE, but it is never silently dropped for having an unexpected
**	type, so that a named pipe or a device asked for by name is read.
**
**  FORMAL PARAMETERS:
**
**	a_spec		The file specification, ASCIZ
**	a_sel		The selection criteria
**	a_rtn		The action routine, called per selected file
**	a_arg		Opaque argument of the action routine
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the file system, calls the action routine.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- at least one file has been selected;
**	STS$K_WARN	- none has, the caller is to signal SEARCH$_NOFILES;
**	STS$K_FATAL	- the action routine has asked for the walk to stop,
**			  and the caller is to stop as well;
**	STS$K_ERROR	- a directory could not be read.
**--
*/
int	srch$forfiles	(
	const	char *		a_spec,
		SEARCH$SEL *	a_sel,
		int		(*a_rtn) (const char *a_fspec, void *a_arg),
		void *		a_arg
			)
{
struct stat	l_st = {0};
const char *	l_slash, *l_pat;
char		l_dir [SEARCH$K_SZ_PATH], l_spec [SEARCH$K_SZ_PATH];
char		l_name [SEARCH$K_SZ_PATH];
unsigned	l_cnt = 0, l_dirlen;
int		l_status;

	if ( !a_spec || !*a_spec )
		return	STS$K_WARN;

	if ( !(1 & srch$strcpy(sizeof(l_spec), l_spec, a_spec)) )
		return	$SRCHMSG(SEARCH$_IVQUAL, a_spec, "P1");

	/* '?' is the '%' of OpenVMS, and the matcher knows the latter alone */
	s_srch$normpat(l_spec);

	/*
	**  A literal specification: a directory stands for everything in it,
	**  a file stands for itself.
	*/
	if ( !(1 & s_srch$iswild(l_spec)) )
		{
		if ( stat(l_spec, &l_st) )
			return	$SRCHMSG(SEARCH$_OPENIN, l_spec, errno, strerror(errno));

		if ( S_ISDIR(l_st.st_mode) )
			{
			l_status	= s_srch$walk(l_spec, "*", a_sel, 0, a_rtn, a_arg, &l_cnt);

			return	(1 & l_status) ? (l_cnt ? STS$K_SUCCESS : STS$K_WARN) : l_status;
			}

		/* The name of the file, for the sake of a slashless /EXCLUDE pattern */
		l_slash	= strrchr(l_spec, '/');
		srch$strcpy(sizeof(l_name), l_name, l_slash ? (l_slash + 1) : l_spec);

		if ( 1 & s_srch$excluded(a_sel, l_spec, l_name) )
			return	STS$K_WARN;

		if ( !(1 & s_srch$intime(a_sel, &l_st)) )
			return	STS$K_WARN;

		if ( STS$K_FATAL == a_rtn(l_spec, a_arg) )
			return	STS$K_FATAL;

		return	STS$K_SUCCESS;
		}

	/*
	**  A pattern: everything up to the last slash is the directory - a
	**  wildcard there is not expanded, /RECURSIVE is the way to walk a
	**  tree - and everything past it is applied to the names within.
	*/
	if ( (l_slash = strrchr(l_spec, '/')) )
		{
		l_dirlen	= (unsigned) (l_slash - l_spec);

		/* "/pattern" - the root, and it does keep its slash */
		if ( !l_dirlen )
			l_dirlen = 1;

		if ( l_dirlen >= sizeof(l_dir) )
			return	$SRCHMSG(SEARCH$_IVQUAL, a_spec, "P1");

		memcpy(l_dir, l_spec, l_dirlen);
		l_dir [l_dirlen]	= '\0';

		l_pat	= l_slash + 1;
		}
	else	{
		srch$strcpy(sizeof(l_dir), l_dir, ".");
		l_pat	= l_spec;
		}

	/* "dir/" - no pattern at all - means everything in the directory */
	if ( !*l_pat )
		l_pat	= "*";

	l_status	= s_srch$walk(l_dir, l_pat, a_sel, 0, a_rtn, a_arg, &l_cnt);

	return	(1 & l_status) ? (l_cnt ? STS$K_SUCCESS : STS$K_WARN) : l_status;
}
