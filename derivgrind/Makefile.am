include $(top_srcdir)/Makefile.tool.am

#EXTRA_DIST = docs/dg-manual.xml

#----------------------------------------------------------------------------
# Headers
#----------------------------------------------------------------------------

pkginclude_HEADERS = \
  derivgrind.h derivgrind-recording.h

#----------------------------------------------------------------------------
# derivgrind-<platform>
#----------------------------------------------------------------------------
# adapted from the Nulgrind Makefile.am

noinst_PROGRAMS  = derivgrind-@VGCONF_ARCH_PRI@-@VGCONF_OS@
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += derivgrind-@VGCONF_ARCH_SEC@-@VGCONF_OS@
endif

DERIVGRIND_SOURCES_COMMON = dg_main.c ../shadow-memory/src/shadow-64.c dg_shadow.c dg_utils.c dg_expressionhandling.c dot/dg_dot.c dot/dg_dot_bitwise.c dot/dg_dot_minmax.c bar/dg_bar.c bar/dg_bar_bitwise.c bar/dg_bar_tape.c

derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_SOURCES      = \
	$(DERIVGRIND_SOURCES_COMMON)
derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_PRI_CAPS@)
derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_PRI_CAPS@)
derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_PRI@ \
	$(LINK) \
	$(derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS) \
	$(derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS)

if VGCONF_HAVE_PLATFORM_SEC
derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_SOURCES      = \
	$(DERIVGRIND_SOURCES_COMMON)
derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS       = $(LTO_CFLAGS) \
	$(AM_CFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_SEC_CAPS@)
derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_SEC_CAPS@)
derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_SEC@ \
	$(LINK) \
	$(derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS) \
	$(derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS)
endif

BUILT_SOURCES = dot/dg_dot_operations.c bar/dg_bar_operations.c
dot/dg_dot_operations.c: gen_operationhandling_code.py
	python3 gen_operationhandling_code.py dot > dot/dg_dot_operations.c
bar/dg_bar_operations.c: gen_operationhandling_code.py
	python3 gen_operationhandling_code.py bar > bar/dg_bar_operations.c
CLEANFILES = dot/dg_dot_operations.c bar/dg_bar_operations.c


#----------------------------------------------------------------------------
# vgpreload_derivgrind-<platform>.so
#----------------------------------------------------------------------------
# adapted from the Memcheck Makefile.am

noinst_PROGRAMS += vgpreload_derivgrind-@VGCONF_ARCH_PRI@-@VGCONF_OS@.so
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += vgpreload_derivgrind-@VGCONF_ARCH_SEC@-@VGCONF_OS@.so
endif

if VGCONF_OS_IS_DARWIN
noinst_DSYMS = $(noinst_PROGRAMS)
endif

# dg_replace_math.c runs on the simulated CPU, and is built with
# AM_CFLAGS_PSO_* (see $(top_srcdir)/Makefile.all.am).
# Generate dg_replace_math.c by gen_replace_math.py.
VGPRELOAD_DERIVGRIND_SOURCES_COMMON = dg_replace_math.c

vgpreload_derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_SOURCES      = \
	$(VGPRELOAD_DERIVGRIND_SOURCES_COMMON)
vgpreload_derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
vgpreload_derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_CFLAGS       = \
	$(AM_CFLAGS_PSO_@VGCONF_PLATFORM_PRI_CAPS@) -O2
vgpreload_derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_LDFLAGS      = \
	$(PRELOAD_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@) 
vgpreload_derivgrind_@VGCONF_ARCH_PRI@_@VGCONF_OS@_so_LDADD      = -lm

if VGCONF_HAVE_PLATFORM_SEC
vgpreload_derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_SOURCES      = \
	$(VGPRELOAD_DERIVGRIND_SOURCES_COMMON)
vgpreload_derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
vgpreload_derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_CFLAGS       = \
	$(AM_CFLAGS_PSO_@VGCONF_PLATFORM_SEC_CAPS@) -O2
vgpreload_derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_LDFLAGS      = \
	$(PRELOAD_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@) 
vgpreload_derivgrind_@VGCONF_ARCH_SEC@_@VGCONF_OS@_so_LDADD      = -lm
endif

dg_replace_math.c: gen_replace_math.py
	python3 gen_replace_math.py
CLEANFILES += dg_replace_math.c

#----------------------------------------------------------------------------
# tape_evaluation
#----------------------------------------------------------------------------

bin_PROGRAMS = \
  tape-evaluation

tape_evaluation_SOURCES = bar/tape-evaluation.cpp

#----------------------------------------------------------------------------
# derivgrind-library-caller
#----------------------------------------------------------------------------
noinst_PROGRAMS += derivgrind-library-caller
derivgrind_library_caller_SOURCES = wrappers/library-caller/derivgrind-library-caller.cpp
derivgrind_library_caller_CPPFLAGS = -I. -I../include  # needs derivgrind.h and valgrind.h
derivgrind_library_caller_LDADD = -ldl

#----------------------------------------------------------------------------
# Compiled client request macros.
#----------------------------------------------------------------------------
pkglib_LIBRARIES = libderivgrind_clientrequests-x86.a
libderivgrind_clientrequests_x86_a_SOURCES = wrappers/compiled/derivgrind_clientrequests.c
libderivgrind_clientrequests_x86_a_CFLAGS = -I. -I../include -m32 # needs derivgrind.h and valgrind.h
pkglib_LIBRARIES += libderivgrind_clientrequests-amd64.a
libderivgrind_clientrequests_amd64_a_SOURCES = wrappers/compiled/derivgrind_clientrequests.c
libderivgrind_clientrequests_amd64_a_CFLAGS = -I. -I../include # needs derivgrind.h and valgrind.h

#----------------------------------------------------------------------------
# Fortran client request wrapper.
#----------------------------------------------------------------------------
# We build a .mod file, equivalent to C header files, which links to the C client request wrappers.
if ENABLE_FORTRAN
pkginclude_HEADERS += wrappers/fortran/derivgrind_clientrequests.mod
wrappers/fortran/derivgrind_clientrequests.mod: wrappers/fortran/derivgrind_clientrequests.f90
	cd wrappers/fortran && $(FC) $(AM_FCFLAGS) $(FCFLAGS) -c derivgrind_clientrequests.f90
CLEANFILES += wrappers/fortran/derivgrind_clientrequests.mod
endif 

