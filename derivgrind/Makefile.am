include $(top_srcdir)/Makefile.tool.am

#EXTRA_DIST = docs/dg-manual.xml

#----------------------------------------------------------------------------
# Headers
#----------------------------------------------------------------------------

pkginclude_HEADERS = \
  derivgrind.h bar/derivgrind-recording.h

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


#----------------------------------------------------------------------------
# tape_evaluation
#----------------------------------------------------------------------------

bin_PROGRAMS = \
  tape-evaluation

tape_evaluation_SOURCES = bar/tape-evaluation.cpp

