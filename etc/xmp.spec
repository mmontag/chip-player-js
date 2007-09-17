Name: xmp
Version: 2.2.1
Release: 1
Summary: A multi-format module player
Group: Sound
Source: %{name}-%{version}.tar.gz
License: GPL
URL: http://xmp.sourceforge.net/
Buildrequires: libalsa-devel >= 1.0.12, audacious-devel
BuildRoot: %{_tmppath}/%{name}-buildroot

%description
The Extended Module Player is a modplayer for Unix-like systems that plays
over 70 mainstream and obscure module formats from Amiga, Atari, Acorn,
Apple IIgs and PC, including Protracker (MOD), Scream Tracker 3 (S3M), Fast
Tracker II (XM) and Impulse Tracker (IT) files.

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

%prep
rm -rf %{buildroot}

%setup -q -n %{name}-%{version}

%build
./configure --prefix=/usr --enable-audacious-plugin
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

%files audacious
%defattr(-,root,root)
%doc README docs/COPYING docs/ChangeLog docs/CREDITS
%{_libdir}/audacious/Input/*
