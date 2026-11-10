#define	__MODULE__	"SRCHMAT"
#define	__IDENT__	"X01-17"
#define	__REV__		"1.17.0"

/*
**++
**
**  FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
**
**  MODULE:	srchmat.c
**
**  ABSTRACT:	The matcher of the SEARCH utility: the table of the search
**		strings, the boolean combination of /MATCH, the extraction
**		of the records of a file and the display of a match
**		together with the window of /WINDOW.
**
**  DESCRIPTION: A search string is a plain substring, exactly as on
**		OpenVMS: no wildcard, no regular expression, nothing to
**		escape.  Unless /EXACT has been given the comparison
**		ignores the case of the ASCII letters; a byte above 0x7f
**		is always compared as it stands, so a Cyrillic string is
**		matched exactly as it has been typed.
**
**		A record is a line.  The last line of a file that ends
**		without a line terminator is a record none the less.  A
**		record is delivered whole however long it is: a mapped file
**		is in memory as it stands, and the read-ahead area of a file
**		that is read grows for a long record up to SEARCH$K_MAXREC.
**		Only a record longer than that is cut, with SEARCH$_LONGREC
**		- the cut affects the display AND the search, which is why
**		it is announced.
**
**		/WINDOW asks for a number of records around a match to be
**		displayed as well.  The records that precede the match are
**		held in a ring of the required depth; the ones that follow
**		it are displayed as they arrive.  Two matches close enough
**		for their windows to overlap produce one continuous run of
**		records - no record is ever displayed twice - and a gap
**		between two runs is marked by an empty line.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:   1-AUG-2026
**
**  MODIFICATION HISTORY:
**
**	X01-17		21-OCT-2026	RRL
**		/CONTINUE: a file marked FOLLOW is not finished at its end -
**		what has been displayed is flushed, the file is looked at
**		again every /CONTINUE seconds and the records appended to it
**		are searched as they come, until SIGINT (G_SRCH$INTERRUPT) or
**		Q at the pager.  A record without its terminator waits for it.
**		A file cut short is read again from its beginning, and one
**		whose name another file has taken is left for that one, both
**		announced (S_SRCH$FOLLOW).  Such a file is read, never mapped.
**
**	X01-16		 2-OCT-2026	RRL
**		A split file is cut into pieces of about SEARCH$K_SPLITPIECE,
**		as many as that makes, run by at most /THREADS threads in their
**		order; every piece is displayed as soon as it and all before it
**		are done and is let go of at once, and the threads run no more
**		than /THREADS + 2 pieces ahead of the display - what waits in
**		the heap no longer grows with the size of the file (500 MB of
**		log, 3 M lines out: 511 MB of heap before, 40 MB now).
**
**	X01-15		 2-OCT-2026	RRL
**		The count of the record terminators of a skipped region was
**		wrong: (w - 01..01) & ~w & 80..80 flags a zero byte, but also
**		the byte after one that borrows - a terminator followed by 0x0B
**		was counted twice, and every number after it was off.  It now
**		uses the exact test and adds up byte lanes instead of calling
**		__POPCOUNTDI2 once a word (3.5 times faster).  /EXACT anchors on
**		a character by MEMCHR and compares by MEMCMP instead of calling
**		MEMMEM per region.  The case is ignored in any script: a string
**		that is not ASCII alone is matched character by character,
**		folded through TOWLOWER, anchored on a character of both its
**		spellings (S_SRCH$FIND, S_SRCH$U8DEC, S_SRCH$U8ENC); the
**		highlighting marks what S_SRCH$FIND finds.
**
**	X01-14		 2-OCT-2026	RRL
**		Under /THREADS a mapped file of SEARCH$K_SPLITMIN or more is
**		searched in pieces, cut at record boundaries, by several
**		threads at once (S_SRCH$SPLIT, S_SRCH$PIECE); where numbers are
**		to be shown, the records of every piece are counted first, in
**		parallel too.  The pieces are displayed in their order, so the
**		display is that of one thread.  Only /MATCH=OR and AND without
**		/WINDOW, and not a binary file noted by /BINARY=NOTE.  The
**		threads of all the files split at once never exceed /THREADS.
**		The heading has a routine of its own, S_SRCH$HEADING.  A file
**		of 4 GB or more is read rather than mapped: the length of a
**		mapping is held in 32 bits.
**
**	X01-13		 2-OCT-2026	RRL
**		/FORMAT=GREP: a record is displayed as spec:number:record, a
**		record of the window as spec-number-record, two runs are parted
**		by "--" and a binary file says "Binary file spec matches" - the
**		lines GREP -n writes, for a tool to read.  No heading is put
**		out in that form.
**
**	X01-12		 2-OCT-2026	RRL
**		A record is delivered whole however long it is: the mapped
**		file has no limit at all, the read-ahead area of a file that is
**		read doubles for a long record up to SEARCH$K_MAXREC (64 MB),
**		and the marking of /HIGHLIGHT grows with it (S_SRCH$MARKBUF).
**		Only past that ceiling is a record cut, and SEARCH$_LONGREC now
**		names the length it was cut to.  A string past the old limit
**		of 8192 bytes - an executable is one long record - was missed.
**
**	X01-11		 2-OCT-2026	RRL
**		A binary file - a NUL among its first SEARCH$K_SZ_BLK bytes,
**		the test GREP makes - is no longer displayed record by record:
**		/BINARY=NOTE (the default) reports in one line that it
**		matches, SKIP leaves it alone, TEXT searches it as before.
**
**	X01-10		 3-AUG-2026	RRL
**		The FAO parameters of a message are cast and counted by
**		$SRCHMSG and $SRCHPUT rather than by hand.
**
**	X01-09		 1-AUG-2026	RRL
**		Only a file worth mapping is mapped: on a small one the
**		three system calls of a mapping cost more than reading it
**		does, and a walk of a large tree is made of small files.
**
**	X01-08		 1-AUG-2026	RRL
**		The case-blind scan anchors on the first character of a
**		string that is not a letter, rather than on the first
**		character whatever it is: a letter has to be looked for
**		in two spellings and is common in text besides, while an
**		underscore or a digit is one MEMCHR and a rare hit.
**
**	X01-07		 1-AUG-2026	RRL
**		The records of a region that carries none of the strings
**		are not even counted unless something is going to display
**		a number: /NUMBERS, /LOG and /STATISTICS ask for one, a
**		plain run does not, and counting a file of a million
**		records costs as much as searching it.
**
**	X01-06		 1-AUG-2026	RRL
**		Speed: a region that carries none of the strings is passed
**		over WHOLE - it is not split into records at all, only its
**		terminators are counted - so a file that matches nothing
**		costs one scan of it and nothing else.
**
**	X01-05		 1-AUG-2026	RRL
**		Speed: a regular file is mapped rather than read, so the
**		bytes are never copied at all and the prefilter looks at
**		the whole file in one pass; a block that carries none of
**		the strings is passed over without the matcher being
**		entered once per record.
**
**	X01-04		 1-AUG-2026	RRL
**		Speed: a record is delivered as a pointer into the buffer
**		of the stream rather than copied out of it, and the scan
**		of a record is anchored by MEMCHR - by MEMMEM under
**		/EXACT - instead of comparing at every position.  Twelve
**		times faster on a file of sixty megabytes.
**
**	X01-03		 1-AUG-2026	RRL
**		The heading of a file carries its time stamp beside the
**		specification.
**
**	X01-02		 1-AUG-2026	RRL
**		The runs of a record that belong to a search string are
**		highlighted, /HIGHLIGHT; a display abandoned at the pager
**		of /PAGE is reported to the caller as STS$K_FATAL, which
**		stops the walk of the tree rather than merely the file.
**
**	X01-01		 1-AUG-2026	RRL
**		Initial version.
**
**--
*/

#define	_GNU_SOURCE	1			/* MEMMEM, an extension of the GNU CRTL	*/

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<sys/mman.h>
#include	<time.h>
#include	<pthread.h>
#include	<wctype.h>

#include	"srchdef.h"

/*
**  One slot of the ring that holds the records preceding a match.  The
**  buffer grows to the longest record that has passed through the slot
**  and is released when the file is closed.
*/
typedef	struct	srch_ring_t
{
	uint64_t	recnum;			/* Number of the record in the file	*/
	unsigned	len,			/* Length of the record			*/
			max;			/* Capacity of the buffer		*/
	char *		buf;			/* The record itself			*/
} SEARCH$RING;

/*
**  The interrupt of a file followed by /CONTINUE: set by the handler of
**  SIGINT that SEARCH.C installs for it, looked at by S_SRCH$FOLLOW.
*/
volatile sig_atomic_t	g_srch$interrupt = 0;


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Decode one character of UTF-8.  A byte that does not begin a well
**	formed sequence stands for itself, as a value no character folds
**	to, so that it matches nothing but the very same byte.
**
**  FORMAL PARAMETERS:
**
**	a_ptr		The first byte
**	a_end		The byte past the last one that may be read
**	a_chr		Receives the character
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	The length of the character, bytes: 1 .. 4.
**--
*/
static	unsigned	s_srch$u8dec	(
	const	char *		a_ptr,
	const	char *		a_end,
		wint_t *	a_chr
			)
{
const unsigned char *	l_p = (const unsigned char *) a_ptr;
unsigned	l_len, l_avail = (unsigned) (a_end - a_ptr);
wint_t		l_chr;

	if ( l_p [0] < 0x80 )
		return	*a_chr = l_p [0], 1;

	l_len	= ((l_p [0] & 0xE0) == 0xC0) ? 2 : ((l_p [0] & 0xF0) == 0xE0) ? 3 : ((l_p [0] & 0xF8) == 0xF0) ? 4 : 0;

	if ( !l_len || (l_len > l_avail) )
		return	*a_chr = 0x110000 + l_p [0], 1;	/* Past Unicode: never a folded character	*/

	l_chr	= l_p [0] & (0x7F >> l_len);

	for ( unsigned i = 1; i < l_len; i++ )
		{
		if ( (l_p [i] & 0xC0) != 0x80 )
			return	*a_chr = 0x110000 + l_p [0], 1;

		l_chr	= (l_chr << 6) | (l_p [i] & 0x3F);
		}

	return	*a_chr = l_chr, l_len;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Encode one character as UTF-8.
**
**  FORMAL PARAMETERS:
**
**	a_chr		The character
**	a_buf		Receives the bytes, 4 at most
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	The number of the bytes.
**--
*/
static	unsigned	s_srch$u8enc	(
		wint_t		a_chr,
		char *		a_buf
			)
{
unsigned char *	l_b = (unsigned char *) a_buf;

	if ( a_chr < 0x80 )
		return	l_b [0] = (unsigned char) a_chr, 1;

	if ( a_chr < 0x800 )
		return	l_b [0] = (unsigned char) (0xC0 | (a_chr >> 6)), l_b [1] = (unsigned char) (0x80 | (a_chr & 0x3F)), 2;

	if ( a_chr < 0x10000 )
		return	l_b [0] = (unsigned char) (0xE0 | (a_chr >> 12)), l_b [1] = (unsigned char) (0x80 | ((a_chr >> 6) & 0x3F)),
			l_b [2] = (unsigned char) (0x80 | (a_chr & 0x3F)), 3;

	return	l_b [0] = (unsigned char) (0xF0 | (a_chr >> 18)), l_b [1] = (unsigned char) (0x80 | ((a_chr >> 12) & 0x3F)),
		l_b [2] = (unsigned char) (0x80 | ((a_chr >> 6) & 0x3F)), l_b [3] = (unsigned char) (0x80 | (a_chr & 0x3F)), 4;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Add a search string to the table of the context, keeping both the
**	original and the upper-cased spelling of it.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_str		The string to be added, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Extends the table of the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the string has been added;
**	STS$K_WARN	- the table is full, or the string is empty.
**--
*/
int	srch$addstr	(
		SEARCH$CTX *	a_ctx,
	const	char *		a_str
			)
{
SEARCH$STR *	l_str;
unsigned	l_len;

	if ( !a_str || !*a_str )
		return	STS$K_WARN;

	if ( a_ctx->strcnt >= SEARCH$K_MAXSTR )
		return	STS$K_WARN;

	l_str	= &a_ctx->strs [a_ctx->strcnt];
	l_len	= (unsigned) strnlen(a_str, sizeof(l_str->str) - 1);

	memcpy(l_str->str, a_str, l_len);
	l_str->str [l_len]	= '\0';
	l_str->len		= l_len;

	srch$upcase(l_len, l_str->str, l_str->ups);
	l_str->ups [l_len]	= '\0';

	/*
	**  The position the case-blind scan will anchor on: the first
	**  character that is not an ASCII letter, if the string has one.
	**  Such a character stands for itself in either case - one MEMCHR
	**  instead of two - and an underscore, a digit or a byte of a
	**  Cyrillic character is far rarer in text than a letter is, so the
	**  comparison is entered far less often.  A string of letters
	**  alone is anchored on its first character, as before.
	*/
	l_str->anchor	= 0;

	for ( unsigned i = 0; i < l_len; i++ )
		{
		unsigned char l_chr = (unsigned char) l_str->ups [i];

		if ( (l_chr >= 'A') && (l_chr <= 'Z') )
			continue;

		l_str->anchor	= i;
		break;
		}

	/*
	**  A string that is not ASCII alone is matched case-blind character
	**  by character, through the case mapping of the locale - SRCH$INIT
	**  has made sure it is a UTF-8 one where that can be had.  Its
	**  characters are kept folded to lower case; the scan anchors on the
	**  first one whose case is no matter - a digit, a sign, a space - or,
	**  wanting one, on the first character, looked for in both of its
	**  spellings.  /EXACT compares bytes and needs none of this.
	*/
	l_str->wide	= 0;

	for ( unsigned i = 0; i < l_len; i++ )
		if ( (unsigned char) l_str->str [i] >= 0x80 )
			l_str->wide	= 1;

	if ( l_str->wide )
		{
		const char *	l_ptr = l_str->str, *l_end = l_str->str + l_len;
		wint_t		l_chr;
		int		l_inv = -1;

		for ( l_str->nchr = 0; l_ptr < l_end; l_str->nchr++ )
			{
			l_ptr	+= s_srch$u8dec(l_ptr, l_end, &l_chr);

			l_str->fold [l_str->nchr]	= (l_chr < 0x110000) ? towlower(l_chr) : l_chr;

			if ( (l_inv < 0) && (l_chr < 0x110000) && (towupper(l_chr) == towlower(l_chr)) )
				l_inv	= (int) l_str->nchr;
			}

		l_str->wanchor	= (l_inv < 0) ? 0 : (unsigned) l_inv;
		l_chr		= l_str->fold [l_str->wanchor];

		if ( l_chr < 0x110000 )
			{
			l_str->alen [0]	= (unsigned char) s_srch$u8enc(l_chr, l_str->alt [0]);
			l_str->alen [1]	= (unsigned char) s_srch$u8enc(towupper(l_chr), l_str->alt [1]);
			}
		else	{
			l_str->alt [0][0] = l_str->alt [1][0] = (char) (l_chr - 0x110000);
			l_str->alen [0]	= l_str->alen [1] = 1;
			}
		}

	a_ctx->strcnt++;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tell whether a record carries a given search string.  The scan is
**	the naive one - a comparison started at every position of the
**	record - which for the string lengths a command line carries is
**	faster than any of the clever algorithms, none of which pays for
**	the table it has to build first.
**
**  FORMAL PARAMETERS:
**
**	a_exact		Non-zero - the case is significant
**	a_reclen	Length of the record
**	a_rec		The record itself
**	a_str		The search string
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the string is present in the record;
**	STS$K_WARN	- it is not.
**--
*/
static	int	s_srch$find	(
		int		a_exact,
		unsigned	a_reclen,
	const	char *		a_rec,
		unsigned	a_from,
	const	SEARCH$STR *	a_str,
		unsigned *	a_at,
		unsigned *	a_mlen
			)
{
const char *	l_cand, *l_up = NULL, *l_lo = NULL, *l_ptr, *l_first, *l_last, *l_end = a_rec + a_reclen;
unsigned char	l_chr, l_cup, l_clo;
int		l_upend = 0, l_loend = 0;

	if ( !a_str->len || (a_from > a_reclen) )
		return	STS$K_WARN;

	/*
	**  The case is significant: the anchor character of the string -
	**  see SRCH$ADDSTR - is looked for by MEMCHR, the vectorised routine
	**  of the CRTL, and the string is compared where it stands.  MEMMEM,
	**  which did this before, sets up its two-way search afresh on every
	**  call, and it is called once per region of the prefilter: a run
	**  over a large file that matched nothing spent half its time in
	**  that setup.
	*/
	if ( a_exact )
		{
		if ( a_str->len > (a_reclen - a_from) )
			return	STS$K_WARN;

		l_cup	= (unsigned char) a_str->str [a_str->anchor];
		l_last	= a_rec + (a_reclen - a_str->len) + a_str->anchor;

		for ( l_ptr = a_rec + a_from + a_str->anchor; l_ptr <= l_last; l_ptr = l_cand + a_str->anchor + 1 )
			{
			if ( !(l_cand = memchr(l_ptr, l_cup, (size_t) (l_last - l_ptr) + 1)) )
				break;

			l_cand	-= a_str->anchor;

			if ( !memcmp(l_cand, a_str->str, a_str->len) )
				return	*a_at = (unsigned) (l_cand - a_rec), *a_mlen = a_str->len, STS$K_SUCCESS;
			}

		return	STS$K_WARN;
		}

	/*
	**  Case-blind, a string that is not ASCII alone: character by
	**  character through the case mapping of the locale.  The anchor
	**  character is looked for in both of its spellings; from where it
	**  stands the record is stepped back over as many characters as
	**  precede it in the string - a character may take more or fewer
	**  bytes in the record than in the string, if its other case is
	**  encoded otherwise - and compared forward, folded, from there.
	*/
	if ( a_str->wide )
		{
		const char *	l_cnd [2] = {NULL, NULL};
		int		l_done [2] = {0, a_str->alen [0] == a_str->alen [1] && !memcmp(a_str->alt [0], a_str->alt [1], a_str->alen [0])};
		wint_t		l_wc;

		if ( a_str->nchr > (a_reclen - a_from) )
			return	STS$K_WARN;

		for ( l_ptr = a_rec + a_from; l_ptr < l_end; )
			{
			/* The earlier of the two candidates, each looked for again once it falls behind */
			for ( int k = 0; k < 2; k++ )
				if ( !l_done [k] && (!l_cnd [k] || (l_cnd [k] < l_ptr)) )
					if ( !(l_cnd [k] = memmem(l_ptr, (size_t) (l_end - l_ptr), a_str->alt [k], a_str->alen [k])) )
						l_done [k]	= 1;

			l_cand	= l_done [0] ? NULL : l_cnd [0];

			if ( !l_done [1] && l_cnd [1] && (!l_cand || (l_cnd [1] < l_cand)) )
				l_cand	= l_cnd [1];

			if ( !l_cand )
				break;

			l_ptr	= l_cand + 1;

			/* Back to where the string would begin: WANCHOR characters, a character's head each */
			l_first	= l_cand;

			for ( unsigned k = 0; k < a_str->wanchor; k++ )
				{
				if ( l_first <= (a_rec + a_from) )
					{
					l_first	= NULL;
					break;
					}

				for ( l_first--; (l_first > (a_rec + a_from)) && ((*(const unsigned char *) l_first & 0xC0) == 0x80); l_first-- )
					;
				}

			if ( !l_first )
				continue;

			/* Forward, folded, character by character */
			l_last	= l_first;

			for ( unsigned k = 0; ; k++ )
				{
				if ( k == a_str->nchr )
					return	*a_at = (unsigned) (l_first - a_rec), *a_mlen = (unsigned) (l_last - l_first), STS$K_SUCCESS;

				if ( l_last >= l_end )
					break;

				l_last	+= s_srch$u8dec(l_last, l_end, &l_wc);

				if ( ((l_wc < 0x110000) ? towlower(l_wc) : l_wc) != a_str->fold [k] )
					break;
				}
			}

		return	STS$K_WARN;
		}

	if ( a_str->len > (a_reclen - a_from) )
		return	STS$K_WARN;

	/*
	**  Case-blind, ASCII.  Rather than comparing at every position of the
	**  record, the scan is anchored on one character of the string,
	**  looked for by MEMCHR - the vectorised routine of the CRTL - and
	**  the comparison is entered only where a candidate really lies.
	**
	**  The anchor is the first character that is not a letter, chosen
	**  when the string was taken in: such a character needs looking for
	**  in one spelling only, and is rarer in text than a letter is.  A
	**  string of letters alone anchors on its first character, which
	**  then has to be looked for in both spellings; the two candidates
	**  are kept between the turns of the loop and are looked for again
	**  only once they fall behind the cursor, which keeps the scan
	**  linear either way.
	*/
	l_cup	= (unsigned char) a_str->ups [a_str->anchor];
	l_clo	= ((l_cup >= 'A') && (l_cup <= 'Z')) ? (unsigned char) (l_cup + ('a' - 'A')) : l_cup;

	l_first	= a_rec + a_from + a_str->anchor;	/* Earliest the anchor may stand	*/
	l_last	= a_rec + (a_reclen - a_str->len) + a_str->anchor;

	for ( l_ptr = l_first; l_ptr <= l_last; )
		{
		if ( !l_upend && (!l_up || (l_up < l_ptr)) )
			if ( !(l_up = memchr(l_ptr, l_cup, (size_t) (l_last - l_ptr) + 1)) )
				l_upend	= 1;

		if ( (l_clo != l_cup) && !l_loend && (!l_lo || (l_lo < l_ptr)) )
			if ( !(l_lo = memchr(l_ptr, l_clo, (size_t) (l_last - l_ptr) + 1)) )
				l_loend	= 1;

		/* The earlier of the two candidates, if there is one at all */
		l_cand	= l_upend ? NULL : l_up;

		if ( (l_clo != l_cup) && !l_loend && l_lo && (!l_cand || (l_lo < l_cand)) )
			l_cand	= l_lo;

		if ( !l_cand )
			break;

		/* The anchor matches by construction, the rest of the string is compared */
		l_cand	-= a_str->anchor;

		for ( unsigned j = 0; ; j++ )
			{
			if ( j == a_str->len )
				return	*a_at = (unsigned) (l_cand - a_rec), *a_mlen = a_str->len, STS$K_SUCCESS;

			if ( j == a_str->anchor )
				continue;

			l_chr	= (unsigned char) l_cand [j];

			if ( (l_chr >= 'a') && (l_chr <= 'z') )
				l_chr	-= ('a' - 'A');

			if ( l_chr != (unsigned char) a_str->ups [j] )
				break;
			}

		l_ptr	= l_cand + a_str->anchor + 1;
		}

	return	STS$K_WARN;
}


/*
**  Whether a string occurs in a record at all: S_SRCH$FIND from the start.
*/
static	int	s_srch$instr	(
		int		a_exact,
		unsigned	a_reclen,
	const	char *		a_rec,
	const	SEARCH$STR *	a_str
			)
{
unsigned	l_at, l_mlen;

	return	s_srch$find(a_exact, a_reclen, a_rec, 0, a_str, &l_at, &l_mlen);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Make sure the buffer of a stream holds something to look at,
**	refilling it when it has run dry.  Used by the caller before it
**	asks the prefilter about the region ahead - the records themselves
**	are delivered by SRCH$GETREC, which refills for itself.
**
**  FORMAL PARAMETERS:
**
**	a_rab		The record stream
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the file of the stream.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the buffer holds at least one byte;
**	STS$K_WARN	- the file is exhausted;
**	STS$K_ERROR	- READ(2) failed, ERRNO is set.
**--
*/
static	int	s_srch$fill	(
		SEARCH$RAB *	a_rab
			)
{
ssize_t		l_red;
unsigned	l_rest;

	for ( ;; )
		{
		if ( (l_rest = a_rab->len - a_rab->pos) )
			return	STS$K_SUCCESS;

		if ( a_rab->eof )
			return	STS$K_WARN;

		a_rab->pos	= a_rab->len = 0;

		l_red	= read(a_rab->fd, a_rab->buf, a_rab->bufsz);

		if ( (l_red < 0) && (errno == EINTR) )
			continue;

		if ( l_red < 0 )
			return	STS$K_ERROR;

		if ( !l_red )
			{
			a_rab->eof	= 1;
			continue;
			}

		a_rab->len	= (unsigned) l_red;
		a_rab->fills++;
		}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Count the line terminators of a block.  The records of a region
**	that carries none of the search strings are never looked at one by
**	one, so this is the whole of what such a region still costs: the
**	numbering of the records has to go on whether they are displayed or
**	not.
**
**  FORMAL PARAMETERS:
**
**	a_len		Length of the block
**	a_blk		The block itself
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	The number of the terminators.
**--
*/
static	uint64_t	s_srch$nlcount	(
		unsigned	a_len,
	const	char *		a_blk
			)
{
uint64_t	l_word, l_acc, l_cnt = 0;
unsigned	l_pos = 0, l_run;

	/*
	**  A word at a time.  Exclusive-or with a word of terminators turns
	**  every terminator into a zero byte, and
	**
	**	~(((w & 7F..7F) + 7F..7F) | w | 7F..7F)
	**
	**  sets the high bit of exactly the bytes that are zero: the sum
	**  carries into the high bit of a byte whose low seven bits are not
	**  all clear, OR-ing W in catches a byte of high bit set, and no
	**  carry crosses into the next byte.  (The shorter (w - 01..01) &
	**  ~w & 80..80 tells whether a word holds a zero byte, but a zero
	**  byte followed by 0x01 - a terminator followed by 0x0B - borrows
	**  and is counted twice: the numbers of the records went wrong.)
	**
	**  The flags are not counted word by word - __BUILTIN_POPCOUNTLL is
	**  a call into the run-time library where the processor is not told
	**  it has POPCNT - but added up in the eight byte lanes of an
	**  accumulator, which holds 255 words before a lane could overflow,
	**  and folded into the count once per run.
	*/
	while ( (l_pos + sizeof(uint64_t)) <= a_len )
		{
		l_acc	= 0;

		for ( l_run = 0; (l_run < 255) && ((l_pos + sizeof(uint64_t)) <= a_len); l_run++, l_pos += sizeof(uint64_t) )
			{
			memcpy(&l_word, a_blk + l_pos, sizeof(l_word));	/* The alignment is none of our business	*/

			l_word	^= UINT64_C(0x0A0A0A0A0A0A0A0A);
			l_word	= ~(((l_word & UINT64_C(0x7F7F7F7F7F7F7F7F)) + UINT64_C(0x7F7F7F7F7F7F7F7F))
					| l_word | UINT64_C(0x7F7F7F7F7F7F7F7F));

			l_acc	+= l_word >> 7;				/* One per zero byte, in its own lane	*/
			}

		/* The lanes, at most 255 each, folded: pairs into 16 bits, then all four */
		l_acc	= (l_acc & UINT64_C(0x00FF00FF00FF00FF)) + ((l_acc >> 8) & UINT64_C(0x00FF00FF00FF00FF));
		l_cnt	+= (l_acc * UINT64_C(0x0001000100010001)) >> 48;
		}

	/* The tail, of less than a word */
	for ( ; l_pos < a_len; l_pos++ )
		if ( a_blk [l_pos] == '\n' )
			l_cnt++;

	return	l_cnt;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Tell whether a block carries any of the search strings at all.
**
**	The point of it is the block that carries none: under /MATCH=OR -
**	and under /MATCH=AND, which asks for more, not for less - not one
**	record of such a block can match, and the whole of it is passed
**	over without the matcher being entered once per record.  On a file
**	whose records are short that is the difference between one scan of
**	sixty kilobytes and twenty thousand scans of fifty bytes.
**
**	The block is the whole of what is buffered, so a record is always
**	covered by it in full: a record is delivered only once its
**	terminator has been read, and a partial one is moved to the front
**	of the buffer before the refill that completes it.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_len		Length of the block
**	a_blk		The block itself
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- at least one string occurs in the block;
**	STS$K_WARN	- none of them does.
**--
*/
static	int	s_srch$blkhas	(
		SEARCH$CTX *	a_ctx,
		unsigned	a_len,
	const	char *		a_blk
			)
{
	for ( unsigned i = 0; i < a_ctx->strcnt; i++ )
		if ( 1 & s_srch$instr(a_ctx->exact, a_len, a_blk, &a_ctx->strs [i]) )
			return	STS$K_SUCCESS;

	return	STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Apply the whole table of the search strings to a record and combine
**	the results the way /MATCH asks for:
**
**	    OR		at least one string is present	(the default)
**	    AND		every string is present
**	    NAND	at least one string is absent
**	    NOR		no string is present
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_reclen	Length of the record
**	a_rec		The record itself
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the record satisfies the match;
**	STS$K_WARN	- it does not.
**--
*/
static	int	s_srch$match	(
		SEARCH$CTX *	a_ctx,
		unsigned	a_reclen,
	const	char *		a_rec
			)
{
unsigned	l_found = 0;

	for ( unsigned i = 0; i < a_ctx->strcnt; i++ )
		{
		if ( 1 & s_srch$instr(a_ctx->exact, a_reclen, a_rec, &a_ctx->strs [i]) )
			l_found++;

		/*
		**  The verdict of OR and NOR is settled by the first string
		**  found, that of AND and NAND by the first one missing.
		*/
		if ( l_found && ((a_ctx->mode == SEARCH$K_MATCH_OR) || (a_ctx->mode == SEARCH$K_MATCH_NOR)) )
			break;

		if ( (l_found != (i + 1)) && ((a_ctx->mode == SEARCH$K_MATCH_AND) || (a_ctx->mode == SEARCH$K_MATCH_NAND)) )
			break;
		}

	switch ( a_ctx->mode )
		{
		case	SEARCH$K_MATCH_AND:
			return	(l_found == a_ctx->strcnt) ? STS$K_SUCCESS : STS$K_WARN;

		case	SEARCH$K_MATCH_NAND:
			return	(l_found == a_ctx->strcnt) ? STS$K_WARN : STS$K_SUCCESS;

		case	SEARCH$K_MATCH_NOR:
			return	l_found ? STS$K_WARN : STS$K_SUCCESS;

		default:				/* SEARCH$K_MATCH_OR			*/
			return	l_found ? STS$K_SUCCESS : STS$K_WARN;
		}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Deliver the next record of a file.  A record is everything up to
**	the next line terminator, the terminator excluded; the last line of
**	a file that ends without one is a record as well.
**
**	The record is NOT copied out: what is delivered is a pointer into
**	the read-ahead buffer of the stream, which is why it stays valid
**	only until the next call - a caller that has to keep a record, as
**	the ring of /WINDOW does, copies it for itself.  On a file of a
**	million records that spares a million copies.
**
**	The buffer is refilled as it empties, a partial record being moved
**	to its front first, so that a record never spans two refills.  A
**	record that would not fit into the whole buffer makes the buffer
**	grow, doubling, up to SEARCH$K_MAXREC; one longer still is
**	delivered cut to the buffer and the rest of it is dropped up to
**	the next terminator.
**
**  FORMAL PARAMETERS:
**
**	a_rab		The record stream
**	a_reclen	Receives the length of the record
**	a_rec		Receives the address of the record within the buffer
**			of the stream
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the file of the stream.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- a record has been delivered;
**	STS$K_INFO	- a record has been delivered, truncated;
**	STS$K_WARN	- the file is exhausted;
**	STS$K_ERROR	- READ(2) failed, ERRNO is set.
**--
*/
static	int	s_srch$getrec	(
		SEARCH$RAB *	a_rab,
		unsigned *	a_reclen,
	const	char **		a_rec
			)
{
const char *	l_nl;
ssize_t		l_red;
unsigned	l_rest, l_len;

	for ( ;; )
		{
		l_rest	= a_rab->len - a_rab->pos;

		/*
		**  The tail of an over-long record, whose head has been
		**  delivered already: it is dropped up to the terminator that
		**  closes it, across as many refills as that takes.
		*/
		if ( a_rab->skip )
			{
			if ( l_rest && (l_nl = memchr(a_rab->buf + a_rab->pos, '\n', l_rest)) )
				{
				a_rab->pos	= (unsigned) (l_nl - a_rab->buf) + 1;
				a_rab->skip	= 0;

				continue;
				}

			if ( a_rab->eof )
				return	STS$K_WARN;

			a_rab->pos	= a_rab->len = 0;

			l_red	= read(a_rab->fd, a_rab->buf, a_rab->bufsz);

			if ( (l_red < 0) && (errno == EINTR) )
				continue;

			if ( l_red < 0 )
				return	STS$K_ERROR;

			if ( !l_red )
				a_rab->eof	= 1;
			else	{
				a_rab->len	= (unsigned) l_red;
				a_rab->fills++;
				}

			continue;
			}

		if ( l_rest && (l_nl = memchr(a_rab->buf + a_rab->pos, '\n', l_rest)) )
			{
			l_len	= (unsigned) (l_nl - (a_rab->buf + a_rab->pos));

			*a_rec		= a_rab->buf + a_rab->pos;
			a_rab->pos	+= l_len + 1;
			a_rab->recnum++;

			/* A record present whole is delivered whole, however long it is */
			*a_reclen	= l_len;

			return	STS$K_SUCCESS;
			}

		/*
		**  No terminator in what is buffered.  At the end of the file
		**  the remainder is the last record; otherwise the buffer is
		**  compacted and refilled.
		*/
		if ( a_rab->eof )
			{
			/* A file that is followed may yet receive the rest of the record */
			if ( !l_rest || a_rab->follow )
				return	STS$K_WARN;

			*a_rec		= a_rab->buf + a_rab->pos;
			a_rab->pos	= a_rab->len;
			a_rab->recnum++;

			*a_reclen	= l_rest;

			return	STS$K_SUCCESS;
			}

		if ( a_rab->pos )
			{
			memmove(a_rab->buf, a_rab->buf + a_rab->pos, l_rest);

			a_rab->len	= l_rest;
			a_rab->pos	= 0;
			}

		/*
		**  A full buffer without a single terminator: the record is
		**  longer than the read-ahead area.  The area is doubled, up to
		**  SEARCH$K_MAXREC, and the read goes on; only a record longer
		**  than that - or one met when the heap refuses - is cut: its
		**  head is delivered and the rest of it is dropped by the
		**  branch above.
		*/
		if ( a_rab->len == a_rab->bufsz )
			{
			if ( a_rab->bufsz < SEARCH$K_MAXREC )
				{
				unsigned	l_newsz = (a_rab->bufsz > (SEARCH$K_MAXREC / 2)) ? SEARCH$K_MAXREC : (a_rab->bufsz * 2);
				char *		l_new = a_rab->grown ? realloc(a_rab->buf, l_newsz) : malloc(l_newsz);

				if ( l_new )
					{
					if ( !a_rab->grown )
						memcpy(l_new, a_rab->buf, a_rab->len);

					a_rab->buf	= l_new;
					a_rab->bufsz	= l_newsz;
					a_rab->grown	= 1;

					continue;
					}
				}

			*a_rec		= a_rab->buf;
			*a_reclen	= a_rab->len;

			a_rab->pos	= a_rab->len;
			a_rab->skip	= 1;
			a_rab->recnum++;

			return	STS$K_INFO;
			}

		l_red	= read(a_rab->fd, a_rab->buf + a_rab->len, a_rab->bufsz - a_rab->len);

		if ( (l_red < 0) && (errno == EINTR) )
			continue;

		if ( l_red < 0 )
			return	STS$K_ERROR;

		if ( !l_red )
			{
			a_rab->eof	= 1;
			continue;
			}

		a_rab->len	+= (unsigned) l_red;
		a_rab->fills++;
		}
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Mark the positions of a record that belong to a search string.  A
**	byte is marked when it is part of an occurrence of any of the
**	strings, so two overlapping occurrences - ALPHA and PHA in
**	ALPHABET - yield one marked run rather than two nested ones.
**
**	The scan repeats the one of S_SRCH$INSTR rather than sharing it:
**	the matcher stops at the first occurrence, and here every one of
**	them is wanted.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_reclen	Length of the record
**	a_rec		The record itself
**	a_mark		Receives one flag per byte of the record
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- at least one byte has been marked;
**	STS$K_WARN	- nothing has, the record carries none of the strings.
**--
*/
static	int	s_srch$mark	(
		SEARCH$CTX *	a_ctx,
		unsigned	a_reclen,
	const	char *		a_rec,
		char *		a_mark
			)
{
unsigned	l_cnt = 0, l_from, l_at, l_mlen;

	memset(a_mark, 0, a_reclen);

	/* Every occurrence of every string, overlapping ones too: the next one is looked for a byte on */
	for ( unsigned s = 0; s < a_ctx->strcnt; s++ )
		for ( l_from = 0; 1 & s_srch$find(a_ctx->exact, a_reclen, a_rec, l_from, &a_ctx->strs [s], &l_at, &l_mlen); l_from = l_at + 1 )
			{
			memset(a_mark + l_at, 1, l_mlen);
			l_cnt++;
			}

	return	l_cnt ? STS$K_SUCCESS : STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Display one record: the number of it when /NUMBERS is in force, the
**	record itself, a line terminator.  Under /FORMAT=GREP the record is
**	led instead by the specification of the file and its number, parted
**	by a colon for a record that matched and by a hyphen for one of the
**	window, as GREP -n writes them.
**
**	When /HIGHLIGHT is in force the runs of the record that belong to a
**	search string are wrapped into the escape sequence of the style and
**	the sequence that cancels it.  The sequences are put into the
**	output WITHOUT being counted, so the pager still measures the page
**	in the lines the screen really shows.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_fspec		The specification of the file, ASCIZ
**	a_sep		':' for a record that matched, '-' for one of the window
**	a_recnum	Number of the record in the file
**	a_reclen	Length of the record
**	a_rec		The record itself
**	a_mark		A scratch area of at least A_RECLEN bytes, or NULL when
**			the record is not to be highlighted
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the output context, advances the counter of the displayed
**	records.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the record has been displayed;
**	STS$K_WARN	- the display has been abandoned at the pager;
**	STS$K_ERROR	- the output has failed.
**--
*/
static	int	s_srch$putrec	(
		SEARCH$CTX *	a_ctx,
	const	char *		a_fspec,
		char		a_sep,
		uint64_t	a_recnum,
		unsigned	a_reclen,
	const	char *		a_rec,
		char *		a_mark
			)
{
const char *	l_seq;
unsigned	l_run;
int		l_status;

	if ( a_ctx->format == SEARCH$K_FMT_GREP )
		{
		char	l_pfx [SEARCH$K_SZ_PATH + 32];
		int	l_len = snprintf(l_pfx, sizeof(l_pfx), "%s%c%llu%c", a_fspec, a_sep,
					(unsigned long long) a_recnum, a_sep);

		if ( l_len >= (int) sizeof(l_pfx) )
			l_len	= (int) sizeof(l_pfx) - 1;

		if ( !(1 & (l_status = srch$put_raw(a_ctx->out, (unsigned) l_len, l_pfx))) )
			return	l_status;
		}
	else if ( a_ctx->numbers )
		{
		l_status	= $SRCHPUT(a_ctx->out, "!#UQ  ", SEARCH$K_NUMWIDTH, a_recnum);

		if ( !(1 & l_status) )
			return	l_status;
		}

	if ( a_mark && (1 & s_srch$mark(a_ctx, a_reclen, a_rec, a_mark)) )
		{
		l_seq	= srch$hlseq(a_ctx->hlstyle);

		for ( unsigned i = 0; i < a_reclen; i += l_run )
			{
			/* One run of bytes that share the state of the marking */
			for ( l_run = 1; ((i + l_run) < a_reclen) && (a_mark [i + l_run] == a_mark [i]); l_run++ )
				;

			if ( a_mark [i] )
				srch$put_ctl(a_ctx->out, (unsigned) strlen(l_seq), l_seq);

			if ( !(1 & (l_status = srch$put_raw(a_ctx->out, l_run, a_rec + i))) )
				return	l_status;

			if ( a_mark [i] )
				srch$put_ctl(a_ctx->out, 4, "\033[0m");
			}
		}
	else if ( !(1 & (l_status = srch$put_raw(a_ctx->out, a_reclen, a_rec))) )
		return	l_status;

	a_ctx->stat.lines++;

	return	srch$put_raw(a_ctx->out, 1, "\n");
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Make the marking area of the highlighting hold a record of a given
**	length.  The area only ever grows, to the longest record displayed
**	from the file; a record is no longer cut at a fixed length, so the
**	area cannot be of one.
**
**  FORMAL PARAMETERS:
**
**	a_mark		The area, NULL before the first call; may be moved
**	a_max		Its capacity, bytes
**	a_len		The length of the record to be marked
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the area holds A_LEN + 1 bytes;
**	STS$K_ERROR	- the heap has refused, the area is left as it was:
**			  the record is displayed unmarked rather than not
**			  at all.
**--
*/
static	int	s_srch$markbuf	(
		char **		a_mark,
		unsigned *	a_max,
		unsigned	a_len
			)
{
char *	l_new;

	if ( *a_max > a_len )
		return	STS$K_SUCCESS;

	if ( !(l_new = realloc(*a_mark, (size_t) a_len + 1)) )
		return	STS$K_ERROR;

	*a_mark	= l_new;
	*a_max	= a_len + 1;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Announce a file before its first displayed record: an asterisk rule,
**	the specification and the time stamp the selection has worked on.
**	Nothing is announced under /NOHEADING or /FORMAT=GREP.
**
**	The time stands in a column of its own, so that a run over several
**	files reads as a listing; a specification too long for the column
**	pushes it right rather than being cut to fit.  The time is a
**	quadword and goes to !%D BY VALUE, per STARLET X.01-09.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_fspec		The specification of the file, ASCIZ
**	a_tim		Its time stamp
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the output context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS, or the status of the output.
**--
*/
static	int	s_srch$heading	(
		SEARCH$CTX *	a_ctx,
	const	char *		a_fspec,
		fao_time_t	a_tim
			)
{
unsigned	l_namlen, l_pad;

	if ( !a_ctx->heading || (a_ctx->format == SEARCH$K_FMT_GREP) )
		return	STS$K_SUCCESS;

	l_namlen	= (unsigned) strlen(a_fspec);
	l_pad		= (l_namlen < SEARCH$K_HDRNAME) ? (SEARCH$K_HDRNAME - l_namlen) : 2;

	$SRCHPUT(a_ctx->out, "\n!#**\n!AZ", SEARCH$K_HDRWIDTH, a_fspec);

	return	$SRCHPUT(a_ctx->out, "!#* !%D\n\n", l_pad, a_tim);
}


/*
**  One piece of a file searched by several threads at once.
*/
typedef	struct	srch_piece_t
{
	SEARCH$CTX *	ctx;			/* The context of the file, read only		*/
	const char *	fspec;			/* The specification of the file		*/
	const char *	beg,			/* First byte of the piece, a record's first	*/
		   *	end;			/* Byte past its last, a record's terminator +1	*/
	int		count;			/* Count the records only, the first pass	*/
	int		needcnt;		/* Records are numbered: count the skipped ones	*/
	uint64_t	base,			/* Records of the file before the piece		*/
			records,		/* Terminators in the piece, the first pass	*/
			matched;		/* Records of the piece that matched		*/
	SEARCH$STAT	stat;			/* The counters of the piece			*/
	char *		mem;			/* Its display, gathered into the heap		*/
	size_t		memlen;			/* Its length					*/
} SEARCH$PIECE;

/*
**  The threads that may run pieces at once, over all the files split at
**  the same time: /THREADS of the run, set by the first split.
*/
static	int	s_pieces = -1;

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Search one piece of a mapped file, as SRCH$FILE searches a whole
**	one under the prefilter: a region of SEARCH$K_SZ_BLK that carries
**	none of the strings is stepped over, its records counted if they are
**	numbered; the records of one that does are matched one by one and
**	displayed into the heap.  The first pass of a split only counts the
**	terminators of the piece, so that the next pieces know where their
**	numbering starts.
**
**  FORMAL PARAMETERS:
**
**	a_arg		The piece, SEARCH$PIECE
**
**  IMPLICIT INPUTS/OUTPUTS:
**	The mapping of the file, read only.
**
**  RETURN VALUE:
**	NULL; the piece carries the result.
**--
*/
static	void *	s_srch$piece	(
		void *		a_arg
			)
{
SEARCH$PIECE *	l_pc = (SEARCH$PIECE *) a_arg;
SEARCH$CTX	l_ctx;
SEARCH$OUT	l_out;
const char *	l_pos = l_pc->beg, *l_regend, *l_nl, *l_top;
uint64_t	l_recnum = l_pc->base;
char *		l_mark = NULL;
unsigned	l_markmax = 0, l_reclen, l_reglen;

	if ( l_pc->count )
		{
		l_pc->records	= s_srch$nlcount((unsigned) (l_pc->end - l_pc->beg), l_pc->beg);

		return	NULL;
		}

	memcpy(&l_ctx, l_pc->ctx, sizeof(SEARCH$CTX));
	memset(&l_ctx.stat, 0, sizeof(l_ctx.stat));

	srch$out_mem(&l_out);
	l_ctx.out	= &l_out;

	while ( l_pos < l_pc->end )
		{
		/*
		**  The region ahead, up to the last terminator within
		**  SEARCH$K_SZ_BLK; a record longer than that is a region of
		**  its own.
		*/
		l_top	= ((size_t) (l_pc->end - l_pos) > SEARCH$K_SZ_BLK) ? (l_pos + SEARCH$K_SZ_BLK) : l_pc->end;

		if ( (l_nl = memrchr(l_pos, '\n', (size_t) (l_top - l_pos))) )
			{
			l_reglen	= (unsigned) (l_nl - l_pos) + 1;

			if ( !(1 & s_srch$blkhas(&l_ctx, l_reglen, l_pos)) )
				{
				if ( l_pc->needcnt )
					{
					uint64_t	l_cnt = s_srch$nlcount(l_reglen, l_pos);

					l_ctx.stat.recsread	+= l_cnt;
					l_recnum		+= l_cnt;
					}

				l_ctx.stat.chars	+= l_reglen;
				l_pos			+= l_reglen;

				continue;
				}

			l_regend	= l_nl + 1;
			}
		else	l_regend	= (l_nl = memchr(l_pos, '\n', (size_t) (l_pc->end - l_pos))) ? (l_nl + 1) : l_pc->end;

		/* The records of a region that carries a string, one by one */
		while ( l_pos < l_regend )
			{
			l_nl		= memchr(l_pos, '\n', (size_t) (l_regend - l_pos));
			l_reclen	= (unsigned) ((l_nl ? l_nl : l_regend) - l_pos);

			l_recnum++;
			l_ctx.stat.recsread++;
			l_ctx.stat.chars	+= l_reclen + 1;

			if ( 1 & s_srch$match(&l_ctx, l_reclen, l_pos) )
				{
				l_pc->matched++;
				l_ctx.stat.recsmatched++;

				s_srch$putrec(&l_ctx, l_pc->fspec, ':', l_recnum, l_reclen, l_pos,
					(l_ctx.hilite && (1 & s_srch$markbuf(&l_mark, &l_markmax, l_reclen))) ? l_mark : NULL);
				}

			l_pos	= l_nl ? (l_nl + 1) : l_regend;
			}
		}

	srch$out_take(&l_out, &l_pc->mem, &l_pc->memlen);
	l_pc->stat	= l_ctx.stat;

	free(l_mark);

	return	NULL;
}


/*
**  The pieces of one split, and the threads that run them.  The threads
**  take the pieces in their order; under the second pass a thread does not
**  run ahead of the display by more than WINDOW pieces, so that what is
**  gathered in the heap and not yet displayed stays within WINDOW pieces'
**  worth whatever the size of the file.
*/
typedef	struct	srch_split_t
{
	SEARCH$PIECE *	pcs;			/* The pieces					*/
	unsigned	cnt,			/* Their number					*/
			next,			/* Next piece to be taken by a thread		*/
			shown,			/* Pieces displayed so far			*/
			window;			/* How far a thread may run ahead, pieces	*/
	int		paced;			/* The window applies: the second pass		*/
	int *		done;			/* Per piece: it has been run			*/

	pthread_mutex_t	lock;
	pthread_cond_t	cond;			/* A piece is done, or one has been displayed	*/
} SEARCH$SPLIT;


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	A thread of a split: take the next piece, run it, mark it done; on
**	the second pass, wait while the display lags too far behind.
**
**  FORMAL PARAMETERS:
**
**	a_arg		The split, SEARCH$SPLIT
**
**  IMPLICIT INPUTS/OUTPUTS:
**	The split, under its lock.
**
**  RETURN VALUE:
**	NULL.
**--
*/
static	void *	s_srch$splitter	(
		void *		a_arg
			)
{
SEARCH$SPLIT *	l_sp = (SEARCH$SPLIT *) a_arg;
unsigned	l_k;

	for ( ;; )
		{
		pthread_mutex_lock(&l_sp->lock);

		while ( l_sp->paced && (l_sp->next < l_sp->cnt) && (l_sp->next >= (l_sp->shown + l_sp->window)) )
			pthread_cond_wait(&l_sp->cond, &l_sp->lock);

		if ( l_sp->next >= l_sp->cnt )
			{
			pthread_mutex_unlock(&l_sp->lock);
			break;
			}

		l_k	= l_sp->next++;

		pthread_mutex_unlock(&l_sp->lock);

		s_srch$piece(&l_sp->pcs [l_k]);

		pthread_mutex_lock(&l_sp->lock);
		l_sp->done [l_k]	= 1;
		pthread_cond_broadcast(&l_sp->cond);
		pthread_mutex_unlock(&l_sp->lock);
		}

	return	NULL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Start the threads of a split; a thread that cannot be created is
**	simply not there - the others take its pieces, and should none be
**	there at all, the caller runs every piece itself.
**
**  FORMAL PARAMETERS:
**
**	a_sp		The split
**	a_nthr		Threads wanted
**	a_tids		Receives their identifiers
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	The number of the threads started.
**--
*/
static	unsigned	s_srch$splitstart	(
		SEARCH$SPLIT *	a_sp,
		unsigned	a_nthr,
		pthread_t *	a_tids
			)
{
unsigned	l_made = 0;

	for ( unsigned i = 0; i < a_nthr; i++ )
		if ( !pthread_create(&a_tids [l_made], NULL, s_srch$splitter, a_sp) )
			l_made++;

	/* None at all: the caller runs every piece - unpaced, it is the one that would display them */
	if ( !l_made )
		{
		a_sp->paced	= 0;
		s_srch$splitter(a_sp);
		}

	return	l_made;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Search a large mapped file by several threads at once.  The mapping
**	is cut at record boundaries into pieces of about SEARCH$K_SPLITPIECE;
**	where the records are numbered, the terminators of every piece are
**	counted first, in parallel, so that every piece knows the number its
**	records start from.  Then the threads search the pieces in their
**	order into the heap, and the caller displays every piece as soon as
**	it and all before it are done - the heading before the first one
**	that matched - and lets go of it: the display is that of a single
**	thread, byte for byte, and what waits in the heap is never more than
**	a window of /THREADS + 2 pieces.
**
**	The threads are taken from a budget shared by every file split at
**	the same time, /THREADS of the run; with fewer than two to be had, or
**	a file too small for two pieces, nothing is done and the caller
**	searches the file the usual way.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context of the file
**	a_fspec		The specification of the file, ASCIZ
**	a_tim		Its time stamp, for the heading
**	a_map		The mapping of the file
**	a_len		Its length
**	a_needcnt	The records are numbered
**	a_matched	Receives the number of the records that matched
**	a_records	Receives the number of the records, where counted
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_PIECES; writes to the output context, adds to the counters.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the file has been searched;
**	STS$K_WARN	- it has not, the caller is to search it;
**	SEARCH$_NOMEM	- the heap has refused, signalled; the caller is to
**			  search the file.
**--
*/
static	int	s_srch$split	(
		SEARCH$CTX *	a_ctx,
	const	char *		a_fspec,
		fao_time_t	a_tim,
	const	char *		a_map,
		size_t		a_len,
		int		a_needcnt,
		uint64_t *	a_matched,
		uint64_t *	a_records
			)
{
SEARCH$SPLIT	l_sp = {0};
SEARCH$CTX *	l_tmpl;
pthread_t	l_tids [SEARCH$K_MAXTHREADS];
unsigned	l_nthr, l_want, l_made, l_cnt;
int		l_avail, l_init = -1, l_heading = 0;
const char *	l_beg = a_map, *l_end = a_map + a_len, *l_cut;
uint64_t	l_base = 0;

	/* The budget is set once, by the first file split: /THREADS of the run */
	__atomic_compare_exchange_n(&s_pieces, &l_init, (int) a_ctx->threads, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);

	l_cnt	= (unsigned) ((a_len + SEARCH$K_SPLITPIECE - 1) / SEARCH$K_SPLITPIECE);

	if ( l_cnt < 2 )
		return	STS$K_WARN;

	l_want	= (a_ctx->threads < l_cnt) ? a_ctx->threads : l_cnt;

	if ( l_want > SEARCH$K_MAXTHREADS )
		l_want	= SEARCH$K_MAXTHREADS;

	/* As many threads as the budget lets go of, two at least */
	l_avail	= __atomic_load_n(&s_pieces, __ATOMIC_RELAXED);

	do	{
		if ( (l_avail < 2) || (l_want < 2) )
			return	STS$K_WARN;

		l_nthr	= ((unsigned) l_avail < l_want) ? (unsigned) l_avail : l_want;
		}
	while ( !__atomic_compare_exchange_n(&s_pieces, &l_avail, l_avail - (int) l_nthr, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED) );

	l_sp.pcs	= calloc(l_cnt, sizeof(SEARCH$PIECE));
	l_sp.done	= calloc(l_cnt, sizeof(int));
	l_tmpl		= malloc(sizeof(SEARCH$CTX));

	if ( !l_sp.pcs || !l_sp.done || !l_tmpl )
		{
		free(l_sp.pcs);
		free(l_sp.done);
		free(l_tmpl);
		__atomic_add_fetch(&s_pieces, (int) l_nthr, __ATOMIC_ACQ_REL);

		return	$SRCHMSG(SEARCH$_NOMEM, l_cnt * (sizeof(SEARCH$PIECE) + sizeof(int)), errno, strerror(errno));
		}

	/*
	**  The pieces start from a copy of the context, not from the context
	**  itself, whose counters this thread adds to while they run.
	*/
	memcpy(l_tmpl, a_ctx, sizeof(SEARCH$CTX));

	pthread_mutex_init(&l_sp.lock, NULL);
	pthread_cond_init(&l_sp.cond, NULL);

	l_sp.cnt	= l_cnt;
	l_sp.window	= l_nthr + 2;

	/* The cuts: about SEARCH$K_SPLITPIECE apart, each moved past the next terminator */
	for ( unsigned i = 0; i < l_cnt; i++ )
		{
		l_cut	= (i == (l_cnt - 1)) ? l_end : (a_map + (a_len / l_cnt) * (i + 1));

		if ( l_cut < l_beg )
			l_cut	= l_beg;

		if ( l_cut < l_end )
			{
			const char *	l_nl = memchr(l_cut, '\n', (size_t) (l_end - l_cut));

			l_cut	= l_nl ? (l_nl + 1) : l_end;
			}

		l_sp.pcs [i].ctx	= l_tmpl;
		l_sp.pcs [i].fspec	= a_fspec;
		l_sp.pcs [i].beg	= l_beg;
		l_sp.pcs [i].end	= l_cut;
		l_sp.pcs [i].needcnt	= a_needcnt;

		l_beg	= l_cut;
		}

	/* The first pass, where the records are numbered: the terminators of every piece */
	if ( a_needcnt )
		{
		for ( unsigned i = 0; i < l_cnt; i++ )
			l_sp.pcs [i].count	= 1;

		l_made	= s_srch$splitstart(&l_sp, l_nthr, l_tids);

		for ( unsigned i = 0; i < l_made; i++ )
			pthread_join(l_tids [i], NULL);

		for ( unsigned i = 0; i < l_cnt; i++ )
			{
			l_sp.pcs [i].count	= 0;
			l_sp.pcs [i].base	= l_base;
			l_base			+= l_sp.pcs [i].records;
			l_sp.done [i]		= 0;
			}

		l_sp.next	= 0;
		}

	/* The second pass: searched by the threads, displayed here in order as they come */
	l_sp.paced	= 1;
	l_made		= s_srch$splitstart(&l_sp, l_nthr, l_tids);

	*a_matched	= 0;
	*a_records	= 0;

	for ( unsigned i = 0; i < l_cnt; i++ )
		{
		SEARCH$PIECE *	l_pc = &l_sp.pcs [i];

		pthread_mutex_lock(&l_sp.lock);

		while ( !l_sp.done [i] )
			pthread_cond_wait(&l_sp.cond, &l_sp.lock);

		pthread_mutex_unlock(&l_sp.lock);

		if ( l_pc->matched && !l_heading )
			{
			l_heading	= 1;
			s_srch$heading(a_ctx, a_fspec, a_tim);
			}

		if ( l_pc->memlen )
			srch$put_raw(a_ctx->out, (unsigned) l_pc->memlen, l_pc->mem);

		free(l_pc->mem);
		l_pc->mem	= NULL;

		*a_matched		+= l_pc->matched;
		*a_records		+= l_pc->stat.recsread;

		a_ctx->stat.recsread	+= l_pc->stat.recsread;
		a_ctx->stat.recsmatched	+= l_pc->stat.recsmatched;
		a_ctx->stat.chars	+= l_pc->stat.chars;
		a_ctx->stat.lines	+= l_pc->stat.lines;

		pthread_mutex_lock(&l_sp.lock);
		l_sp.shown	= i + 1;
		pthread_cond_broadcast(&l_sp.cond);
		pthread_mutex_unlock(&l_sp.lock);
		}

	for ( unsigned i = 0; i < l_made; i++ )
		pthread_join(l_tids [i], NULL);

	__atomic_add_fetch(&s_pieces, (int) l_nthr, __ATOMIC_ACQ_REL);

	pthread_cond_destroy(&l_sp.cond);
	pthread_mutex_destroy(&l_sp.lock);
	free(l_sp.done);
	free(l_sp.pcs);
	free(l_tmpl);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	/CONTINUE: the file followed has been read to its end.  What has
**	been displayed is flushed - the end of this file may never come -
**	the routine sleeps /CONTINUE seconds and looks at the file again:
**
**	    - it is shorter than what has been read of it: it has been cut
**	      short (a log truncated in place), and is read again from its
**	      beginning;
**	    - its name is another file's now (a log rotated by renaming): the
**	      old one has been read to its end, so the new one is opened and
**	      read from its beginning; while the old one is still growing it
**	      is read on;
**	    - otherwise whatever has been appended is read on.
**
**	A name that has vanished - rotated, not yet created again - leaves
**	the old file followed until it appears.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_rab		The record stream of the file
**	a_fspec		The specification of the file, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	G_SRCH$INTERRUPT, read; the output context, flushed; the stream
**	may be given a new descriptor.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the stream may be read on;
**	STS$K_INFO	- it has been started anew, the record numbers and
**			  whatever the caller keeps per file begin again;
**	STS$K_WARN	- the follow is over: SIGINT, or Q at the pager;
**	STS$K_ERROR	- the file cannot be looked at, signalled.
**--
*/
static	int	s_srch$follow	(
		SEARCH$CTX *	a_ctx,
		SEARCH$RAB *	a_rab,
	const	char *		a_fspec
			)
{
struct stat	l_fst, l_nst;
struct timespec	l_ts = { .tv_sec = (time_t) a_ctx->contint, .tv_nsec = 0 };
off_t		l_off;
int		l_fd;

	srch$flush(a_ctx->out);

	if ( a_ctx->out->quit || g_srch$interrupt )
		return	STS$K_WARN;

	/* SIGINT cuts the sleep short - the handler is installed without SA_RESTART */
	nanosleep(&l_ts, NULL);

	if ( g_srch$interrupt )
		return	STS$K_WARN;

	if ( fstat(a_rab->fd, &l_fst) || (0 > (l_off = lseek(a_rab->fd, 0, SEEK_CUR))) )
		return	$SRCHMSG(SEARCH$_READERR, a_fspec, errno, strerror(errno));

	l_fd	= -1;

	if ( S_ISREG(l_fst.st_mode) && (l_fst.st_size < l_off) )
		{
		$SRCHMSG(SEARCH$_TRUNCATED, a_fspec);

		if ( 0 > lseek(a_rab->fd, 0, SEEK_SET) )
			return	$SRCHMSG(SEARCH$_READERR, a_fspec, errno, strerror(errno));
		}
	/* Rotated: only once the old file has nothing more to give */
	else if ( (l_fst.st_size == l_off) && !stat(a_fspec, &l_nst)
		&& ((l_nst.st_ino != l_fst.st_ino) || (l_nst.st_dev != l_fst.st_dev)) )
		{
		/* Not to be opened yet - created, but not readable: the next look tries again */
		if ( 0 > (l_fd = open(a_fspec, O_RDONLY)) )
			{
			a_rab->eof	= 0;

			return	STS$K_SUCCESS;
			}

		$SRCHMSG(SEARCH$_REOPENED, a_fspec);

		close(a_rab->fd);
		a_rab->fd	= l_fd;
		}
	else	{
		a_rab->eof	= 0;

		return	STS$K_SUCCESS;
		}

	/*
	**  Started anew: whatever is buffered belongs to the old contents.
	**  FILLS moves on, so that the prefilter does not take the region it
	**  has examined for one of the new contents.
	*/
	a_rab->len	= a_rab->pos = 0;
	a_rab->eof	= a_rab->skip = 0;
	a_rab->recnum	= 0;
	a_rab->fills++;

	return	STS$K_INFO;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Search one file and display every record that satisfies the match,
**	together with the window of records asked for by /WINDOW.  A file
**	marked FOLLOW by /CONTINUE is not finished at its end: the records
**	appended to it are searched as they come - see S_SRCH$FOLLOW.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The search context
**	a_fspec		The specification of the file, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the file, writes to the output context, advances the counters
**	of the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the file has been searched, something matched;
**	STS$K_WARN	- the file has been searched, nothing matched;
**	STS$K_ERROR	- the file could not be opened or read, the
**			  condition has been signalled.
**--
*/
int	srch$file	(
		SEARCH$CTX *	a_ctx,
	const	char *		a_fspec
			)
{
struct stat	l_st = {0};
fao_time_t	l_tim = 0;
SEARCH$RAB *	l_rab = NULL;
SEARCH$RING *	l_ring = NULL;
const char *	l_rec = NULL;
char *		l_mark = NULL;
uint64_t	l_matched = 0, l_lastput = 0, l_first;
unsigned	l_reclen = 0, l_slot = 0, l_pending = 0, l_ringcnt = 0;
const char *	l_cleanend = NULL;
uint64_t	l_seen = 0;
int		l_status, l_fd, l_heading = 0, l_longrec = 0;
int		l_prefilt, l_needcnt, l_binary = 0, l_atend = 0;
unsigned	l_markmax = 0;

	/* The reader has answered Q at the pager - nothing more is to be displayed */
	if ( a_ctx->out->quit )
		return	STS$K_FATAL;

	if ( 0 > (l_fd = open(a_fspec, O_RDONLY)) )
		return	$SRCHMSG(SEARCH$_OPENIN, a_fspec, errno, strerror(errno));

	/*
	**  The read-ahead buffer and the record are far too large for the
	**  stack of a routine that may be reached through a deep "..."
	**  walk, so both live in the heap.
	*/
	if ( !(l_rab = calloc(1, sizeof(SEARCH$RAB))) )
		{
		close(l_fd);

		return	$SRCHMSG(SEARCH$_NOMEM, sizeof(SEARCH$RAB), errno, strerror(errno));
		}

	/*
	**  The marking of the highlighting: one flag per byte of a record.
	**  It grows with the longest record displayed - see S_SRCH$MARKBUF -
	**  and starts at the size of the read-ahead area.
	*/
	if ( a_ctx->hilite && !(1 & s_srch$markbuf(&l_mark, &l_markmax, SEARCH$K_SZ_BUF)) )
		{
		free(l_rab);
		close(l_fd);

		return	$SRCHMSG(SEARCH$_NOMEM, SEARCH$K_SZ_BUF, errno, strerror(errno));
		}

	if ( a_ctx->winbef && !(l_ring = calloc(a_ctx->winbef, sizeof(SEARCH$RING))) )
		{
		free(l_mark);
		free(l_rab);
		close(l_fd);

		return	$SRCHMSG(SEARCH$_NOMEM, (a_ctx->winbef * sizeof(SEARCH$RING)), errno, strerror(errno));
		}

	l_rab->fd	= l_fd;

	/*
	**  Whether a block that carries none of the strings may be passed
	**  over as a whole.  Under NAND and NOR a record matches by what it
	**  does NOT carry, so such a block is exactly the one full of
	**  matches; under /WINDOW a record of a block that matched nothing
	**  may still have to be displayed as the neighbour of a match in the
	**  next one.  In both cases every record is looked at, as before.
	*/
	l_prefilt	= ((a_ctx->mode == SEARCH$K_MATCH_OR) || (a_ctx->mode == SEARCH$K_MATCH_AND))
				&& !a_ctx->winbef && !a_ctx->winaft;

	/*
	**  Whether the records of a region passed over as a whole have to be
	**  counted at all.  A number is displayed by /NUMBERS and reported
	**  by /LOG and /STATISTICS; a plain run shows none, and counting the
	**  terminators of a file of a million records costs as much again as
	**  searching it does.
	*/
	l_needcnt	= a_ctx->numbers || a_ctx->log || a_ctx->statistics;

	/*
	**  The stream reads through its own area unless a mapping of the
	**  file is made below, in which case BUF is made to point at that
	**  instead and not a byte is ever copied.
	*/
	l_rab->buf	= l_rab->own;
	l_rab->bufsz	= (unsigned) sizeof(l_rab->own);

	/*
	**  The stamp the heading displays is the one the selection works on,
	**  so that a run with /SINCE shows exactly the time it has compared
	**  against.  A file whose status cannot be read is displayed with a
	**  zero time rather than not at all: it has been opened, so it is
	**  going to be searched.
	*/
	if ( !fstat(l_fd, &l_st) )
		{
		l_tim	= (fao_time_t) ((a_ctx->timsrc == SEARCH$K_TIME_CREATED) ? l_st.st_ctime : l_st.st_mtime);

		/*
		**  A regular file is mapped rather than read: the bytes are
		**  then never copied into a buffer of ours at all, and the
		**  whole file is one block, so the prefilter looks at it in a
		**  single pass.  Anything that cannot be mapped - a pipe, a
		**  device, a file too large for the address space, a mapping
		**  the system refuses - is read as before, which is why the
		**  failure is not even reported.
		**
		**  A file truncated by another process while it is being
		**  searched would raise SIGBUS on the vanished pages; that is
		**  the price of the mapping, and it is the price every reader
		**  of a file being rewritten pays in one form or another.
		*/
		/*
		**  A small file is read rather than mapped: MMAP, MADVISE and
		**  MUNMAP are three system calls and a page table to build, and
		**  on a file of a few tens of kilobytes that costs more than
		**  the copying the mapping was to spare.  A walk of a large
		**  tree is made of exactly such files.
		*/
		/*
		**  A file followed by /CONTINUE is read: a mapping is of the
		**  size the file had when it was made.
		*/
		if ( !a_ctx->follow && S_ISREG(l_st.st_mode) && (l_st.st_size >= SEARCH$K_MAPMIN) && ((uint64_t) l_st.st_size <= UINT32_MAX) )
			{
			void *	l_map = mmap(NULL, (size_t) l_st.st_size, PROT_READ, MAP_PRIVATE, l_fd, 0);

			if ( l_map != MAP_FAILED )
				{
				madvise(l_map, (size_t) l_st.st_size, MADV_SEQUENTIAL);

				l_rab->buf	= (char *) l_map;
				l_rab->bufsz	= (unsigned) l_st.st_size;
				l_rab->len	= (unsigned) l_st.st_size;
				l_rab->mapped	= 1;
				l_rab->mapsz	= (size_t) l_st.st_size;
				l_rab->eof	= 1;	/* The whole of it is present at once	*/
				}
			}
		}

	/*
	**  A binary file - one that carries a NUL among its first
	**  SEARCH$K_SZ_BLK bytes, which is the test GREP makes - is not
	**  displayed record by record: its "records" are whatever happens to
	**  lie between two bytes of 0x0A, and on a terminal they are noise or
	**  worse.  /BINARY=TEXT searches it as any other file, SKIP leaves it
	**  alone, and NOTE, the default, says in one line that it matches.
	**
	**  A file that is read rather than mapped is looked at in its first
	**  buffer; a READ(2) failure here is met again by the loop below,
	**  which reports it.
	*/
	if ( a_ctx->binary != SEARCH$K_BIN_TEXT )
		{
		unsigned	l_head;

		if ( !l_rab->mapped )
			s_srch$fill(l_rab);

		l_head		= l_rab->len - l_rab->pos;
		l_binary	= (NULL != memchr(l_rab->buf + l_rab->pos, '\0',
					(l_head > SEARCH$K_SZ_BLK) ? SEARCH$K_SZ_BLK : l_head));

		/* Left alone: the file is made to look exhausted before a byte of it is examined */
		if ( l_binary && (a_ctx->binary == SEARCH$K_BIN_SKIP) )
			{
			l_rab->pos	= l_rab->len;
			l_rab->eof	= 1;
			}
		}

	/* /CONTINUE, and not a binary file that is to be left alone */
	l_rab->follow	= a_ctx->follow && !(l_binary && (a_ctx->binary == SEARCH$K_BIN_SKIP));

	/*
	**  A large mapped file under /THREADS is searched in pieces by several
	**  threads at once - only where the records are looked at one by one
	**  with no regard to their neighbours: /MATCH=OR or AND, no /WINDOW
	**  (which is what L_PREFILT says), and not a binary file, whose first
	**  match is all that is to be said of it or which is to be left alone.
	**  The loop below then finds the file exhausted.
	*/
	if ( l_prefilt && l_rab->mapped && (a_ctx->threads > 1) && (l_rab->len >= SEARCH$K_SPLITMIN)
		&& !(l_binary && (a_ctx->binary != SEARCH$K_BIN_TEXT))
		&& (1 & s_srch$split(a_ctx, a_fspec, l_tim, l_rab->buf, l_rab->len, l_needcnt, &l_matched, &l_rab->recnum)) )
		{
		l_rab->pos	= l_rab->len;
		l_rab->eof	= 1;
		}

	for ( ;; )
		{
		/*
		**  /CONTINUE: the end of the file has been met.  The file is
		**  waited on; started anew, it is a new file to the display as
		**  well - announced again, with no window carried over.  Once
		**  the follow is over, a record left without its terminator is
		**  delivered as the last one, and the file ends as any other.
		*/
		if ( l_atend )
			{
			l_atend		= 0;
			l_status	= s_srch$follow(a_ctx, l_rab, a_fspec);

			if ( l_status == STS$K_INFO )
				{
				l_heading	= l_longrec = 0;
				l_lastput	= 0;
				l_pending	= l_ringcnt = l_slot = 0;
				l_cleanend	= NULL;

				/* The heading of the new contents shows their own time stamp */
				if ( !fstat(l_rab->fd, &l_st) )
					l_tim	= (fao_time_t) ((a_ctx->timsrc == SEARCH$K_TIME_CREATED) ? l_st.st_ctime : l_st.st_mtime);
				}
			else if ( !(1 & l_status) )
				l_rab->follow	= 0;
			}

		/*
		**  The prefilter, applied to the region that lies ahead rather
		**  than to the record in hand: a region that carries none of
		**  the strings is not split into records at all - its
		**  terminators are counted, its bytes are accounted for, and
		**  the whole of it is stepped over.  A file that matches
		**  nothing therefore costs one scan of it, which is what the
		**  walk of a large tree spends nearly all of its time on.
		**
		**  The region ends at the last record boundary within
		**  SEARCH$K_SZ_BLK, so every record it covers is covered whole;
		**  a partial record at the end of the buffer is left to
		**  SRCH$GETREC, which refills for it.  A region that DOES carry
		**  a string is remembered as examined, so that the scan is not
		**  repeated for every record of it.
		**
		**  A record too long for SEARCH$K_MAXREC goes unreported within
		**  a clean region - and rightly so: SEARCH$_LONGREC warns that
		**  the search may have missed something, and in a region that
		**  carries none of the strings there is nothing to miss.
		*/
		if ( l_prefilt )
			{
			if ( l_seen != l_rab->fills )
				{
				l_seen		= l_rab->fills;
				l_cleanend	= NULL;
				}

			if ( !l_cleanend || ((l_rab->buf + l_rab->pos) >= l_cleanend) )
				{
				const char *	l_beg, *l_lim, *l_top, *l_nl;
				unsigned	l_reglen;

				if ( !(1 & (l_status = s_srch$fill(l_rab))) )
					{
					if ( l_status == STS$K_ERROR )
						$SRCHMSG(SEARCH$_READERR, a_fspec, errno, strerror(errno));
					else if ( l_rab->follow )
						{
						l_atend	= 1;
						continue;
						}

					break;
					}

				l_seen	= l_rab->fills;
				l_beg	= l_rab->buf + l_rab->pos;
				l_lim	= l_rab->buf + l_rab->len;
				l_top	= ((size_t) (l_lim - l_beg) > SEARCH$K_SZ_BLK) ? (l_beg + SEARCH$K_SZ_BLK) : l_lim;
				l_nl	= memrchr(l_beg, '\n', (size_t) (l_top - l_beg));

				/*
				**  Without a terminator in reach there is no region of
				**  whole records to speak of: the record is left to
				**  SRCH$GETREC, which either completes it from the next
				**  refill or truncates it.
				*/
				if ( l_nl )
					{
					l_reglen	= (unsigned) (l_nl - l_beg) + 1;
					l_cleanend	= l_nl + 1;

					if ( !(1 & s_srch$blkhas(a_ctx, l_reglen, l_beg)) )
						{
						if ( l_needcnt )
							{
							uint64_t	l_nlcnt = s_srch$nlcount(l_reglen, l_beg);

							a_ctx->stat.recsread	+= l_nlcnt;
							l_rab->recnum		+= l_nlcnt;
							}

						a_ctx->stat.chars	+= l_reglen;
						l_rab->pos		+= l_reglen;

						continue;
						}
					}
				else	l_cleanend	= NULL;
				}
			}

		l_status	= s_srch$getrec(l_rab, &l_reclen, &l_rec);

		if ( l_status == STS$K_WARN )
			{
			if ( l_rab->follow )
				{
				l_atend	= 1;
				continue;
				}

			break;
			}

		if ( !(1 & l_status) )
			{
			$SRCHMSG(SEARCH$_READERR, a_fspec, errno, strerror(errno));
			break;
			}

		/* A truncated record is announced once per file, not once per record */
		if ( (l_status == STS$K_INFO) && !l_longrec )
			{
			l_longrec	= 1;

			/*
			**  The number of the record is known only where the records
			**  have been counted; a run that displays no numbers has
			**  counted none, and says so by naming no number.
			*/
			if ( l_needcnt || !l_prefilt )
				$SRCHMSG(SEARCH$_LONGREC, a_fspec, l_rab->recnum, l_reclen);
			else	$SRCHMSG(SEARCH$_LONGRECU, a_fspec, l_reclen);
			}

		a_ctx->stat.recsread++;
		a_ctx->stat.chars	+= l_reclen + 1;

		if ( 1 & s_srch$match(a_ctx, l_reclen, l_rec) )
			{
			l_matched++;
			a_ctx->stat.recsmatched++;

			/*
			**  A binary file is reported at its first match and is
			**  read no further: one match is all the line says.
			*/
			if ( l_binary )
				{
				if ( a_ctx->format == SEARCH$K_FMT_GREP )
					$SRCHPUT(a_ctx->out, "Binary file !AZ matches\n", a_fspec);
				else	$SRCHPUT(a_ctx->out, "%SEARCH-I-BINMATCH, binary file !AZ matches\n", a_fspec);

				break;
				}

			/* The name of the file is announced before its first match only */
			if ( !l_heading )
				{
				l_heading	= 1;
				s_srch$heading(a_ctx, a_fspec, l_tim);
				}

			/*
			**  The records preceding the match, as far back as the ring
			**  reaches, but never one that has been displayed already.
			*/
			l_first	= (l_rab->recnum > a_ctx->winbef) ? (l_rab->recnum - a_ctx->winbef) : 1;

			if ( l_first <= l_lastput )
				l_first	= l_lastput + 1;

			/* A gap between two runs of records is marked by an empty line, by "--" under /FORMAT=GREP */
			if ( (a_ctx->winbef || a_ctx->winaft) && l_lastput && (l_first < l_rab->recnum)
				&& (l_first > (l_lastput + 1)) )
				{
				if ( a_ctx->format == SEARCH$K_FMT_GREP )
					srch$put_raw(a_ctx->out, 3, "--\n");
				else	srch$put_raw(a_ctx->out, 1, "\n");
				}

			for ( unsigned i = 0; i < l_ringcnt; i++ )
				{
				/* The ring is walked oldest first: the slot past the newest one */
				SEARCH$RING *l_ent = &l_ring [(l_slot + a_ctx->winbef - l_ringcnt + i) % a_ctx->winbef];

				if ( (l_ent->recnum < l_first) || (l_ent->recnum >= l_rab->recnum) )
					continue;

				s_srch$putrec(a_ctx, a_fspec, '-', l_ent->recnum, l_ent->len, l_ent->buf,
					(l_mark && (1 & s_srch$markbuf(&l_mark, &l_markmax, l_ent->len))) ? l_mark : NULL);
				l_lastput	= l_ent->recnum;
				}

			if ( l_rab->recnum > l_lastput )
				{
				s_srch$putrec(a_ctx, a_fspec, ':', l_rab->recnum, l_reclen, l_rec,
					(l_mark && (1 & s_srch$markbuf(&l_mark, &l_markmax, l_reclen))) ? l_mark : NULL);
				l_lastput	= l_rab->recnum;
				}

			l_pending	= a_ctx->winaft;
			}
		else if ( l_pending )
			{
			/* A record of the window carries no match, so it is never marked */
			s_srch$putrec(a_ctx, a_fspec, '-', l_rab->recnum, l_reclen, l_rec, NULL);

			l_lastput	= l_rab->recnum;
			l_pending--;
			}

		/* The reader has had enough: the file, and the walk, end here */
		if ( a_ctx->out->quit )
			break;

		/* Whatever became of it, the record enters the ring for the next match */
		if ( a_ctx->winbef )
			{
			SEARCH$RING *l_ent = &l_ring [l_slot];

			if ( l_ent->max < (l_reclen + 1) )
				{
				char *	l_new = realloc(l_ent->buf, l_reclen + 1);

				if ( l_new )
					{
					l_ent->buf	= l_new;
					l_ent->max	= l_reclen + 1;
					}
				}

			if ( l_ent->max >= (l_reclen + 1) )
				{
				memcpy(l_ent->buf, l_rec, l_reclen);

				l_ent->len	= l_reclen;
				l_ent->recnum	= l_rab->recnum;

				l_slot		= (l_slot + 1) % a_ctx->winbef;

				if ( l_ringcnt < a_ctx->winbef )
					l_ringcnt++;
				}
			}
		}

	if ( a_ctx->log && !a_ctx->out->quit )
		$SRCHMSG(SEARCH$_SEARCHED, a_fspec, l_rab->recnum, l_matched);

	a_ctx->stat.files++;

	for ( unsigned i = 0; i < a_ctx->winbef; i++ )
		free(l_ring [i].buf);

	if ( l_rab->mapped )
		munmap(l_rab->buf, l_rab->mapsz);
	else if ( l_rab->grown )
		free(l_rab->buf);

	/* Not L_FD: under /CONTINUE the stream may have opened the file again - see S_SRCH$FOLLOW */
	close(l_rab->fd);

	free(l_ring);
	free(l_mark);
	free(l_rab);

	if ( a_ctx->out->quit )
		return	STS$K_FATAL;

	return	l_matched ? STS$K_SUCCESS : STS$K_WARN;
}
