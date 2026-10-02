#define	__MODULE__	"SRCHHLP"
#define	__IDENT__	"X01-02"
#define	__REV__		"1.2.0"

/*
**++
**
**  FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
**
**  MODULE:	srchhlp.c
**
**  ABSTRACT:	The /HELP of the utility, answered out of the help library
**		of the product rather than out of a string built into the
**		image.
**
**  DESCRIPTION: The description of the utility is a help library, and the
**		reading side of the HELP subsystem is a library of its own,
**		so the utility answers for its own help instead of leaving
**		the reader to invoke the browser:
**
**		    $ search /HELP			the whole topic
**		    $ search /HELP window		one subtopic
**		    $ search /HELP -- /WINDOW		the same, spelled out
**
**		A subtopic of this library is a qualifier, and a qualifier
**		is spelled with a slash - which the command line of this
**		very utility swallows.  A word that names no subtopic is
**		therefore tried once more with a slash in front of it, so
**		that the two forms above are one and the same.
**
**		The library is looked for
**
**		  1. where SEARCH$HELPLIB says, when it says anything;
**		  2. beside the image - <image>/../share/help/search.hlb -
**		     which makes an installed tree relocatable, and
**		     <image>/search.hlb, which makes a build tree work;
**		  3. where this product was configured to install it.
**
**		Failing all of them the caller displays the summary built
**		into the image: a missing library must not leave /HELP
**		with nothing to say.
**
**		This module is compiled only when the HELP product was
**		found at configuration time; see SEARCH_WITH_HLB in
**		CMakeLists.txt.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:   1-AUG-2026
**
**  MODIFICATION HISTORY:
**
**	X01-02		 3-AUG-2026	RRL
**		The FAO parameters of a message are cast and counted by
**		$SRCHMSG rather than by hand.
**
**	X01-01		 1-AUG-2026	RRL
**		Initial version.
**
**--
*/

#include	<stdlib.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/stat.h>

#include	"srchdef.h"

#ifdef	SEARCH_WITH_HLB

/*
**  HELPDEF.H of the HELP product defines __MODULE__ and the rest only
**  when they are absent, so the identification of this module stands.
*/
#include	"helpdef.h"

/*
**  The environment variable that overrides the search, and the name of
**  the library within a directory of it.
*/
#define	SEARCH$K_HLBLOG		"SEARCH$HELPLIB"
#define	SEARCH$K_HLBNAME	"search.hlb"

/*
**  The top level key of the library: everything the utility documents
**  hangs below it.
*/
#define	SEARCH$K_HLBTOP		"SEARCH"


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tell whether a file specification names a readable regular file.
**
**  FORMAL PARAMETERS:
**
**	a_fspec		The specification, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the file system.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the file is there and can be read;
**	STS$K_WARN	- it is not.
**--
*/
static	int	s_srch$readable	(
	const	char *		a_fspec
			)
{
struct stat	l_st = {0};

	if ( !a_fspec || !*a_fspec )
		return	STS$K_WARN;

	if ( stat(a_fspec, &l_st) || !S_ISREG(l_st.st_mode) )
		return	STS$K_WARN;

	return	access(a_fspec, R_OK) ? STS$K_WARN : STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Locate the help library of the product.  The search is described in
**	the abstract of the module; the first candidate that is a readable
**	file wins.
**
**  FORMAL PARAMETERS:
**
**	a_bufsz		Capacity of the buffer
**	a_buf		Receives the specification, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the environment and the file system.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- <a_buf> holds the specification of a library;
**	STS$K_WARN	- no library can be found.
**--
*/
static	int	s_srch$findlib	(
		unsigned	a_bufsz,
		char *		a_buf
			)
{
const char *	l_env;
char		l_image [SEARCH$K_SZ_PATH] = {0};
char *		l_slash;
ssize_t		l_len;

	/* 1. The environment variable: a library, taken as it stands */
	if ( (l_env = getenv(SEARCH$K_HLBLOG)) && *l_env )
		{
		srch$strcpy(a_bufsz, a_buf, l_env);

		if ( 1 & s_srch$readable(a_buf) )
			return	STS$K_SUCCESS;
		}

	/*
	**  2. Beside the image.  The path of the running program is read
	**  from /proc/self/exe rather than taken from argv[0]: the latter
	**  is whatever the caller chose to pass, and a utility invoked
	**  through PATH does not carry a directory in it at all.
	*/
	if ( 0 < (l_len = readlink("/proc/self/exe", l_image, sizeof(l_image) - 1)) )
		{
		l_image [l_len]	= '\0';

		if ( (l_slash = strrchr(l_image, '/')) )
			{
			*l_slash	= '\0';

			/* <image>/../share/help/search.hlb - an installed tree */
			srch$strcpy(a_bufsz, a_buf, l_image);
			srch$strcpy(a_bufsz - (unsigned) strlen(a_buf), a_buf + strlen(a_buf),
				"/../share/help/" SEARCH$K_HLBNAME);

			if ( 1 & s_srch$readable(a_buf) )
				return	STS$K_SUCCESS;

			/* <image>/search.hlb - a build tree, where both are made */
			srch$strcpy(a_bufsz, a_buf, l_image);
			srch$strcpy(a_bufsz - (unsigned) strlen(a_buf), a_buf + strlen(a_buf),
				"/" SEARCH$K_HLBNAME);

			if ( 1 & s_srch$readable(a_buf) )
				return	STS$K_SUCCESS;
			}
		}

	/* 3. Where the product was configured to install the library */
#ifdef	SEARCH_K_HLBSPEC
	srch$strcpy(a_bufsz, a_buf, SEARCH_K_HLBSPEC);

	if ( 1 & s_srch$readable(a_buf) )
		return	STS$K_SUCCESS;
#endif

	a_buf [0]	= '\0';

	return	STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Descend one level of the topic tree.  The word, or the run of words
**	that a key carrying blanks is made of, is looked up as it has been
**	given; failing that it is looked up once more with a slash in front
**	of it, so that a qualifier is asked for as "window" as well as as
**	"/window".
**
**  FORMAL PARAMETERS:
**
**	a_lib		The library
**	a_parent	Index of the topic to descend from
**	a_wordcnt	Number of the words left
**	a_words		The words themselves
**	a_used		Receives the number of the words consumed
**	a_index		Receives the index of the topic found
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the topic has been found;
**	STS$K_WARN	- no subtopic of this name.
**--
*/
static	int	s_srch$descend	(
		HLB$LIB *	a_lib,
		int		a_parent,
		int		a_wordcnt,
		char **		a_words,
		int *		a_used,
		int *		a_index
			)
{
char		l_slashed [SEARCH$K_SZ_STR] = {'/'};
char *		l_word [1];

	if ( 1 & hlb$lookup_run(a_lib, a_parent, a_wordcnt, a_words, a_used, a_index) )
		return	STS$K_SUCCESS;

	/* The same word, spelled the way the qualifier is */
	if ( a_words[0][0] == '/' )
		return	STS$K_WARN;

	srch$strcpy((unsigned) sizeof(l_slashed) - 1, l_slashed + 1, a_words [0]);

	l_word [0]	= l_slashed;

	if ( !(1 & hlb$lookup_run(a_lib, a_parent, 1, l_word, a_used, a_index)) )
		return	STS$K_WARN;

	*a_used	= 1;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Answer /HELP out of the help library of the product: locate the
**	library, descend to the topic that has been asked for and display
**	its text, the path of the topic standing as the heading.
**
**  FORMAL PARAMETERS:
**
**	a_wordcnt	Number of the words naming the topic, 0 - the whole
**			description of the utility
**	a_words		The words themselves
**	a_page		Non-zero - stop at the end of a page, /PAGE
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the library, writes to the standard output.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the topic has been displayed;
**	STS$K_WARN	- no library can be found, the caller is to display
**			  the summary built into the image;
**	STS$K_ERROR	- the library is there but names no such topic.
**--
*/
int	srch$help	(
		int		a_wordcnt,
		char **		a_words,
		int		a_page
			)
{
HLB$LIB *	l_lib = NULL;
HELP$OUT	l_out;
const char *	l_txt = NULL;
char		l_fspec [SEARCH$K_SZ_PATH] = {0}, l_path [SEARCH$K_SZ_PATH] = {0};
unsigned	l_txtlen = 0, l_pathlen = 0;
int		l_parent, l_index, l_used, l_pos = 0;

	if ( !(1 & s_srch$findlib((unsigned) sizeof(l_fspec), l_fspec)) )
		return	STS$K_WARN;

	if ( !(1 & hlb$open(l_fspec, &l_lib)) )
		return	STS$K_WARN;

	/*
	**  Everything this library documents hangs below the one key of
	**  level one, so the descent starts there rather than at the root:
	**  the reader asks for a qualifier, not for "search /window".
	*/
	if ( !(1 & hlb$lookup(l_lib, -1, (unsigned) sizeof(SEARCH$K_HLBTOP) - 1, SEARCH$K_HLBTOP, &l_parent)) )
		{
		hlb$close(l_lib);

		return	STS$K_WARN;
		}

	while ( l_pos < a_wordcnt )
		{
		if ( !(1 & s_srch$descend(l_lib, l_parent, a_wordcnt - l_pos, a_words + l_pos, &l_used, &l_index)) )
			{
			$SRCHMSG(SEARCH$_NOTOPIC, a_words [l_pos]);
			hlb$close(l_lib);

			return	STS$K_ERROR;
			}

		l_parent	= l_index;
		l_pos		+= l_used;
		}

	if ( !(1 & hlb$gettext(l_lib, l_parent, &l_txtlen, &l_txt)) )
		{
		hlb$close(l_lib);

		return	STS$K_WARN;
		}

	/*
	**  The display is the one of the HELP subsystem, so that a topic
	**  looks here exactly as it looks under the browser - the indent of
	**  a level, the width of the terminal and the pager included.
	*/
	help$out_init(&l_out, STDOUT_FILENO, STDIN_FILENO, a_page, 0);

	if ( 1 & hlb$path(l_lib, l_parent, (unsigned) sizeof(l_path), l_path, &l_pathlen) )
		help$put(&l_out, "\n  !AD\n\n", 2, (fao_prm_t) l_pathlen, (fao_prm_t) l_path);

	help$put_text(&l_out, 4, l_txtlen, l_txt);
	help$put(&l_out, "\n", 0);
	help$flush(&l_out);

	hlb$close(l_lib);

	return	STS$K_SUCCESS;
}

#else	/* SEARCH_WITH_HLB */

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The stand-in of the routine above, compiled when the HELP product
**	was not found: there is no library to read, so the caller displays
**	the summary built into the image.
**
**  FORMAL PARAMETERS:
**
**	a_wordcnt	Number of the words naming the topic
**	a_words		The words themselves
**	a_page		Non-zero - stop at the end of a page, /PAGE
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_WARN always.
**--
*/
int	srch$help	(
		int		a_wordcnt,
		char **		a_words,
		int		a_page
			)
{
	return	STS$K_WARN;
}

#endif	/* SEARCH_WITH_HLB */
