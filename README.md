# SEARCH

An OpenVMS-style record search utility for Linux: display the records of
a set of files that carry a given combination of strings.

    $ search '*.c' alpha,beta /MATCH=AND /NUMBERS

    ******************************************************************
    srchmat.c                                1-AUG-2026 13:25:53.00

        41  the alpha and the beta of it

A search string is a plain **substring** -- no wildcard, no regular
expression, nothing to escape -- exactly as on OpenVMS.  A file
specification, in contrast, is expanded by the utility itself, so quote
it against the shell:

    $ search '*.txt' error            one specification, expanded here
    $ search 'src/*.c,include/*.h' STS$K_       two of them

## Syntax

    SEARCH file-spec[,...] search-string[,...] [/qualifiers]

| Qualifier | Meaning |
|---|---|
| `/EXACT` | the case of the letters is significant; ignored by default, in any script the locale knows (Cyrillic too) |
| `/MATCH=option` | how the strings are combined: `OR` (default), `AND`, `NAND`, `NOR` |
| `/WINDOW[=(n,m)]` | display `n` records before and `m` after a match; one value -- both sides; no value -- one on either side |
| `/[NO]PAGE` | stop at the end of a page and wait; a terminal only, off by default |
| `/[NO]HIGHLIGHT[=style]` | mark what has been found: `BOLD` (default), `REVERSE`, `UNDERLINE`, `RED`, `GREEN`, `YELLOW` |
| `/[NO]NUMBERS` | prefix the number of the record |
| `/[NO]HEADING` | announce every file -- specification and time stamp -- before its first match; on by default |
| `/OUTPUT=filespec` | write the result to a file instead of the standard output |
| `/[NO]LOG` | report every file searched, and what the command was made to mean |
| `/[NO]STATISTICS` | report the totals at the end |
| `/EXCLUDE=(pat,...)` | drop the files matching a pattern |
| `/SINCE=time`, `/BEFORE=time` | take the files not older than / older than |
| `/MODIFIED`, `/CREATED` | which time stamp the two above apply to |
| `/RECURSIVE` | descend into the subdirectories -- the analogue of `[...]` |
| `/FORMAT=option` | `VMS` (default): headings and records; `GREP`: `spec:number:record`, one line each, for a tool |
| `/THREADS=n` | search n files at once (default: the processors, at most 16), and a file of 32 MB or more in pieces; the display is that of one thread, byte for byte |
| `/BINARY=option` | a binary file (a NUL in its first 32 KB): `NOTE` that it matches (default), `SKIP` it, or search it as `TEXT` |
| `/HELP [topic]` | display the description -- the help library of the product |

A qualifier may be abbreviated as long as the abbreviation is
unambiguous (`/NOHEAD`, `/MATCH=A`), and may be spelled in the Unix long
form: `--EXACT` for `/EXACT`.  A single dash begins an ordinary value --
a file spelled `-notes.txt` is asked for as it stands -- and `-h` alone
is the short alias of `/HELP`.

A word beginning with a slash is a qualifier only when it *names* one --
`/RECURSIVE` does, `/usr/src/*.c` does not -- because a slash begins an
absolute file specification here as well, so absolute specifications are
written as they stand:

    $ search '/usr/src/*.c' STS$K_ /RECURSIVE
    $ search /etc hostname

A word that names no qualifier is a parameter -- the first one the file
specification, every later one a search string -- so `/RECURSVE` quietly
becomes a search string rather than a diagnostic.  Spell it in the long
form, `--RECURSVE`, and a misspelling is reported as such.  A lone `--`
ends the qualifiers, so that a file or a string spelled like one can be
asked for:

    $ search -- notes.txt /EXACT

## Details worth knowing

**Lists.**  The first word that is not a qualifier is the list of the
file specifications; every word after it is a list of the search
strings.  Elements are separated by commas; an element carrying a comma
of its own goes in double quotes -- or, simpler, as a word of its own:

    $ search '*.c' alpha,beta        two strings
    $ search '*.c' '"alpha,beta"'    one string
    $ search '*.c' alpha beta        two strings

**Wildcards** in a file specification are `*` (any run), `%` and `?`
(one character; `?` is turned into `%` when the pattern is taken in,
since the matching itself is `__util$pattern_match()` of the STARLET
library, which knows the OpenVMS spelling alone).  They apply to the name within a directory: the part of
the specification up to the last slash is a directory and is not
expanded.  Walk a tree with `/RECURSIVE` instead:

    $ search '*.c' STS$K_ /RECURSIVE /EXCLUDE=build/%*

A pattern of `/EXCLUDE` that carries a slash is matched against the
whole specification of a file, one that does not -- against its name
alone.  An excluded directory is not descended into at all.

**The screen.**  `/PAGE` stops at the end of every page and waits; an
empty line goes on, `Q` abandons the display and the files still to be
searched are not opened at all.  `/HIGHLIGHT` marks the runs of a record
that belong to a search string.  Both are meant for a terminal: paging
is granted only when the output is one and an answer can be read (from
the standard input, or from `/dev/tty` when the output is piped), and
the marking is granted by default on the same terminal-only condition --
`/HIGHLIGHT` given outright overrides that, `/NOHIGHLIGHT` suppresses it
whatever the output is.  The escape sequences occupy no column and are
not counted by the pager.

**Symbolic links** are never followed, neither to a file nor to a
directory: a link pointing at an ancestor would otherwise make the walk
an endless one.

**Time** is given the way `UTIL$BINTIM` reads it, plus the keywords
`TODAY`, `YESTERDAY`, `TOMORROW` and `NOW`:

    /SINCE=1-AUG-2026        an absolute date
    /SINCE=14:30             today at half past two
    /SINCE=0-1:00            a delta: within the last hour
    /SINCE=TODAY             since midnight

`/LOG` reports what such a value resolved to, since neither a delta nor
a keyword shows the date it lands on, and accounts for the rest of the
command in the same breath:

    %SEARCH-I-SELTIME, /SINCE=0-1:00 is 21-AUG-2026 10:52:20.00
    %SEARCH-I-SEARCHING, for "alpha", "beta" combined by AND, the case is ignored
    %SEARCH-I-SELECTING, in *.c, /RECURSIVE, /EXCLUDE=(build/%*)

Worth a look when a run finds nothing: it shows whether the quoting
survived the shell and whether the defaults were the ones assumed.

`/CREATED` is served by the inode change time -- the closest analogue
Linux has to a creation time.

**Records** are lines.  The last line of a file that ends without a
terminator is a record none the less.  A record is searched and displayed
whole however long it is; only a record longer than 64 MB in a file that
cannot be mapped (a pipe, a device) is cut, which affects the display
*and* the search, and is therefore announced by `%SEARCH-W-LONGREC`.

**Diagnostics** are signalled by the message subsystem of the STARLET
library, on the standard error stream -- never on the standard output,
where the records go:

    14-08-2026 12:25:29.279    841 %SEARCH-W-NOFILES, no file matches *.lis

The date, the time and the thread come from the subsystem; the rest is
the condition in the OpenVMS format, out of the catalogue this product
registers with `__util$inimsg()`.  A build made for debugging names the
module, the routine and the line as well.

**Completion.**  The image exits with the codes of `grep`: 0 when at least
one record matched, 1 when nothing did, 2 when an error was signalled (a
file that could not be opened or read, a command that could not be
carried out), whatever else has matched.  Nothing matched at all is
reported by `%SEARCH-W-NOMATCHES`.

## Build

    cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/usr/local
    cmake --build build
    ctest --test-dir build
    cmake --install build

This product carries **no copy** of the libraries it uses.  Both are
consumed as installed CMake packages:

| package | target | required |
|---|---|---|
| `StarLet` (UTILITY$ROUTINES) 1.4.6+ | `StarLet::starlet` | yes |
| `HELP` | `HELP::help`, `HELP::helptran`, `help_add_library()` | no |

    cmake -S . -B build -DCMAKE_PREFIX_PATH=/where/they/were/installed

The configuration says what it found:

    -- SEARCH: StarLet 1.0.0 from /usr/local/lib/cmake/StarLet
    -- SEARCH: /HELP answered out of the library, HELP 1.0.0 from ...
    -- SEARCH: help libraries translated by the HELP package

A vendored copy of a library means a second revision of the very same
symbols in the system, and the two drift apart: a fix applied to the
installed library leaves the vendored one behind, and an image built
against the vendored one, linked with a product built against the
installed one, is a duplicate symbol at best.  One copy in a system,
and every product built against it.

HELP stays optional: without it `/HELP` falls back on the summary built
into the image and the help libraries are simply not translated.

## Install

    cmake --install build --prefix /usr/local

    <prefix>/bin/search
    <prefix>/share/man/man1/search.1
    <prefix>/share/help/search.hlb
    <prefix>/share/help/ru/search_ru.hlb
    <prefix>/share/doc/search/README.md
    <prefix>/share/doc/search/searchlib[_ru].md

The destinations come from `GNUInstallDirs`, so the layout of the host
distribution -- `lib64`, a multiarch `share/man` -- is honoured; each of
them may be overridden the usual way, e.g.
`-DCMAKE_INSTALL_MANDIR=share/man9`.  `DESTDIR` is honoured for a staged
installation:

    DESTDIR=/tmp/stage cmake --install build --prefix /usr/local

`cmake --build build --target kit` makes the kit,
`SEARCH-<ident>.tar.gz`, out of the sources, the documentation and the
tests; the identification comes from the head module, so the name of the
archive follows the product.

`cmake --build build --target uninstall` removes exactly the files the
install step recorded in `install_manifest.txt`, and no directory: a
shared one is none of this product's business, and an empty one does no
harm.

Options:

| option | default | |
|---|---|---|
| `SEARCH$M_STRICT` | OFF | `-Werror` |
| `SEARCH$M_HELPAPI` | ON | answer `/HELP` out of the library through `HELP::help` when the HELP package is found |
| `SEARCH$M_HELPLIB` | ON | translate and install the help libraries through `help_add_library()` |
| `SEARCH$K_HLBDIR` | `share/help` | where the libraries go |
| `SEARCH$M_INSTALL_DOC` | ON | install the manual page, the README and the sources of the libraries |
| `BUILD_TESTING` | ON | the spelling CMake uses |
| `SEARCH$M_TESTS` | ON | the tests of this product |

The identification of the product is stated once, by `__IDENT__` of the
head module `src/search.c`, and the build reads it out of there and
stamps it into the help libraries and the manual page.  There is nothing
to keep in step by hand and nothing to pass on the command line: change
it in the module, and the next build carries it everywhere.

Every module carries its own `__IDENT__` and `__REV__`, two spellings of
one number:

| | | |
|---|---|---|
| `__IDENT__` | `X01-07` | the OpenVMS spelling, `Xnn-nn`; a minor correction of an error is marked `X01-07ECO02` after it |
| `__REV__` | `1.7.0` | the Unix spelling, made out of `__IDENT__`: `major.minor.nn`, the `nn` of an `ECOnn` standing as the patch level and `0` where there is no ECO |

The two are kept in step by hand, so the build checks them: a module
revised without its `__REV__` being brought along stops the
configuration, and so does an `__IDENT__` that is not of the form
`Xnn-nn[ECOnn]`.

In the head module `__IDENT__` carries the identification of the
*product*, so it runs apart from the labels of that module's own
modification history; everywhere else the two are the same number.

## Layout

    include/srchdef.h    condition values, contexts, prototypes
    src/search.c         the CLI tables and the entry point
    src/srchutl.c        messages, buffered output, time conversion
    src/srchfil.c        file selection: expansion, walk, filters
    src/srchmat.c        the matcher and the per-file scan
    src/srchhlp.c        /HELP out of the help library
    doc/search.1.in      the manual page; its header carries the
                         identification, stamped in by the build
    doc/searchlib.md     the help library, source form; its identification
                         is stamped in by the build too
    doc/searchlib_ru.md  the same in Russian
    cmake/               the script behind the 'uninstall' target
    test/smoke.sh        34 functional checks
    test/helplib.sh      7 checks of the libraries, where HELP is present

Neither `<stdio.h>` nor any of its derivatives is used anywhere in the
facility: every record is built by the FAO engine of the STARLET module
and leaves the process through `write(2)`.

## Not implemented

`/CONFIRM`, `/BACKUP`, `/EXPIRED`, `/BY_OWNER`, `/FORMAT` and
`/REMAINING` of the OpenVMS original.  `/WILDCARD_MATCHING` has no
meaning here: a search string is never a pattern.

## Author

StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan).
