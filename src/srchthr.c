#define	__MODULE__	"SRCHTHR"
#define	__IDENT__	"X01-02"
#define	__REV__		"1.2.0"

/*
**++
**
**  FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
**
**  MODULE:	srchthr.c
**
**  ABSTRACT:	The pool of threads of /THREADS: several files searched at
**		once, the result displayed as a single thread displays it.
**
**  DESCRIPTION:
**
**		The walk of SRCH$FORFILES stays where it is, in the main
**		thread, and hands every file it picks to SRCH$POOL_FILE in
**		place of SRCH$FILE.  The file is queued; a thread of the pool
**		takes it, searches it with a context of its own - a copy of
**		the run's, with its own counters and an output context that
**		gathers into the heap - and marks it done.
**
**		Whatever a file has produced is displayed by the main thread,
**		whole and in the order the walk picked the files: the head of
**		the queue is displayed as soon as it is done, and only the
**		head.  The display of a run under /THREADS is therefore the
**		very display of a run without it, byte for byte, which is
**		what the tests compare.  The counters of a file are added to
**		those of the run as it is displayed.
**
**		At most SEARCH$K_INFLIGHT files a thread are queued or being
**		searched ahead of the head: the walk waits for the head when
**		that many are, so that a slow file at the head does not let
**		the gathered output of all the files behind it pile up
**		without bound.
**
**		A diagnostic signalled by a thread goes to the standard error
**		stream at once, under the lock of $SRCHMSG; the order of two
**		diagnostics of two files is therefore the order they happened
**		in, not the order of the walk.  /LOG, which reports every
**		file in the order of the walk, is run by a single thread for
**		that reason - see SEARCH.C - and so is /PAGE, whose pager has
**		to see the records as they are displayed.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:   2-OCT-2026
**
**  MODIFICATION HISTORY:
**
**	X01-02		 2-OCT-2026	RRL
**		The file at the head of the queue - the next to be displayed -
**		puts its gathered output out as it goes, a megabyte at a time
**		(S_SRCH$SPILL), instead of holding all of it until the file is
**		done: every file before it has been displayed, so nobody else
**		writes meanwhile, and the order is the same.  A file with a
**		result of hundreds of megabytes no longer holds all of it.
**
**	X01-01		 2-OCT-2026	RRL
**		Initial revision: "доработай search", a multi-threaded walk.
**
**--
*/

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<pthread.h>

#include	"srchdef.h"

/*
**  One file of the queue.
*/
typedef	struct	srch_job_t
{
	char *		fspec;			/* The specification of the file, heap		*/
	uint64_t	seq;			/* Its place in the order of the walk		*/
	int		done;			/* The search is over, the output gathered	*/
	int		status;			/* What SRCH$FILE has returned			*/
	SEARCH$STAT	stat;			/* The counters of this file alone		*/
	char *		mem;			/* The output it has produced, heap		*/
	size_t		memlen;			/* Its length					*/
} SEARCH$JOB;

/*
**  The pool.  The queue is a ring of INFLIGHT slots: HEAD is the next file
**  to be displayed, TAKEN the next one a thread is to search, TAIL the next
**  free slot - all three counted from the start of the run, the slot being
**  the count modulo INFLIGHT.
*/
static	struct
{
	SEARCH$CTX *	ctx;			/* The context of the run			*/
	SEARCH$CTX	tmpl;			/* Its copy the threads start from, read only	*/
	unsigned	nthreads;		/* Threads of the pool				*/
	unsigned	inflight;		/* Slots of the ring				*/
	SEARCH$JOB *	ring;			/* The ring itself				*/

	uint64_t	head,			/* Next file to be displayed			*/
			taken,			/* Next file to be searched			*/
			tail;			/* Next free slot				*/

	int		stop;			/* The threads are to leave			*/

	pthread_mutex_t	lock;
	pthread_cond_t	work,			/* A file has been queued, or STOP is set	*/
			done;			/* A file has been searched			*/

	pthread_t	tids [SEARCH$K_MAXTHREADS];
} s_pool = {
	.lock	= PTHREAD_MUTEX_INITIALIZER,
	.work	= PTHREAD_COND_INITIALIZER,
	.done	= PTHREAD_COND_INITIALIZER
	};


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	The SPILL routine of the output a thread gathers for a file: once
**	the file is at the head of the queue, every file before it has been
**	displayed and nothing else writes to the output until this file is
**	done - the main thread waits for it - so what has been gathered is
**	put out at once rather than held to the end of the file.
**
**  FORMAL PARAMETERS:
**
**	a_out		The output context of the thread
**	a_arg		The file, SEARCH$JOB
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_POOL.HEAD, read; the output context of the run, written.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- put out, the area is to be emptied;
**	STS$K_WARN	- not this file's turn yet, keep gathering.
**--
*/
static	int	s_srch$spill	(
		SEARCH$OUT *	a_out,
		void *		a_arg
			)
{
SEARCH$JOB *	l_job = (SEARCH$JOB *) a_arg;

	if ( l_job->seq != __atomic_load_n(&s_pool.head, __ATOMIC_ACQUIRE) )
		return	STS$K_WARN;

	srch$put_raw(s_pool.ctx->out, (unsigned) a_out->memlen, a_out->mem);

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	A thread of the pool: take the next queued file, search it with a
**	context of its own into the heap, mark it done; until told to stop.
**
**  FORMAL PARAMETERS:
**
**	a_arg		Unused
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_POOL, under its lock.
**
**  RETURN VALUE:
**	NULL.
**--
*/
static	void *	s_srch$worker	(
		void *		a_arg
			)
{
SEARCH$CTX	l_ctx;
SEARCH$OUT	l_out;
SEARCH$JOB *	l_job;

	(void) a_arg;

	for ( ;; )
		{
		pthread_mutex_lock(&s_pool.lock);

		while ( !s_pool.stop && (s_pool.taken == s_pool.tail) )
			pthread_cond_wait(&s_pool.work, &s_pool.lock);

		if ( s_pool.taken == s_pool.tail )
			{
			pthread_mutex_unlock(&s_pool.lock);
			break;
			}

		l_job		= &s_pool.ring [s_pool.taken % s_pool.inflight];
		l_job->seq	= s_pool.taken;
		s_pool.taken++;

		pthread_mutex_unlock(&s_pool.lock);

		/*
		**  The template of the context - the strings, the mode, the
		**  qualifiers, zero counters - is copied whole, and given an
		**  output that gathers into the heap.  The context of the run
		**  itself is not read here: its counters are being added to by
		**  the main thread meanwhile.
		*/
		memcpy(&l_ctx, &s_pool.tmpl, sizeof(SEARCH$CTX));

		srch$out_mem(&l_out);
		l_out.spill	= s_srch$spill;
		l_out.spillarg	= l_job;
		l_ctx.out	= &l_out;

		l_job->status	= srch$file(&l_ctx, l_job->fspec);

		srch$out_take(&l_out, &l_job->mem, &l_job->memlen);
		l_job->stat	= l_ctx.stat;

		pthread_mutex_lock(&s_pool.lock);

		l_job->done	= 1;
		pthread_cond_broadcast(&s_pool.done);

		pthread_mutex_unlock(&s_pool.lock);
		}

	return	NULL;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Display the files at the head of the queue that are done, in their
**	order, and add their counters to those of the run.  Called by the
**	main thread alone, with the lock of the pool NOT held: the output
**	may block on the descriptor, and the threads are to go on meanwhile.
**
**  FORMAL PARAMETERS:
**
**	a_wait		Wait for the head if it is not done yet: once for the
**			walk to get a free slot, to the end for the drain
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_POOL; the output context and the counters of the run.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- as far as it went, the queue has been displayed;
**	STS$K_FATAL	- the output has been abandoned (Q at a pager).
**--
*/
static	int	s_srch$emit_done	(
		int		a_wait
			)
{
SEARCH$JOB *	l_job;
SEARCH$CTX *	l_ctx = s_pool.ctx;

	for ( ;; )
		{
		pthread_mutex_lock(&s_pool.lock);

		if ( s_pool.head == s_pool.tail )
			{
			pthread_mutex_unlock(&s_pool.lock);
			break;
			}

		l_job	= &s_pool.ring [s_pool.head % s_pool.inflight];

		if ( !l_job->done )
			{
			if ( !a_wait )
				{
				pthread_mutex_unlock(&s_pool.lock);
				break;
				}

			while ( !l_job->done )
				pthread_cond_wait(&s_pool.done, &s_pool.lock);
			}

		pthread_mutex_unlock(&s_pool.lock);

		if ( l_job->memlen && !l_ctx->out->quit )
			srch$put_raw(l_ctx->out, (unsigned) l_job->memlen, l_job->mem);

		l_ctx->stat.files	+= l_job->stat.files;
		l_ctx->stat.recsread	+= l_job->stat.recsread;
		l_ctx->stat.recsmatched	+= l_job->stat.recsmatched;
		l_ctx->stat.chars	+= l_job->stat.chars;
		l_ctx->stat.lines	+= l_job->stat.lines;

		free(l_job->mem);
		free(l_job->fspec);
		memset(l_job, 0, sizeof(SEARCH$JOB));

		pthread_mutex_lock(&s_pool.lock);
		__atomic_store_n(&s_pool.head, s_pool.head + 1, __ATOMIC_RELEASE);
		pthread_mutex_unlock(&s_pool.lock);

		/* A slot has been freed: the walk waited for one, and has it now */
		if ( a_wait == 1 )
			a_wait	= 0;
		}

	return	l_ctx->out->quit ? STS$K_FATAL : STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Start the pool: the ring of the queue and the threads.
**
**  FORMAL PARAMETERS:
**
**	a_ctx		The context of the run; read only by the threads
**	a_threads	Number of the threads, 2 .. SEARCH$K_MAXTHREADS
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_POOL.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the pool runs, with at least one thread;
**	SEARCH$_NOMEM	- the ring could not be allocated, signalled;
**	STS$K_ERROR	- not a thread could be created: the run is to be
**			  made by the main thread alone.
**--
*/
int	srch$pool_init	(
		SEARCH$CTX *	a_ctx,
		unsigned	a_threads
			)
{
int		l_rc;

	if ( a_threads > SEARCH$K_MAXTHREADS )
		a_threads	= SEARCH$K_MAXTHREADS;

	s_pool.ctx	= a_ctx;

	memcpy(&s_pool.tmpl, a_ctx, sizeof(SEARCH$CTX));
	memset(&s_pool.tmpl.stat, 0, sizeof(s_pool.tmpl.stat));
	s_pool.tmpl.out	= NULL;

	s_pool.inflight	= a_threads * SEARCH$K_INFLIGHT;
	s_pool.head	= s_pool.taken = s_pool.tail = 0;
	s_pool.stop	= 0;

	if ( !(s_pool.ring = calloc(s_pool.inflight, sizeof(SEARCH$JOB))) )
		return	$SRCHMSG(SEARCH$_NOMEM, s_pool.inflight * sizeof(SEARCH$JOB), errno, strerror(errno));

	for ( s_pool.nthreads = 0; s_pool.nthreads < a_threads; s_pool.nthreads++ )
		{
		if ( (l_rc = pthread_create(&s_pool.tids [s_pool.nthreads], NULL, s_srch$worker, NULL)) )
			{
			$LOG(STS$K_WARN, "pthread_create()->%d (%s), %u thread(s) run", l_rc, strerror(l_rc), s_pool.nthreads);
			break;
			}
		}

	if ( !s_pool.nthreads )
		{
		free(s_pool.ring);
		s_pool.ring	= NULL;

		return	STS$K_ERROR;
		}

	return	STS$K_SUCCESS;
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	Queue a file the walk has picked - the routine SRCH$FORFILES calls
**	in place of SRCH$FILE under /THREADS.  When the ring is full, the
**	head is waited for and displayed first; whatever else is done at
**	the head is displayed on the way.
**
**  FORMAL PARAMETERS:
**
**	a_fspec		The specification of the file, ASCIZ; copied
**	a_arg		Unused, the context is that of SRCH$POOL_INIT
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_POOL.
**
**  RETURN VALUE:
**	STS$K_SUCCESS	- the file has been queued;
**	STS$K_FATAL	- the output has been abandoned, the walk is to stop;
**	SEARCH$_NOMEM	- the specification could not be copied, signalled.
**--
*/
int	srch$pool_file	(
	const	char *		a_fspec,
		void *		a_arg
			)
{
char *		l_spec;

	(void) a_arg;

	/* Room in the ring first: the head is displayed if the ring is full */
	if ( (s_pool.tail - s_pool.head) >= s_pool.inflight )
		if ( STS$K_FATAL == s_srch$emit_done(1) )
			return	STS$K_FATAL;

	if ( !(l_spec = strdup(a_fspec)) )
		return	$SRCHMSG(SEARCH$_NOMEM, strlen(a_fspec) + 1, errno, strerror(errno));

	pthread_mutex_lock(&s_pool.lock);

	s_pool.ring [s_pool.tail % s_pool.inflight].fspec	= l_spec;
	s_pool.tail++;

	pthread_cond_signal(&s_pool.work);
	pthread_mutex_unlock(&s_pool.lock);

	/* Whatever is done at the head meanwhile is displayed now, without waiting */
	return	s_srch$emit_done(0);
}


/*
**++
**  FUNCTIONAL DESCRIPTION:
**
**	End of the walk: display every file still queued, in its turn, and
**	stop the threads.
**
**  FORMAL PARAMETERS:
**
**	None.
**
**  IMPLICIT INPUTS/OUTPUTS:
**	S_POOL.
**
**  RETURN VALUE:
**	STS$K_SUCCESS, or STS$K_FATAL when the output has been abandoned.
**--
*/
int	srch$pool_drain	(void)
{
int		l_status;

	l_status	= s_srch$emit_done(2);

	pthread_mutex_lock(&s_pool.lock);
	s_pool.stop	= 1;
	pthread_cond_broadcast(&s_pool.work);
	pthread_mutex_unlock(&s_pool.lock);

	for ( unsigned i = 0; i < s_pool.nthreads; i++ )
		pthread_join(s_pool.tids [i], NULL);

	free(s_pool.ring);
	s_pool.ring	= NULL;

	return	l_status;
}
