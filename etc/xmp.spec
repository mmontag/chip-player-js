%define enable_audacious 1
%define enable_bmp 0
%define enable_xmms 1

Name: xmp
Version: 2.4.0
Release: 1
Summary: A multi-format module player
Group: Sound
Source: %{name}-%{version}.tar.gz
License: GPL
URL: http://xmp.sourceforge.net/
Buildrequires: libalsa-devel
%if %enable_audacious
Buildrequires: audacious-devel
%endif
%if %enable_bmp
Buildrequires: beep-media-player-devel
%endif
%if %enable_xmms
Buildrequires: xmms-devel
%endif
BuildRoot: %{_tmppath}/%{name}-buildroot

%description
The Extended Module Player is a modplayer for Unix-like systems that plays
over 70 mainstream and obscure module formats from Amiga, Atari, Acorn,
Apple IIgs and PC, including Protracker (MOD), Scream Tracker 3 (S3M), Fast
Tracker II (XM) and Impulse Tracker (IT) files.

%if %enable_audacious
%package audacious
Summary: xmp plugin for Audacious
Group: Sound
Requires: audacious

%description audacious
The Extended Module Player is a modplayer for Unix-like systems that plays
over 70 mainstream and obscure module formats from Amiga, Atari, Acorn,
Apple IIgs and PC, including Protracker (MOD), Scream Tracker 3 (S3M), Fast
Tracker II (XM) and Impulse Tracker (IT) files.

This package contains the xmp plugin for the Audacious media player.
%endif

%if %enable_bmp
%package bmp
Summary: xmp plugin for the Beep Media Player
Group: Sound
Requires: beep-media-player

%description bmp
The Extended Module Player is a modplayer for Unix-like systems that plays
over 70 mainstream and obscure module formats from Amiga, Atari, Acorn,
Apple IIgs and PC, including Protracker (MOD), Scream Tracker 3 (S3M), Fast
Tracker II (XM) and Impulse Tracker (IT) files.

This package contains the xmp plugin for the Beep Media Player.
%endif

%if %enable_xmms
%package xmms
Summary: xmp plugin for XMMS
Group: Sound
Requires: xmms

%description xmms
The Extended Module Player is a modplayer for Unix-like systems that plays
over 70 mainstream and obscure module formats from Amiga, Atari, Acorn,
Apple IIgs and PC, including Protracker (MOD), Scream Tracker 3 (S3M), Fast
Tracker II (XM) and Impulse Tracker (IT) files.

This package contains the xmp plugin for XMMS.
%endif


%prep
rm -rf %{buildroot}

%setup -q

%build
PLUGINS=""
%if %enable_audacious
PLUGINS="$PLUGINS --enable-audacious-plugin"
%endif
%if %enable_bmp
PLUGINS="$PLUGINS --enable-bmp-plugin"
%endif
%if %enable_xmms
PLUGINS="$PLUGINS --enable-xmms-plugin"
%endif
./configure --prefix=/usr $PLUGINS
make

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc README docs/COPYING docs/README.* docs/ChangeLog docs/CREDITS
%config(noreplace) %{_sysconfdir}/*
%{_bindir}/*
%{_mandir}/man1/xmp.1*

%if %enable_audacious
%files audacious
%defattr(-,root,root)
%doc README docs/COPYING docs/ChangeLog docs/CREDITS
%{_libdir}/audacious/Input/*
%endif

%if %enable_bmp
%files bmp
%defattr(-,root,root)
%doc README docs/COPYING docs/ChangeLog docs/CREDITS
%{_libdir}/bmp/Input/*
%endif

%if %enable_xmms
%files xmms
%defattr(-,root,root)
%doc README docs/COPYING docs/ChangeLog docs/CREDITS
%{_libdir}/xmms/Input/*
%endif
