#define	__MODULE__	"SRCHMAT"
#define	__IDENT__	"X01-12"
#define	__REV__		"1.12.0"

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

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<sys/mman.h>

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
static	int	s_srch$instr	(
		int		a_exact,
		unsigned	a_reclen,
	const	char *		a_rec,
	const	SEARCH$STR *	a_str
			)
{
const char *	l_cand, *l_up = NULL, *l_lo = NULL, *l_ptr, *l_first, *l_last;
unsigned char	l_chr, l_cup, l_clo;
int		l_upend = 0, l_loend = 0;

	if ( !a_str->len || (a_str->len > a_reclen) )
		return	STS$K_WARN;

	/*
	**  The case is significant: the CRTL does the whole of it, and does
	**  it a machine word at a time.
	*/
	if ( a_exact )
		return	memmem(a_rec, a_reclen, a_str->str, a_str->len) ? STS$K_SUCCESS : STS$K_WARN;

	/*
	**  Case-blind.  Rather than comparing at every position of the
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

	l_first	= a_rec + a_str->anchor;		/* Earliest the anchor may stand	*/
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
				return	STS$K_SUCCESS;

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
uint64_t	l_word, l_hits, l_cnt = 0;
unsigned	l_pos = 0;

	/*
	**  A word at a time, by the classic trick: exclusive-or with a word
	**  of terminators turns every terminator into a zero byte, and
	**  (w - ONES) & ~w & HIGHS then carries a set high bit in exactly
	**  the bytes that were zero.  Counting those bits is one
	**  instruction.
	**
	**  MEMCHR is faster still at finding the FIRST terminator, but it
	**  would have to be called once per record; here the whole region
	**  is counted in one pass, which is the point of the exercise - a
	**  region that carries none of the search strings costs nothing but
	**  this.
	*/
	for ( ; (l_pos + sizeof(uint64_t)) <= a_len; l_pos += sizeof(uint64_t) )
		{
		memcpy(&l_word, a_blk + l_pos, sizeof(l_word));	/* The alignment is none of our business	*/

		l_word	^= UINT64_C(0x0A0A0A0A0A0A0A0A);
		l_hits	= (l_word - UINT64_C(0x0101010101010101)) & ~l_word & UINT64_C(0x8080808080808080);

		l_cnt	+= (uint64_t) __builtin_popcountll(l_hits);
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
			if ( !l_rest )
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
SEARCH$STR *	l_str;
unsigned char	l_chr;
unsigned	l_cnt = 0;

	memset(a_mark, 0, a_reclen);

	for ( unsigned s = 0; s < a_ctx->strcnt; s++ )
		{
		l_str	= &a_ctx->strs [s];

		if ( !l_str->len || (l_str->len > a_reclen) )
			continue;

		for ( unsigned i = 0; i <= (a_reclen - l_str->len); i++ )
			{
			unsigned j;

			for ( j = 0; j < l_str->len; j++ )
				{
				if ( a_ctx->exact )
					{
					if ( a_rec [i + j] != l_str->str [j] )
						break;

					continue;
					}

				l_chr	= (unsigned char) a_rec [i + j];

				if ( (l_chr >= 'a') && (l_chr <= 'z') )
					l_chr	-= ('a' - 'A');

				if ( l_chr != (unsigned char) l_str->ups [j] )
					break;
				}

			if ( j != l_str->len )
				continue;

			memset(a_mark + i, 1, l_str->len);
			l_cnt++;
			}
		}

	return	l_cnt ? STS$K_SUCCESS : STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Display one record: the number of it when /NUMBERS is in force, the
**	record itself, a line terminator.
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
		uint64_t	a_recnum,
		unsigned	a_reclen,
	const	char *		a_rec,
		char *		a_mark
			)
{
const char *	l_seq;
unsigned	l_run;
int		l_status;

	if ( a_ctx->numbers )
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
**	Search one file and display every record that satisfies the match,
**	together with the window of records asked for by /WINDOW.
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
unsigned	l_reclen = 0, l_slot = 0, l_pending = 0, l_ringcnt = 0, l_namlen, l_pad;
const char *	l_cleanend = NULL;
uint64_t	l_seen = 0;
int		l_status, l_fd, l_heading = 0, l_longrec = 0;
int		l_prefilt, l_needcnt, l_binary = 0;
unsigned	l_markmax = 0;

	/* The reader has answered Q at the pager - nothing more is to be displayed */
	if ( a_ctx->out->quit )
		return	STS$K_FATAL;

	if ( 0 > (l_fd = open(a_fspec, O_RDONLY)) )
		return	$SRCHMSG(SEARCH$_OPENIN, a_fspec, errno, strerror(errno));

	/*
	**  The read-ahead buffer and the record are far too large for the
	**  stack of a routine that may be reached through a deep /RECURSIVE
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
		if ( S_ISREG(l_st.st_mode) && (l_st.st_size >= SEARCH$K_MAPMIN) )
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

	for ( ;; )
		{
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
			break;

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
				$SRCHPUT(a_ctx->out, "%SEARCH-I-BINMATCH, binary file !AZ matches\n", a_fspec);

				break;
				}

			/* The name of the file is announced before its first match only */
			if ( a_ctx->heading && !l_heading )
				{
				l_heading	= 1;

				/*
				**  The time stands in a column of its own, so that a
				**  run over several files reads as a listing; a
				**  specification too long for the column pushes it
				**  right rather than being cut to fit.  The time is a
				**  quadword and goes to !%D BY VALUE, per STARLET
				**  X.01-09.
				*/
				l_namlen	= (unsigned) strlen(a_fspec);

				l_pad		= (l_namlen < SEARCH$K_HDRNAME)
							? (SEARCH$K_HDRNAME - l_namlen) : 2;

				$SRCHPUT(a_ctx->out, "\n!#**\n!AZ", SEARCH$K_HDRWIDTH, a_fspec);

				$SRCHPUT(a_ctx->out, "!#* !%D\n\n", l_pad, l_tim);
				}

			/*
			**  The records preceding the match, as far back as the ring
			**  reaches, but never one that has been displayed already.
			*/
			l_first	= (l_rab->recnum > a_ctx->winbef) ? (l_rab->recnum - a_ctx->winbef) : 1;

			if ( l_first <= l_lastput )
				l_first	= l_lastput + 1;

			/* A gap between two runs of records is marked by an empty line */
			if ( (a_ctx->winbef || a_ctx->winaft) && l_lastput && (l_first < l_rab->recnum)
				&& (l_first > (l_lastput + 1)) )
				srch$put_raw(a_ctx->out, 1, "\n");

			for ( unsigned i = 0; i < l_ringcnt; i++ )
				{
				/* The ring is walked oldest first: the slot past the newest one */
				SEARCH$RING *l_ent = &l_ring [(l_slot + a_ctx->winbef - l_ringcnt + i) % a_ctx->winbef];

				if ( (l_ent->recnum < l_first) || (l_ent->recnum >= l_rab->recnum) )
					continue;

				s_srch$putrec(a_ctx, l_ent->recnum, l_ent->len, l_ent->buf,
					(l_mark && (1 & s_srch$markbuf(&l_mark, &l_markmax, l_ent->len))) ? l_mark : NULL);
				l_lastput	= l_ent->recnum;
				}

			if ( l_rab->recnum > l_lastput )
				{
				s_srch$putrec(a_ctx, l_rab->recnum, l_reclen, l_rec,
					(l_mark && (1 & s_srch$markbuf(&l_mark, &l_markmax, l_reclen))) ? l_mark : NULL);
				l_lastput	= l_rab->recnum;
				}

			l_pending	= a_ctx->winaft;
			}
		else if ( l_pending )
			{
			/* A record of the window carries no match, so it is never marked */
			s_srch$putrec(a_ctx, l_rab->recnum, l_reclen, l_rec, NULL);

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

	free(l_ring);
	free(l_mark);
	free(l_rab);
	close(l_fd);

	if ( a_ctx->out->quit )
		return	STS$K_FATAL;

	return	l_matched ? STS$K_SUCCESS : STS$K_WARN;
}
