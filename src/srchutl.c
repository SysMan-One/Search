#define	__MODULE__	"SRCHUTL"
#define	__IDENT__	"X01-11ECO1"
#define	__REV__		"1.11.1"

/*
**++
**
**  FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
**
**  MODULE:	srchutl.c
**
**  ABSTRACT:	Service routines shared by the modules of the SEARCH
**		facility: the message catalogue and the signalling
**		routine, the buffered output engine, upper-casing and the
**		conversion of a time string.
**
**  DESCRIPTION: Diagnostics that are of interest to the developer only go
**		to $LOG with an explicit STS$K_* severity; diagnostics that
**		are of interest to the user are signalled by SRCH$PUTMSG in
**		the OpenVMS format, e.g.
**
**		    %SEARCH-W-NOFILES, no file matches *.LIS
**
**		Every record the utility displays is built by the FAO
**		engine of the STARLET module and is accumulated in the
**		output context; nothing but SRCH$FLUSH and the signalling
**		routine ever calls WRITE(2).
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:   1-AUG-2026
**
**  MODIFICATION HISTORY:
**
**	X01-11ECO1	11-NOV-2026	CLD
**		SEARCH$_NORECURSE in the catalogue: /RECURSIVE is gone, a
**		"..." directory of the file specification walks a tree.
**
**	X01-11		21-OCT-2026	RRL
**		SEARCH$_TRUNCATED and SEARCH$_REOPENED, for /CONTINUE.
**
**	X01-10		 2-OCT-2026	RRL
**		A context gathering into the heap offers what it holds to its
**		SPILL routine once that is SEARCH$K_SPILL.  SRCH$FAIL: a condition signalled below severity E that is to
**		count as an error for the completion code all the same - a
**		file specification that selected no file.
**
**	X01-09		 2-OCT-2026	RRL
**		For /THREADS: an output context over no descriptor gathers into
**		the heap (SRCH$OUT_MEM, SRCH$OUT_TAKE, S_SRCH$EMIT), and the
**		message subsystem is entered under a lock (SRCH$MSGLOCK,
**		SRCH$MSGUNLOCK), its reentrancy not being known.
**
**	X01-08		 2-OCT-2026	RRL
**		SRCH$NOTE and SRCH$ERRORS: every condition signalled by
**		$SRCHMSG passes SRCH$NOTE, which counts those of severity E
**		and F, so that the completion code can tell an error (2) from
**		a run that found nothing (1).  The count is atomic, ready for a
**		search run by several threads.
**
**	X01-07		14-AUG-2026	RRL
**		SEARCH$_SEARCHING and SEARCH$_SELECTING in the catalogue.
**
**	X01-06		14-AUG-2026	RRL
**		The catalogue is registered with the message subsystem of
**		the STARLET library and the messages are signalled by
**		UTIL$PUTMSG_FAO: the envelope, the lookup and the severity
**		were ours to keep and are the library's since V.01-24.
**
**	X01-05		 1-AUG-2026	RRL
**		SEARCH$_NOTOPIC in the catalogue.
**
**	X01-04		 1-AUG-2026	RRL
**		SEARCH$_SELTIME in the catalogue: the value of a time
**		qualifier and the absolute time it stands for.
**
**	X01-03		 1-AUG-2026	RRL
**		The output engine pages the display, /PAGE, and carries the
**		escape sequences of the highlighting, /HIGHLIGHT.
**
**	X01-02		 1-AUG-2026	RRL
**		Initial version.
**
**--
*/

#include	<stdarg.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<time.h>
#include	<unistd.h>
#include	<sys/ioctl.h>
#include	<fcntl.h>
#include	<pthread.h>

#include	"srchdef.h"

/*
**  The message catalogue of the facility, in the form UTIL$INIMSG takes:
**  the condition value and the text, the identification of the message
**  standing at the head of the text.  The envelope of the diagnostic -
**  the facility, the severity - is put on by UTIL$PUTMSG_FAO out of the
**  facility name of the descriptor and the severity carried by the
**  condition value being signalled.
**
**  The array is NOT const: UTIL$INIMSG sorts it in place, so that the
**  lookup of a message can be a binary search.
*/
/*
**  The parameters are named A_ - and not STS, TEXT - on purpose: a
**  designated initializer is macro-expanded like anything else, so a
**  parameter spelled like a member of EMSG_RECORD would turn .STS into
**  the argument itself.
*/
#define	$SRCHREC(a_sts, a_txt)	{ .sts = (int) (a_sts), .textl = (unsigned char) (sizeof(a_txt) - 1), .text = a_txt }

static	EMSG_RECORD	s_msgtab [] = {
	$SRCHREC(SEARCH$_NORMAL,	"NORMAL, normal successful completion"),
	$SRCHREC(SEARCH$_NOMATCHES,	"NOMATCHES, no strings matched"),
	$SRCHREC(SEARCH$_NOFILES,	"NOFILES, no file matches !AZ"),
	$SRCHREC(SEARCH$_NOSTRING,	"NOSTRING, no search string has been given"),
	$SRCHREC(SEARCH$_OPENIN,	"OPENIN, error opening !AZ as input, errno=!SL (!AZ)"),
	$SRCHREC(SEARCH$_OPENOUT,	"OPENOUT, error creating !AZ as output, errno=!SL (!AZ)"),
	$SRCHREC(SEARCH$_READERR,	"READERR, error reading !AZ, errno=!SL (!AZ)"),
	$SRCHREC(SEARCH$_WRITERR,	"WRITERR, error writing !AZ, errno=!SL (!AZ)"),
	$SRCHREC(SEARCH$_OPENDIR,	"OPENDIR, error reading the directory !AZ, errno=!SL (!AZ)"),
	$SRCHREC(SEARCH$_NOMEM,	"NOMEM, cannot allocate !UL bytes, errno=!SL (!AZ)"),
	$SRCHREC(SEARCH$_IVQUAL,	"IVQUAL, illegal value !AZ for the qualifier /!AZ"),
	$SRCHREC(SEARCH$_IVTIME,	"IVTIME, illformed time value !AZ for the qualifier /!AZ"),
	$SRCHREC(SEARCH$_CONFQUAL,	"CONFQUAL, /!AZ and /!AZ cannot be given together"),
	$SRCHREC(SEARCH$_SEARCHED,	"SEARCHED, !AZ, !UL record!%S searched, !UL matched"),
	$SRCHREC(SEARCH$_SELTIME,	"SELTIME, /!AZ=!AZ is !%D"),
	$SRCHREC(SEARCH$_LONGREC,	"LONGREC, !AZ:!UL, record truncated to !UL bytes"),
	$SRCHREC(SEARCH$_TOODEEP,	"TOODEEP, !AZ, nesting exceeds !UL levels, not descended"),
	$SRCHREC(SEARCH$_TOOMANY,	"TOOMANY, too many !AZ, only the first !UL are used"),
	$SRCHREC(SEARCH$_NOTOPIC,	"NOTOPIC, sorry, no documentation on !AZ"),
	$SRCHREC(SEARCH$_LONGRECU,	"LONGREC, !AZ, a record has been truncated to !UL bytes"),
	$SRCHREC(SEARCH$_SEARCHING,	"SEARCHING, for !AZ!AZ, the case is !AZ"),
	$SRCHREC(SEARCH$_SELECTING,	"SELECTING, in !AZ!AZ"),
	$SRCHREC(SEARCH$_TRUNCATED,	"TRUNCATED, !AZ has been cut short, it is read from its beginning again"),
	$SRCHREC(SEARCH$_REOPENED,	"REOPENED, !AZ is another file now, it is read from its beginning"),
	$SRCHREC(SEARCH$_NORECURSE,	"NORECURSE, !AZ: /RECURSIVE is gone, a tree is walked by a \"...\" directory of the file specification, e.g. '.../*.c'")
	};

static	EMSG_RECORD_DESC	s_msgdsc = {
	.msgnr		= (unsigned) $ARRSZ(s_msgtab),
	.facno		= SEARCH$K_FACNO,
	.msgrec		= s_msgtab,
	.facname	= __FAC__
	};

/*
**  The prompt of the pager, and the escape sequences of the styles of
**  the highlighting, indexed by SEARCH$K_HL_*.  Nothing but ECMA-48 is
**  used: every terminal emulator in use understands these, and a
**  terminal that does not is spared them - the highlighting is granted
**  only when the output really is a terminal, unless it has been asked
**  for outright.
*/
static	const char	s_pagemsg [] = { "                Press RETURN to continue (Q to quit) ... " };

static	const char *	s_hlseq [SEARCH$K_HL_MAX] = {
	"\033[1m",				/* SEARCH$K_HL_BOLD		*/
	"\033[7m",				/* SEARCH$K_HL_REVERSE		*/
	"\033[4m",				/* SEARCH$K_HL_UNDERLINE	*/
	"\033[31m",				/* SEARCH$K_HL_RED		*/
	"\033[32m",				/* SEARCH$K_HL_GREEN		*/
	"\033[33m"				/* SEARCH$K_HL_YELLOW		*/
	};



/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Write a memory block to a file descriptor, restarting the transfer
**	on a partial write and on EINTR.  This is the only place in the
**	whole facility where WRITE(2) is called.
**
**  FORMAL PARAMETERS:
**
**	a_chan		Descriptor to write to
**	a_len		Number of bytes to be written
**	a_buf		Address of the data
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the whole block has been written;
**	STS$K_ERROR	- WRITE(2) failed, ERRNO is set.
**--
*/
static	int	s_srch$write	(
		int		a_chan,
		unsigned	a_len,
	const	char *		a_buf
			)
{
ssize_t		l_wrt;
unsigned	l_rest = a_len;
const char *	l_ptr = a_buf;

	while ( l_rest )
		{
		l_wrt = write(a_chan, l_ptr, l_rest);

		if ( l_wrt > 0 )
			{
			l_ptr	+= l_wrt;
			l_rest	-= (unsigned) l_wrt;
			continue;
			}

		/* An interrupted transfer is retried, anything else is fatal for the record */
		if ( (l_wrt < 0) && (errno == EINTR) )
			continue;

		return	$LOG(STS$K_ERROR, "write(%d, %u), errno=%d (%s)", a_chan, l_rest, errno, strerror(errno));
		}

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Register the message catalogue of the facility with the message
**	subsystem of the STARLET library, and send the diagnostics to the
**	standard error stream.
**
**	The stream matters: the subsystem writes to the standard output by
**	default, where the records the utility displays are going, so a run
**	piped into another utility - or one writing to the file of /OUTPUT -
**	would have its result contaminated by its own diagnostics.
**
**	Called once, before anything can be signalled.
**
**  FORMAL PARAMETERS:
**	None.
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Sorts the module-static S_MSGTAB and links S_MSGDSC into the list of
**	the subsystem; sets the descriptor every logging routine writes to.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the catalogue has been registered;
**	STS$K_WARN	- a catalogue of this facility is registered already.
**--
*/
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Put out a block of an output context: to its descriptor, or - for a
**	context made by SRCH$OUT_MEM, whose descriptor is negative - to the
**	heap area it gathers in, which doubles as it fills.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**	a_len		Length of the block
**	a_buf		The block
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the descriptor, or grows MEM of the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the block has gone out;
**	STS$K_ERROR	- WRITE(2) failed or the heap refused, logged.
**--
*/
static	int	s_srch$emit	(
		SEARCH$OUT *	a_out,
		unsigned	a_len,
	const	char *		a_buf
			)
{
	if ( a_out->fd >= 0 )
		return	s_srch$write(a_out->fd, a_len, a_buf);

	if ( (a_out->memlen + a_len) > a_out->memmax )
		{
		size_t	l_max = a_out->memmax ? a_out->memmax : SEARCH$K_SZ_OUT;
		char *	l_new;

		while ( l_max < (a_out->memlen + a_len) )
			l_max	*= 2;

		if ( !(l_new = realloc(a_out->mem, l_max)) )
			return	$LOG(STS$K_ERROR, "realloc(%zu), errno=%d (%s)", l_max, errno, strerror(errno));

		a_out->mem	= l_new;
		a_out->memmax	= l_max;
		}

	memcpy(a_out->mem + a_out->memlen, a_buf, a_len);
	a_out->memlen	+= a_len;

	/* Enough gathered: the owner may put it out now - see SRCHTHR.C */
	if ( a_out->spill && (a_out->memlen >= SEARCH$K_SPILL) && (1 & a_out->spill(a_out, a_out->spillarg)) )
		a_out->memlen	= 0;

	return	STS$K_SUCCESS;
}


/*
**  The lock the message subsystem is entered under.  The subsystem of the
**  STARLET library is not known to be reentrant, and under /THREADS a
**  message may be signalled by several threads at once.
*/
static	pthread_mutex_t	s_msglock = PTHREAD_MUTEX_INITIALIZER;

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Take and release the lock of the message subsystem - see $SRCHMSG.
**--
*/
void	srch$msglock	(void)
{
	pthread_mutex_lock(&s_msglock);
}

void	srch$msgunlock	(void)
{
	pthread_mutex_unlock(&s_msglock);
}


int	srch$inimsg	(void)
{
	__util$setlogfd(STDERR_FILENO);

	return	(int) __util$inimsg(&s_msgdsc);
}


/*
**  The number of the conditions of severity E and F signalled so far.
*/
static	int	s_errors;

/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Note a condition that has just been signalled by $SRCHMSG: one of
**	severity E or F is counted.  The completion code of the image is
**	decided by the count - an error is an error whether or not
**	something has also matched, as with GREP.
**
**  FORMAL PARAMETERS:
**
**	a_sts		The condition value, as $PUTMSG_FAO has returned it
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_ERRORS, advanced atomically.
**
**  RETURN VALUE:
**	A_STS, unchanged, so that $SRCHMSG still yields the condition.
**--
*/
int	srch$note	(
		int	a_sts
			)
{
	if ( ((a_sts & 7) == STS$K_ERROR) || ((a_sts & 7) == STS$K_FATAL) )
		__atomic_add_fetch(&s_errors, 1, __ATOMIC_RELAXED);

	return	a_sts;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Count an error that has been signalled at a lower severity - a
**	file specification that selected no file is %SEARCH-W-NOFILES, as
**	on OpenVMS, but the completion code is 2, as with GREP, whose shell
**	would have handed it the specification as a name that is not there.
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_ERRORS, advanced atomically.
**
**  RETURN VALUE:
**	None.
**--
*/
void	srch$fail	(void)
{
	__atomic_add_fetch(&s_errors, 1, __ATOMIC_RELAXED);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The number of the errors noted by SRCH$NOTE so far.
**
**  RETURN VALUE:
**	The count, zero when nothing has gone wrong.
**--
*/
int	srch$errors	(void)
{
	return	__atomic_load_n(&s_errors, __ATOMIC_RELAXED);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Initialize an output context over a file descriptor, asking the
**	driver for the geometry of the page when the descriptor is a
**	terminal.  Paging itself is turned on separately, by SRCH$OUT_PAGE.
**
**  FORMAL PARAMETERS:
**
**	a_out		The context to be initialized
**	a_fd		Descriptor the buffer is to be flushed to
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Queries the terminal driver.
**
**  RETURN VALUE:
**	STS$K_SUCCESS always.
**--
*/
int	srch$out_init	(
		SEARCH$OUT *	a_out,
		int		a_fd
			)
{
struct winsize	l_ws = {0};

	memset(a_out, 0, sizeof(SEARCH$OUT));

	a_out->fd	= a_fd;
	a_out->ttyin	= -1;
	a_out->rows	= SEARCH$K_DEFROWS;
	a_out->cols	= SEARCH$K_DEFCOLS;
	a_out->tty	= isatty(a_fd) ? 1 : 0;

	/*
	**  The geometry is asked of the driver, not of the environment: a
	**  window resized after the shell was started has the correct size
	**  here and a stale one in COLUMNS.
	*/
	if ( a_out->tty && !ioctl(a_fd, TIOCGWINSZ, &l_ws) )
		{
		if ( l_ws.ws_row >= SEARCH$K_MINROWS )
			a_out->rows	= l_ws.ws_row;

		if ( l_ws.ws_col > 1 )
			a_out->cols	= l_ws.ws_col;
		}

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Turn the paging of the display on or off.  Paging is granted only
**	when the output really is a terminal and an answer can be read from
**	one, so a redirected run - or one whose input has been redirected
**	as well - never waits for a keystroke whichever form of the
**	qualifier has been given.
**
**	The answer is read from the standard input when that is a terminal,
**	and from /dev/tty otherwise: the output of the utility is routinely
**	piped into another one, and the input then still belongs to the
**	user.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**	a_page		Non-zero - paging has been asked for
**
**  IMPLICIT INPUTS/OUTPUTS:
**	May open /dev/tty.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- paging is in force;
**	STS$K_WARN	- it is not, and will not be.
**--
*/
int	srch$out_page	(
		SEARCH$OUT *	a_out,
		int		a_page
			)
{
	a_out->page	= 0;

	if ( !a_page || !a_out->tty )
		return	STS$K_WARN;

	if ( isatty(STDIN_FILENO) )
		a_out->ttyin	= STDIN_FILENO;
	else if ( 0 > (a_out->ttyin = open("/dev/tty", O_RDONLY)) )
		return	STS$K_WARN;

	a_out->page	= 1;
	a_out->lineno	= 0;
	a_out->col	= 0;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Close an output context: flush what is buffered and release the
**	terminal the pager has opened for itself.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the descriptor of the context.
**
**  RETURN VALUE:
**	Whatever the flush returned.
**--
*/
/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Initialize an output context that gathers into the heap rather than
**	writing to a descriptor: a file searched by a thread of the pool is
**	displayed whole, in its turn, out of what its context has gathered.
**	Such a context is never a terminal and is never paged.
**
**  FORMAL PARAMETERS:
**
**	a_out		The context to be initialized
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS always.
**--
*/
int	srch$out_mem	(
		SEARCH$OUT *	a_out
			)
{
	memset(a_out, 0, sizeof(SEARCH$OUT));

	a_out->fd	= -1;
	a_out->ttyin	= -1;
	a_out->rows	= SEARCH$K_DEFROWS;
	a_out->cols	= SEARCH$K_DEFCOLS;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Hand over what an output context made by SRCH$OUT_MEM has gathered:
**	the buffer is flushed into the heap area first, and the area passes
**	to the caller, who frees it; the context is left empty.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**	a_mem		Receives the area, NULL when nothing was gathered
**	a_len		Receives its length, bytes
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS, or the status of the last flush.
**--
*/
int	srch$out_take	(
		SEARCH$OUT *	a_out,
		char **		a_mem,
		size_t *	a_len
			)
{
int		l_status;

	l_status	= srch$flush(a_out);

	*a_mem		= a_out->mem;
	*a_len		= a_out->memlen;

	a_out->mem	= NULL;
	a_out->memlen	= a_out->memmax = 0;

	return	l_status;
}


int	srch$out_final	(
		SEARCH$OUT *	a_out
			)
{
int		l_status;

	l_status	= srch$flush(a_out);

	if ( (a_out->ttyin >= 0) && (a_out->ttyin != STDIN_FILENO) )
		close(a_out->ttyin);

	a_out->ttyin	= -1;
	a_out->page	= 0;

	return	l_status;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Return the escape sequence of a highlighting style.
**
**  FORMAL PARAMETERS:
**
**	a_style		SEARCH$K_HL_*
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the module-static S_HLSEQ.
**
**  RETURN VALUE:
**	The sequence, ASCIZ; the one of SEARCH$K_HL_BOLD for a style out of
**	range, so that a caller never has to check.
**--
*/
const char *	srch$hlseq	(
		int		a_style
			)
{
	if ( (a_style < 0) || (a_style >= SEARCH$K_HL_MAX) )
		return	s_hlseq [SEARCH$K_HL_BOLD];

	return	s_hlseq [a_style];
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Hand the buffered output over to the file descriptor and empty the
**	buffer.  An empty buffer is not written at all, so the routine is
**	safe to call at any point.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the descriptor of the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the buffer has been written;
**	STS$K_ERROR	- WRITE(2) failed.
**--
*/
int	srch$flush	(
		SEARCH$OUT *	a_out
			)
{
int		l_status;

	if ( !a_out->len )
		return	STS$K_SUCCESS;

	l_status	= s_srch$emit(a_out, a_out->len, a_out->buf);
	a_out->len	= 0;

	return	l_status;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Stop at the end of a page: flush what has been accumulated, display
**	the prompt, read the answer and erase the prompt again.
**
**	An answer beginning with Q, and an end of file - the ^D of the
**	terminal - abandon the display: the flag is set in the context and
**	every later record is discarded, so a run over a large tree is
**	stopped by the reader rather than by a signal.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Writes to the descriptor of the context, reads the terminal.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the display goes on;
**	STS$K_WARN	- it has been abandoned.
**--
*/
static	int	s_srch$pager	(
		SEARCH$OUT *	a_out
			)
{
char		l_ans [SEARCH$K_SZ_ANS] = {0};
ssize_t		l_red;
unsigned	l_len = 0;

	srch$flush(a_out);

	s_srch$write(a_out->fd, (unsigned) sizeof(s_pagemsg) - 1, s_pagemsg);

	/* One line of the answer, of which only the first character matters */
	for ( ;; )
		{
		l_red	= read(a_out->ttyin, l_ans + l_len, 1);

		if ( (l_red < 0) && (errno == EINTR) )
			continue;

		if ( l_red <= 0 )
			{
			a_out->quit	= 1;		/* End of file - nothing more is wanted	*/
			break;
			}

		if ( l_ans [l_len] == '\n' )
			break;

		if ( l_len < (sizeof(l_ans) - 1) )
			l_len++;
		}

	/*
	**  Erase the prompt: a carriage return alone only moves the cursor,
	**  the text stays on the screen until something overwrites it - and
	**  the record that follows is usually shorter.
	*/
	s_srch$write(a_out->fd, 1, "\r");

	for ( unsigned i = 0; i < (unsigned) sizeof(s_pagemsg) - 1; i++ )
		s_srch$write(a_out->fd, 1, " ");

	s_srch$write(a_out->fd, 1, "\r");

	a_out->lineno	= 0;
	a_out->col	= 0;

	if ( l_len && ((l_ans[0] == 'Q') || (l_ans[0] == 'q')) )
		a_out->quit	= 1;

	return	a_out->quit ? STS$K_WARN : STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Append one character to the buffer, flushing it when it is full.
**	A helper of the paged path of SRCH$PUT_RAW.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**	a_chr		The character
**
**  IMPLICIT INPUTS/OUTPUTS:
**	May write to the descriptor of the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the character has been accepted;
**	STS$K_ERROR	- a flush has failed.
**--
*/
static	int	s_srch$putchr	(
		SEARCH$OUT *	a_out,
		char		a_chr
			)
{
int		l_status;

	if ( a_out->len == sizeof(a_out->buf) )
		if ( !(1 & (l_status = srch$flush(a_out))) )
			return	l_status;

	a_out->buf [a_out->len++]	= a_chr;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Put a block of characters into the output context, flushing it as
**	often as the block does not fit.  A block longer than the buffer is
**	written straight through, without being copied at all.
**
**	When paging is in force the block is looked at character by
**	character instead: a line of the display is a record up to a line
**	terminator OR up to the width of the terminal, since a record that
**	does not fit takes as many lines as it wraps into, and the page is
**	as tall as the terminal reported.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**	a_len		Number of bytes to be put
**	a_buf		Address of the data
**
**  IMPLICIT INPUTS/OUTPUTS:
**	May write to the descriptor of the context, may read the terminal.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the block has been accepted;
**	STS$K_WARN	- the display has been abandoned at the prompt;
**	STS$K_ERROR	- a flush has failed.
**--
*/
int	srch$put_raw	(
		SEARCH$OUT *	a_out,
		unsigned	a_len,
	const	char *		a_buf
			)
{
int		l_status;

	if ( a_out->quit )
		return	STS$K_WARN;

	if ( !a_len )
		return	STS$K_SUCCESS;

	if ( a_out->page )
		{
		for ( unsigned i = 0; i < a_len; i++ )
			{
			if ( !(1 & (l_status = s_srch$putchr(a_out, a_buf [i]))) )
				return	l_status;

			if ( a_buf [i] == '\n' )
				a_out->col	= 0;
			else if ( ++a_out->col < a_out->cols )
				continue;
			else	a_out->col	= 0;

			/* A line of the display has been completed - one page is one less */
			if ( (++a_out->lineno + 1) < a_out->rows )
				continue;

			if ( !(1 & s_srch$pager(a_out)) )
				return	STS$K_WARN;
			}

		return	STS$K_SUCCESS;
		}

	/* The buffer is emptied before a block that would not fit is looked at */
	if ( (a_out->len + a_len) > sizeof(a_out->buf) )
		if ( !(1 & (l_status = srch$flush(a_out))) )
			return	l_status;

	/* An over-long block bypasses the buffer altogether */
	if ( a_len > sizeof(a_out->buf) )
		return	s_srch$emit(a_out, a_len, a_buf);

	memcpy(a_out->buf + a_out->len, a_buf, a_len);
	a_out->len	+= a_len;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Put a block of characters into the output context WITHOUT counting
**	it: an escape sequence of the highlighting occupies no column of
**	the screen, and a page counted as if it did would be a short one.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**	a_len		Number of bytes to be put
**	a_buf		Address of the data
**
**  IMPLICIT INPUTS/OUTPUTS:
**	May write to the descriptor of the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the block has been accepted;
**	STS$K_WARN	- the display has been abandoned already;
**	STS$K_ERROR	- a flush has failed.
**--
*/
int	srch$put_ctl	(
		SEARCH$OUT *	a_out,
		unsigned	a_len,
	const	char *		a_buf
			)
{
int		l_status;

	if ( a_out->quit )
		return	STS$K_WARN;

	for ( unsigned i = 0; i < a_len; i++ )
		if ( !(1 & (l_status = s_srch$putchr(a_out, a_buf [i]))) )
			return	l_status;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Format a record by the FAO engine and put it into the output
**	context.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context
**	a_fmt		FAO control string
**	a_prmcnt	Number of the FAO parameters that follow, 0 .. SEARCH$K_MAXPRM
**	...		The FAO parameters, each cast to FAO_PRM_T by the caller
**
**  IMPLICIT INPUTS/OUTPUTS:
**	May write to the descriptor of the context.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the record has been accepted;
**	STS$K_ERROR	- a flush has failed.
**--
*/
int	srch$put	(
		SEARCH$OUT *	a_out,
	const	char *		a_fmt,
		int		a_prmcnt,
				...
			)
{
va_list		l_ap;
fao_prm_t	l_prm [SEARCH$K_MAXPRM];
char		l_rec [SEARCH$K_SZ_MSG];
fao_desc_t	l_recdsc = $FAO_DESC(l_rec);
unsigned short	l_reclen = 0;
int		l_cnt = a_prmcnt;

	if ( l_cnt > SEARCH$K_MAXPRM )
		l_cnt = SEARCH$K_MAXPRM;

	if ( l_cnt < 0 )
		l_cnt = 0;

	va_start(l_ap, a_prmcnt);

	for ( int i = 0; i < l_cnt; i++ )
		l_prm [i] = va_arg(l_ap, fao_prm_t);

	va_end(l_ap);

	__util$faol(a_fmt, &l_reclen, &l_recdsc, l_prm, l_cnt);

	return	srch$put_raw(a_out, l_reclen, l_rec);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Copy a block of characters, upper-casing the ASCII letters on the
**	way.  A byte above 0x7f is copied untouched: the case of a
**	multi-byte character is none of this routine's business, so a
**	Cyrillic search string is matched exactly as it has been given
**	whether /EXACT is present or not.
**
**  FORMAL PARAMETERS:
**
**	a_len		Number of bytes to be converted
**	a_src		Address of the source
**	a_dst		Address of the destination, may equal <a_src>
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS always.
**--
*/
int	srch$upcase	(
		unsigned	a_len,
	const	char *		a_src,
		char *		a_dst
			)
{
unsigned char	l_chr;

	for ( unsigned i = 0; i < a_len; i++ )
		{
		l_chr		= (unsigned char) a_src [i];
		a_dst [i]	= (char) (((l_chr >= 'a') && (l_chr <= 'z')) ? (l_chr - ('a' - 'A')) : l_chr);
		}

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Copy an ASCIZ string into a buffer of a known capacity, always
**	leaving the result zero-terminated.
**
**  FORMAL PARAMETERS:
**
**	a_bufsz		Capacity of the destination, bytes
**	a_buf		The destination
**	a_src		The string to be copied, ASCIZ
**
**  IMPLICIT INPUTS/OUTPUTS:
**	None.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the whole string has been copied;
**	STS$K_WARN	- the result has been truncated.
**--
*/
int	srch$strcpy	(
		unsigned	a_bufsz,
		char *		a_buf,
	const	char *		a_src
			)
{
unsigned	l_len;

	if ( !a_bufsz )
		return	STS$K_WARN;

	l_len	= (unsigned) strnlen(a_src, a_bufsz);

	if ( l_len >= a_bufsz )
		l_len	= a_bufsz - 1;

	memcpy(a_buf, a_src, l_len);
	a_buf [l_len]	= '\0';

	return	(a_src [l_len] == '\0') ? STS$K_SUCCESS : STS$K_WARN;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Convert a time string of /SINCE or /BEFORE into a binary time.
**	Accepted are the keywords TODAY, YESTERDAY, TOMORROW and NOW - an
**	unambiguous abbreviation of any of them will do - and everything
**	UTIL$BINTIM understands:
**
**	    dd-MMM-yyyy[ hh:mm[:ss]]	an absolute time
**	    hh:mm[:ss]			today at that time
**	    -[dd ]hh:mm[:ss]		a delta, taken back from now
**
**	A delta is returned by UTIL$BINTIM as a negative value, per the
**	OpenVMS convention; it is subtracted from the current time here, so
**	that /SINCE="-1 0:0:0" means "in the last twenty four hours".
**
**  FORMAL PARAMETERS:
**
**	a_str		The string to be converted, ASCIZ
**	a_time		Receives the binary time
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the clock of the system.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- <a_time> holds the result;
**	STS$K_ERROR	- the string cannot be converted.
**--
*/
int	srch$cvttim	(
	const	char *		a_str,
		fao_time_t *	a_time
			)
{
static	const struct	srch_kwtim_t
	{
	const char *	name;			/* Keyword, upper case			*/
	int		days;			/* Offset in days from today		*/
	int		midnight;		/* Round down to the midnight		*/
	} l_kwtab [] = {
	{ "TODAY",	0,	1 },
	{ "YESTERDAY",	-1,	1 },
	{ "TOMORROW",	1,	1 },
	{ "NOW",	0,	0 }
	};

fao_time_t	l_now, l_tim = 0;
struct tm	l_tm = {0};
char		l_ups [SEARCH$K_SZ_STR] = {0};
unsigned	l_len;
time_t		l_t;

	if ( !a_str || !*a_str )
		return	STS$K_ERROR;

	l_len	= (unsigned) strnlen(a_str, sizeof(l_ups) - 1);
	srch$upcase(l_len, a_str, l_ups);

	if ( !(1 & __util$gettim(&l_now)) )
		return	STS$K_ERROR;

	/*
	**  A keyword first: an abbreviation is accepted as long as it is
	**  the leading part of exactly one keyword of the table.
	*/
	for ( int i = 0; i < (int) $ARRSZ(l_kwtab); i++ )
		{
		if ( strncmp(l_ups, l_kwtab[i].name, l_len) )
			continue;

		l_t	= (time_t) l_now;

		if ( !localtime_r(&l_t, &l_tm) )
			return	STS$K_ERROR;

		l_tm.tm_mday	+= l_kwtab[i].days;

		if ( l_kwtab[i].midnight )
			l_tm.tm_hour = l_tm.tm_min = l_tm.tm_sec = 0;

		l_tm.tm_isdst	= -1;			/* Let MKTIME decide, the day may cross a shift	*/

		if ( (l_t = mktime(&l_tm)) == (time_t) -1 )
			return	STS$K_ERROR;

		*a_time	= (fao_time_t) l_t;

		return	STS$K_SUCCESS;
		}

	/* Otherwise the string is handed over to the STARLET converter */
	if ( !(1 & __util$bintim(a_str, &l_tim)) )
		return	STS$K_ERROR;

	/* A delta comes back negative and is taken back from the current time */
	*a_time	= (l_tim < 0) ? (l_now + l_tim) : l_tim;

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Read the monotonic clock of the system, in milliseconds.  Used for
**	the elapsed time of /STATISTICS only, so an unavailable clock is
**	not an error - the counter simply stays at zero.
**
**  FORMAL PARAMETERS:
**
**	a_msec		Receives the reading, milliseconds
**
**  IMPLICIT INPUTS/OUTPUTS:
**	Reads the clock of the system.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- <a_msec> holds the reading;
**	STS$K_WARN	- no monotonic clock, <a_msec> is set to zero.
**--
*/
int	srch$msec	(
		uint64_t *	a_msec
			)
{
struct timespec	l_ts = {0};

	if ( clock_gettime(CLOCK_MONOTONIC, &l_ts) )
		{
		*a_msec	= 0;

		return	STS$K_WARN;
		}

	*a_msec	= ((uint64_t) l_ts.tv_sec * 1000) + ((uint64_t) l_ts.tv_nsec / 1000000);

	return	STS$K_SUCCESS;
}
