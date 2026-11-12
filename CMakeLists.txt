#+++
#
#	FACILITY:	SEARCH - OpenVMS-style record search utility for Linux
#
#	MODULE:		CMakeLists.txt
#
#	ABSTRACT:	Build of the SEARCH facility: the utility itself and
#			its functional tests.
#
#	AUTHOR:		StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE:	 1-AUG-2026
#
#	MODIFICATION HISTORY:
#
#		12-NOV-2026	CLD	X-25 : The form X.aa-bb[ECOcc] is checked strictly:
#					two digits in every field, X.01-24ECO3 is
#					refused; the older Xaa-bb[ECOc] as it was.
#
#		12-NOV-2026	CLD	X-24 : An __IDENT__ may be X.aa-bb[ECOcc]
#					(the form of the releases from now on, the
#					ECO of two digits) as well as Xaa-bb[ECOc]
#					of the modules not changed since; __REV__
#					aa.bb.cc is checked against both.
#
#		 2-OCT-2026	RRL	X-23 : src/srchthr.c, the pool of threads of
#					/THREADS; the image links Threads::Threads.
#		 2-OCT-2026	RRL	X-22 : The help libraries are translated
#					NOFACILITY.  HELP_ADD_LIBRARY() roots a
#					library at the project name by default;
#					doc/searchlib[_ru].md already begin with
#					the one topic SEARCH, so the root put every
#					topic a level too deep and "HELP SEARCH
#					/EXACT" found nothing.
#		21-AUG-2026	RRL	X-21 : __REV__ of every module is checked
#					against its __IDENT__ at configuration
#					time; they had all stood at 1.0.0.
#
#		14-AUG-2026	RRL	X-20 : /LOG accounts for what the command
#					has been made to mean, before the first
#					file is opened.
#
#		14-AUG-2026	RRL	X-19 : The diagnostics are signalled by the
#					message subsystem of StarLet 1.2.2 and
#					later; the catalogue and the envelope
#					this product carried are gone.
#
#		 7-AUG-2026	RRL	X-18 : Redundancy removed: one walk of the
#					command line, a narrower public interface,
#					dead constants dropped.
#
#		 5-AUG-2026	RRL	X-17 : The Unix spelling of a qualifier is
#					--NAME since CLI_ROUTINES X.00-14.
#
#		 4-AUG-2026	RRL	X-16 : The identification of the product is
#					read out of the head module; the manual
#					page is generated from it.
#
#		 3-AUG-2026	RRL	X-15 : The help libraries are translated by
#					HELP_ADD_LIBRARY() of the HELP package,
#					which brings the librarian along as an
#					imported executable.
#
#		 3-AUG-2026	RRL	X-14 : The FAO parameters of a message are
#					cast and counted by the preprocessor, on
#					the machinery UTILS V.01-21 provides.
#
#		 3-AUG-2026	RRL	X-13 : The images of the HELP product are
#					VHELP and VHELPTRAN since X-22; both
#					spellings are looked for.
#
#		 2-AUG-2026	RRL	X-12 : StarLet is required as an installed
#					package; the vendored copy under external/
#					is gone, so one revision of the library
#					serves every product of the system.
#
#		 2-AUG-2026	RRL	X-11 : A word beginning with a slash is a
#					qualifier only when it names one, so an
#					absolute file specification works.
#
#		 1-AUG-2026	RRL	X-10 : Speed: a region that matches
#					nothing is stepped over whole and its
#					records are not counted unless a number
#					is to be shown; the case-blind scan
#					anchors on a character that is not a
#					letter; only a large file is mapped.
#
#		 1-AUG-2026	RRL	X-09 : Speed: a mapped file, records
#					delivered without a copy, a vectorised
#					scan and a prefilter of the region.
#
#		 1-AUG-2026	RRL	X-08 : /HELP is answered out of the help
#					library through the reading API of the
#					HELP product, when that is installed.
#
#		 1-AUG-2026	RRL	X-07 : The heading of a file carries its
#					time stamp.
#
#		 1-AUG-2026	RRL	X-06 : /LOG reports the absolute time a
#					value of /SINCE or /BEFORE stands for.
#
#		 1-AUG-2026	RRL	X-05 : /PAGE and /HIGHLIGHT; the
#					identification of the kit is carried to
#					the help libraries.
#
#		 1-AUG-2026	RRL	X-04 : The description of the utility is a HELP
#					library: doc/searchlib[_ru].md are translated
#					by HELPTRAN, when one is to be found, and are
#					installed where the HELP browser looks.
#
#		 1-AUG-2026	RRL	X-03 : The installation is a complete one: the
#					manual page and the README are installed
#					beside the image, the destinations follow
#					GNUInstallDirs, and an 'uninstall' target
#					removes what has been recorded.
#
#		 1-AUG-2026	RRL	X-02 : UTILITY$ROUTINES installs itself as
#					the CMake package StarLet, so an installed
#					copy is preferred and the bundled one is
#					the fallback; the target is namespaced.
#
#		 1-AUG-2026	RRL	X-01 : Initial version.
#
#---

cmake_minimum_required(VERSION 3.16)

project(SEARCH VERSION 1.0.0 LANGUAGES C DESCRIPTION "OpenVMS-style SEARCH utility")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

#	The destinations of the installation - BINDIR, MANDIR, DOCDIR - come
#	from here rather than being spelled out, so that the layout of the
#	host distribution is honoured
include(GNUInstallDirs)

#	GNUInstallDirs derives DOCDIR from the name of the project, which is
#	spelled in upper case here - the documentation belongs beside the
#	image, under the name the image is known by
set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/search")

#
#	Product identification and configuration
#
#	The identification of the product is stated ONCE, by __IDENT__ of
#	the head module, and is read out of it here.  Written out a second
#	time it drifts - and it had: the module said X-09, this file X-15
#	and the manual page X-14, all of one kit.  Everything that needs it
#	- the help libraries, the manual page - now takes it from here.
set(SEARCH$K_HEADMOD "${CMAKE_CURRENT_SOURCE_DIR}/src/search.c")

file(STRINGS "${SEARCH$K_HEADMOD}" SEARCH$K_IDENTLINE
		REGEX "^#define[ \t]+__IDENT__")

if ( NOT SEARCH$K_IDENTLINE MATCHES "\"([^\"]+)\"" )
	message(FATAL_ERROR "No __IDENT__ in ${SEARCH$K_HEADMOD}")
endif()

set(SEARCH$K_IDENT "${CMAKE_MATCH_1}")

#
#	__IDENT__ and __REV__ are two spellings of one number and are kept
#	in step by hand, so the build checks them: X.nn-nn[ECOnn] (or the
#	older Xnn-nn[ECOn] of a module not changed since) against
#	major.minor.nn, the nn of an ECO standing as the patch level and 0
#	standing where there is no ECO.  A module that has been revised without its
#	__REV__ being brought along stops the configuration - the two would
#	otherwise drift apart in silence, as the identification of the
#	product itself once did.
#
#	The name carries no '$': CMake does not take one in the name of a
#	command, which is why the helper of the HELP package is spelled
#	HELP_ADD_LIBRARY and this one is spelled likewise
function(s_search_check_ident)
	foreach ( F ${ARGN} )
		file(STRINGS "${F}" L_IDENT REGEX "^#define[ \t]+__IDENT__")
		file(STRINGS "${F}" L_REV   REGEX "^#define[ \t]+__REV__")

		if ( NOT L_IDENT MATCHES "\"([^\"]+)\"" )
			message(FATAL_ERROR "No __IDENT__ in ${F}")
		endif()

		set(L_I "${CMAKE_MATCH_1}")

		if ( NOT L_REV MATCHES "\"([^\"]+)\"" )
			message(FATAL_ERROR "No __REV__ in ${F}")
		endif()

		set(L_R "${CMAKE_MATCH_1}")

		#	X.nn-nn[ECOnn] is taken with two digits in every field and
		#	no others (X.01-24ECO3 and X.01-24ECO003 are refused); the
		#	older Xnn-nn[ECOn] as it was
		if ( L_I MATCHES "^X\\." AND NOT L_I MATCHES "^X\\.[0-9][0-9]-[0-9][0-9](ECO[0-9][0-9])?$" )
			message(FATAL_ERROR "${F}: __IDENT__ \"${L_I}\" is not of the form X.nn-nn[ECOnn] (two digits each)")
		endif()

		#	X.nn-nn (or Xnn-nn), and ECOnn when there is one
		if ( NOT L_I MATCHES "^X\\.?0*([0-9]+)-0*([0-9]+)(ECO0*([0-9]+))?$" )
			message(FATAL_ERROR "${F}: __IDENT__ \"${L_I}\" is not of the form X.nn-nn[ECOnn] (or Xnn-nn[ECOnn])")
		endif()

		set(L_MAJ "${CMAKE_MATCH_1}")
		set(L_MIN "${CMAKE_MATCH_2}")
		set(L_ECO "${CMAKE_MATCH_4}")

		if ( L_ECO STREQUAL "" )
			set(L_ECO "0")
		endif()

		if ( NOT L_R STREQUAL "${L_MAJ}.${L_MIN}.${L_ECO}" )
			message(FATAL_ERROR
				"${F}: __REV__ \"${L_R}\" does not follow __IDENT__ \"${L_I}\""
				" - it should be \"${L_MAJ}.${L_MIN}.${L_ECO}\"")
		endif()
	endforeach()
endfunction()

file(GLOB SEARCH$K_MODULES
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)

s_search_check_ident(${SEARCH$K_MODULES})

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SEARCH$K_MODULES})

#	A change of the identification is a change of the configuration:
#	without this the help libraries would keep the stamp of the run
#	that configured the tree
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SEARCH$K_HEADMOD}")

#	CONFIGURE_FILE() replaces @NAME@ only for a name of letters, digits
#	and the like - a '$' in it is left alone and the template would ship
#	with the placeholder in it - so the substitution goes through a
#	plain alias.  The same reason HELP carries HELP_LIBDIR beside
#	HELP$K_LIBDIR.
set(SEARCH_IDENT "${SEARCH$K_IDENT}")

message(STATUS "SEARCH: ${SEARCH$K_IDENT}, from src/search.c")

option(SEARCH$M_STRICT "Treat the compilation diagnostics as errors" OFF)

#	BUILD_TESTING is the spelling CMake uses; SEARCH$M_TESTS turns the
#	tests of this product off without touching it
option(BUILD_TESTING  "Build the regression tests"                 ON)
option(SEARCH$M_TESTS "Build the functional tests of this product" ON)

#
#	Translation options, carried to every target of the product
#
add_library(search_options INTERFACE)

target_compile_options(search_options INTERFACE
	-Wall -Wextra -Wwrite-strings -Wno-unused-parameter)

if ( SEARCH$M_STRICT )
	target_compile_options(search_options INTERFACE -Werror)
endif()

#
#	HELP - the reading side of the help subsystem is a library of its
#	own, HELP::help, so this product answers for its own /HELP instead
#	of leaving the reader to invoke the browser.  It is optional: found,
#	it is used; absent, /HELP falls back on the summary built into the
#	image and the product loses nothing else.
#
#	It is looked for BEFORE StarLet on purpose.  HELP is built against
#	an installed StarLet and its config asks for that package with
#	FIND_DEPENDENCY, so the very copy HELP was built against is the one
#	this product then links - two revisions of it in one image being
#	exactly what a duplicate symbol at link time is made of.
#
option(SEARCH$M_HELPAPI "Answer /HELP out of the help library when HELP is found" ON)

if ( SEARCH$M_HELPAPI )
	find_package(HELP 1.0 QUIET)
endif()

if ( HELP_FOUND )
	message(STATUS "SEARCH: /HELP answered out of the library, HELP ${HELP_VERSION} from ${HELP_DIR}")
else()
	message(STATUS "SEARCH: /HELP answered by the summary built into the image")
endif()

#
#	UTILITY$ROUTINES - required, and required as an INSTALLED package:
#	the set installs itself as the CMake package StarLet, whose target is
#	StarLet::starlet.
#
#	No copy of it is carried in this kit, and that is deliberate.  A
#	vendored copy means a second revision of the very same symbols in the
#	system, and the two drift: a fix applied to the installed library
#	leaves the vendored one behind, while an image built against the
#	vendored one and linked with a product built against the installed
#	one is a duplicate symbol at best.  One copy in a system, and every
#	product built against that.
#
#	Point at it with -DCMAKE_PREFIX_PATH=<where StarLet was installed>
#	when it does not sit in a default place.
#
find_package(StarLet 1.0 REQUIRED CONFIG)

message(STATUS "SEARCH: StarLet ${StarLet_VERSION} from ${StarLet_DIR}")

#
#	The modules of the product.  The headers of the subproject are
#	deliberately NOT marked as system ones: a diagnostic from a header
#	this product includes is a diagnostic this product has to answer
#	for.
#
add_executable(search
	src/search.c
	src/srchutl.c
	src/srchfil.c
	src/srchmat.c
	src/srchthr.c
	src/srchhlp.c)

find_package(Threads REQUIRED)

target_include_directories(search PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(search PRIVATE StarLet::starlet search_options Threads::Threads)

#
#	The name of a macro carrying a '$' is expanded away by MAKE before
#	the compiler sees it, so the two definitions below are spelled with
#	plain identifiers - the value of the second one is a path and may
#	carry anything.
#
if ( HELP_FOUND )
	target_link_libraries(search PRIVATE HELP::help)

	target_compile_definitions(search PRIVATE
		SEARCH_WITH_HLB=1
		SEARCH_K_HLBSPEC="${CMAKE_INSTALL_PREFIX}/${SEARCH$K_HLBDIR}/search.hlb")
endif()

#
#	The help libraries of the product.  The description of the utility
#	lives in doc/searchlib.md and doc/searchlib_ru.md, in the source
#	format of the HELPTRAN librarian, and is translated into .hlb at
#	build time.
#
#	The translation is not stated here: HELP_ADD_LIBRARY() of the HELP
#	package states it, and every product of the line states it the same
#	way - the rule, the target and the install were the same dozen lines
#	in each of them, which is a dozen lines each of them could get
#	differently wrong.  The librarian comes as the imported executable
#	HELP::helptran, so nothing here has to know where the package was
#	installed.
#
#	The libraries go where the HELP browser looks for them:
#	<prefix>/share/help, the Russian one under ru/, so that a plain
#	'vhelp search' finds the topic with nothing being set.
#
option(SEARCH$M_HELPLIB "Translate and install the help libraries" ON)

set(SEARCH$K_HLBDIR "${CMAKE_INSTALL_DATAROOTDIR}/help"
		CACHE STRING "Directory the help libraries are installed into")

if ( SEARCH$M_HELPLIB AND HELP_FOUND )
	message(STATUS "SEARCH: help libraries translated by the HELP package")

	help_add_library(NAME searchlib ALL NOFACILITY
		SOURCE		${CMAKE_CURRENT_SOURCE_DIR}/doc/searchlib.md
		OUTPUT		${CMAKE_CURRENT_BINARY_DIR}/search.hlb
		IDENT		${SEARCH$K_IDENT}
		DESTINATION	${SEARCH$K_HLBDIR})

	help_add_library(NAME searchlib_ru ALL NOFACILITY
		SOURCE		${CMAKE_CURRENT_SOURCE_DIR}/doc/searchlib_ru.md
		OUTPUT		${CMAKE_CURRENT_BINARY_DIR}/search_ru.hlb
		IDENT		${SEARCH$K_IDENT}RU
		DESTINATION	${SEARCH$K_HLBDIR}/ru)
elseif ( SEARCH$M_HELPLIB )
	message(STATUS "SEARCH: no HELP package found, the help libraries are not built")
endif()

#
#	Installation.  The destinations follow GNUInstallDirs, so a
#	distribution's own layout - lib64, share/man of a multiarch host -
#	is honoured without this file knowing about it:
#
#		<prefix>/bin/search
#		<prefix>/share/man/man1/search.1[.gz]
#		<prefix>/share/help/search.hlb
#		<prefix>/share/help/ru/search_ru.hlb
#		<prefix>/share/doc/search/README.md
#		<prefix>/share/doc/search/searchlib[_ru].md
#
#	SEARCH$M_INSTALL_DOC=OFF leaves the manual page, the README and the
#	sources of the libraries out, which is what a kit built for a
#	container wants.
#
option(SEARCH$M_INSTALL_DOC "Install the manual page and the README" ON)

install(TARGETS search
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if ( SEARCH$M_INSTALL_DOC )
	#	The manual page names the identification in its header, so it
	#	is generated rather than shipped ready-made
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/doc/search.1.in
		${CMAKE_CURRENT_BINARY_DIR}/search.1  @ONLY)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/search.1
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)

	install(FILES
		${CMAKE_CURRENT_SOURCE_DIR}/README.md
		${CMAKE_CURRENT_SOURCE_DIR}/doc/searchlib.md
		${CMAKE_CURRENT_SOURCE_DIR}/doc/searchlib_ru.md
		DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

#
#	'uninstall' - CMake generates no such target of its own.  The
#	manifest is written by the install step, so the target is of use
#	only after one has been run, and it removes exactly what that step
#	has put in place - nothing else, no directories.
#
if ( NOT TARGET uninstall )
	configure_file(
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in
		${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake  @ONLY)

	add_custom_target(uninstall
		COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake
		COMMENT "Removing the files listed in install_manifest.txt")
endif()

#
#	The kit: <product>-<ident>.tar.gz, the directory within it named
#	the same, as every kit of the line is.  The identification comes
#	from the head module, so the name of the archive follows the
#	product without being written out a second time.
#
#	Nothing of the build tree goes in - the sources, the documentation
#	and the tests do, and that is the whole of what a kit is.
#
add_custom_target(kit
	COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_CURRENT_BINARY_DIR}/kit
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/kit
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/src
			${CMAKE_CURRENT_BINARY_DIR}/kit/SEARCH-${SEARCH$K_IDENT}/src
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/include
			${CMAKE_CURRENT_BINARY_DIR}/kit/SEARCH-${SEARCH$K_IDENT}/include
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/doc
			${CMAKE_CURRENT_BINARY_DIR}/kit/SEARCH-${SEARCH$K_IDENT}/doc
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/test
			${CMAKE_CURRENT_BINARY_DIR}/kit/SEARCH-${SEARCH$K_IDENT}/test
	COMMAND ${CMAKE_COMMAND} -E copy_directory
			${CMAKE_CURRENT_SOURCE_DIR}/cmake
			${CMAKE_CURRENT_BINARY_DIR}/kit/SEARCH-${SEARCH$K_IDENT}/cmake
	COMMAND ${CMAKE_COMMAND} -E copy
			${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
			${CMAKE_CURRENT_SOURCE_DIR}/README.md
			${CMAKE_CURRENT_BINARY_DIR}/kit/SEARCH-${SEARCH$K_IDENT}/
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/kit
			${CMAKE_COMMAND} -E tar czf
			${CMAKE_CURRENT_BINARY_DIR}/SEARCH-${SEARCH$K_IDENT}.tar.gz
			SEARCH-${SEARCH$K_IDENT}
	COMMENT "Making SEARCH-${SEARCH$K_IDENT}.tar.gz")

#
#	Functional tests
#
if ( BUILD_TESTING )
	enable_testing()

	if ( SEARCH$M_TESTS )
		add_subdirectory(test)
	endif()
endif()
