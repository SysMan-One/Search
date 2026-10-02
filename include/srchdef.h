#ifndef	__SRCHDEF$H__
#define	__SRCHDEF$H__	1

/*
**++
**
**  FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
**
**  MODULE:	srchdef.h
**
**  ABSTRACT:	Public definitions shared by all modules of the SEARCH
**		facility: the condition values of the facility, the
**		contexts of the output engine, of the file selection and
**		of the matcher, and the prototypes of the SRCH$ routines.
**
**  DESCRIPTION: The utility reads the records (lines) of every selected
**		file and displays those that satisfy a boolean combination
**		of the given search strings:
**
**		    SEARCH file-spec[,...] search-string[,...] [/qualifiers]
**
**		A search string is a plain substring - no wildcards, no
**		regular expressions - exactly as on OpenVMS.  A file
**		specification, in contrast, IS expanded by the utility
**		itself ('*', '%', '?'), so that a pattern survives being
**		quoted against the shell and so that /RECURSIVE can play
**		the part of the [...] of an OpenVMS directory spec.
**
**		The processing chain is
**
**		    SRCH$FORFILES  - selection: expansion of a file-spec,
**		    (srchfil.c)      the tree walk, /EXCLUDE, /SINCE,
**				     /BEFORE
**			|
**		    SRCH$FILE      - one file: record extraction, the
**		    (srchmat.c)      /WINDOW ring, the per-file statistics
**			|
**		    S_SRCH$MATCH   - one record: the search strings and
**		    (srchmat.c)      /MATCH, /EXACT; static there, as are
**				     the record reader and the matcher of a
**				     file name - only what another module
**				     calls is declared here
**			|
**		    SRCH$PUT       - output: buffered, FAO-formatted
**		    (srchutl.c)
**
**		Neither <stdio.h> nor any of its derivatives is used
**		anywhere in the facility: the output leaves the process
**		through WRITE(2) and every formatted record is built by
**		the FAO engine of the STARLET module.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:   1-AUG-2026
**
**  MODIFICATION HISTORY:
**
**	X01-16		 2-OCT-2026	RRL
**		SEARCH$K_FMT_* and the FORMAT member of the context, /FORMAT;
**		SEARCH$K_EXIT_*, the completion codes of GREP (0 a match, 1
**		none, 2 an error); $SRCHMSG goes through SRCH$NOTE, which
**		counts the errors signalled so that the completion code can
**		tell an error from a run that found nothing.
**
**	X01-15		 2-OCT-2026	RRL
**		A record is no longer cut at 8192 bytes: SEARCH$K_SZ_REC gives
**		way to SEARCH$K_MAXREC, the ceiling of the read-ahead area
**		grown for a long record, and the RAB says whether its area has
**		been grown (GROWN).
**
**	X01-14		 2-OCT-2026	RRL
**		SEARCH$K_BIN_* and the BINARY member of the context: what is
**		done with a binary file, /BINARY.
**
**	X01-13		14-AUG-2026	RRL
**		SEARCH$_SEARCHING and SEARCH$_SELECTING - what the utility
**		has made of the command, reported before the first file is
**		opened.
**
**	X01-12		14-AUG-2026	RRL
**		$SRCHMSG signals through UTIL$PUTMSG_FAO of the STARLET
**		library; SRCH$PUTMSG, which did the same by hand, is gone
**		and SRCH$INIMSG registers the catalogue in its place.
**
**	X01-11		 7-AUG-2026	RRL
**		The routines nobody calls from outside their own module -
**		SRCH$WILDMAT, SRCH$MATCH, SRCH$GETREC - are static there
**		now and are gone from here; SEARCH$K_SZ_ESC, which nothing
**		used, is gone as well.
**
**	X01-10		 3-AUG-2026	RRL
**		$SRCHMSG and $SRCHPUT: the FAO parameters are cast and
**		counted by the preprocessor, on the machinery UTILS
**		V.01-21 provides for $PUTMSG_FAO.
**
**	X01-09		 1-AUG-2026	RRL
**		SEARCH$STR carries the position the case-blind scan
**		anchors on.
**
**	X01-08		 1-AUG-2026	RRL
**		SEARCH$_LONGRECU - a record truncated in a run that does
**		not number its records.
**
**	X01-07		 1-AUG-2026	RRL
**		SEARCH$RAB reads through a mapping of the file when one
**		can be made, so BUF is a pointer now and the read-ahead
**		area it may point at is a member of its own.
**
**	X01-06		 1-AUG-2026	RRL
**		SRCH$GETREC delivers a record as a pointer into the buffer
**		of the stream instead of copying it out; SEARCH$RAB
**		carries the state of a record being skipped.
**
**	X01-05		 1-AUG-2026	RRL
**		SEARCH$_NOTOPIC and the prototype of SRCH$HELP: /HELP is
**		answered out of the help library of the product when the
**		HELP product is at hand.
**
**	X01-04		 1-AUG-2026	RRL
**		SEARCH$CTX carries the time stamp of a file the heading
**		is to display, SEARCH$K_TIME_*.
**
**	X01-03		 1-AUG-2026	RRL
**		SEARCH$_SELTIME - the absolute time a value of /SINCE or
**		/BEFORE has been resolved to.
**
**	X01-02		 1-AUG-2026	RRL
**		The output engine pages the display and highlights the
**		strings that have been found: SEARCH$OUT carries the
**		geometry of the terminal and the state of the page,
**		SEARCH$CTX the style of the highlighting.
**
**	X01-01		 1-AUG-2026	RRL
**		Initial version.
**
**--
*/

#ifndef	__MODULE__
#define	__MODULE__	"SRCHDEF"
#endif

#ifndef	__IDENT__
#define	__IDENT__	"X01-16"
#endif

#ifndef	__REV__
#define	__REV__		"1.16.0"
#endif

#ifndef	__FAC__
#define	__FAC__		"SEARCH"
#endif

#include	<stdint.h>
#include	<stddef.h>
#include	<sys/types.h>

#include	"utility_routines.h"
#include	"starlet.h"
#include	"cli_routines.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
**  Facility number of the SEARCH subsystem, see $DEFFAC/$DEFMSG/$DEFSEV
**  in UTILITY_ROUTINES.H for the layout of a condition value.
*/
#define	SEARCH$K_FACNO		0x0111

#define	$SRCHSTS(msg, sev)	( $DEFFAC(SEARCH$K_FACNO) | $DEFMSG(msg) | $DEFSEV(sev) )

/*
**  Message numbers of the facility.  Ordered - hence an enumeration; the
**  text and the identification of every message live in the catalogue
**  S_MSGTAB (SRCHUTL.C).
*/
enum	{
	SEARCH$K_MSG_NORMAL	= 1,		/* Normal successful completion		*/
	SEARCH$K_MSG_NOMATCHES,			/* No strings matched			*/
	SEARCH$K_MSG_NOFILES,			/* No file matches a specification	*/
	SEARCH$K_MSG_NOSTRING,			/* No search string has been given	*/
	SEARCH$K_MSG_OPENIN,			/* Error opening an input file		*/
	SEARCH$K_MSG_OPENOUT,			/* Error creating the output file	*/
	SEARCH$K_MSG_READERR,			/* Error reading a file			*/
	SEARCH$K_MSG_WRITERR,			/* Error writing a file			*/
	SEARCH$K_MSG_OPENDIR,			/* Error reading a directory		*/
	SEARCH$K_MSG_NOMEM,			/* Memory allocation failure		*/
	SEARCH$K_MSG_IVQUAL,			/* Illegal value of a qualifier		*/
	SEARCH$K_MSG_IVTIME,			/* Illformed time value			*/
	SEARCH$K_MSG_CONFQUAL,			/* Conflicting qualifiers		*/
	SEARCH$K_MSG_SEARCHED,			/* Per-file report of /LOG		*/
	SEARCH$K_MSG_SELTIME,			/* What /SINCE, /BEFORE resolved to	*/
	SEARCH$K_MSG_LONGREC,			/* Record truncated			*/
	SEARCH$K_MSG_TOODEEP,			/* Directory nesting too deep		*/
	SEARCH$K_MSG_TOOMANY,			/* List truncated to the maximum	*/
	SEARCH$K_MSG_NOTOPIC,			/* No such topic in the help library	*/
	SEARCH$K_MSG_LONGRECU,			/* Record truncated, its number unknown	*/
	SEARCH$K_MSG_SEARCHING,			/* What is looked for, and how		*/
	SEARCH$K_MSG_SELECTING,			/* Where it is looked for		*/

	SEARCH$K_MSG_MAX			/* End-Of-List				*/
	};

#define	SEARCH$_NORMAL		$SRCHSTS(SEARCH$K_MSG_NORMAL,	STS$K_SUCCESS)
#define	SEARCH$_NOMATCHES	$SRCHSTS(SEARCH$K_MSG_NOMATCHES,STS$K_WARN)
#define	SEARCH$_NOFILES		$SRCHSTS(SEARCH$K_MSG_NOFILES,	STS$K_WARN)
#define	SEARCH$_NOSTRING	$SRCHSTS(SEARCH$K_MSG_NOSTRING,	STS$K_ERROR)
#define	SEARCH$_OPENIN		$SRCHSTS(SEARCH$K_MSG_OPENIN,	STS$K_ERROR)
#define	SEARCH$_OPENOUT		$SRCHSTS(SEARCH$K_MSG_OPENOUT,	STS$K_FATAL)
#define	SEARCH$_READERR		$SRCHSTS(SEARCH$K_MSG_READERR,	STS$K_ERROR)
#define	SEARCH$_WRITERR		$SRCHSTS(SEARCH$K_MSG_WRITERR,	STS$K_ERROR)
#define	SEARCH$_OPENDIR		$SRCHSTS(SEARCH$K_MSG_OPENDIR,	STS$K_ERROR)
#define	SEARCH$_NOMEM		$SRCHSTS(SEARCH$K_MSG_NOMEM,	STS$K_FATAL)
#define	SEARCH$_IVQUAL		$SRCHSTS(SEARCH$K_MSG_IVQUAL,	STS$K_ERROR)
#define	SEARCH$_IVTIME		$SRCHSTS(SEARCH$K_MSG_IVTIME,	STS$K_ERROR)
#define	SEARCH$_CONFQUAL	$SRCHSTS(SEARCH$K_MSG_CONFQUAL,	STS$K_ERROR)
#define	SEARCH$_SEARCHED	$SRCHSTS(SEARCH$K_MSG_SEARCHED,	STS$K_INFO)
#define	SEARCH$_SELTIME		$SRCHSTS(SEARCH$K_MSG_SELTIME,	STS$K_INFO)
#define	SEARCH$_LONGREC		$SRCHSTS(SEARCH$K_MSG_LONGREC,	STS$K_WARN)
#define	SEARCH$_TOODEEP		$SRCHSTS(SEARCH$K_MSG_TOODEEP,	STS$K_WARN)
#define	SEARCH$_TOOMANY		$SRCHSTS(SEARCH$K_MSG_TOOMANY,	STS$K_WARN)
#define	SEARCH$_NOTOPIC		$SRCHSTS(SEARCH$K_MSG_NOTOPIC,	STS$K_ERROR)
#define	SEARCH$_LONGRECU	$SRCHSTS(SEARCH$K_MSG_LONGRECU,	STS$K_WARN)
#define	SEARCH$_SEARCHING	$SRCHSTS(SEARCH$K_MSG_SEARCHING,STS$K_INFO)
#define	SEARCH$_SELECTING	$SRCHSTS(SEARCH$K_MSG_SELECTING,STS$K_INFO)

/*
**  Sizes and limits of the subsystem.
*/
#define	SEARCH$K_SZ_PATH	1024		/* Maximum length of a file specification	*/
#define	SEARCH$K_MAXREC		(64 * 1024 * 1024)	/* Longest record of a file that is read,	*/
						/* not mapped: the read-ahead area grows to	*/
						/* hold a record up to this length, a longer	*/
						/* one is cut; a mapped file has no limit	*/
#define	SEARCH$K_MAPMIN	262144		/* Below this a file is read, not mapped	*/
#define	SEARCH$K_SZ_BLK		32768		/* Region the prefilter looks at in one go	*/
#define	SEARCH$K_SZ_BUF		32768		/* Size of the read-ahead buffer of a file	*/
						/* Both are kept small on purpose: a region is	*/
						/* looked at twice - once by the prefilter,	*/
						/* once record by record - and one that fits	*/
						/* into the second level cache is measurably	*/
						/* faster than a large one			*/
#define	SEARCH$K_SZ_OUT		8192		/* Size of the output buffer			*/
#define	SEARCH$K_SZ_MSG		512		/* Maximum length of a formatted message	*/
#define	SEARCH$K_SZ_LIST	256		/* A list of items as it is reported	*/
#define	SEARCH$K_SZ_STR		256		/* Maximum length of a search string		*/

#define	SEARCH$K_MAXSTR		32		/* Maximum number of the search strings		*/
#define	SEARCH$K_MAXSPEC	32		/* Maximum number of the file specifications	*/
#define	SEARCH$K_MAXEXCL	16		/* Maximum number of the /EXCLUDE patterns	*/
#define	SEARCH$K_MAXPRM		16		/* Maximum number of the FAO parameters		*/
#define	SEARCH$K_MAXWIN		4096		/* Sanity ceiling for a /WINDOW half		*/
#define	SEARCH$K_MAXDEPTH	32		/* Maximum nesting of the /RECURSIVE walk	*/

#define	SEARCH$K_DEFWIN		1		/* /WINDOW given without a value		*/
#define	SEARCH$K_DEFROWS	24		/* Height of the page, no terminal to ask	*/
#define	SEARCH$K_DEFCOLS	80		/* Width of the page, no terminal to ask	*/
#define	SEARCH$K_MINROWS	5		/* A page shorter than this is not paged	*/
#define	SEARCH$K_SZ_ANS		16		/* The answer to the prompt of the pager	*/
#define	SEARCH$K_NUMWIDTH	6		/* Width of the record number of /NUMBERS	*/
#define	SEARCH$K_HDRWIDTH	66		/* Width of the asterisk rule of /HEADING	*/
#define	SEARCH$K_HDRNAME	40		/* Column the time of /HEADING starts in	*/

/*
**  How the search strings of a record are combined, /MATCH.  Ordered -
**  hence an enumeration; the keyword table lives in SEARCH.C.
*/
enum	{
	SEARCH$K_MATCH_OR	= 0,		/* Any string is present			*/
	SEARCH$K_MATCH_AND,			/* Every string is present			*/
	SEARCH$K_MATCH_NAND,			/* At least one string is absent		*/
	SEARCH$K_MATCH_NOR,			/* No string is present				*/

	SEARCH$K_MATCH_MAX			/* End-Of-List					*/
	};

/*
**  The style the strings that have been found are highlighted in,
**  /HIGHLIGHT.  Ordered - hence an enumeration; the escape sequences and
**  the keyword table live in SRCHUTL.C and SEARCH.C respectively.
*/
enum	{
	SEARCH$K_HL_BOLD	= 0,		/* The default					*/
	SEARCH$K_HL_REVERSE,
	SEARCH$K_HL_UNDERLINE,
	SEARCH$K_HL_RED,
	SEARCH$K_HL_GREEN,
	SEARCH$K_HL_YELLOW,

	SEARCH$K_HL_MAX				/* End-Of-List					*/
	};

/*
**  What is done with a binary file - one that carries a NUL among its
**  first SEARCH$K_SZ_BLK bytes, /BINARY.  Ordered - hence an enumeration;
**  the keyword table lives in SEARCH.C.
*/
enum	{
	SEARCH$K_BIN_NOTE	= 0,		/* Searched, a match is noted in one line	*/
	SEARCH$K_BIN_SKIP,			/* Not searched at all				*/
	SEARCH$K_BIN_TEXT,			/* Searched and displayed as any other file	*/

	SEARCH$K_BIN_MAX			/* End-Of-List					*/
	};

/*
**  The form of the display, /FORMAT.  Ordered - hence an enumeration; the
**  keyword table lives in SEARCH.C.
*/
enum	{
	SEARCH$K_FMT_VMS	= 0,		/* Headings and numbered records, the default	*/
	SEARCH$K_FMT_GREP,			/* spec:number:record, one line each, for tools	*/

	SEARCH$K_FMT_MAX			/* End-Of-List					*/
	};

/*
**  The completion codes of the image, those of GREP: a script tells a run
**  that found something from one that found nothing from one that failed.
*/
enum	{
	SEARCH$K_EXIT_MATCH	= 0,		/* At least one record matched			*/
	SEARCH$K_EXIT_NOMATCH,			/* Nothing matched, nothing went wrong		*/
	SEARCH$K_EXIT_ERROR			/* An error was signalled, matched or not	*/
	};

/*
**  Which time stamp of a file /SINCE and /BEFORE are applied to.
*/
enum	{
	SEARCH$K_TIME_MODIFIED	= 0,		/* ST_MTIME, the default			*/
	SEARCH$K_TIME_CREATED,			/* ST_CTIME - the closest analogue Linux has	*/

	SEARCH$K_TIME_MAX			/* End-Of-List					*/
	};

/*
**  One search string, kept in both spellings: the case-blind scan - which
**  is the default - runs over the upper-cased copy, /EXACT runs over the
**  original one.
*/
typedef	struct	srch_str_t
{
	unsigned	len;			/* Length of the string, characters		*/
	unsigned	anchor;			/* Position the case-blind scan anchors on	*/
	char		str [SEARCH$K_SZ_STR];	/* The string as it has been given		*/
	char		ups [SEARCH$K_SZ_STR];	/* The same, upper-cased			*/
} SEARCH$STR;

/*
**  The output context.  Every character the utility emits is accumulated
**  here and leaves the process in blocks of SEARCH$K_SZ_OUT.
*/
typedef	struct	srch_out_t
{
	int		fd;			/* Descriptor the buffer is flushed to		*/
	unsigned	len;			/* Bytes currently buffered			*/

	int		tty,			/* The descriptor is a terminal			*/
			ttyin,			/* Descriptor the pager reads the answer from	*/
			page,			/* Paging is in force				*/
			quit;			/* The user has answered Q at the prompt	*/

	unsigned	rows,			/* Height of the page, records			*/
			cols,			/* Width of the page, characters		*/
			lineno,			/* Records displayed on the current page	*/
			col;			/* Current column, for the wrapped records	*/

	char		buf [SEARCH$K_SZ_OUT];	/* The buffer itself				*/
} SEARCH$OUT;

/*
**  The counters of /STATISTICS, accumulated over the whole run.
*/
typedef	struct	srch_stat_t
{
	uint64_t	files;			/* Files actually searched			*/
	uint64_t	recsread;		/* Records read					*/
	uint64_t	recsmatched;		/* Records that satisfied the match		*/
	uint64_t	chars;			/* Characters read				*/
	uint64_t	lines;			/* Records displayed, /WINDOW included		*/
	uint64_t	msec;			/* Elapsed time, milliseconds			*/
} SEARCH$STAT;

/*
**  The file selection criteria - everything the utility needs to decide
**  whether a file takes part in the search at all.
*/
typedef	struct	srch_sel_t
{
	int		recurse;		/* /RECURSIVE: descend into the subdirectories	*/
	int		timsrc;			/* SEARCH$K_TIME_*, /MODIFIED or /CREATED	*/

	int		hassince,		/* /SINCE has been given			*/
			hasbefore;		/* /BEFORE has been given			*/

	fao_time_t	since,			/* Take a file at or after this time		*/
			before;			/* Take a file strictly before this time	*/

	unsigned	exclcnt;		/* Number of the /EXCLUDE patterns		*/
	char		excl [SEARCH$K_MAXEXCL] [SEARCH$K_SZ_PATH];
} SEARCH$SEL;

/*
**  The context of the search: the strings, the display options and the
**  running counters.  One such structure describes the whole run.
*/
typedef	struct	srch_ctx_t
{
	SEARCH$OUT *	out;			/* Where the matched records go			*/

	unsigned	strcnt;			/* Number of the search strings			*/
	SEARCH$STR	strs [SEARCH$K_MAXSTR];

	int		mode;			/* SEARCH$K_MATCH_*, /MATCH			*/
	int		hilite;			/* /HIGHLIGHT: mark what has been found		*/
	int		timsrc;			/* SEARCH$K_TIME_*, the stamp of the heading	*/
	int		hlstyle;		/* SEARCH$K_HL_*, the style of the marking	*/
	int		exact;			/* /EXACT: the case is significant		*/
	int		binary;			/* SEARCH$K_BIN_*, /BINARY			*/
	int		format;			/* SEARCH$K_FMT_*, /FORMAT			*/
	int		numbers;		/* /NUMBERS: prefix the record number		*/
	int		heading;		/* /HEADING: announce every file		*/
	int		log;			/* /LOG: report every file searched		*/
	int		statistics;		/* /STATISTICS: report the totals at the end	*/

	unsigned	winbef,			/* /WINDOW: records displayed before a match	*/
			winaft;			/* /WINDOW: records displayed after a match	*/

	SEARCH$STAT	stat;			/* The counters of /STATISTICS			*/
} SEARCH$CTX;

/*
**  A record stream over an open file: the read-ahead buffer and the
**  bookkeeping of SRCH$GETREC.  A record never spans two refills - a
**  partial one is moved to the front of the buffer before the read.
*/
typedef	struct	srch_rab_t
{
	int		fd;			/* Descriptor of the file being read		*/
	int		eof;			/* READ(2) has reported the end of the file	*/
	int		skip;			/* The tail of an over-long record is dropped	*/

	int		mapped;			/* BUF is a mapping of the file, not OWN	*/
	int		grown;			/* BUF is a heap area grown past OWN		*/
	size_t		mapsz;			/* Size of the mapping, bytes			*/

	unsigned	len,			/* Bytes present in the buffer			*/
			pos,			/* Offset of the next unread byte		*/
			bufsz;			/* Capacity of the buffer BUF points at		*/

	uint64_t	recnum,			/* Number of the record just delivered		*/
			fills;			/* Refills of the buffer, for the prefilter	*/

	char *		buf;			/* The whole file, or the read-ahead area	*/
	char		own [SEARCH$K_SZ_BUF];	/* The read-ahead area, when no mapping is made	*/
} SEARCH$RAB;


/*
**  A diagnostic of the utility is signalled by $SRCHMSG, which is
**  $PUTMSG_FAO of the STARLET library under the name of this facility:
**  the catalogue registered by SRCH$INIMSG supplies the text, and the
**  envelope - %SEARCH-W-NOFILES, ... - is put on by the library out of
**  the facility name of the descriptor and the severity carried by the
**  condition value.
**
**  $SRCHPUT is the same machinery over the output engine of the utility,
**  which knows about the pager and the highlighting and is therefore
**  ours.
**
**  Neither the cast of a parameter nor the count of them is written by
**  hand: the parameters travel through varargs and are read as 64-bit
**  FAO_PRM_T, so an un-cast 32-bit value would be read wrong, and a count
**  too large sends the formatter reading arguments that were never
**  passed.  FAO$_EACH() casts every parameter and FAO$_NARG() counts
**  them; the leading comma lives inside the macro, so a call carrying no
**  parameter expands cleanly too.
**
**	$SRCHMSG(SEARCH$_OPENIN, a_fspec, errno, strerror(errno));
**	$SRCHPUT(a_out, "!AZ  !%D\n", a_fspec, l_tim);
*/
#ifndef	$PUTMSG_FAO
#error	"UTILITY_ROUTINES.H of UTILS V.01-24 or later is required: $PUTMSG_FAO is missing"
#endif

#define	$SRCHMSG(a_sts, ...)	srch$note((int) $PUTMSG_FAO((a_sts), ##__VA_ARGS__))
#define	$SRCHPUT(out, fmt, ...)	srch$put((out), (fmt), FAO$_NARG(__VA_ARGS__) FAO$_EACH(__VA_ARGS__))

/*
**  SRCHUTL.C - the message catalogue, the output engine and the string
**  and time services shared by the whole facility.
*/
int	srch$inimsg	(void);
int	srch$note	(int a_sts);
int	srch$errors	(void);
int	srch$out_init	(SEARCH$OUT *a_out, int a_fd);
int	srch$out_page	(SEARCH$OUT *a_out, int a_page);
int	srch$out_final	(SEARCH$OUT *a_out);
int	srch$put_raw	(SEARCH$OUT *a_out, unsigned a_len, const char *a_buf);
int	srch$put_ctl	(SEARCH$OUT *a_out, unsigned a_len, const char *a_buf);
int	srch$put	(SEARCH$OUT *a_out, const char *a_fmt, int a_prmcnt, ...);
int	srch$flush	(SEARCH$OUT *a_out);
const char *	srch$hlseq	(int a_style);
int	srch$upcase	(unsigned a_len, const char *a_src, char *a_dst);
int	srch$strcpy	(unsigned a_bufsz, char *a_buf, const char *a_src);
int	srch$cvttim	(const char *a_str, fao_time_t *a_time);
int	srch$msec	(uint64_t *a_msec);

/*
**  SRCHFIL.C - the file selection: expansion of a file specification, the
**  /RECURSIVE tree walk and the /EXCLUDE, /SINCE, /BEFORE filters.
*/
int	srch$forfiles	(const char *a_spec, SEARCH$SEL *a_sel,
			int (*a_rtn) (const char *a_fspec, void *a_arg), void *a_arg);

/*
**  SRCHHLP.C - /HELP out of the help library of the product.  Compiled to
**  a stand-in that reports STS$K_WARN when the HELP product was not found
**  at configuration time.
*/
int	srch$help	(int a_wordcnt, char **a_words, int a_page);

/*
**  SRCHMAT.C - the matcher and the per-file scan.
*/
int	srch$addstr	(SEARCH$CTX *a_ctx, const char *a_str);
int	srch$file	(SEARCH$CTX *a_ctx, const char *a_fspec);

#ifdef	__cplusplus
	}
#endif

#endif	/* __SRCHDEF$H__ */
