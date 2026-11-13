#define	__MODULE__	"SEARCH"
#define	__IDENT__	"X.01-33ECO03"
#define	__REV__		"1.33.3"

/*
**++
**
**  FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
**
**  MODULE:	search.c
**
**  ABSTRACT:	The SEARCH utility - display of the records of a set of
**		files that carry a given combination of strings, after the
**		DCL SEARCH command of OpenVMS.
**
**  DESCRIPTION: Command syntax:
**
**		    SEARCH file-spec[,...] search-string[,...] [/qualifiers]
**
**		    /EXACT		the case of the letters of the search
**					strings is significant; file names are
**					never affected
**		    /MATCH=option	how the strings are combined:
**					OR (default), AND, NAND, NOR
**		    /WINDOW[=(n,m)]	display n records before and m after
**					a match; one value - both sides
**		    /[NO]PAGE		stop, or do not stop, at the end of
**					a page
**		    /[NO]HIGHLIGHT[=style]
**					mark, or do not mark, the strings
**					that have been found
**		    /[NO]NUMBERS	prefix, or do not prefix, the record
**					number
**		    /[NO]HEADING	announce, or do not announce, the
**					name of every file; on by default
**		    /OUTPUT=filespec	write the result to a file
**		    /[NO]LOG		report every file searched
**		    /[NO]STATISTICS	report the totals at the end
**		    /EXCLUDE=(pat,...)	drop the files matching a pattern
**		    /SINCE=time		take the files not older than
**		    /BEFORE=time	take the files older than
**		    /MODIFIED		/SINCE and /BEFORE apply to the time
**					of the last modification (default)
**		    /CREATED		... to the time of the last change
**					of the inode - the closest analogue
**					Linux has to a creation time
**		    /BINARY=option	a binary file: NOTE that it matches
**					(default), SKIP it, or search it as TEXT
**		    /FORMAT=option	VMS (default), or GREP: spec:number:record
**					one line a record, for a tool to read
**		    /THREADS[=n]	search n files at once, the display that
**					of a single thread; default the processors
**		    /CONTINUE[=n]	the last file selected is followed past
**					its end: looked at again every n seconds
**					(default 1), the records appended to it
**					searched as they come, until ^C
**		    /HELP		display the syntax
**
**		The first word that is not a qualifier is the list of the
**		file specifications, every word after it is a list of the
**		search strings; a list is separated by commas, and an
**		element carrying a comma of its own is to be enclosed in
**		double quotes - or passed as a word of its own, which the
**		shell makes easier:
**
**		    $ search '*.c' alpha,beta		two strings
**		    $ search '*.c' '"alpha,beta"'	one string
**		    $ search '*.c' alpha beta		two strings
**
**		A search string is a plain substring: no wildcard, no
**		regular expression, nothing to escape - exactly as on
**		OpenVMS.  A file specification, in contrast, IS expanded
**		by the utility ('*', '%', '?'), so quote it against the
**		shell to have it expanded here - which is what /EXCLUDE
**		and the "..." directory need to be of any use.
**
**		A directory "..." walks the tree below the one before it -
**		the [...] of OpenVMS: '.../x*.com', './.../x*.com',
**		'/usr/src/.../x*.c', 'src/...'.  The names of files are
**		matched without regard to case, as on OpenVMS - of the Latin
**		(ASCII) letters only, and whatever /EXACT says: a pattern and a
**		name spelled out in full alike.
**
**		A lone "--" ends the qualifiers, so that a file or a
**		string spelled like one can be asked for.
**
**		A word beginning with a slash is a qualifier only when it
**		names one - /EXACT does, /usr/src/x.c and /etc do not -
**		since on this system a slash begins an absolute file
**		specification as well.  A word beginning with a DOUBLE dash
**		is always a qualifier - --EXACT - and a word beginning
**		with a single one is an ordinary value, so a file spelled
**		-notes.txt needs no ceremony.  -h alone asks for /HELP.
**		/RECURSIVE, which is gone, is refused in any spelling it used
**		to be accepted in (S_SEARCH$ISRECUR), so that a command of old
**		is not taken for a search string in silence.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:   1-AUG-2026
**
**  NOTE:	__IDENT__ and __REV__ are two spellings of one number and
**		are kept in step by hand, which the build checks: see
**		CMakeLists.txt, S_SEARCH_CHECK_IDENT.
**
**		    __IDENT__	the OpenVMS spelling, Xnn-nn - the major
**				and the minor of the revision - with
**				ECOnn after it for a minor correction of
**				an error;
**		    __REV__	the Unix spelling, made out of __IDENT__:
**				major.minor.nn, the nn of an ECO standing
**				as the patch level and 0 standing where
**				there is no ECO.
**
**		So X01-23 is 1.23.0 and X01-23ECO02 is 1.23.2.
**
**		This is the head module of the product, and __IDENT__ above
**		is the identification of the PRODUCT, not of this module
**		alone: the labels of the modification history below number
**		the revisions of THIS module, and the two therefore run
**		apart - a release that did not touch this file bears no
**		label here.  Every other module carries the revision of
**		that module in both places.  It is stated here and nowhere else: the build reads
**		it out of this file and stamps it into the help libraries
**		and the manual page, so that the kit, the libraries and the
**		documentation cannot disagree about what they are.  Every
**		other module carries the revision of that module, as usual.
**
**  MODIFICATION HISTORY:
**
**	X.01-33ECO03	13-NOV-2026	RRL
**		The product: the first one released with Debian packages
**		(search_1.33.3, amd64 and arm64) and a signed SHA256SUMS on
**		GitHub; the build options lost their '$' (SEARCH_M_*,
**		SEARCH_K_*, the old names still taken).  No change to this
**		module but the identification.
**
**	X.01-33ECO02	12-NOV-2026	CLD
**		The product: the image looked for its help library at
**		<prefix>//search.hlb - CMakeLists.txt (X-26) made SEARCH_K_HLBSPEC
**		of the directory before it was set.  It is now
**		<prefix>/share/help/search.hlb, where the library is installed.
**		No change to this module but the identification.
**
**	X01-33ECO1	11-NOV-2026	CLD
**		/RECURSIVE, /REC, --RECURSIVE, /NORECURSIVE ... were taken for
**		a search string (the slash form) or refused as an unknown
**		qualifier (the dash form): S_SEARCH$ISRECUR catches every
**		spelling of the qualifier that used to be accepted and signals
**		SEARCH$_NORECURSE, completion code 2.  A file named in full is
**		looked for without regard to case too (SRCHFIL X01-06ECO1).
**		The /EXCLUDE patterns are folded to upper case once, here, not
**		at every name.  /HELP: /EXACT concerns the search strings
**		only, the case of file names is ignored for ASCII letters.
**
**	X01-33		10-NOV-2026	RRL
**		"search ... ничего не искал, поиск был рекурсивным ... при
**		указании явно одного файла - находил": the names of files are
**		matched without regard to case, so '*.com' takes X.COM again;
**		and "давай вместо ключа recur сделаем это указанием пути":
**		a directory "..." walks the tree below it (SRCHFIL X01-06),
**		and /RECURSIVE is gone - RRL: "убрать совсем".
**
**	X01-32		21-OCT-2026	RRL
**		"добавим ключ /continue[=секунды]": the last file the command
**		selects is held back by the walk (S_SEARCH$HOLDFILE), searched
**		after every other one has been displayed, and followed past its
**		end - every /CONTINUE seconds the records appended to it are
**		searched, a file cut short or rotated is read anew (SRCHMAT
**		X01-17).  ^C ends the follow, and the run ends as usual - the
**		statistics, the completion code.
**
**	X01-31		 2-OCT-2026	RRL
**		"делай 3-5": a file specification that selects no file is
**		still %SEARCH-W-NOFILES, but completes with 2, as GREP does with
**		a name that is not there (SRCH$FAIL).  The walk asks LSTAT only
**		where it must (SRCHFIL X01-05); a large result is put out as it
**		comes rather than gathered whole (SRCHTHR X01-02, SRCHMAT
**		X01-16).
**
**	X01-30		 2-OCT-2026	RRL
**		"делай 1 и 2": the case is ignored in any script the locale
**		knows, not in ASCII alone - LC_CTYPE is the user's if it is a
**		UTF-8 locale, C.UTF-8 otherwise; the faster matcher and the
**		exact count of the records are SRCHMAT X01-15.
**
**	X01-29		 2-OCT-2026	RRL
**		Under /THREADS a single file of 32 MB or more is searched in
**		pieces by several threads at once - see SRCHMAT X01-14;
**		nothing in this module changed but the ident.
**
**	X01-28		 2-OCT-2026	RRL
**		/THREADS[=n]: the files the walk picks are searched by a pool
**		of n threads (SRCHTHR.C) and displayed in the order of the
**		walk, so the display is that of a single thread.  The default
**		is the number of the processors, at most 16; /THREADS=1, /LOG
**		and /PAGE run the walk in the main thread alone, as before.
**
**	X01-27		 2-OCT-2026	RRL
**		/FORMAT=VMS|GREP: GREP displays spec:number:record, one line a
**		record, for a tool to read, and implies /NUMBERS; no heading.
**		The completion codes are those of GREP: 0 a match, 1 none, 2
**		an error - signalled anywhere in the run, matched or not, or a
**		command that could not be parsed.
**
**	X01-26		 2-OCT-2026	RRL
**		A record is no longer cut at 8192 bytes - see SRCHMAT X01-12;
**		nothing in this module changed but the ident.
**
**	X01-25		 2-OCT-2026	RRL
**		"доработай search": /BINARY=NOTE|SKIP|TEXT - what is done with
**		a binary file, one with a NUL among its first 32 KB.  NOTE, the
**		default, reports in one line that the file matches instead of
**		displaying its "records".
**
**	X01-24ECO1	 2-OCT-2026	RRL
**		"почини справку": the help libraries are built NOFACILITY -
**		HELP_ADD_LIBRARY() rooted them at the facility SEARCH, and the
**		source, which already begins with its own topic SEARCH, sank a
**		level, so "HELP SEARCH /EXACT" found nothing (the test helplib
**		failed 6 of 7).  Nothing in this module changed but the ident.
**
**	X01-19		21-AUG-2026	RRL
**		The patterns of /EXCLUDE are normalized after the account
**		of the command is made, so that the account shows them as
**		they were given.
**
**	X01-18		21-AUG-2026	RRL
**		The identification is spelled Xnn-nn[ECOnn], as the line
**		has it.
**
**	X01-17		21-AUG-2026	RRL
**		The authorship is spelled in full and the marks of the
**		modification history are RRL, as the line has them.
**
**	X01-16		21-AUG-2026	RRL
**		The convention __IDENT__ and __REV__ are kept by is stated
**		here and checked by the build.
**
**	X01-15		14-AUG-2026	RRL
**		S_SEARCH$SPLIT left the count unwritten when the vector
**		filled up, so a list of more than SEARCH$K_MAXSTR elements
**		was reported as no list at all - SEARCH$_NOSTRING instead
**		of SEARCH$_TOOMANY.
**
**	X01-14		14-AUG-2026	RRL
**		/LOG reports what the command has been made to mean - the
**		strings, how they combine, the files - before the first
**		file is opened.
**
**	X01-13		14-AUG-2026	RRL
**		The message catalogue is registered with the subsystem of
**		the STARLET library before the command is looked at.
**
**	X01-12		 7-AUG-2026	RRL
**		One walk of the command line settles /PAGE as well:
**		S_SEARCH$PAGEARG walked it a second time for that alone.
**
**	X01-11		 5-AUG-2026	RRL
**		The Unix spelling of a qualifier is the GNU long form --NAME
**		since CLI_ROUTINES X.00-14; a lone -NAME is an ordinary
**		value now, and was being handed to a service that silently
**		steps over it.  -h is still accepted for /HELP.
**
**	X01-10		 4-AUG-2026	RRL
**		__IDENT__ of this module is the identification of the whole
**		product now: it was written out three times - here, in
**		CMakeLists.txt and in the manual page - and the three had
**		drifted to X-09, X-15 and X-14 respectively.
**
**	X01-09		 3-AUG-2026	RRL
**		The FAO parameters of a message are cast and counted by
**		$SRCHMSG and $SRCHPUT rather than by hand.
**
**	X01-08		 3-AUG-2026	RRL
**		The summary names the /HELP of this utility rather than the
**		browser: the images of the HELP product are VHELP and
**		VHELPTRAN since X-22, and naming them here would only tie
**		the summary to a revision of another product.
**
**	X01-07		 2-AUG-2026	RRL
**		A word beginning with a slash is a qualifier only when it
**		names one: on this system a slash begins an absolute file
**		specification as well, and /tmp/x.c was being taken for a
**		qualifier called 'tmp/x.c'.
**
**	X01-06		 1-AUG-2026	RRL
**		/HELP is answered out of the help library of the product,
**		the summary built into the image standing in only when no
**		library can be found.  The words of the command name the
**		topic: search /HELP window.
**
**	X01-05		 1-AUG-2026	RRL
**		Which stamp the heading of a file displays follows
**		/MODIFIED and /CREATED.
**
**	X01-04		 1-AUG-2026	RRL
**		/LOG reports the absolute time a value of /SINCE or
**		/BEFORE has been resolved to: a delta and a keyword say
**		nothing about the date they land on.
**
**	X01-03		 1-AUG-2026	RRL
**		/PAGE stops the display at the end of a page, /HIGHLIGHT
**		marks what has been found; both are granted by default
**		only when the output really is a terminal.
**
**	X01-02		 1-AUG-2026	RRL
**		The summary of /HELP points at the help library of the
**		product, which is the description proper.
**
**	X01-01		 1-AUG-2026	RRL
**		Initial version.
**
**--
*/

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<locale.h>
#include	<langinfo.h>
#include	<signal.h>

#include	"srchdef.h"

/*
**  Indices of the qualifier table.  Ordered - hence an enumeration; the
**  table itself must be kept in exactly this order.
*/
enum	{
	SEARCH$K_QUAL_EXACT	= 0,
	SEARCH$K_QUAL_MATCH,
	SEARCH$K_QUAL_WINDOW,
	SEARCH$K_QUAL_PAGE,
	SEARCH$K_QUAL_HIGHLIGHT,
	SEARCH$K_QUAL_NUMBERS,
	SEARCH$K_QUAL_HEADING,
	SEARCH$K_QUAL_OUTPUT,
	SEARCH$K_QUAL_LOG,
	SEARCH$K_QUAL_STATISTICS,
	SEARCH$K_QUAL_EXCLUDE,
	SEARCH$K_QUAL_SINCE,
	SEARCH$K_QUAL_BEFORE,
	SEARCH$K_QUAL_MODIFIED,
	SEARCH$K_QUAL_CREATED,
	SEARCH$K_QUAL_BINARY,
	SEARCH$K_QUAL_FORMAT,
	SEARCH$K_QUAL_THREADS,
	SEARCH$K_QUAL_CONTINUE,
	SEARCH$K_QUAL_HELP,

	SEARCH$K_QUAL_MAX
	};

/*
**  The keywords of /MATCH.  The value of a keyword is the SEARCH$K_MATCH_*
**  constant it stands for.
*/
static	CLI_KEYWORD	s_matchkwd [] = {
	{ .name = {$ASCINI("OR")},	.val = SEARCH$K_MATCH_OR },
	{ .name = {$ASCINI("AND")},	.val = SEARCH$K_MATCH_AND },
	{ .name = {$ASCINI("NAND")},	.val = SEARCH$K_MATCH_NAND },
	{ .name = {$ASCINI("NOR")},	.val = SEARCH$K_MATCH_NOR },
	{ .name = { .len = 0 } }
	};

/*
**  The keywords of /HIGHLIGHT.  The value of a keyword is the
**  SEARCH$K_HL_* constant it stands for.
*/
static	CLI_KEYWORD	s_hlkwd [] = {
	{ .name = {$ASCINI("BOLD")},		.val = SEARCH$K_HL_BOLD },
	{ .name = {$ASCINI("REVERSE")},		.val = SEARCH$K_HL_REVERSE },
	{ .name = {$ASCINI("UNDERLINE")},	.val = SEARCH$K_HL_UNDERLINE },
	{ .name = {$ASCINI("RED")},		.val = SEARCH$K_HL_RED },
	{ .name = {$ASCINI("GREEN")},		.val = SEARCH$K_HL_GREEN },
	{ .name = {$ASCINI("YELLOW")},		.val = SEARCH$K_HL_YELLOW },
	{ .name = { .len = 0 } }
	};

/*
**  The keywords of /BINARY.  The value of a keyword is the SEARCH$K_BIN_*
**  constant it stands for.
*/
static	CLI_KEYWORD	s_binkwd [] = {
	{ .name = {$ASCINI("NOTE")},	.val = SEARCH$K_BIN_NOTE },
	{ .name = {$ASCINI("SKIP")},	.val = SEARCH$K_BIN_SKIP },
	{ .name = {$ASCINI("TEXT")},	.val = SEARCH$K_BIN_TEXT },
	{ .name = { .len = 0 } }
	};

/*
**  The keywords of /FORMAT.  The value of a keyword is the SEARCH$K_FMT_*
**  constant it stands for.
*/
static	CLI_KEYWORD	s_fmtkwd [] = {
	{ .name = {$ASCINI("VMS")},	.val = SEARCH$K_FMT_VMS },
	{ .name = {$ASCINI("GREP")},	.val = SEARCH$K_FMT_GREP },
	{ .name = { .len = 0 } }
	};

/*
**  The qualifier table of the utility.  An empty name closes the table.
*/
static	CLI_PQDESC	s_quals [] = {
	{ .name = {$ASCINI("EXACT")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL,	.flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("MATCH")},		.type = CLI$K_KWD,	.pn = CLI$K_QUAL,	.kwd = s_matchkwd },
	{ .name = {$ASCINI("WINDOW")},		.type = CLI$K_NUM,	.pn = CLI$K_QUAL,	.flag = CLI$M_LIST },
	{ .name = {$ASCINI("PAGE")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL,	.flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("HIGHLIGHT")},	.type = CLI$K_KWD,	.pn = CLI$K_QUAL,	.flag = CLI$M_NEGATABLE,
												.kwd = s_hlkwd },
	{ .name = {$ASCINI("NUMBERS")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL,	.flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("HEADING")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL,	.flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("OUTPUT")},		.type = CLI$K_QSTRING,	.pn = CLI$K_QUAL },
	{ .name = {$ASCINI("LOG")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL,	.flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("STATISTICS")},	.type = CLI$K_OPT,	.pn = CLI$K_QUAL,	.flag = CLI$M_NEGATABLE },
	{ .name = {$ASCINI("EXCLUDE")},		.type = CLI$K_QSTRING,	.pn = CLI$K_QUAL,	.flag = CLI$M_LIST },
	{ .name = {$ASCINI("SINCE")},		.type = CLI$K_QSTRING,	.pn = CLI$K_QUAL },
	{ .name = {$ASCINI("BEFORE")},		.type = CLI$K_QSTRING,	.pn = CLI$K_QUAL },
	{ .name = {$ASCINI("MODIFIED")},	.type = CLI$K_OPT,	.pn = CLI$K_QUAL },
	{ .name = {$ASCINI("CREATED")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL },
	{ .name = {$ASCINI("BINARY")},		.type = CLI$K_KWD,	.pn = CLI$K_QUAL,	.kwd = s_binkwd },
	{ .name = {$ASCINI("FORMAT")},		.type = CLI$K_KWD,	.pn = CLI$K_QUAL,	.kwd = s_fmtkwd },
	{ .name = {$ASCINI("THREADS")},		.type = CLI$K_NUM,	.pn = CLI$K_QUAL },
	{ .name = {$ASCINI("CONTINUE")},	.type = CLI$K_NUM,	.pn = CLI$K_QUAL },
	{ .name = {$ASCINI("HELP")},		.type = CLI$K_OPT,	.pn = CLI$K_QUAL },
	{ .name = { .len = 0 } }
	};

/*
**  The verb the CLI service is shown.  It cannot be "SEARCH" spelled with
**  a slash or a dash, and it is not: the service intercepts /HELP, -h and
**  --help anywhere in the command and prints a summary of its own tables
**  instead of parsing, so the request for the syntax is caught here and
**  the token is never handed over.
*/
static	char	s_clivrb [] = { "SEARCH$CMD" };

static	CLI_VERB	s_verbs [] = {
	{ .name = {$ASCINI("SEARCH$CMD")}, .quals = s_quals },
	{ .name = { .len = 0 } }
	};

static	const char	s_usage [] = {
	"Usage: search file-spec[,...] search-string[,...] [/qualifiers]\n"
	"\n"
	"    /EXACT              the case of the search strings is significant\n"
	"                        (never of file names)\n"
	"    /MATCH=option       OR (default), AND, NAND, NOR\n"
	"    /WINDOW[=(n,m)]     display n records before and m after a match\n"
	"    /[NO]PAGE           stop at the end of a page (a terminal only)\n"
	"    /[NO]HIGHLIGHT[=s]  mark what has been found: BOLD, REVERSE,\n"
	"                        UNDERLINE, RED, GREEN, YELLOW\n"
	"    /[NO]NUMBERS        prefix the record number\n"
	"    /[NO]HEADING        announce the name of every file (default: on)\n"
	"    /OUTPUT=filespec    write the result to a file\n"
	"    /[NO]LOG            report every file searched\n"
	"    /[NO]STATISTICS     report the totals at the end\n"
	"    /EXCLUDE=(pat,...)  drop the files matching a pattern\n"
	"    /SINCE=time         take the files not older than\n"
	"    /BEFORE=time        take the files older than\n"
	"    /MODIFIED /CREATED  which time stamp /SINCE and /BEFORE apply to\n"
	"    /BINARY=option      a binary file: NOTE (default) that it matches,\n"
	"                        SKIP it, or search it as TEXT\n"
	"    /FORMAT=option      VMS (default): headings and records; GREP:\n"
	"                        spec:number:record, one line each, for tools\n"
	"    /THREADS[=n]        search n files at once (default: the processors,\n"
	"                        at most 16); the display is that of one thread\n"
	"    /CONTINUE[=n]       follow the last file: every n seconds (default 1)\n"
	"                        search what has been appended to it, until ^C\n"
	"    /HELP               display this text\n"
	"\n"
	"A search string is a plain substring; a file specification is expanded\n"
	"by the utility itself, so quote it against the shell: search '*.c' foo\n"
	"A directory \"...\" walks the tree below: search '.../*.c' foo, or\n"
	"'./.../*.com', '/usr/src/.../*.h'.  The case of file names is ignored\n"
	"(Latin letters only), whatever /EXACT says.\n"
	"A lone -- ends the qualifiers.\n"
	"\n"
	"The full description is a help library of the HELP subsystem:\n"
	"\n"
	"    $ search /HELP           $ search /HELP window\n"
	};


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tell whether a word of the command is a spelling of /RECURSIVE,
**	which is gone: a "..." directory of the file specification walks a
**	tree now.  Without this the slash form would name no qualifier and
**	become a parameter - a search string, the tree not walked, a
**	completion code of success - and the dash form would be refused as
**	an unknown qualifier, with no word on what stands in its place.
**
**	A spelling is whatever used to be accepted: /RECURSIVE or --RECURSIVE,
**	a leading part of the name (no other qualifier begins with R, so
**	/R was taken as well), the negated form /NORECURSIVE and its leading
**	parts down to /NOR, a value after '=' or ':'.  A word carrying a
**	second slash is a path, as for S_SEARCH$ISQUAL.
**
**  FORMAL PARAMETERS:
**
**	a_word		The word, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the word spells /RECURSIVE;
**	STS$K_WARN	- it does not.
**--
*/
static	int	s_search$isrecur	(
	const	char *		a_word
			)
{
static	const char	s_rec [] = "RECURSIVE";
const char *	l_name;
char		l_ups [sizeof(s_rec) + 2] = {0};
unsigned	l_len;

	if ( (a_word [0] == '-') && (a_word [1] == '-') )
		l_name	= a_word + 2;
	else if ( a_word [0] == '/' )
		l_name	= a_word + 1;
	else	return	STS$K_WARN;

	for ( l_len = 0; l_name [l_len] && (l_name [l_len] != '=') && (l_name [l_len] != ':'); l_len++ )
		{
		if ( l_name [l_len] == '/' )
			return	STS$K_WARN;
		}

	/* Longer than NORECURSIVE - no spelling of it */
	if ( !l_len || (l_len >= sizeof(l_ups)) )
		return	STS$K_WARN;

	srch$upcase(l_len, l_name, l_ups);

	if ( (l_len < sizeof(s_rec)) && !strncmp(l_ups, s_rec, l_len) )
		return	STS$K_SUCCESS;

	if ( (l_len > 2) && !strncmp(l_ups, "NO", 2) && !strncmp(l_ups + 2, s_rec, l_len - 2) )
		return	STS$K_SUCCESS;

	return	STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tell whether a word of the command is a qualifier.
**
**	On OpenVMS the question does not arise: a slash begins a qualifier
**	and nothing else.  Here it begins an absolute file specification
**	too, so the word is looked up in the table: /EXACT names a
**	qualifier, /usr/src/x.c does not and is a file specification.  An
**	abbreviation counts as a name - the CLI service accepts one, and so
**	must this - and so does the negated form of a negatable qualifier.
**
**	A word that names no qualifier is a parameter: /etc and /usr are
**	asked for as readily as /EXACT is, and nothing but the table tells
**	the two apart.  A misspelt qualifier therefore becomes a parameter
**	rather than a diagnostic; the dash form, which is never anything
**	but a qualifier, is the way to have the misspelling refused.  The
**	one exception is /RECURSIVE, which is gone and is caught before
**	this routine is asked, by S_SEARCH$ISRECUR.
**
**	The Unix spelling of a qualifier is the GNU long form, --NAME: that
**	is what CLI_ROUTINES X.00-14 accepts, and a token spelled -NAME is
**	stepped over by it in silence, so such a token must never be handed
**	over as a qualifier.  A single dash therefore begins an ordinary
**	value here - a file spelled -notes.txt is asked for as it stands -
**	and "--" alone still ends the qualifiers.
**
**  FORMAL PARAMETERS:
**
**	a_word		The word, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the module-static S_QUALS.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the word is a qualifier;
**	STS$K_WARN	- it is a file specification or a search string.
**--
*/
static	int	s_search$isqual	(
	const	char *		a_word
			)
{
const char *	l_name;
char		l_ups [SEARCH$K_SZ_STR] = {0};
unsigned	l_len;

	if ( !a_word [0] || !a_word [1] )
		return	STS$K_WARN;

	/* The GNU long form: a qualifier and nothing else.  "--" alone is the marker */
	if ( (a_word [0] == '-') && (a_word [1] == '-') )
		return	a_word [2] ? STS$K_SUCCESS : STS$K_WARN;

	/* A single dash begins a value now, not a qualifier */
	if ( a_word [0] != '/' )
		return	STS$K_WARN;

	l_name	= a_word + 1;

	/* The name of a qualifier ends at its value */
	for ( l_len = 0; l_name [l_len] && (l_name [l_len] != '=') && (l_name [l_len] != ':'); l_len++ )
		{
		/* A second slash makes it a path, whatever it might have spelled */
		if ( l_name [l_len] == '/' )
			return	STS$K_WARN;
		}

	if ( !l_len || (l_len >= sizeof(l_ups)) )
		return	STS$K_WARN;

	srch$upcase(l_len, l_name, l_ups);

	for ( int i = 0; $ASCLEN(&s_quals[i].name); i++ )
		{
		const char *	l_qual = $ASCPTR(&s_quals[i].name);
		unsigned	l_quallen = (unsigned) $ASCLEN(&s_quals[i].name);

		/* The name itself, or a leading part of it */
		if ( (l_len <= l_quallen) && !strncmp(l_ups, l_qual, l_len) )
			return	STS$K_SUCCESS;

		/* ... and the negated form of a negatable one */
		if ( (s_quals[i].flag & CLI$M_NEGATABLE) && (l_len > 2) && (l_len <= (l_quallen + 2))
			&& !strncmp(l_ups, "NO", 2) && !strncmp(l_ups + 2, l_qual, l_len - 2) )
			return	STS$K_SUCCESS;
		}

	/*
	**  No qualifier of that name: a file specification, then.  /etc and
	**  /usr are directories a reader asks for as readily as he asks for
	**  /EXACT, and there is no telling the two apart by their spelling
	**  alone - so the table decides, and what the table does not know is
	**  a file.
	**
	**  The price is that a misspelt qualifier becomes a parameter - the
	**  file specification if it is the first word, a search string if it
	**  is a later one - instead of being refused.  A reader who would
	**  rather have the sharper diagnostic spells his qualifiers with a
	**  dash, which is never anything else.
	*/
	return	STS$K_WARN;
}




/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The action routine of the file selection: search one file.  Called
**	by SRCH$FORFILES once per selected file.
**
**  FORMAL PARAMETERS:
**
**	a_fspec		The specification of the file, ASCIZ
**	a_arg		The search context
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the file, writes to the output context.
**
**  RETURN VALUE:
**	Whatever SRCH$FILE returned.
**--
*/
static	int	s_search$onefile	(
	const	char *		a_fspec,
		void *		a_arg
			)
{
	return	srch$file((SEARCH$CTX *) a_arg, a_fspec);
}


/*
**  /CONTINUE: the file selected last is held back from the action routine
**  of the run until the next one is selected - when the walk is over, the
**  one still held is the one to be followed.
*/
typedef	struct	srch_hold_t
{
	int		(*rtn) (const char *a_fspec, void *a_arg);
						/* The action routine of the run		*/
	void *		arg;			/* Its argument					*/
	char *		fspec;			/* The file held back, heap; NULL - none	*/
} SEARCH$HOLD;


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The action routine of the file selection under /CONTINUE: hand the
**	file held back so far to the action routine of the run, and hold
**	this one back instead.  The order of the files is the order of the
**	walk all the same.
**
**  FORMAL PARAMETERS:
**
**	a_fspec		The specification of the file, ASCIZ
**	a_arg		The hold, SEARCH$HOLD
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	Whatever the action routine returned for the file held back before;
**	STS$K_SUCCESS where there was none.
**--
*/
static	int	s_search$holdfile	(
	const	char *		a_fspec,
		void *		a_arg
			)
{
SEARCH$HOLD *	l_hold = (SEARCH$HOLD *) a_arg;
char *		l_new;
int		l_status = STS$K_SUCCESS;

	/* No room to hold it: it is searched at once, and the one held before stays held */
	if ( !(l_new = strdup(a_fspec)) )
		{
		$SRCHMSG(SEARCH$_NOMEM, (unsigned) (strlen(a_fspec) + 1), errno, strerror(errno));

		return	l_hold->rtn(a_fspec, l_hold->arg);
		}

	if ( l_hold->fspec )
		{
		l_status	= l_hold->rtn(l_hold->fspec, l_hold->arg);
		free(l_hold->fspec);
		}

	l_hold->fspec	= l_new;

	return	l_status;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The handler of SIGINT while a file is followed by /CONTINUE: the
**	follow is over, the run ends as usual.
**
**  FORMAL PARAMETERS:
**
**	a_sig		The signal, SIGINT
**
**  IMPLICIT INPUTS/OUTPUTS:
**	G_SRCH$INTERRUPT, set.
**
**  RETURN VALUE:
**	None.
**--
*/
static	void	s_search$sigint	(
		int		a_sig
			)
{
	(void) a_sig;

	g_srch$interrupt	= 1;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Split a comma separated list into its elements, in place.  An
**	element enclosed in double quotes is taken as it stands, commas
**	included, and loses the quotes on the way - which is the only way
**	to ask for a search string that carries a comma.
**
**  FORMAL PARAMETERS:
**
**	a_str		The list, ASCIZ; modified in place
**	a_max		Capacity of the vector
**	a_elem		Receives the addresses of the elements
**	a_cnt		Receives the number of the elements
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Overwrites the separators of <a_str> with the terminators.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the whole list has been split;
**	STS$K_WARN	- the vector is full and the tail has been dropped;
**			  <a_cnt> holds what was taken in.
**--
*/
static	int	s_search$split	(
		char *		a_str,
		unsigned	a_max,
		char **		a_elem,
		unsigned *	a_cnt
			)
{
char *		l_ptr = a_str;
unsigned	l_cnt = 0;

	while ( *l_ptr )
		{
		/*
		**  The vector is full.  The count is written even so: what has
		**  been split is usable, and a caller told nothing at all would
		**  take an over-long list for an empty one.
		*/
		if ( l_cnt >= a_max )
			{
			*a_cnt	= l_cnt;

			return	STS$K_WARN;
			}

		if ( *l_ptr == '"' )
			{
			a_elem [l_cnt++]	= ++l_ptr;

			while ( *l_ptr && (*l_ptr != '"') )
				l_ptr++;

			if ( *l_ptr )
				*l_ptr++ = '\0';	/* Drop the closing quote	*/

			/* Whatever follows the quoted element up to the comma is ignored */
			while ( *l_ptr && (*l_ptr != ',') )
				l_ptr++;
			}
		else	{
			a_elem [l_cnt++]	= l_ptr;

			while ( *l_ptr && (*l_ptr != ',') )
				l_ptr++;
			}

		if ( *l_ptr == ',' )
			*l_ptr++ = '\0';
		}

	*a_cnt	= l_cnt;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Map the value of a keyword qualifier - /MATCH, /HIGHLIGHT - onto
**	the constant the keyword stands for.  The value has been validated
**	by the CLI service against the very same table, so an abbreviation
**	is unambiguous by the time it gets here.
**
**  FORMAL PARAMETERS:
**
**	a_kwd		The keyword table, closed by an empty name
**	a_val		The value as it has been given, ASCIZ
**	a_res		Receives the constant
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- <a_res> holds the constant;
**	STS$K_ERROR	- the value matches no keyword.
**--
*/
static	int	s_search$cvtkwd	(
		CLI_KEYWORD *	a_kwd,
	const	char *		a_val,
		int *		a_res
			)
{
char		l_ups [SEARCH$K_SZ_STR] = {0};
unsigned	l_len;

	l_len	= (unsigned) strnlen(a_val, sizeof(l_ups) - 1);
	srch$upcase(l_len, a_val, l_ups);

	for ( int i = 0; $ASCLEN(&a_kwd[i].name); i++ )
		{
		if ( l_len > (unsigned) $ASCLEN(&a_kwd[i].name) )
			continue;

		if ( strncmp(l_ups, $ASCPTR(&a_kwd[i].name), l_len) )
			continue;

		/*
		**  The first keyword the value is a leading part of wins.  The
		**  CLI service has matched the very same value against the very
		**  same table before this routine is reached, so an ambiguous
		**  abbreviation has already been refused and a prefix that gets
		**  here belongs to exactly one keyword.
		*/
		*a_res	= (int) a_kwd[i].val;

		return	STS$K_SUCCESS;
		}

	return	STS$K_ERROR;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Collect the display options of the search context and the selection
**	criteria from the parse context.
**
**  FORMAL PARAMETERS:
**
**	a_clictx	The parse context
**	a_ctx		Receives the display options
**	a_sel		Receives the selection criteria
**	a_page		Receives the state of /PAGE
**	a_outspec	Receives the value of /OUTPUT, ASCIZ, empty when absent
**	a_outsz		Capacity of <a_outspec>
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the parse context, signals the conditions it finds.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the options have been collected;
**	STS$K_ERROR	- a value is illegal, the condition has been signalled.
**--
*/
static	int	s_search$options	(
		CLI_CTX *	a_clictx,
		SEARCH$CTX *	a_ctx,
		SEARCH$SEL *	a_sel,
		int *		a_page,
		char *		a_outspec,
		unsigned	a_outsz
			)
{
ASC		l_val = {0};
char		l_str [ASC$K_SZ + 1] = {0};
unsigned	l_state = 0, l_win [2] = {0};
int		l_wincnt = 0;

	/* The options that are plain switches ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_EXACT], &l_state) )
		a_ctx->exact	= !(l_state & CLI$M_NEGATED);

	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_PAGE], &l_state) )
		*a_page		= !(l_state & CLI$M_NEGATED);

	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_NUMBERS], &l_state) )
		a_ctx->numbers	= !(l_state & CLI$M_NEGATED);

	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_HEADING], &l_state) )
		a_ctx->heading	= !(l_state & CLI$M_NEGATED);

	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_LOG], &l_state) )
		a_ctx->log	= !(l_state & CLI$M_NEGATED);

	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_STATISTICS], &l_state) )
		a_ctx->statistics = !(l_state & CLI$M_NEGATED);

	/* ... /MODIFIED and /CREATED, which are the two faces of one option ... */
	if ( (1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_MODIFIED], NULL))
		&& (1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_CREATED], NULL)) )
		return	$SRCHMSG(SEARCH$_CONFQUAL, "MODIFIED", "CREATED");

	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_CREATED], NULL) )
		a_sel->timsrc	= SEARCH$K_TIME_CREATED;

	/*
	**  ... /HIGHLIGHT, which is negatable AND carries a keyword.  Absent,
	**  it leaves the state at -1: whether the marking is granted is then
	**  decided by the output itself, once it is known whether that is a
	**  terminal at all.
	*/
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_HIGHLIGHT], &l_state) )
		{
		a_ctx->hilite	= !(l_state & CLI$M_NEGATED);

		if ( a_ctx->hilite
			&& (1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_HIGHLIGHT], &l_val)) )
			{
			__util$asc2str(&l_val, l_str);

			if ( !(1 & s_search$cvtkwd(s_hlkwd, l_str, &a_ctx->hlstyle)) )
				return	$SRCHMSG(SEARCH$_IVQUAL, l_str, "HIGHLIGHT");
			}
		}

	/* ... /BINARY ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_BINARY], NULL) )
		{
		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_BINARY], &l_val) )
			{
			__util$asc2str(&l_val, l_str);

			if ( !(1 & s_search$cvtkwd(s_binkwd, l_str, &a_ctx->binary)) )
				return	$SRCHMSG(SEARCH$_IVQUAL, l_str, "BINARY");
			}
		}

	/* ... /THREADS, the number of the files searched at once ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_THREADS], NULL) )
		{
		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_THREADS], &l_val) )
			{
			__util$asc2str(&l_val, l_str);

			a_ctx->threads	= (unsigned) strtoul(l_str, NULL, 0);

			if ( !a_ctx->threads || (a_ctx->threads > SEARCH$K_MAXTHREADS) )
				return	$SRCHMSG(SEARCH$_IVQUAL, l_str, "THREADS");
			}
		}

	/* ... /CONTINUE, the seconds between two looks at the file followed ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_CONTINUE], NULL) )
		{
		a_ctx->contint	= SEARCH$K_DEFCONT;

		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_CONTINUE], &l_val) )
			{
			__util$asc2str(&l_val, l_str);

			a_ctx->contint	= (unsigned) strtoul(l_str, NULL, 0);

			if ( !a_ctx->contint || (a_ctx->contint > SEARCH$K_MAXCONT) )
				return	$SRCHMSG(SEARCH$_IVQUAL, l_str, "CONTINUE");
			}
		}

	/* ... /FORMAT, whose GREP form numbers every record ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_FORMAT], NULL) )
		{
		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_FORMAT], &l_val) )
			{
			__util$asc2str(&l_val, l_str);

			if ( !(1 & s_search$cvtkwd(s_fmtkwd, l_str, &a_ctx->format)) )
				return	$SRCHMSG(SEARCH$_IVQUAL, l_str, "FORMAT");
			}

		if ( a_ctx->format == SEARCH$K_FMT_GREP )
			a_ctx->numbers	= 1;
		}

	/* ... /MATCH ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_MATCH], NULL) )
		{
		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_MATCH], &l_val) )
			{
			__util$asc2str(&l_val, l_str);

			if ( !(1 & s_search$cvtkwd(s_matchkwd, l_str, &a_ctx->mode)) )
				return	$SRCHMSG(SEARCH$_IVQUAL, l_str, "MATCH");
			}
		}

	/*
	**  ... /WINDOW, a list of at most two numbers.  /WINDOW alone means
	**  SEARCH$K_DEFWIN records on either side, /WINDOW=n means n on
	**  either side, /WINDOW=(n,m) means n before and m after.
	*/
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_WINDOW], NULL) )
		{
		while ( (l_wincnt < 2) && (1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_WINDOW], &l_val)) )
			{
			__util$asc2str(&l_val, l_str);

			l_win [l_wincnt]	= (unsigned) strtoul(l_str, NULL, 0);

			if ( l_win [l_wincnt] > SEARCH$K_MAXWIN )
				return	$SRCHMSG(SEARCH$_IVQUAL, l_str, "WINDOW");

			l_wincnt++;
			}

		switch ( l_wincnt )
			{
			case	0:			/* /WINDOW		*/
				a_ctx->winbef = a_ctx->winaft = SEARCH$K_DEFWIN;
				break;

			case	1:			/* /WINDOW=n		*/
				a_ctx->winbef = a_ctx->winaft = l_win [0];
				break;

			default:			/* /WINDOW=(n,m)	*/
				a_ctx->winbef = l_win [0];
				a_ctx->winaft = l_win [1];
				break;
			}
		}

	/* ... /OUTPUT ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_OUTPUT], NULL) )
		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_OUTPUT], &l_val) )
			{
			__util$asc2str(&l_val, l_str);
			srch$strcpy(a_outsz, a_outspec, l_str);
			}

	/* ... /EXCLUDE, a list of patterns ... */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_EXCLUDE], NULL) )
		{
		while ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_EXCLUDE], &l_val) )
			{
			if ( a_sel->exclcnt >= SEARCH$K_MAXEXCL )
				{
				$SRCHMSG(SEARCH$_TOOMANY, "/EXCLUDE patterns", SEARCH$K_MAXEXCL);
				break;
				}

			__util$asc2str(&l_val, l_str);
			srch$strcpy(SEARCH$K_SZ_PATH, a_sel->excl [a_sel->exclcnt], l_str);

			a_sel->exclcnt++;
			}
		}

	/* ... and the two time filters */
	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_SINCE], NULL) )
		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_SINCE], &l_val) )
			{
			__util$asc2str(&l_val, l_str);

			if ( !(1 & srch$cvttim(l_str, &a_sel->since)) )
				return	$SRCHMSG(SEARCH$_IVTIME, l_str, "SINCE");

			a_sel->hassince	= 1;

			/*
			**  A delta - 0-1:00 - and a keyword - TODAY - say nothing
			**  about the date they land on, and a run that selected
			**  nothing is then hard to account for.  The resolved time
			**  is therefore reported, under the qualifier that exists
			**  to tell the reader what the utility is doing.
			**
			**  The time is a quadword and is passed to !%D BY VALUE,
			**  per STARLET X.01-09.
			*/
			if ( a_ctx->log )
				$SRCHMSG(SEARCH$_SELTIME, "SINCE", l_str, a_sel->since);
			}

	if ( 1 & __cli$present(a_clictx, &s_quals[SEARCH$K_QUAL_BEFORE], NULL) )
		if ( 1 & __cli$get_value(a_clictx, &s_quals[SEARCH$K_QUAL_BEFORE], &l_val) )
			{
			__util$asc2str(&l_val, l_str);

			if ( !(1 & srch$cvttim(l_str, &a_sel->before)) )
				return	$SRCHMSG(SEARCH$_IVTIME, l_str, "BEFORE");

			a_sel->hasbefore	= 1;

			if ( a_ctx->log )
				$SRCHMSG(SEARCH$_SELTIME, "BEFORE", l_str, a_sel->before);
			}

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Append an item to a list being composed for a report, putting the
**	separator in front of every item but the first, and marking the
**	list as cut short rather than letting it run past the buffer.
**
**  FORMAL PARAMETERS:
**
**	a_bufsz		Capacity of the buffer
**	a_buf		The list, ASCIZ; empty on the first call
**	a_sep		The separator, ASCIZ
**	a_item		The item to be appended, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the item has been appended;
**	STS$K_WARN	- it would not fit, and the list ends in an ellipsis.
**--
*/
static	int	s_search$additem	(
		unsigned	a_bufsz,
		char *		a_buf,
	const	char *		a_sep,
	const	char *		a_item
			)
{
unsigned	l_len = (unsigned) strlen(a_buf);
unsigned	l_seplen = l_len ? (unsigned) strlen(a_sep) : 0;
unsigned	l_itemlen = (unsigned) strlen(a_item);

	/* The item, the separator, the terminator - and room for the ellipsis */
	if ( (l_len + l_seplen + l_itemlen + 1 + 3) > a_bufsz )
		{
		srch$strcpy(a_bufsz - l_len, a_buf + l_len, "...");

		return	STS$K_WARN;
		}

	if ( l_seplen )
		{
		memcpy(a_buf + l_len, a_sep, l_seplen);
		l_len	+= l_seplen;
		}

	memcpy(a_buf + l_len, a_item, l_itemlen + 1);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Report what the utility has made of the command: the strings it is
**	going to look for and how they are combined, and the files it is
**	going to look in.  A command carries quoting, wildcards and
**	defaults, and a reader who gets no output at all has no way of
**	telling a search that found nothing from a command that asked for
**	something other than he meant:
**
**	    %SEARCH-I-SEARCHING, for "alpha", "beta" combined by AND, the
**		case is ignored
**	    %SEARCH-I-SELECTING, in src/.../x.c, /EXCLUDE=(build/%*)
**
**	Reported under /LOG, beside the other accounts of what is being
**	done.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context, the strings and /MATCH within
**	a_sel		The selection criteria
**	a_elemcnt	Number of the file specifications
**	a_elem		The file specifications themselves
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Signals the two conditions; reads the module-static S_MATCHKWD.
**
**  RETURN VALUE:
**	STS$K_SUCCESS always.
**--
*/
static	int	s_search$report	(
		SEARCH$CTX *	a_ctx,
		SEARCH$SEL *	a_sel,
		unsigned	a_elemcnt,
		char **		a_elem
			)
{
char		l_list [SEARCH$K_SZ_LIST] = {0}, l_item [SEARCH$K_SZ_STR + 4];
char		l_how [SEARCH$K_SZ_STR] = {0};

	/* The strings, quoted, so that a leading or a trailing blank is seen */
	for ( unsigned i = 0; i < a_ctx->strcnt; i++ )
		{
		l_item [0]	= '"';
		srch$strcpy((unsigned) sizeof(l_item) - 2, l_item + 1, a_ctx->strs[i].str);
		srch$strcpy((unsigned) sizeof(l_item) - (unsigned) strlen(l_item),
			l_item + strlen(l_item), "\"");

		if ( !(1 & s_search$additem((unsigned) sizeof(l_list), l_list, ", ", l_item)) )
			break;
		}

	/*
	**  How they are combined.  A single string is combined with nothing,
	**  so the mode is named only where it can make a difference - naming
	**  /MATCH=OR of one string would say more about the defaults of the
	**  utility than about the command.
	*/
	if ( a_ctx->strcnt > 1 )
		{
		srch$strcpy((unsigned) sizeof(l_how), l_how, " combined by ");
		srch$strcpy((unsigned) sizeof(l_how) - (unsigned) strlen(l_how),
			l_how + strlen(l_how), $ASCPTR(&s_matchkwd[a_ctx->mode].name));
		}

	$SRCHMSG(SEARCH$_SEARCHING, l_list, l_how,
		a_ctx->exact ? "significant" : "ignored");

	/* The file specifications, and what narrows them down */
	l_list [0]	= '\0';

	for ( unsigned i = 0; i < a_elemcnt; i++ )
		if ( !(1 & s_search$additem((unsigned) sizeof(l_list), l_list, ", ", a_elem [i])) )
			break;

	l_how [0]	= '\0';

	for ( unsigned i = 0; i < a_sel->exclcnt; i++ )
		{
		srch$strcpy((unsigned) sizeof(l_how) - (unsigned) strlen(l_how),
			l_how + strlen(l_how), i ? "," : ", /EXCLUDE=(");

		srch$strcpy((unsigned) sizeof(l_how) - (unsigned) strlen(l_how),
			l_how + strlen(l_how), a_sel->excl [i]);
		}

	if ( a_sel->exclcnt )
		srch$strcpy((unsigned) sizeof(l_how) - (unsigned) strlen(l_how),
			l_how + strlen(l_how), ")");

	$SRCHMSG(SEARCH$_SELECTING, l_list, l_how);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Display the counters of /STATISTICS.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the output context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS always.
**--
*/
static	int	s_search$stats	(
		SEARCH$CTX *	a_ctx
			)
{
	$SRCHPUT(a_ctx->out, "\nFiles searched:      !UQ\n", a_ctx->stat.files);
	$SRCHPUT(a_ctx->out, "Records searched:    !UQ\n", a_ctx->stat.recsread);
	$SRCHPUT(a_ctx->out, "Characters searched: !UQ\n", a_ctx->stat.chars);
	$SRCHPUT(a_ctx->out, "Records matched:     !UQ\n", a_ctx->stat.recsmatched);
	$SRCHPUT(a_ctx->out, "Lines printed:       !UQ\n", a_ctx->stat.lines);
	$SRCHPUT(a_ctx->out, "Elapsed time:        !UQ msec\n", a_ctx->stat.msec);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The entry point of the SEARCH utility: parse the command, expand
**	every file specification and display the records that satisfy the
**	match.
**
**  FORMAL PARAMETERS:
**
**	a_argc, a_argv	The command line
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the files, writes to the standard output or to the file of
**	/OUTPUT.
**
**  RETURN VALUE:
**	SEARCH$K_EXIT_MATCH	- at least one record matched, no error;
**	SEARCH$K_EXIT_NOMATCH	- nothing matched, no error;
**	SEARCH$K_EXIT_ERROR	- an error was signalled, whatever matched,
**				  or the command could not be carried out.
**--
*/
int	main	(
		int		a_argc,
		char **		a_argv
		)
{
CLI_CTX *	l_clictx = NULL;
SEARCH$CTX	l_ctx = {0};
SEARCH$SEL	l_sel = {0};
SEARCH$OUT	l_out;
SEARCH$HOLD	l_hold = {0};
char *		l_argv [1 + 64], *l_words [SEARCH$K_MAXSPEC + SEARCH$K_MAXSTR];
char *		l_elem [SEARCH$K_MAXSTR];
char		l_outspec [SEARCH$K_SZ_PATH] = {0};
uint64_t	l_beg = 0, l_end = 0;
unsigned	l_elemcnt = 0;
int		l_argc = 1, l_wordcnt = 0, l_sep = a_argc, l_usage = 0, l_page = -1;
int		l_status, l_fd = STDOUT_FILENO;

	/*
	**  The catalogue of the facility goes to the message subsystem before
	**  anything can be signalled - the diagnostics of the command line
	**  included.
	*/
	srch$inimsg();

	srch$msec(&l_beg);

	/*
	**  The case of a letter outside ASCII is told by the locale: the
	**  user's own if it is a UTF-8 one, C.UTF-8 otherwise - the text a
	**  search runs over is UTF-8 here whatever LANG says, and the C
	**  locale knows the case of no letter but the Latin ones.  Only the
	**  classification of the characters is taken, nothing else of it.
	*/
	setlocale(LC_CTYPE, "");

	if ( strcmp(nl_langinfo(CODESET), "UTF-8") )
		setlocale(LC_CTYPE, "C.UTF-8");

	/*
	**  The defaults of the display: the name of a file is announced, the
	**  strings are combined by OR, the case is ignored.
	*/
	l_ctx.heading	= 1;
	l_ctx.mode	= SEARCH$K_MATCH_OR;
	l_ctx.hilite	= -1;			/* Undecided: the output settles it	*/
	l_ctx.hlstyle	= SEARCH$K_HL_BOLD;

	/*
	**  A lone "--" ends the qualifiers: what follows it is a file
	**  specification or a search string whatever it looks like.
	*/
	for ( int i = 1; i < a_argc; i++ )
		{
		if ( !strcmp(a_argv[i], "--") )
			{
			l_sep	= i;
			break;
			}
		}

	/*
	**  The command is split before it is parsed.  The CLI service is
	**  shown the verb and the qualifiers, and nothing else: the words
	**  are collected here, because there may be more of them than the
	**  eight positional parameters the service knows about, and because
	**  a request for the syntax has to be caught before the service
	**  prints a summary of its own tables instead of ours.
	*/
	l_argv [0]	= s_clivrb;

	for ( int i = 1; i < l_sep; i++ )
		{
		/*
		**  The short alias of the request for the syntax, which the CLI
		**  service honours as well and which is spelled with ONE dash
		*/
		if ( !strcasecmp(a_argv [i], "-h") )
			{
			l_usage	= 1;
			continue;
			}

		/* /RECURSIVE is gone: said so, not taken for a parameter */
		if ( 1 & s_search$isrecur(a_argv [i]) )
			{
			$SRCHMSG(SEARCH$_NORECURSE, a_argv [i]);

			return	SEARCH$K_EXIT_ERROR;
			}

		if ( 1 & s_search$isqual(a_argv [i]) )
			{
			const char *l_p = a_argv[i] + ((a_argv[i][0] == '-') ? 2 : 1);

			/*
			**  /HELP and --HELP, abbreviated no further than HEL: HE is
			**  a leading part of HEADING as well, and H of three
			**  qualifiers.
			*/
			if ( (strlen(l_p) >= 3) && !strncasecmp(l_p, "HELP", strlen(l_p)) && (strlen(l_p) <= 4) )
				{
				l_usage	= 1;
				continue;
				}

			/*
			**  /PAGE is answered from here as well: the display of
			**  /HELP is made before the command is parsed - the CLI
			**  service is never shown a command that carries no
			**  qualifier at all - so the one walk of the words settles
			**  it for both.
			*/
			if ( !strcasecmp(l_p, "PAGE") )
				l_page	= 1;
			else if ( !strcasecmp(l_p, "NOPAGE") )
				l_page	= 0;

			if ( l_argc < (int) $ARRSZ(l_argv) )
				l_argv [l_argc++] = a_argv [i];

			continue;
			}

		if ( l_wordcnt < (int) $ARRSZ(l_words) )
			l_words [l_wordcnt++] = a_argv [i];
		}

	/* Past the "--" marker everything is a word, without being looked at */
	for ( int i = l_sep + 1; (i < a_argc) && (l_wordcnt < (int) $ARRSZ(l_words)); i++ )
		l_words [l_wordcnt++] = a_argv [i];

	if ( l_usage || (a_argc < 2) )
		{
		/*
		**  The description proper is the help library of the product,
		**  and the words of the command name the topic within it.  A
		**  library that cannot be found - the product installed without
		**  it, or a build of a tree that has no HELPTRAN at hand -
		**  leaves the summary built into the image to answer, which is
		**  why /HELP never depends on anything being installed.
		*/
		/*
		**  The topic is paged the way the browser pages it - at a
		**  terminal, unless /NOPAGE says otherwise.
		*/
		l_status	= l_usage
				? srch$help(l_wordcnt, l_words,
					(l_page < 0) ? (isatty(STDOUT_FILENO) ? 1 : 0) : l_page)
				: STS$K_WARN;

		if ( l_status == STS$K_WARN )
			{
			srch$out_init(&l_out, STDOUT_FILENO);
			srch$put_raw(&l_out, (unsigned) sizeof(s_usage) - 1, s_usage);
			srch$flush(&l_out);
			}

		return	(l_usage && (1 & l_status || (l_status == STS$K_WARN))) ? SEARCH$K_EXIT_MATCH : SEARCH$K_EXIT_ERROR;
		}

	if ( !(1 & __cli$parse(s_verbs, CLI$M_OPSIGNAL, l_argc, l_argv, (void **) &l_clictx)) )
		return	SEARCH$K_EXIT_ERROR;

	/*
	**  Every lookup from here on is an optional one - a qualifier that is
	**  absent is not a diagnostic - and the drain of a list qualifier ends
	**  precisely by asking for a value that is not there.  The signalling
	**  option of the service would announce each of those, so it is
	**  dropped now that the command itself has been parsed.
	*/
	l_clictx->opts	&= ~CLI$M_OPSIGNAL;

	l_page	= 0;

	if ( !(1 & s_search$options(l_clictx, &l_ctx, &l_sel, &l_page, l_outspec, sizeof(l_outspec))) )
		{
		__cli$cleanup(l_clictx);

		return	SEARCH$K_EXIT_ERROR;
		}

	/* The first word is the list of the file specifications ... */
	if ( l_wordcnt < 2 )
		{
		$SRCHMSG(SEARCH$_NOSTRING);
		__cli$cleanup(l_clictx);

		return	SEARCH$K_EXIT_ERROR;
		}

	/* ... every word after it is a list of the search strings */
	for ( int i = 1; i < l_wordcnt; i++ )
		{
		/* A list longer than the vector is cut short by the splitter itself */
		if ( !(1 & s_search$split(l_words [i], (unsigned) $ARRSZ(l_elem), l_elem, &l_elemcnt)) )
			$SRCHMSG(SEARCH$_TOOMANY, "search strings", (unsigned) $ARRSZ(l_elem));

		for ( unsigned j = 0; j < l_elemcnt; j++ )
			if ( !(1 & srch$addstr(&l_ctx, l_elem [j])) )
				{
				$SRCHMSG(SEARCH$_TOOMANY, "search strings", SEARCH$K_MAXSTR);
				break;
				}
		}

	if ( !l_ctx.strcnt )
		{
		$SRCHMSG(SEARCH$_NOSTRING);
		__cli$cleanup(l_clictx);

		return	SEARCH$K_EXIT_ERROR;
		}

	/* /OUTPUT is created only after the command has proved to be a sound one */
	if ( l_outspec [0] )
		{
		if ( 0 > (l_fd = open(l_outspec, O_WRONLY | O_CREAT | O_TRUNC, 0644)) )
			{
			$SRCHMSG(SEARCH$_OPENOUT, l_outspec, errno, strerror(errno));
			__cli$cleanup(l_clictx);

			return	SEARCH$K_EXIT_ERROR;
			}
		}

	srch$out_init(&l_out, l_fd);
	srch$out_page(&l_out, l_page);

	/*
	**  Whether what has been found is marked is decided here when the
	**  qualifier said nothing about it: escape sequences belong on a
	**  screen, not in a file or in a pipe.  /HIGHLIGHT given outright
	**  overrides that - the reader may well be piping into a pager that
	**  understands them.
	*/
	if ( l_ctx.hilite < 0 )
		l_ctx.hilite	= l_out.tty;

	/* The heading displays the very stamp the selection has worked on */
	l_ctx.timsrc	= l_sel.timsrc;

	l_ctx.out	= &l_out;

	/* The first word, split into the file specifications, drives the search */
	if ( !(1 & s_search$split(l_words [0], (unsigned) $ARRSZ(l_elem), l_elem, &l_elemcnt)) )
		$SRCHMSG(SEARCH$_TOOMANY, "file specifications", (unsigned) $ARRSZ(l_elem));

	/*
	**  What the command has been made to mean, before the first file is
	**  opened: the reader sees whether his quoting survived the shell and
	**  whether the defaults are the ones he took for granted.
	*/
	if ( l_ctx.log )
		s_search$report(&l_ctx, &l_sel, l_elemcnt, l_elem);

	/*
	**  '?' is the '%' of OpenVMS, and the matcher of the library knows
	**  the latter alone; and the case of a file name is not significant,
	**  so the pattern is folded to upper case once, here, rather than at
	**  every name it is matched against.  Done here rather than where the
	**  qualifier is collected: the account above is to show the pattern
	**  as it was given, not as the utility keeps it.
	*/
	for ( unsigned i = 0; i < l_sel.exclcnt; i++ )
		{
		unsigned	l_len = 0;

		for ( char *l_p = l_sel.excl [i]; *l_p; l_p++, l_len++ )
			if ( *l_p == '?' )
				*l_p	= '%';

		srch$upcase(l_len, l_sel.excl [i], l_sel.excl [i]);
		}

	/*
	**  The pool of /THREADS.  Without the qualifier the run takes as many
	**  threads as there are processors, at most SEARCH$K_DEFTHREADS.  The
	**  walk is made by the main thread alone - exactly as without a pool -
	**  when one thread is asked for, under /LOG, which reports every file
	**  in the order of the walk as it is searched, and under /PAGE, whose
	**  pager has to see the records as they come.
	*/
	if ( !l_ctx.threads )
		{
		long	l_cpus = sysconf(_SC_NPROCESSORS_ONLN);

		l_ctx.threads	= (l_cpus < 1) ? 1 : (l_cpus > SEARCH$K_DEFTHREADS) ? SEARCH$K_DEFTHREADS : (unsigned) l_cpus;
		}

	if ( l_ctx.log || l_out.page )
		l_ctx.threads	= 1;

	if ( (l_ctx.threads > 1) && !(1 & srch$pool_init(&l_ctx, l_ctx.threads)) )
		l_ctx.threads	= 1;

	l_hold.rtn	= (l_ctx.threads > 1) ? srch$pool_file : s_search$onefile;
	l_hold.arg	= &l_ctx;

	for ( unsigned i = 0; i < l_elemcnt; i++ )
		{
		l_status	= l_ctx.contint
				? srch$forfiles(l_elem [i], &l_sel, s_search$holdfile, &l_hold)
				: srch$forfiles(l_elem [i], &l_sel, l_hold.rtn, &l_ctx);

		/* A specification that selected no file: a warning, as on OpenVMS, and completion code 2, as with GREP */
		if ( l_status == STS$K_WARN )
			{
			$SRCHMSG(SEARCH$_NOFILES, l_elem [i]);
			srch$fail();
			}

		/* The reader has abandoned the display at the pager */
		if ( l_status == STS$K_FATAL )
			break;
		}

	/* The files still queued are displayed, in their turn, before anything is reported */
	if ( l_ctx.threads > 1 )
		srch$pool_drain();

	/*
	**  /CONTINUE: the file held back is the last one of the walk, and is
	**  searched now that every other one has been displayed - by this
	**  thread, followed past its end until ^C.  SA_RESTART is not given,
	**  so that the signal cuts the sleep between two looks short.
	*/
	if ( l_hold.fspec )
		{
		struct sigaction	l_sa = { .sa_handler = s_search$sigint };

		sigemptyset(&l_sa.sa_mask);

		if ( sigaction(SIGINT, &l_sa, NULL) )
			$LOG(STS$K_WARN, "sigaction(SIGINT), errno=%d (%s)", errno, strerror(errno));

		l_ctx.follow	= 1;
		srch$file(&l_ctx, l_hold.fspec);
		l_ctx.follow	= 0;

		free(l_hold.fspec);
		}

	srch$msec(&l_end);
	l_ctx.stat.msec	= (l_end > l_beg) ? (l_end - l_beg) : 0;

	if ( l_ctx.statistics && !l_out.quit )
		s_search$stats(&l_ctx);

	srch$out_final(&l_out);

	if ( l_fd != STDOUT_FILENO )
		close(l_fd);

	if ( !l_ctx.stat.recsmatched )
		$SRCHMSG(SEARCH$_NOMATCHES);

	__cli$cleanup(l_clictx);

	/* As GREP: an error decides, whatever else has matched */
	if ( srch$errors() )
		return	SEARCH$K_EXIT_ERROR;

	return	l_ctx.stat.recsmatched ? SEARCH$K_EXIT_MATCH : SEARCH$K_EXIT_NOMATCH;
}
