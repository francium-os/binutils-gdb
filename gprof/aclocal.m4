dnl aclocal.m4 generated automatically by aclocal 1.2

# Do all the work for Automake.  This macro actually does too much --
# some checks are only needed if your package does certain things.
# But this isn't really a big deal.

# serial 1

dnl Usage:
dnl AM_INIT_AUTOMAKE(package,version, [no-define])

AC_DEFUN(AM_INIT_AUTOMAKE,
[AC_REQUIRE([AM_PROG_INSTALL])
PACKAGE=[$1]
AC_SUBST(PACKAGE)
VERSION=[$2]
AC_SUBST(VERSION)
dnl test to see if srcdir already configured
if test "`cd $srcdir && pwd`" != "`pwd`" && test -f $srcdir/config.status; then
  AC_MSG_ERROR([source directory already configured; run "make distclean" there first])
fi
ifelse([$3],,
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
AC_DEFINE_UNQUOTED(VERSION, "$VERSION"))
AM_SANITY_CHECK
AC_ARG_PROGRAM
dnl FIXME This is truly gross.
missing_dir=`cd $ac_aux_dir && pwd`
AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)
AM_MISSING_PROG(MAKEINFO, makeinfo, $missing_dir)
AC_PROG_MAKE_SET])


# serial 1

AC_DEFUN(AM_PROG_INSTALL,
[AC_REQUIRE([AC_PROG_INSTALL])
test -z "$INSTALL_SCRIPT" && INSTALL_SCRIPT='${INSTALL_PROGRAM}'
AC_SUBST(INSTALL_SCRIPT)dnl
])

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environment is sane])
# Just in case
sleep 1
echo timestamp > conftestfile
# Do `set' in a subshell so we don't clobber the current shell's
# arguments.  Must try -L first in case configure is actually a
# symlink; some systems play weird games with the mod time of symlinks
# (eg FreeBSD returns the mod time of the symlink's containing
# directory).
if (
   set X `ls -Lt $srcdir/configure conftestfile 2> /dev/null`
   if test "$@" = "X"; then
      # -L didn't work.
      set X `ls -t $srcdir/configure conftestfile`
   fi
   test "[$]2" = conftestfile
   )
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

dnl AM_MISSING_PROG(NAME, PROGRAM, DIRECTORY)
dnl The program must properly implement --version.
AC_DEFUN(AM_MISSING_PROG,
[AC_MSG_CHECKING(for working $2)
# Run test in a subshell; some versions of sh will print an error if
# an executable is not found, even if stderr is redirected.
# Redirect stdin to placate older versions of autoconf.  Sigh.
if ($2 --version) < /dev/null > /dev/null 2>&1; then
   $1=$2
   AC_MSG_RESULT(found)
else
   $1="$3/missing $2"
   AC_MSG_RESULT(missing)
fi
AC_SUBST($1)])


# serial 19 AM_PROG_LIBTOOL
AC_DEFUN(AM_PROG_LIBTOOL,
[AC_REQUIRE([AC_CANONICAL_HOST])
AC_REQUIRE([AC_PROG_RANLIB])
AC_REQUIRE([AC_PROG_CC])
AC_REQUIRE([AM_PROG_LD])
AC_REQUIRE([AM_PROG_NM])
AC_REQUIRE([AC_PROG_LN_S])

# Always use our own libtool.
LIBTOOL='$(SHELL) $(top_builddir)/libtool'
AC_SUBST(LIBTOOL)

dnl CYGNUS LOCAL arguments to enable-shared
dnl Allow the --disable-shared flag to stop us from building shared libs.
AC_ARG_ENABLE(shared,
[  --enable-shared         build shared libraries [default=yes]],
[p=${PACKAGE-bogus-package-name}
 case "$enableval" in
  yes) libtool_enable_shared=yes ;;
  no) libtool_enable_shared=no ;;
  # The value of $p (aka $PACKAGE) is assumed to come from AM_INIT_AUTOMAKE.
  # If it didn't, it'll be `bogus-package-name', thus making this condition
  #  not be used.
  *$p*) libtool_enable_shared=yes ;;
  *) libtool_enable_shared=no ;;
esac])
test -n "$libtool_enable_shared" && enable_shared="$libtool_enable_shared"
libtool_shared=
test "$enable_shared" = no && libtool_shared=" --disable-shared"
dnl END CYGNUS LOCAL

dnl Allow the --disable-static flag to stop us from building static libs.
AC_ARG_ENABLE(static,
[  --enable-static         build static libraries [default=yes]],
[if test "$enableval" = no; then
  libtool_enable_static=no
else
  libtool_enable_static=yes
fi])
test -n "$libtool_enable_static" && enable_static="$libtool_enable_static"
libtool_static=
test "$enable_static" = no && libtool_static=" --disable-static"

libtool_flags="$libtool_shared$libtool_static"
test "$silent" = yes && libtool_flags="$libtool_flags --silent"
test "$ac_cv_prog_gcc" = yes && libtool_flags="$libtool_flags --with-gcc"
test "$ac_cv_prog_gnu_ld" = yes && libtool_flags="$libtool_flags --with-gnu-ld"

# Some flags need to be propagated to the compiler or linker for good
# libtool support.
case "$host" in
*-*-irix*)
  # Find out which ABI we are using.
  echo '[#]line __oline__ "configure"' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case "`/usr/bin/file conftest.o`" in
    *32-bit*)
      LD="${LD-ld} -32"
      ;;
    *N32*)
      LD="${LD-ld} -n32"
      ;;
    *64-bit*)
      LD="${LD-ld} -64"
      ;;
    esac
  fi
  rm -rf conftest*
  ;;

*-*-sco3.2v5*)
  # On SCO OpenServer 5, we need -belf to get full-featured binaries.
  CFLAGS="$CFLAGS -belf"
  ;;
esac

# Actually configure libtool.  ac_aux_dir is where install-sh is found.
CC="$CC" CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" \
LD="$LD" NM="$NM" RANLIB="$RANLIB" LN_S="$LN_S" \
${CONFIG_SHELL-/bin/sh} $ac_aux_dir/ltconfig \
$libtool_flags --no-verify $ac_aux_dir/ltmain.sh $host \
|| AC_MSG_ERROR([libtool configure failed])
])

# AM_PROG_LD - find the path to the GNU or non-GNU linker
AC_DEFUN(AM_PROG_LD,
[AC_ARG_WITH(gnu-ld,
[  --with-gnu-ld           assume the C compiler uses GNU ld [default=no]],
test "$withval" = no || with_gnu_ld=yes, with_gnu_ld=no)
AC_REQUIRE([AC_PROG_CC])
ac_prog=ld
if test "$ac_cv_prog_gcc" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by GCC])
  ac_prog=`($CC -print-prog-name=ld) 2>&5`
  case "$ac_prog" in
  # Accept absolute paths.
  /*)
    test -z "$LD" && LD="$ac_prog"
    ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test "$with_gnu_ld" = yes; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL(ac_cv_path_LD,
[if test -z "$LD"; then
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  for ac_dir in $PATH; do
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog"; then
      ac_cv_path_LD="$ac_dir/$ac_prog"
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some GNU ld's only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      if "$ac_cv_path_LD" -v 2>&1 < /dev/null | egrep '(GNU|with BFD)' > /dev/null; then
	test "$with_gnu_ld" != no && break
      else
        test "$with_gnu_ld" != yes && break
      fi
    fi
  done
  IFS="$ac_save_ifs"
else
  ac_cv_path_LD="$LD" # Let the user override the test with a path.
fi])
LD="$ac_cv_path_LD"
if test -n "$LD"; then
  AC_MSG_RESULT($LD)
else
  AC_MSG_RESULT(no)
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
AC_SUBST(LD)
AM_PROG_LD_GNU
])

AC_DEFUN(AM_PROG_LD_GNU,
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], ac_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  ac_cv_prog_gnu_ld=yes
else
  ac_cv_prog_gnu_ld=no
fi])
])

# AM_PROG_NM - find the path to a BSD-compatible name lister
AC_DEFUN(AM_PROG_NM,
[AC_MSG_CHECKING([for BSD-compatible nm])
AC_CACHE_VAL(ac_cv_path_NM,
[case "$NM" in
/*)
  ac_cv_path_NM="$NM" # Let the user override the test with a path.
  ;;
*)
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  for ac_dir in /usr/ucb /usr/ccs/bin $PATH /bin; do
    test -z "$ac_dir" && dir=.
    if test -f $ac_dir/nm; then
      # Check to see if the nm accepts a BSD-compat flag.
      # Adding the `sed 1!d' prevents false positives on HP-UX, which says:
      #   nm: unknown option "B" ignored
      if ($ac_dir/nm -B /dev/null 2>&1 | sed '1!d'; exit 0) | egrep /dev/null >/dev/null; then
        ac_cv_path_NM="$ac_dir/nm -B"
      elif ($ac_dir/nm -p /dev/null 2>&1 | sed '1!d'; exit 0) | egrep /dev/null >/dev/null; then
        ac_cv_path_NM="$ac_dir/nm -p"
      else
        ac_cv_path_NM="$ac_dir/nm"
      fi
      break
    fi
  done
  IFS="$ac_save_ifs"
  test -z "$ac_cv_path_NM" && ac_cv_path_NM=nm
  ;;
esac])
NM="$ac_cv_path_NM"
AC_MSG_RESULT([$NM])
AC_SUBST(NM)
])

# Like AC_CONFIG_HEADER, but automatically create stamp file.

AC_DEFUN(AM_CONFIG_HEADER,
[AC_PREREQ([2.12])
AC_CONFIG_HEADER([$1])
dnl When config.status generates a header, we must update the stamp-h file.
dnl This file resides in the same directory as the config header
dnl that is generated.  We must strip everything past the first ":",
dnl and everything past the last "/".
AC_OUTPUT_COMMANDS(changequote(<<,>>)dnl
ifelse(patsubst(<<$1>>, <<[^ ]>>, <<>>), <<>>,
<<test -z "<<$>>CONFIG_HEADERS" || echo timestamp > patsubst(<<$1>>, <<^\([^:]*/\)?.*>>, <<\1>>)stamp-h<<>>dnl>>,
<<am_indx=1
for am_file in <<$1>>; do
  case " <<$>>CONFIG_HEADERS " in
  *" <<$>>am_file "*<<)>>
    echo timestamp > `echo <<$>>am_file | sed -e 's%:.*%%' -e 's%[^/]*$%%'`stamp-h$am_indx
    ;;
  esac
  am_indx=`expr "<<$>>am_indx" + 1`
done<<>>dnl>>)
changequote([,]))])

# Add --enable-maintainer-mode option to configure.
# From Jim Meyering

# serial 1

AC_DEFUN(AM_MAINTAINER_MODE,
[AC_MSG_CHECKING([whether to enable maintainer-specific portions of Makefiles])
  dnl maintainer-mode is disabled by default
  AC_ARG_ENABLE(maintainer-mode,
[  --enable-maintainer-mode enable make rules and dependencies not useful
                          (and sometimes confusing) to the casual installer],
      USE_MAINTAINER_MODE=$enableval,
      USE_MAINTAINER_MODE=no)
  AC_MSG_RESULT($USE_MAINTAINER_MODE)
  if test $USE_MAINTAINER_MODE = yes; then
    MAINT=
  else
    MAINT='#M#'
  fi
  AC_SUBST(MAINT)dnl
]
)

# Check to see if we're running under Cygwin32, without using
# AC_CANONICAL_*.  If so, set output variable CYGWIN32 to "yes".
# Otherwise set it to "no".

dnl AM_CYGWIN32()
AC_DEFUN(AM_CYGWIN32,
[AC_CACHE_CHECK(for Cygwin32 environment, am_cv_cygwin32,
[AC_TRY_COMPILE(,[return __CYGWIN32__;],
am_cv_cygwin32=yes, am_cv_cygwin32=no)
rm -f conftest*])
CYGWIN32=
test "$am_cv_cygwin32" = yes && CYGWIN32=yes])

# Check to see if we're running under Win32, without using
# AC_CANONICAL_*.  If so, set output variable EXEEXT to ".exe".
# Otherwise set it to "".

dnl AM_EXEEXT()
dnl This knows we add .exe if we're building in the Cygwin32
dnl environment. But if we're not, then it compiles a test program
dnl to see if there is a suffix for executables.
AC_DEFUN(AM_EXEEXT,
[AC_REQUIRE([AM_CYGWIN32])
AC_REQUIRE([AM_MINGW32])
AC_MSG_CHECKING([for executable suffix])
AC_CACHE_VAL(am_cv_exeext,[
if test "$CYGWIN32" = yes -o "$MINGW32" = yes; then
am_cv_exeext=.exe
else
cat > am_c_test.c << 'EOF'
int main() {
/* Nothing needed here */
}
EOF
${CC-cc} -o am_c_test $CFLAGS $CPPFLAGS $LDFLAGS am_c_test.c $LIBS 1>&5
am_cv_exeext=`echo am_c_test.* | grep -v am_c_test.c | sed -e s/am_c_test//`
rm -f am_c_test*])
test x"${am_cv_exeext}" = x && am_cv_exeext=no
fi
EXEEXT=""
test x"${am_cv_exeext}" != xno && EXEEXT=${am_cv_exeext}
AC_MSG_RESULT(${am_cv_exeext})
AC_SUBST(EXEEXT)])

# Check to see if we're running under Mingw, without using
# AC_CANONICAL_*.  If so, set output variable MINGW32 to "yes".
# Otherwise set it to "no".

dnl AM_MINGW32()
AC_DEFUN(AM_MINGW32,
[AC_CACHE_CHECK(for Mingw32 environment, am_cv_mingw32,
[AC_TRY_COMPILE(,[return __MINGW32__;],
am_cv_mingw32=yes, am_cv_mingw32=no)
rm -f conftest*])
MINGW32=
test "$am_cv_mingw32" = yes && MINGW32=yes])

