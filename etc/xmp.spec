%define name xmp
%define version 2.0.3
%define release 3

Summary: Extended Module Player
Name: %{name}
Version: %{version}
Release: %{release}
Packager: Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
Vendor: Claudio Matsuoka <claudio@helllabs.org>
URL: http://xmp.helllabs.org
Group: Applications/Multimedia
Source: http://xmp.helllabs.org/pkg/latest/%{name}-%{version}.tar.bz2
Patch0: xmp-gcc-2.96.patch
Copyright: GPL
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: XFree86-devel xmms-devel gtk+-devel

%package X11
Summary: X11 version of Extended Module Player
Group: Applications/Multimedia
Requires: XFree86-libs

%package xmms
Summary: Extended Module Player XMMS input plugin
Group: Applications/Multimedia
Requires: xmms

%description
xmp is a multi-format module player for UNIX. In machines with GUS or
AWE cards xmp takes advantage of the OSS sequencer to play modules with
virtually no system load. Using software mixing, xmp plays at sampling
rates up to 48kHz in mono or stereo, 8 or 16 bits, signed or unsigned,
little or big endian samples with 32 bit linear interpolation.

%description X11
xmp is a multi-format module player for UNIX. In machines with GUS or
AWE cards xmp takes advantage of the OSS sequencer to play modules with
virtually no system load. Using software mixing, xmp plays at sampling
rates up to 48kHz in mono or stereo, 8 or 16 bits, signed or unsigned,
little or big endian samples with 32 bit linear interpolation.

This package contains xxmp, the XMP sound player with X11 support.

%description xmms
xmp is a multi-format module player for UNIX. In machines with GUS or
AWE cards xmp takes advantage of the OSS sequencer to play modules with
virtually no system load. Using software mixing, xmp plays at sampling
rates up to 48kHz in mono or stereo, 8 or 16 bits, signed or unsigned,
little or big endian samples with 32 bit linear interpolation.

This package contains a xmms plugin based on xmp.

%prep
%setup -q
%patch0 -p1 -b .gcc

%build
CFLAGS="$RPM_OPT_FLAGS $CFLAGS" \
./configure --prefix=%{_prefix} \
  --disable-alsa --with-x
make

%install
if [ $RPM_BUILD_ROOT!="/" ]; then
	rm -rf $RPM_BUILD_ROOT
fi
mkdir -p $RPM_BUILD_ROOT/etc
mkdir -p $RPM_BUILD_ROOT`xmms-config --input-plugin-dir`
make install DEST_DIR=$RPM_BUILD_ROOT MAN_DIR=$RPM_BUILD_ROOT%{_mandir}/man1

%clean
rm -rf $RPM_BUILD_ROOT

%post

%files
%defattr(-,root,root)
%doc README docs/README.* etc/magic etc/xmp.lsm docs/ChangeLog docs/COPYING docs/CREDITS mod.*
%config /etc/xmp.conf
%config /etc/xmp-modules.conf
%{_bindir}/xmp
%{_mandir}/man1/xmp.*

%files X11
%defattr(-,root,root)
%doc README docs/README.* etc/magic etc/xmp.lsm docs/ChangeLog docs/COPYING docs/CREDITS mod.*
%config /etc/xmp.conf
%config /etc/xmp-modules.conf
%{_bindir}/xxmp
%{_mandir}/man1/xxmp.*

%files xmms
%defattr(-,root,root)
%doc src/xmms/README docs/ChangeLog docs/COPYING docs/CREDITS
%{_xmms_input_plugin_dir}/libxmp.so

%changelog
* Fri Jan 12 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- fixed %install section
- removed redundant install paths patch

* Sun Jan 07 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- a small patch to remove stupid warning in src/xmms/plugin.c
- merged distributed changelog
- merged distributed spec
- added a build-time autoconfiguration of xmms' input plugin dir
  (requires my custom macro in ~/.rpmmacros)
- changed Group: fields to valid group from current GROUPS file (rpm-4.0-4)

* Sat Jan 06 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- new upstream release: 2.0.3
  * fixes in configure.in
  * support for gcc 2.96 (possibly incomplete)
  * support for RAR packed files
  * improved powerpacker decrunching
  * IT lowpass filters in the software mixer
  * misc minor bugfixes
- removed my gcc.patch (merged into the main source)
- updated xmms-install-paths.patch and spec %install section
- new subpackage descriptions
- added new doc files to %files sections

* Mon Nov 20 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- corrected X11 subpackage Requires: to XFree86-libs
- removed X11 subpackage dependency on textmode version

* Sun Oct 08 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- added a patch to compile under gcc 2.96 (still needs some work)
- modified paths to be FHS compatible

* Mon May 15 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- split the xmms plugin into a separate subpackage

* Mon May 15 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- split the X11 version into a separate subpackage
- added build dependency for xmms-devel and XFree86-devel

* Mon May 15 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- new upstream release: 2.0.2
  * fixed sample size for MED synth instruments
  * fixed the set offset effect for (offset > sample length) bug
  * fixed S3M tone portamento bug introduced in 2.0.1
  * fixed option --fix-sample-loops
  * improved Noisetracker and Octalyser module detection
  * fixed UNIC tracker and Mod's Grave module detection
  * fixed Protracker song detection
  * fixed event loading in S3M
  * fixed ALSA 0.5 driver
  * removed calls to tempnam(3)

* Thu Feb 24 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- new upstream release: 2.0.1
  * endianism problems in Linux/PPC (Amiga) fixed
  * added enhanced NetBSD/OpenBSD drivers
  * fixed sample loop detection bug in the MOD loader
  * ALSA 0.5 support fixes
  * added extra sanity tests for 15 instrument MODs
  * fixed pathname for Protracker song sample loading
  * fixed XM loader for nonstandard mods
  * added workaround for IT fine global volume slides
  * added support for EXO4/EXO8 Startrekker/Audio Sculpture modules
  * added support for Soundtracker 2.6/Ice Tracker modules
  * MED synth instruments MUCH better now (but still far from perfection)
  * fixed S3M instrument retriggering on portamento bug

* Fri Feb 04 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- updated to 2.0.0 final
- changed the etc/Makefile patch to work with new version

* Fri Jan 14 2000 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- fixed .spec and rebuilt for RH6.1
- added a patch for etc/Makefile to put conf files in $RPM_BUILD_ROOT

* Thu Sep 16 1999 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- built package for RH6 (no ALSA support)
