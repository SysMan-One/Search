# SEARCH -- display the records that carry given strings

Displays the records of a set of files that satisfy a boolean
combination of the given search strings.

Format:

```
SEARCH file-spec[,...] search-string[,...] [/qualifiers]
```

A search string is a plain substring -- no wildcard, no regular
expression, nothing to escape. A file specification, in contrast,
is expanded by the utility itself, so quote it against the shell to
have it expanded here:

```
$ search '*.c' STS$K_
```

The first word that is not a qualifier is the list of the file
specifications; every word after it is a list of the search
strings. A list is separated by commas; an element that carries a
comma of its own goes in double quotes, or is passed as a word of
its own:

```
$ search '*.c' alpha,beta        two strings
$ search '*.c' '"alpha,beta"'    one string
$ search '*.c' alpha beta        two strings
```

A qualifier may be abbreviated as long as the abbreviation is
unambiguous, and may be spelled in the Unix long form -- --EXACT
for /EXACT. A single dash begins an ordinary value, so a file
spelled -notes.txt is asked for as it stands; -h alone is the short
alias of /HELP.

A word beginning with a slash is taken for a qualifier only when it
names one -- /EXACT does, /usr/src/*.c and /etc do not --
because on this system a slash begins an absolute file
specification as well. A word that names no qualifier is a
parameter: the first such word is the list of the file
specifications, every later one a list of search strings. A
misspelt qualifier therefore becomes a parameter instead of being
refused -- spell it in the long form, --NAME, which is never
anything else, to have a misspelling reported.

A lone \`--\` ends the qualifiers, so that a file or a string spelled
like one can still be asked for:

```
$ search -- notes.txt /EXACT
```

The image exits with the codes of grep: 0 when at least one record
matched, 1 when nothing did, 2 when an error was signalled -- a file
that could not be opened or read, a file specification that
selected no file (%SEARCH-W-NOFILES), a command that could not be
carried out -- whatever else has matched. A run may be tested the
way grep is.

## Parameters

### file-spec

The files to be searched, separated by commas. Wildcards are
expanded by the utility; see the topic WILDCARDS.

A specification that carries no wildcard is taken literally: a
directory stands for everything in it, a file stands for itself.
A name that is not there as it is spelled is looked for in its
directory without regard to case: x.com finds X.COM. When several
names match so -- x.com and X.COM side by side -- all of them are
searched, as a wildcard would take them; when none does, the name is
reported with %SEARCH-E-OPENIN.

An absolute specification is written as it stands -- /usr/src/*.c,
/etc, /usr/src/.../*.c -- and is not mistaken for a qualifier;
see the description of the command for how the two are told apart.

### search-string

The strings to be looked for, separated by commas. A string is
matched as a substring, at any position of a record.

Unless /EXACT is given the case of the letters is ignored, in any
script the locale knows: "поиск" finds Поиск and ПОИСК as "alpha"
finds Alpha. The text is taken as UTF-8; the locale is the user's if
it is a UTF-8 one, C.UTF-8 otherwise.

How several strings are combined is decided by /MATCH.

## /EXACT -- make the case significant

Compares the letters as they stand, so that ALPHA, Alpha and alpha
are three different strings.

By default the case of the letters is ignored, Latin, Cyrillic or
any other the locale knows.

/EXACT concerns the search strings only, never the names of files:
a file specification is matched without regard to case whether
/EXACT is given or not (see the topic WILDCARDS).

## /MATCH -- combine several search strings

```
/MATCH=option
```

Says what makes a record match when more than one search string has
been given:

```
OR      at least one string is present   (the default)
AND     every string is present
NAND    at least one string is absent
NOR     no string is present
```

An option may be abbreviated as long as the abbreviation is
unambiguous: /MATCH=A is AND, /MATCH=N is neither.

/MATCH=NOR with a single string displays the records that do not
carry it, which is the "everything but" of the utility.

## /WINDOW -- display the records around a match

```
/WINDOW[=(n,m)]
```

Displays n records before and m records after every matching one.
One value means that many on either side; no value at all means one
on either side.

Two matches close enough for their windows to overlap produce one
continuous run of records -- no record is ever displayed twice --
and a gap between two runs is marked by an empty line.

The parentheses are metacharacters of the shell, so quote them:

```
$ search '*.c' TODO "/WINDOW=(1,2)"
```

## /PAGE -- stop at the end of a page

The qualifier is negatable: /PAGE stops the display at the end of
every page and waits, /NOPAGE lets it run through.

At the prompt an empty line displays the next page; an answer
beginning with Q, and the end of file of the terminal, abandon the
display altogether -- the files still to be searched are not opened
at all.

Paging is granted only when the output really is a terminal and an
answer can be read from one, so a redirected or piped run never
waits for a keystroke whichever form of the qualifier is given. The
height of the page is the height the terminal driver reports, and a
record wider than the screen counts for as many lines as it wraps
into.

The answer is read from the standard input when that is a terminal,
and from /dev/tty otherwise, so a run piped into another utility can
still be paged.

Paging is off by default.

## /HIGHLIGHT -- mark what has been found

```
/HIGHLIGHT[=style]
```

Marks, within every displayed record, the runs that belong to a
search string. The style is one of:

```
BOLD        the default
REVERSE
UNDERLINE
RED
GREEN
YELLOW
```

Two occurrences that overlap -- ALPHA and PHA within ALPHABET --
are marked as one run rather than as two nested ones. A record
displayed as part of the window of /WINDOW carries no match of its
own and is never marked.

The marking is done with the escape sequences of ECMA-48, which
belong on a screen and not in a file, so it is granted by default
only when the output is a terminal. /HIGHLIGHT given outright
overrides that -- a run piped into a pager that understands the
sequences is the usual reason -- and /NOHIGHLIGHT suppresses the
marking whatever the output is.

The sequences occupy no column of the screen and are not counted by
the pager of /PAGE.

## /NUMBERS -- prefix the number of the record

The qualifier is negatable: /NUMBERS asks for the number of every
displayed record, /NONUMBERS suppresses it.

Records are numbered from one within each file, whether they are
displayed or not, so a number is the number of the line in the
file.

Numbering is off by default.

## /HEADING -- announce the name of a file

The qualifier is negatable: /HEADING asks for the name of a file to
be displayed before its first matching record, /NOHEADING
suppresses it.

The heading is a rule of asterisks, the specification of the file
and its time stamp, so that a run over several files reads as a
listing:

```
******************************************************************
src/srchmat.c                            1-AUG-2026 13:25:53.00
```

The stamp is the one the selection works on -- the time of the last
modification, or that of the last change of the inode under
/CREATED -- so a run with /SINCE displays exactly the time it has
compared against. A specification too long for the column pushes
the stamp right rather than being cut to fit.

A file that matched nothing is never announced, so a run over a
whole tree names only the files that had something to say.

Heading is on by default.

## /OUTPUT -- write the result to a file

```
/OUTPUT=filespec
```

Writes the matched records to the named file instead of the
standard output. An existing file is truncated.

The diagnostics keep going to the standard error stream, so the
file holds the result and nothing else.

## /LOG -- report every file searched

The qualifier is negatable. /LOG reports each file as it is
finished:

```
%SEARCH-I-SEARCHED, alpha.txt, 7 records searched, 1 matched
```

Before the first file is opened it also accounts for what the
command has been made to mean -- the strings, how they combine, the
files and what narrows them down -- and for the absolute time a
value of /SINCE or /BEFORE has been resolved to:

```
%SEARCH-I-SELTIME, /SINCE=TODAY is 21-AUG-2026 00:00:00.00
%SEARCH-I-SEARCHING, for "alpha", "beta" combined by AND, the case is ignored
%SEARCH-I-SELECTING, in .../*.c, /EXCLUDE=(build/%*)
```

That account is worth a look when a run finds nothing: a command
carries quoting, wildcards and defaults, and it shows whether the
quoting survived the shell and whether the defaults are the ones
that were taken for granted.

The report goes to the standard error stream. Logging is off by
default.

## /STATISTICS -- report the totals

The qualifier is negatable. /STATISTICS displays, at the end of the
run, the number of files and records searched, the number of
characters read, the number of records matched, the number of lines
printed and the elapsed time.

The totals go where the records go, so /OUTPUT captures them as
well.

## /EXCLUDE -- leave files out

```
/EXCLUDE=(pattern[,...])
```

Drops the files whose name matches a pattern. The wildcards are
those of a file specification.

A pattern that carries a slash is matched against the whole
specification of a file, one that does not -- against its name
alone:

```
/EXCLUDE=*.o            every object file of the tree
/EXCLUDE=build/%*       one directory of it
```

An excluded directory is not descended into at all, which is what
makes /EXCLUDE cheap on a large tree.

## /SINCE -- take the files not older than

```
/SINCE=time
```

Searches only the files whose time stamp is at or after the given
time. See the topic TIME for how a time is written, and /MODIFIED
for which stamp is meant.

A delta -- 0-1:00 -- and a keyword -- TODAY -- say nothing about the
date they land on, so /LOG reports what the value resolved to:

```
%SEARCH-I-SELTIME, /SINCE=0-1:00 is  1-AUG-2026 13:52:20.00
```

## /BEFORE -- take the files older than

```
/BEFORE=time
```

Searches only the files whose time stamp is before the given time.
Given together with /SINCE it bounds an interval.

The resolved time is reported under /LOG, as that of /SINCE is.

## /MODIFIED -- apply the time filters to the modification time

Says that /SINCE and /BEFORE are to be applied to the time of the
last modification of a file. This is the default.

/MODIFIED and /CREATED cannot be given together; the attempt is
refused with %SEARCH-E-CONFQUAL.

## /CREATED -- apply the time filters to the creation time

Says that /SINCE and /BEFORE are to be applied to the time a file
was created.

Linux keeps no creation time a portable program may rely upon, so
the qualifier is served by the time of the last change of the
inode -- the closest analogue the system has to offer. That stamp
moves when the permissions or the owner of a file change, not only
when its contents do.

## /BINARY -- what is done with a binary file

```
/BINARY=NOTE     say in one line that the file matches (default)
/BINARY=SKIP     do not search the file at all
/BINARY=TEXT     search and display it as any other file
```

A file is binary when it carries a NUL byte among its first 32 KB,
the test GREP makes. Its "records" are whatever happens to lie
between two line feeds, and displayed on a terminal they are noise
or worse, so by default the file is searched up to its first match
and reported by one line in place of the records:

```
%SEARCH-I-BINMATCH, binary file build/app.o matches
```

The completion status counts such a file as a match.

## /FORMAT -- the form of the display

```
/FORMAT=VMS      headings and records, for a reader (default)
/FORMAT=GREP     spec:number:record, one line a record, for a tool
```

GREP writes every record on a line of its own, led by the
specification of the file and the number of the record, in the
form GREP -n uses, so that an editor or a script can read it:

```
$ search '.../*.c' rte_mbuf /FORMAT=GREP
src/rx.c:118:    struct rte_mbuf *m = NULL;
```

No heading is put out, and the records are numbered whatever
/NUMBERS says. A record of /WINDOW is parted from its number by a
hyphen instead of a colon, two runs of records by a line "--", and
a binary file is reported as "Binary file spec matches".

## /THREADS -- search several files at once

```
/THREADS=n       n files are searched at once, 1 to 64
```

The files the walk picks are searched by a pool of threads and
displayed whole, each in its turn, in the order the walk picked
them: the display, the numbers and the totals of /STATISTICS are
those of a single thread, byte for byte. Without the qualifier as
many threads are taken as there are processors, at most 16.

A single large file -- 32 MB or more -- is cut into pieces at the
ends of its records, and the pieces are searched by several threads
at once and displayed in their order; where the records are
numbered, the records of every piece are counted first. This is
done under /MATCH=OR and AND without /WINDOW, and not for a binary
file; the threads of all the files cut at once never exceed
/THREADS.

/THREADS=1 searches every file in the main thread, one after the
other, and so does a run under /LOG, which reports every file in
the order of the walk as it is searched, or under /PAGE, whose
pager has to see the records as they come. A diagnostic of a file -
it cannot be opened, a record is too long - is signalled as it
happens, so two diagnostics may come in an order other than that of
the walk.

## /CONTINUE -- follow a file as it grows

```
/CONTINUE        look at the file again every second
/CONTINUE=n      look at the file again every n seconds, 1 to 3600
```

Use it to watch a log. The last file the command selects is
searched as usual, and then SEARCH does not stop at its end. It
waits, looks at the file again, and searches the new records that
have been added to it. It goes on like this until you press Ctrl-C.

```
$ search /var/log/syslog error /continue
$ search '/var/log/app/*.log' timeout,refused /continue=5 /numbers
```

Only ONE file is followed: the last one selected. If the
specification selects several files, the others are searched
first, in the usual way, and the last one is followed.

Some details:

- A record is shown only when it is complete. If a program has
  written half a line, SEARCH waits for the rest of it.
- If the file becomes shorter (somebody cut it, for example with
  `> file`), SEARCH says %SEARCH-I-TRUNCATED and reads it again from
  the beginning.
- If the file is renamed and a new file with the same name appears
  (log rotation), SEARCH first reads the rest of the old file, then
  says %SEARCH-I-REOPENED and reads the new file from the beginning.
- After a new start the numbers of the records start from 1 again,
  and the name of the file is shown again before its first match.
- What has been found is written out before every wait, so you see
  it at once, also with /OUTPUT or in a pipe.
- Ctrl-C ends the run in a normal way: /STATISTICS is reported and
  the completion code is the usual one -- 0 if something matched.

## /HELP -- display the description

Displays the description of the utility -- this very library -- and
stops. The words that follow name a topic within it:

```
$ search /HELP                  the utility as a whole
$ search /HELP window           one qualifier
$ search /HELP -- /WINDOW       the same, spelled out
$ search /HELP wildcards        one of the descriptive topics
```

A subtopic of this library is a qualifier, and a qualifier is
spelled with a slash -- which the command line of this very utility
swallows. A word that names no subtopic is therefore tried once
more with a slash in front of it, so the two middle forms above are
one and the same.

The topic is displayed the way the HELP browser displays it, the
pager of /PAGE included; at a terminal it is paged unless /NOPAGE
says otherwise.

The library is looked for where SEARCH$HELPLIB says, then beside
the image -- which makes an installed tree relocatable -- then
where the product was configured to install it. Failing all of
them, and in a build made without the HELP product at hand, /HELP
falls back on a summary of the qualifiers built into the image, so
it always has something to say.

## Wildcards -- how a file specification is expanded

A file specification is expanded by the utility itself, not by the
shell:

```
*       any run of characters, the empty one included
%       exactly one character
?       exactly one character (the spelling of the shell)
```

The wildcards apply to the name within a directory: the part of a
specification up to the last slash is a directory and is not
expanded. The case of the letters of a name is not significant, as
on OpenVMS: '*.com' takes X.COM and x.com alike, and so does a
pattern of /EXCLUDE and a name written out in full. This holds for
the Latin letters (ASCII) only: a letter of any other script in a
file name is compared as it stands. /EXACT does not change any of
it -- it concerns the search strings alone.

A tree is walked with the directory "..." -- the \[...\] of an
OpenVMS directory specification: everything below the directory
before it, the pattern after it applied at every level:

```
$ search 'src/*.c' alpha            one directory
$ search '.../*.c' alpha            the tree below the current one
$ search './.../*.com' alpha        the same
$ search '/usr/src/.../*.h' alpha   the tree below /usr/src
$ search 'src/...' alpha            every file of the tree below src
```

"..." is a whole component of the path and the last directory of
it: 'src/.../include/*.h' is refused with %SEARCH-E-IVQUAL, a name
such as a...b is just a name. It walks its own specification only:
in '.../*.c,*.h' the *.h are those of the current directory. Up to
X01-32 a tree was walked by the qualifier /RECURSIVE; it is gone,
and a command that still gives it -- /RECURSIVE, /REC, --RECURSIVE
-- is refused with %SEARCH-E-NORECURSE.

A symbolic link is never followed, neither to a file nor to a
directory: a link pointing at an ancestor would otherwise make the
walk an endless one. Nesting deeper than 32 levels is not descended
into, and is reported with %SEARCH-W-TOODEEP.

Quoting matters. An unquoted pattern is expanded by the shell
before the utility ever sees it, which usually works but defeats
/EXCLUDE and "...", and turns a pattern that matches nothing
into a diagnostic of the shell rather than %SEARCH-W-NOFILES.

Within a directory the names are read in the collating order of the
bytes, so two identical runs display the files in the same order.

## Time -- how a time value is written

The value of /SINCE and /BEFORE is read the way UTIL$BINTIM of the
STARLET module reads it, plus four keywords:

```
1-AUG-2026              an absolute date, midnight
1-AUG-2026 14:30:00     an absolute date and time
14:30                   today at half past two
0-1:00                  a delta: one hour back from now
2-0:00                  a delta: two days back from now
TODAY                   midnight of today
YESTERDAY               midnight of yesterday
TOMORROW                midnight of tomorrow
NOW                     this very moment
```

A keyword may be abbreviated as long as the abbreviation is
unambiguous.

A delta is a day count, a dash or a blank, and a time of day; it is
taken back from the current time, so /SINCE=0-1:00 means "changed
within the last hour".

## Records -- what a record is

A record is a line. The last line of a file that ends without a
line terminator is a record none the less.

A record is searched and displayed whole, however long it is: a
large file is mapped into memory as it stands, and a small one is
read through an area that grows for a long record. Only a record
longer than 64 MB in a file that cannot be mapped -- a pipe, a
device -- is cut to that length. The cut affects the display and
the search alike -- a string past it is not found -- which is why
it is announced with %SEARCH-W-LONGREC, once per file.

The message names the record where the records have been counted,
which they are only when /NUMBERS, /LOG or /STATISTICS asks for a
number; a plain run counts none, being the faster for it, and the
message then names the file and the limit alone.

A file that is not a regular one is never picked up by a wildcard;
named explicitly, it is read like any other.

## Examples

```
$ search '*.txt' error
```

Every record of every .TXT file of the current directory that
carries the string, the case being ignored.

```
$ search '*.c' alpha,beta /MATCH=AND /NUMBERS
```

The records that carry both strings, prefixed by the number of the
line.

```
$ search '.../*' STS$K_ /EXCLUDE=(build/%*,*.o)
```

The whole tree below the current directory, one directory and every
object file left out.

```
$ search 'src/*.c' TODO "/WINDOW=(1,2)" /OUTPUT=todo.lis
```

Every match with one record before it and two after it, written to
a file.

```
$ search '*.log' error /SINCE=0-1:00 /LOG
```

Only the logs changed within the last hour, each of them reported
as it is finished.

```
$ search '.../*.c' alpha /PAGE /HIGHLIGHT=REVERSE
```

The whole tree, a page at a time, with every occurrence of the
string shown in reverse video.

```
$ search -- notes.txt /EXACT
```

The string /EXACT within the file, rather than the qualifier of the
utility.

## Author

StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).

## Messages -- the conditions the utility signals

```
%SEARCH-W-NOMATCHES     nothing matched at all
%SEARCH-W-NOFILES       a file specification selected no file
%SEARCH-E-NOSTRING      no search string has been given
%SEARCH-E-OPENIN        a file cannot be opened
%SEARCH-F-OPENOUT       the file of /OUTPUT cannot be created
%SEARCH-E-READERR       a file cannot be read
%SEARCH-E-OPENDIR       a directory cannot be read
%SEARCH-F-NOMEM         memory cannot be allocated
%SEARCH-E-IVQUAL        the value of a qualifier is illegal
%SEARCH-E-IVTIME        the value of /SINCE or /BEFORE is illformed
%SEARCH-E-CONFQUAL      two qualifiers exclude each other
%SEARCH-I-SEARCHED      the report of /LOG
%SEARCH-I-SELTIME       what /SINCE or /BEFORE resolved to
%SEARCH-I-SEARCHING     what is looked for, and how
%SEARCH-I-SELECTING     where it is looked for
%SEARCH-W-LONGREC       a record has been truncated
%SEARCH-W-TOODEEP       a directory is nested too deep to descend
%SEARCH-W-TOOMANY       a list is longer than the utility accepts
%SEARCH-E-NOTOPIC       /HELP names a topic the library has not
%SEARCH-I-BINMATCH      a binary file matches (output, /BINARY=NOTE)
%SEARCH-I-TRUNCATED     /CONTINUE: the file has become shorter
%SEARCH-I-REOPENED      /CONTINUE: another file has taken its name
%SEARCH-E-NORECURSE     /RECURSIVE is gone: use a "..." directory
```

A diagnostic is signalled by the message subsystem of the STARLET
library, on the standard error stream. It carries the date, the
time and the thread that signalled it, and then the condition in
the OpenVMS format:

```
14-08-2026 12:25:29.279    841 %SEARCH-W-NOFILES, no file matches *.lis
```

A build made for debugging names the module, the routine and the
line as well -- \[SEARCH\\main:1039\] -- which is of interest to
whoever is looking into the utility rather than using it.
