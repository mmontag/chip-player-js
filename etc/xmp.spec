# $Id: xmp.spec,v 1.2 2002-09-03 10:57:23 dmierzej Exp $

# bcond:
# _with_alsa		- with ALSA support
# _without_arts		- without aRts support
# _without_esound	- without EsounD support
# _with_nas		- with NAS support
# _without_oss		- without OSS support
# _without_xmms		- don't build xmms plugin
# _without_x11		- don't build x11 frontend
# _with_debug		- compile for debugging

%define name xmp
%define version 2.1.0
%define realver 2.1

%define cvsbuild 20020903

%define rel 0

%define cvs 1

%if%{cvs}
%define fversion %{cvsbuild}
%define release %{rel}.%{cvsbuild}.1
%else
%define fversion %{realver}
%define release %{rel}
%endif

%define _xmms_input_plugin_dir %(xmms-config --input-plugin-dir)
%{?_with_debug:%define         no_install_post_strip   1}

Summary: Extended Module Player
Name: %{name}
Version: %{version}
Release: %{release}
Vendor: Claudio Matsuoka <cmatsuoka@users.sourceforge.net>
URL: http://xmp.sourceforge.net
Group: Applications/Multimedia
Source: http://prdownloads.sourceforge.net/xmp/%{name}-%{fversion}.tar.bz2
Patch0: %{name}-warnings.patch
License: GPL
BuildRoot: %{_tmppath}/%{name}-%{version}-root
%{!?_without_x11:BuildRequires: XFree86-devel, gtk+-devel}
%{!?_without_xmms:BuildRequires: xmms-devel}
%{!?_without_esound:BuildRequires: esound-devel}
BuildRequires: audiofile-devel
%{!?_without_arts:BuildRequires: arts-devel}
%{?_with_alsa:BuildRequires: alsa-libs-devel}
%{?_with_nas:BuildRequires: nas-devel}
Requires: xmp-libs = %{version}
Requires: xmp-output = %{version}
Provides: xmp-player

%package libs
Summary: xmp library and common files needed for Extended Module Player
Group: Development/Libraries

%package libs-devel
Summary: xmp library headers
Group: Development/Libraries

%package libs-static
Summary: Static xmp library
Group: Development/Libraries

%package x11
Summary: X11 version of Extended Module Player
Group: Applications/Multimedia
Requires: xmp-libs = %{version}
Requires: xmp-output = %{version}
Provides: xmp-player

%package -n xmms-input-xmp
Summary: Extended Module Player XMMS input plugin
Group: Applications/Multimedia
Requires: xmms

%package output-wav
Summary: WAVE file output for Extended Module Player
Group: Applications/Multimedia
Requires: xmp-libs = %{version}
Provides: xmp-output

%package output-oss
Summary: OSS drivers for Extended Module Player
Group: Applications/Multimedia
Requires: xmp-libs = %{version}
Provides: xmp-output

%package output-esound
Summary: EsounD driver for Extended Module Player
Group: Applications/Multimedia
Requires: xmp-libs = %{version}
Provides: xmp-output

%package output-arts
Summary: aRts driver for Extended Module Player
Group: Applications/Multimedia
Requires: xmp-libs = %{version}
Provides: xmp-output

%package output-alsa
Summary: ALSA driver for Extended Module Player
Group: Applications/Multimedia
Requires: xmp-libs = %{version}
Provides: xmp-output

%package output-nas
Summary: NAS driver for Extended Module Player
Group: Applications/Multimedia
Requires: xmp-libs = %{version}
Provides: xmp-output

%description
This package contains xmp, the console player.

%description libs
xmp is a multi-format module player for UNIX. In machines with GUS or
AWE cards xmp takes advantage of the OSS sequencer to play modules with
virtually no system load. Using software mixing, xmp plays at sampling
rates up to 48kHz in mono or stereo, 8 or 16 bits, signed or unsigned,
little or big endian samples with 32 bit linear interpolation.

This package contains the dynamic library and config files.

%description libs-devel
This package contains the header files needed for development.

%description libs-static
This package contains the static library.

%description x11
This package contains xxmp, the XMP sound player with X11 support.

%description -n xmms-input-xmp
This package contains a XMP plugin based on xmp.

%description output-oss
This package contains OSS drivers (software and hardware mixer) for xmp.

%description output-wav
This package contains WAVE file output driver for xmp.

%description output-esound
This package contains an ESounD driver for xmp.

%description output-arts
This package contains an aRts driver for xmp.

%description output-alsa
This package contains an ALSA driver for xmp.

%description output-nas
This package contains a NAS driver for xmp.

%prep
%if %{cvs}
%setup -q -n xmp2
%else
%setup -q -n xmp-%{fversion}
%endif
%patch0 -p1 -b .warn

%build
%{__autoconf}
#%%configure \
%if%{?_with_debug:0}%{!?_with_debug:1}
CFLAGS="$RPM_OPT_FLAGS"
export CFLAGS
CXXFLAGS="$RPM_OPT_FLAGS"
export CXXFLAGS
%else
CFLAGS="-g"
%endif
./configure --prefix=%{_prefix} --exec-prefix=%{_prefix} --bindir=%{_bindir} --sbindir=%{_sbindir} --sysconfdir=%{_sysconfdir} --datadir=%{_datadir} --includedir=%{_includedir} --libdir=%{_libdir} --mandir=%{_mandir} \
			--enable-dynamic \
%{?_with_alsa:		--enable-alsa } \
%{!?_with_alsa:		--disable-alsa } \
%{!?_without_arts:	--enable-arts } \
%{?_without_arts:	--disable-arts } \
%{!?_without_esound:	--enable-esd } \
%{?_without_esound:	--disable-esd } \
%{?_with_nas:		--enable-nas } \
%{!?_with_nas:		--disable-nas } \
%{!?_without_oss:	--enable-oss } \
%{?_without_oss:	--disable-oss } \
%{!?_without_x11:	--with-x } \
%{?_without_x11:	--without-x } \
%{!?_without_xmms:	--enable-xmms } \
%{?_without_xmms:	--disable-xmms } \

make

%install
if [ $RPM_BUILD_ROOT!="/" ]; then
	rm -rf $RPM_BUILD_ROOT
fi
install -d $RPM_BUILD_ROOT{%{_sysconfdir},%{_includedir},%{_xmms_input_plugin_dir}}
make DEST_DIR=$RPM_BUILD_ROOT install %{!?_without_xmms:install-plugin}
install -m644 lib/libxmp.a $RPM_BUILD_ROOT%{_libdir}/
install -m644 src/include/xmp.h $RPM_BUILD_ROOT%{_includedir}/

%clean
rm -rf $RPM_BUILD_ROOT

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%files
%defattr(644,root,root,755)
%attr(755,root,root) %{_bindir}/xmp
%attr(755,root,root) %{_bindir}/mod2mp3
%{_mandir}/man1/xmp.*

%files libs
%defattr(644,root,root,755)
%doc README docs/README.*
%doc etc/magic etc/xmp.lsm
%doc docs/COPYING docs/CREDITS docs/ChangeLog
%doc docs/formats docs/adlib_sb.txt
%config(noreplace) %verify(not size mtime md5) %{_sysconfdir}/xmp.conf
%config %{_sysconfdir}/xmp-modules.conf
%attr(755,root,root) %{_libdir}/libxmp.so.%{realver}

%files libs-devel
%defattr(644,root,root,755)
%{_includedir}/xmp.h

%files libs-static
%defattr(644,root,root,755)
%{_libdir}/libxmp.a

%if%{?_without_x11:0}%{!?_without_x11:1}
%files x11
%defattr(644,root,root,755)
%attr(755,root,root) %{_bindir}/xxmp
%{_mandir}/man1/xxmp.*
%endif

%if%{?_without_xmms:0}%{!?_without_xmms:1}
%files -n xmms-input-xmp
%defattr(644,root,root,755)
%doc src/xmms/README
%attr(755,root,root) %{_xmms_input_plugin_dir}/xmp-plugin.so
%endif

%if%{?_without_oss:0}%{!?_without_oss:1}
%files output-oss
%attr(755,root,root) %{_libdir}/xmp/drivers/oss_*.so
%endif

%files output-wav
%attr(755,root,root) %{_libdir}/xmp/drivers/wav.so

%if%{?_without_esound:0}%{!?_without_esound:1}
%files output-esound
%attr(755,root,root) %{_libdir}/xmp/drivers/esd.so
%endif

%if%{?_without_arts:0}%{!?_without_arts:1}
%files output-arts
%attr(755,root,root) %{_libdir}/xmp/drivers/arts.so
%endif

%if%{?_with_alsa:1}%{!?_with_alsa:0}
%files output-alsa
%attr(755,root,root) %{_libdir}/xmp/drivers/alsa_mix.so
%endif

%if%{?_with_nas:1}%{!?_with_nas:0}
%files output-nas
%attr(755,root,root) %{_libdir}/xmp/drivers/nas.so
%endif

%changelog
* Tue Sep 03 2002 Dominik Mierzejewski <dominik@rangers.eu.org>
- updated to latest CVS snapshot
- removed patches which are now in upstream

* Fri Aug 30 2002 Dominik Mierzejewski <dominik@rangers.eu.org>
- fixed dynamic loading of drivers (finally!)
- spec file cleanups
- added wav.so driver

* Wed Aug 28 2002 Dominik Mierzejewski <dominik@rangers.eu.org>
- fixed xmms plugin building

* Sat Aug 03 2002 Dominik Mierzejewski <dominik@rangers.eu.org>
- updated to latest CVS snapshot
- spec file cleanups
- removed obsolete patches and workarounds
- added more subpackages
- renamed all xmp-driver-* packages to xmp-output-*

* Thu Nov 08 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- spec file cleanups
- fixed missing includes
- added aRts driver
- split OSS drivers from main package
- package descriptions cleanups
- renamed all packages containing drivers to xmp-driver-*
- prepare specfile for ALSA support (can someone with alsa-libs
  uncomment relevant lines and recompile?)

* Thu Aug 23 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- define the _xmms_input_plugin_dir macro within the specfile

* Fri Mar 16 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- update to 2.0.5pre3
- work around "XMP_LIB_PATH not set" bug

* Wed Mar 07 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- fixed *stupid* dynamic library loading bug

* Fri Jan 26 2001 Dominik Mierzejewski <dmierzej@elka.pw.edu.pl>
- new upstream release: 2.0.4
  * Added driver for synthesized sounds
  * Added Tatsuyuki Satoh's YM3812 emulator
  * Added support to The Player 6.0a modules (using Sylvain "Asle"
    Chipaux's P60A loader)
  * Added seek capability to XMMS plugin
  * Added (very) experimental AIX driver
  * Added support to dynamic linked drivers (for better packaging)
  * Fixed external drivers problem with the XMMS plugin (reported
    by greg <gjones@computelnet.com>)
  * Started adding support to MED 1.11, 1.12, 2.00 and 3.22
  * Replaced RPM spec with Dominik Mierzejewski's version
- new package layout: xmp-common, xmp, xmp-x11, xmp-xmms, xmp-esound
- alsa support still disabled in binary packages, because I don't have
  alsa installed - get SRPM and rebuild if you want it

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
