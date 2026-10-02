#+++
#	FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
#	MODULE:		test/CMakeLists.txt
#	ABSTRACT:	Functional tests of the SEARCH utility.
#	AUTHOR:		StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#	CREATION DATE:	 1-AUG-2026
#
#	MODIFICATION HISTORY:
#
#		 1-AUG-2026	RRL	X-02 : The check of the help libraries,
#					which runs only where a HELP browser is
#					to be found.
#
#		 1-AUG-2026	RRL	X-01 : Initial version.
#---

add_test(NAME smoke
	COMMAND ${CMAKE_COMMAND} -E env
		SEARCH_BIN=$<TARGET_FILE_DIR:search>
		sh ${CMAKE_CURRENT_SOURCE_DIR}/smoke.sh)

#
#	The libraries are translated by one product and read by another, so
#	the check runs only when both utilities of the HELP product are at
#	hand - it is the rendering of the topics that is being tested here,
#	not the translation alone.
#
#	VHELP since HELP X-22 - 'help' is a builtin of the shell - and the
#	plain name in a build tree
find_program(HELP_EXECUTABLE NAMES vhelp help DOC "The HELP browser of the HELP product")

if ( TARGET searchlib AND HELP_EXECUTABLE )
	add_test(NAME helplib
		COMMAND ${CMAKE_COMMAND} -E env
			HELP_EXE=${HELP_EXECUTABLE}
			SEARCH_BIN=$<TARGET_FILE_DIR:search>
			HLB_DIR=${CMAKE_CURRENT_BINARY_DIR}/..
			sh ${CMAKE_CURRENT_SOURCE_DIR}/helplib.sh)
endif()
