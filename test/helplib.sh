#!/bin/sh
#+++
#
#	FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
#
#	MODULE:		test/helplib.sh
#
#	ABSTRACT:	Checks of the help libraries of the product: that
#			every qualifier of the utility has a topic of its
#			own, that a topic renders, and that the Russian
#			library answers to a Russian key.
#
#	DESCRIPTION:	SEARCH_BIN, when set, names the directory of the
#			utility, whose /HELP reads the same libraries.
#
#			The libraries are translated by HELPTRAN as a part
#			of the build; this script reads them with the HELP
#			browser, so it runs only where both utilities of
#			the HELP product are at hand.
#
#			HELP_EXE names the browser, HLB_DIR the directory
#			the libraries have been built in.
#
#	AUTHOR:		StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE:	 1-AUG-2026
#
#	MODIFICATION HISTORY:
#
#		 2-OCT-2026	RRL	X-05 : /THREADS added to the list of the
#					qualifiers a topic is looked for.
#
#		 2-OCT-2026	RRL	X-04 : /FORMAT added to the list of the
#					qualifiers a topic is looked for.
#
#		 2-OCT-2026	RRL	X-03 : /BINARY added to the list of the
#					qualifiers a topic is looked for.
#		 1-AUG-2026	RRL	X-02 : /PAGE and /HIGHLIGHT added to the
#					list of the qualifiers a topic is
#					required for.
#
#		 1-AUG-2026	RRL	X-01 : Initial version.
#
#---

set -u

HELP_EXE=${HELP_EXE:-help}
HLB_DIR=${HLB_DIR:-.}

HLB=$HLB_DIR/search.hlb
HLBRU=$HLB_DIR/search_ru.hlb

FAILED=0
CHECK=0

ok ()
{
	CHECK=`expr $CHECK + 1`
	echo "%SEARCH-S-HELPLIB, $1"
}

ko ()
{
	CHECK=`expr $CHECK + 1`
	FAILED=`expr $FAILED + 1`
	echo "%SEARCH-E-HELPLIB, $1"
}

#
#	ask <library> <key ...>  - one topic, the dialogue and the pager off
#
ask ()
{
	LIB=$1
	shift

	$HELP_EXE /LIBRARY="$LIB" /NOPROMPT /NOPAGE -- "$@" 2>&1
}

[ -f "$HLB" ] || { echo "%SEARCH-F-HELPLIB, no $HLB"; exit 1; }

#
#	Every qualifier the utility declares has a topic of its own.  The
#	list is spelled out rather than harvested from the source: a
#	qualifier added without a topic is exactly what this check is for.
#
MISSING=
for Q in /EXACT /MATCH /WINDOW /PAGE /HIGHLIGHT /NUMBERS /HEADING \
	 /OUTPUT /LOG /STATISTICS /EXCLUDE /SINCE /BEFORE /MODIFIED \
	 /CREATED /RECURSIVE /BINARY /FORMAT /THREADS /HELP
do
	if ! ask "$HLB" search "$Q" | grep -q "^ *$Q$"
		then MISSING="$MISSING $Q"
	fi
done

if [ -z "$MISSING" ]
	then ok "every qualifier of the utility has a topic"
	else ko "no topic for:$MISSING"
fi

#
#	The topics that describe the utility rather than a qualifier
#
MISSING=
for T in PARAMETERS WILDCARDS TIME RECORDS EXAMPLES MESSAGES
do
	if ! ask "$HLB" search "$T" | grep -q "^ *$T$"
		then MISSING="$MISSING $T"
	fi
done

if [ -z "$MISSING" ]
	then ok "the descriptive topics are present"
	else ko "no topic for:$MISSING"
fi

#
#	A topic renders with its text, not with a heading alone
#
if ask "$HLB" search /WINDOW | grep -q 'records before'
	then ok "a topic renders with its text"
	else ko "a topic renders with its text"
fi

#
#	The subtopics of a two-level key
#
if ask "$HLB" search parameters file-spec | grep -q 'searched'
	then ok "a subtopic is reached by a run of keys"
	else ko "a subtopic is reached by a run of keys"
fi

#
#	The Russian library, asked for by a Russian key
#
if [ -f "$HLBRU" ]
	then
		if ask "$HLBRU" search ШАБЛОНЫ | grep -q 'утилита'
			then ok "the Russian library answers to a Russian key"
			else ko "the Russian library answers to a Russian key"
		fi
	else	ko "no $HLBRU"
fi

#
#	/HELP of the utility reads the very same library
#
if [ -n "${SEARCH_BIN:-}" ] && [ -x "$SEARCH_BIN/search" ]
	then
		if $SEARCH_BIN/search /HELP window /NOPAGE 2>&1 | grep -q 'records before'
			then ok "/HELP of the utility answers out of the library"
			else ko "/HELP of the utility answers out of the library"
		fi

		$SEARCH_BIN/search /HELP nosuchtopic /NOPAGE > /dev/null 2>&1

		if [ $? -ne 0 ]
			then ok "an unknown topic fails with %SEARCH-E-NOTOPIC"
			else ko "an unknown topic fails with %SEARCH-E-NOTOPIC"
		fi
	else	ok "the utility is not at hand, its /HELP is not checked"
fi

echo "%SEARCH-I-HELPLIB, $CHECK check(s), $FAILED failure(s)"

[ $FAILED -eq 0 ] || exit 1

exit 0
