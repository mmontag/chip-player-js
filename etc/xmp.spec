Name: xmp
Version: 2.2.0
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

%prep
%setup -q -n %{name}-%{version}-pre4

%build
%configure --enable audacious-plugin
%make

%install
rm -rf %buildroot
make install DESTDIR=%{buildroot}

%clean
rm -rf %buildroot

%files
%defattr(0755,root,root,0755)
%{_bindir}/*
%defattr(0644,root,root,0755)
%{_sysconfdir}/*
%{_mandir}/*
%doc README docs/COPYING docs/README.* docs/ChangeLog docs/CREDITS
