#!/bin/sh
#+++
#
#	FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
#
#	MODULE:		test/smoke.sh
#
#	ABSTRACT:	Functional checks of the SEARCH utility: the match
#			modes, the case, the window, the numbering, the
#			heading, the expansion of a file specification,
#			/RECURSIVE, /EXCLUDE, /SINCE, /OUTPUT and the
#			completion codes.
#
#	DESCRIPTION:	The script works in a temporary directory of its own
#			and needs no privileges.  SEARCH_BIN names the
#			directory the image has been built in.
#
#	AUTHOR:		StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE:	 1-AUG-2026
#
#	MODIFICATION HISTORY:
#
#		 2-OCT-2026	RRL	X-12 : The checks of /THREADS: a tree of many
#					files searched by 1 and by 8 threads is
#					displayed byte for byte alike.
#
#		 2-OCT-2026	RRL	X-11 : The checks of /FORMAT=GREP - the lines of
#					GREP -n, the window and the parting "--"
#					- and of the completion codes 0, 1, 2.
#
#		 2-OCT-2026	RRL	X-10 : The checks of /BINARY: a binary file
#					is noted, skipped or searched as text;
#					a record far past the old limit of 8192
#					bytes is searched and displayed whole,
#					read or mapped, highlighted or not.
#		21-AUG-2026	RRL	X-09 : The checks of the two spellings of a
#					single-character wildcard.
#
#		14-AUG-2026	RRL	X-08 : The checks of the account /LOG makes
#					of the command, and of an over-long list.
#
#		 5-AUG-2026	RRL	X-07 : The Unix spelling of a qualifier is
#					--NAME since CLI_ROUTINES X.00-14.
#
#		 2-AUG-2026	RRL	X-06 : The checks of an absolute file
#					specification against a qualifier.
#
#		 1-AUG-2026	RRL	X-05 : /HELP is checked in the wording
#					common to both of its sources; the
#					last-wins rule of a repeated qualifier.
#
#		 1-AUG-2026	RRL	X-04 : The check of the time stamp of a
#					heading.
#
#		 1-AUG-2026	RRL	X-03 : The check of SEARCH$_SELTIME.
#
#		 1-AUG-2026	RRL	X-02 : The checks of /HIGHLIGHT and of
#					/PAGE, the latter as far as a script
#					without a terminal can carry it.
#
#		 1-AUG-2026	RRL	X-01 : Initial version.
#
#---

set -u

SEARCH_BIN=${SEARCH_BIN:-.}
SEARCH=$SEARCH_BIN/search

WRK=`mktemp -d /tmp/srchsmoke.XXXXXX`
FAILED=0
CHECK=0

trap 'rm -rf "$WRK"' EXIT INT TERM

#
#	ok <text>   - the check has passed
#	ko <text>   - the check has failed
#
ok ()
{
	CHECK=`expr $CHECK + 1`
	echo "%SEARCH-S-SMOKE, $1"
}

ko ()
{
	CHECK=`expr $CHECK + 1`
	FAILED=`expr $FAILED + 1`
	echo "%SEARCH-E-SMOKE, $1"
}

#
#	The tree the checks are run against
#
mkdir -p "$WRK/sub"

cat > "$WRK/alpha.txt" <<'EOF'
one alpha line
two BETA line
three alpha beta line
four nothing here
five padding
six padding
seven alpha tail
EOF

cat > "$WRK/beta.lis" <<'EOF'
a beta record
b nothing
EOF

cat > "$WRK/sub/gamma.txt" <<'EOF'
deep alpha record
EOF

cd "$WRK" || exit 1

#
#	A plain search of one file
#
if [ "`$SEARCH alpha.txt alpha /NOHEADING | wc -l`" = "3" ]
	then ok "a substring is found in every record that carries it"
	else ko "a substring is found in every record that carries it"
fi

#
#	The case is ignored by default and significant under /EXACT
#
if [ "`$SEARCH alpha.txt beta /NOHEADING | wc -l`" = "2" ]
	then ok "the case is ignored by default"
	else ko "the case is ignored by default"
fi

if [ "`$SEARCH alpha.txt beta /EXACT /NOHEADING | wc -l`" = "1" ]
	then ok "/EXACT makes the case significant"
	else ko "/EXACT makes the case significant"
fi

#
#	The match modes
#
if [ "`$SEARCH alpha.txt alpha,beta /MATCH=AND /NOHEADING | wc -l`" = "1" ]
	then ok "/MATCH=AND wants every string"
	else ko "/MATCH=AND wants every string"
fi

if [ "`$SEARCH alpha.txt alpha,beta /MATCH=OR /NOHEADING | wc -l`" = "4" ]
	then ok "/MATCH=OR wants any string"
	else ko "/MATCH=OR wants any string"
fi

if [ "`$SEARCH alpha.txt alpha,beta /MATCH=NOR /NOHEADING | wc -l`" = "3" ]
	then ok "/MATCH=NOR wants none of them"
	else ko "/MATCH=NOR wants none of them"
fi

if [ "`$SEARCH alpha.txt alpha,beta /MATCH=NAND /NOHEADING | wc -l`" = "6" ]
	then ok "/MATCH=NAND wants at least one of them missing"
	else ko "/MATCH=NAND wants at least one of them missing"
fi

#
#	An abbreviated keyword and an abbreviated qualifier
#
if [ "`$SEARCH alpha.txt alpha,beta /MATCH=A /NOHEAD | wc -l`" = "1" ]
	then ok "a keyword and a qualifier may be abbreviated"
	else ko "a keyword and a qualifier may be abbreviated"
fi

#
#	/WINDOW: the records around a match, without a repetition
#
if [ "`$SEARCH alpha.txt seven /WINDOW=2 /NOHEADING | wc -l`" = "3" ]
	then ok "/WINDOW=n displays n records on either side"
	else ko "/WINDOW=n displays n records on either side"
fi

if [ "`$SEARCH alpha.txt alpha /WINDOW=1 /NOHEADING | grep -c 'three alpha'`" = "1" ]
	then ok "overlapping windows do not repeat a record"
	else ko "overlapping windows do not repeat a record"
fi

#
#	/NUMBERS
#
if $SEARCH alpha.txt seven /NUMBERS /NOHEADING | grep -q '^ *7  seven'
	then ok "/NUMBERS prefixes the number of the record"
	else ko "/NUMBERS prefixes the number of the record"
fi

#
#	The expansion of a file specification, the heading and /RECURSIVE
#
if [ "`$SEARCH '*.txt' alpha /NOHEADING | wc -l`" = "3" ]
	then ok "a wildcard specification is expanded by the utility"
	else ko "a wildcard specification is expanded by the utility"
fi

if [ "`$SEARCH '*' alpha /RECURSIVE /NOHEADING | wc -l`" = "4" ]
	then ok "/RECURSIVE descends into the subdirectories"
	else ko "/RECURSIVE descends into the subdirectories"
fi

if [ "`$SEARCH '*' alpha /RECURSIVE /EXCLUDE='*.lis' /HEADING | grep -c 'gamma.txt'`" = "1" ]
	then ok "/HEADING announces the name of a file"
	else ko "/HEADING announces the name of a file"
fi

if $SEARCH alpha.txt alpha /HEADING | grep -q '^alpha\.txt  *[0-9][0-9]*-[A-Z][A-Z][A-Z]-[0-9][0-9][0-9][0-9] '
	then ok "/HEADING carries the time stamp of the file"
	else ko "/HEADING carries the time stamp of the file"
fi

if [ "`$SEARCH '*' beta /RECURSIVE /EXCLUDE='*.lis' /NOHEADING | wc -l`" = "2" ]
	then ok "/EXCLUDE drops the files matching a pattern"
	else ko "/EXCLUDE drops the files matching a pattern"
fi

#
#	The wildcards: '%' of OpenVMS and '?' of the shell mean the same
#
if [ "`$SEARCH '?lpha.txt' alpha /NOHEADING | wc -l`" = "3" ]
	then ok "? matches one character"
	else ko "? matches one character"
fi

if [ "`$SEARCH '%lpha.txt' alpha /NOHEADING | wc -l`" = "3" ]
	then ok "% matches one character"
	else ko "% matches one character"
fi

if [ "`$SEARCH '*' alpha /EXCLUDE='?lpha.txt' /NOHEADING | wc -l`" = "0" ]
	then ok "? is understood in a pattern of /EXCLUDE"
	else ko "? is understood in a pattern of /EXCLUDE"
fi

if $SEARCH '*' alpha /EXCLUDE='?lpha.txt' /LOG 2>&1 >/dev/null | grep -q 'EXCLUDE=(?lpha.txt)'
	then ok "the account shows a pattern as it was given"
	else ko "the account shows a pattern as it was given"
fi

#
#	/SINCE and /BEFORE
#
if [ "`$SEARCH '*.txt' alpha /SINCE=TODAY /NOHEADING | wc -l`" = "3" ]
	then ok "/SINCE=TODAY takes the files of today"
	else ko "/SINCE=TODAY takes the files of today"
fi

if $SEARCH '*.txt' alpha /BEFORE=YESTERDAY /NOHEADING 2>&1 | grep -q 'NOMATCHES'
	then ok "/BEFORE=YESTERDAY takes none of them"
	else ko "/BEFORE=YESTERDAY takes none of them"
fi

if $SEARCH '*.txt' alpha /SINCE=TODAY /LOG /NOHEADING 2>&1 >/dev/null | grep -q 'SELTIME, /SINCE=TODAY is'
	then ok "/LOG reports what a time value resolved to"
	else ko "/LOG reports what a time value resolved to"
fi

#
#	/OUTPUT and /STATISTICS
#
$SEARCH alpha.txt alpha /OUTPUT=out.lis /NOHEADING /STATISTICS

if [ -f out.lis ] && [ "`grep -c 'alpha' out.lis`" = "3" ] && grep -q 'Records matched:     3' out.lis
	then ok "/OUTPUT writes the result to a file, /STATISTICS reports the totals"
	else ko "/OUTPUT writes the result to a file, /STATISTICS reports the totals"
fi

#
#	/HIGHLIGHT: the escape sequences are granted only to a terminal,
#	unless the qualifier asks for them outright
#
ESC=`printf '\033'`

if [ "`$SEARCH alpha.txt alpha /NOHEADING | grep -c \"$ESC\"`" = "0" ]
	then ok "nothing is highlighted when the output is not a terminal"
	else ko "nothing is highlighted when the output is not a terminal"
fi

if [ "`$SEARCH alpha.txt alpha /HIGHLIGHT /NOHEADING | grep -c \"$ESC\[1m\"`" = "3" ]
	then ok "/HIGHLIGHT marks what has been found"
	else ko "/HIGHLIGHT marks what has been found"
fi

if $SEARCH alpha.txt alpha /HIGHLIGHT=REVERSE /NOHEADING | grep -q "$ESC\[7m"
	then ok "/HIGHLIGHT=style selects the style"
	else ko "/HIGHLIGHT=style selects the style"
fi

if [ "`$SEARCH alpha.txt alpha /NOHIGHLIGHT /NOHEADING | grep -c \"$ESC\"`" = "0" ]
	then ok "/NOHIGHLIGHT suppresses the marking"
	else ko "/NOHIGHLIGHT suppresses the marking"
fi

#
#	/PAGE: a run whose output is not a terminal is never paged, so the
#	whole file arrives and nothing waits for a keystroke.  The pager
#	itself needs a terminal and is checked by hand.
#
if [ "`$SEARCH alpha.txt alpha /PAGE /NOHEADING < /dev/null | wc -l`" = "3" ]
	then ok "/PAGE does not page what is not a terminal"
	else ko "/PAGE does not page what is not a terminal"
fi

#
#	The completion codes and the diagnostics
#
$SEARCH alpha.txt zzz /NOHEADING > /dev/null 2>&1

if [ $? -ne 0 ]
	then ok "a run that matched nothing fails"
	else ko "a run that matched nothing fails"
fi

if $SEARCH 'nosuch*' alpha 2>&1 | grep -q 'NOFILES'
	then ok "an unmatched file specification is announced"
	else ko "an unmatched file specification is announced"
fi

#	/HELP is answered out of the help library where one is at hand and
#	by the summary built into the image where none is; both spell out
#	the format of the command, which is what is checked here
if $SEARCH /HELP 2>&1 | grep -q 'file-spec'
	then ok "/HELP displays the syntax of the utility"
	else ko "/HELP displays the syntax of the utility"
fi

#	The fix of CLI_ROUTINES X.00-12: of a repeated qualifier the LAST
#	occurrence decides
if [ "`$SEARCH alpha.txt alpha /HIGHLIGHT /NOHIGHLIGHT /NOHEADING | grep -c \"$ESC\"`" = "0" ]
	then ok "of a repeated qualifier the last occurrence wins"
	else ko "of a repeated qualifier the last occurrence wins"
fi

#
#	An absolute specification is a file, not a qualifier: on this
#	system a slash begins both
#
if [ "`$SEARCH \"$WRK/*.txt\" alpha /NOHEADING | wc -l`" = "3" ]
	then ok "an absolute specification with a wildcard is a file"
	else ko "an absolute specification with a wildcard is a file"
fi

if [ "`$SEARCH $WRK alpha /NOHEADING /EXCLUDE='*.lis' | wc -l`" = "3" ]
	then ok "an absolute directory is a file specification"
	else ko "an absolute directory is a file specification"
fi

#	A word that names no qualifier is a parameter: the first one is the
#	file specification, every later one a search string.  A misspelt
#	qualifier therefore becomes a string that matches nothing, and it
#	is the dash form that reports the misspelling as such.
if [ "`$SEARCH alpha.txt alpha /RECURSVE /NOHEADING | wc -l`" = "3" ]
	then ok "a misspelt qualifier becomes a parameter"
	else ko "a misspelt qualifier becomes a parameter"
fi

if $SEARCH alpha.txt alpha --RECURSVE 2>&1 | grep -q 'Unrecognized qualifier'
	then ok "the long form of a misspelt qualifier is reported as one"
	else ko "the long form of a misspelt qualifier is reported as one"
fi

#	The GNU long form is a qualifier, a single dash is an ordinary value
if [ "`$SEARCH alpha.txt alpha --NOHEADING | wc -l`" = "3" ]
	then ok "--NAME is the Unix spelling of a qualifier"
	else ko "--NAME is the Unix spelling of a qualifier"
fi

if $SEARCH alpha.txt alpha -NOHEADING 2>&1 | grep -q '^alpha.txt'
	then ok "a single dash begins a value, not a qualifier"
	else ko "a single dash begins a value, not a qualifier"
fi

if $SEARCH -h 2>&1 | grep -q 'file-spec'
	then ok "-h is the short alias of /HELP"
	else ko "-h is the short alias of /HELP"
fi

#
#	/LOG accounts for what the command has been made to mean
#
if $SEARCH alpha.txt alpha,beta /MATCH=AND /EXACT /LOG 2>&1 >/dev/null \
	| grep -q 'SEARCHING, for "alpha", "beta" combined by AND, the case is significant'
	then ok "/LOG reports the strings and how they combine"
	else ko "/LOG reports the strings and how they combine"
fi

if $SEARCH '*' alpha /RECURSIVE /EXCLUDE='*.lis' /LOG 2>&1 >/dev/null \
	| grep -q 'SELECTING, in \*, /RECURSIVE, /EXCLUDE=(\*\.lis)'
	then ok "/LOG reports the files and what narrows them down"
	else ko "/LOG reports the files and what narrows them down"
fi

if $SEARCH alpha.txt alpha 2>&1 >/dev/null | grep -q 'SEARCHING'
	then ko "the account is made under /LOG alone"
	else ok "the account is made under /LOG alone"
fi

#
#	A list longer than the utility takes is cut short, and says so
#
LONGLIST=`awk 'BEGIN { for (i = 0; i < 40; i++) printf "s%02d,", i; print "s40" }'`

if $SEARCH alpha.txt "$LONGLIST" 2>&1 >/dev/null | grep -q 'TOOMANY, too many search strings'
	then ok "an over-long list of strings is reported"
	else ko "an over-long list of strings is reported"
fi

#
#	A string spelled like a qualifier, past the "--" marker
#
echo "the /EXACT of it" > qual.txt

if [ "`$SEARCH -- qual.txt /EXACT | grep -c 'the /EXACT'`" = "1" ]
	then ok "a lone -- ends the qualifiers"
	else ko "a lone -- ends the qualifiers"
fi

#
#	A binary file - a NUL among its first bytes - is noted in one line by
#	default, left alone under /BINARY=SKIP, displayed under /BINARY=TEXT
#
printf 'head alpha\n\000\001 alpha junk\nmore alpha\n' > bin.dat

if [ "`$SEARCH bin.dat alpha /NOHEADING`" = "%SEARCH-I-BINMATCH, binary file bin.dat matches" ]
	then ok "a binary file is noted in one line"
	else ko "a binary file is noted in one line"
fi

if $SEARCH bin.dat alpha /BINARY=SKIP >/dev/null 2>&1
	then ko "/BINARY=SKIP leaves a binary file alone"
	else ok "/BINARY=SKIP leaves a binary file alone"
fi

if [ "`$SEARCH bin.dat alpha /BINARY=TEXT /NOHEADING | wc -l`" = "3" ]
	then ok "/BINARY=TEXT searches a binary file as text"
	else ko "/BINARY=TEXT searches a binary file as text"
fi

if [ "`$SEARCH alpha.txt alpha /NOHEADING | wc -l`" = "3" ]
	then ok "a text file is not taken for a binary one"
	else ko "a text file is not taken for a binary one"
fi

rm -f bin.dat

#
#	A long record is searched and displayed whole: 100 KB in a file small
#	enough to be read, 1 MB in one large enough to be mapped
#
awk 'BEGIN { printf "first\n"; for (i = 0; i < 10000; i++) printf "xxxxxxxxxx"; print "NEEDLE tail"; print "last" }' > long1.txt
awk 'BEGIN { for (i = 0; i < 40000; i++) printf "yyyyyyyyyy"; print ""; for (i = 0; i < 100000; i++) printf "xxxxxxxxxx"; print "NEEDLE tail" }' > long2.txt

if [ "`$SEARCH long1.txt needle /NOHEADING 2>&1 | wc -c`" = "100012" ]
	then ok "a record of 100 KB is found and displayed whole"
	else ko "a record of 100 KB is found and displayed whole"
fi

if [ "`$SEARCH long2.txt needle /NOHEADING 2>&1 | wc -c`" = "1000012" ]
	then ok "a record of 1 MB in a mapped file is found and displayed whole"
	else ko "a record of 1 MB in a mapped file is found and displayed whole"
fi

if [ "`$SEARCH long1.txt needle /NOHEADING /HIGHLIGHT 2>&1 | grep -c 'NEEDLE'`" = "1" ]
	then ok "a long record is highlighted"
	else ko "a long record is highlighted"
fi

if $SEARCH long1.txt needle 2>&1 >/dev/null | grep -q 'LONGREC'
	then ko "a long record is not reported as truncated"
	else ok "a long record is not reported as truncated"
fi

rm -f long1.txt long2.txt

#
#	/FORMAT=GREP writes the lines of GREP -n: spec:number:record, the window
#	parted by hyphens, two runs by "--"
#
printf 'one alpha\ntwo\nthree\nfour alpha\nfive\nsix\nseven\neight alpha\n' > fmt.txt

if [ "`$SEARCH fmt.txt alpha /FORMAT=GREP | head -1`" = "fmt.txt:1:one alpha" ]
	then ok "/FORMAT=GREP writes spec:number:record"
	else ko "/FORMAT=GREP writes spec:number:record"
fi

printf 'fmt.txt:1:one alpha\nfmt.txt-2-two\nfmt.txt-3-three\nfmt.txt:4:four alpha\nfmt.txt-5-five\n--\nfmt.txt-7-seven\nfmt.txt:8:eight alpha\n' > fmt.want

if $SEARCH fmt.txt alpha /FORMAT=GREP /WINDOW=1 | cmp -s - fmt.want
	then ok "/FORMAT=GREP /WINDOW writes what GREP -n -C1 does"
	else ko "/FORMAT=GREP /WINDOW writes what GREP -n -C1 does"
fi

#
#	The completion codes of GREP: 0 a match, 1 none, 2 an error
#
$SEARCH fmt.txt alpha > /dev/null 2>&1;		L_RC0=$?
$SEARCH fmt.txt zzz > /dev/null 2>&1;		L_RC1=$?
$SEARCH fmt.txt alpha /FORMAT=XML > /dev/null 2>&1;	L_RC2=$?

if [ "$L_RC0$L_RC1$L_RC2" = "012" ]
	then ok "the completion codes are 0, 1 and 2"
	else ko "the completion codes are 0, 1 and 2 ($L_RC0$L_RC1$L_RC2)"
fi

chmod 000 fmt.want 2>/dev/null

if [ "`id -u`" != "0" ] ; then
	$SEARCH fmt.txt,fmt.want alpha > /dev/null 2>&1

	if [ $? = 2 ]
		then ok "an unreadable file is an error even when something matched"
		else ko "an unreadable file is an error even when something matched"
	fi
fi

rm -f fmt.txt fmt.want

#
#	/THREADS: the display of a pool is that of a single thread, byte for
#	byte - a tree of files of every size, some matching, some not
#
mkdir -p thr/a/b thr/c
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 ; do
	awk -v n=$i 'BEGIN { for (r = 1; r <= n * 50; r++) print (r % (n + 2)) ? "plain " r : "the alpha of " n " at " r }' > thr/f$i.txt
	cp thr/f$i.txt thr/a/g$i.txt
	cp thr/f$i.txt thr/a/b/h$i.txt
done
echo "nothing here" > thr/c/none.txt

$SEARCH 'thr/*.txt' alpha /RECURSIVE /NUMBERS /WINDOW=1 /THREADS=1 > thr1.out 2>&1
$SEARCH 'thr/*.txt' alpha /RECURSIVE /NUMBERS /WINDOW=1 /THREADS=8 > thr8.out 2>&1

if [ -s thr1.out ] && cmp -s thr1.out thr8.out
	then ok "/THREADS=8 displays what a single thread does"
	else ko "/THREADS=8 displays what a single thread does"
fi

$SEARCH 'thr/*.txt' alpha /RECURSIVE /FORMAT=GREP /THREADS=1 > thr1.out 2>&1
$SEARCH 'thr/*.txt' alpha /RECURSIVE /FORMAT=GREP /THREADS=8 > thr8.out 2>&1

if [ -s thr1.out ] && cmp -s thr1.out thr8.out
	then ok "/THREADS=8 /FORMAT=GREP displays what a single thread does"
	else ko "/THREADS=8 /FORMAT=GREP displays what a single thread does"
fi

if $SEARCH 'thr/*.txt' alpha /THREADS=0 > /dev/null 2>&1
	then ko "/THREADS=0 is refused"
	else ok "/THREADS=0 is refused"
fi

rm -rf thr thr1.out thr8.out

echo "%SEARCH-I-SMOKE, $CHECK check(s), $FAILED failure(s)"

[ $FAILED -eq 0 ] || exit 1

exit 0
