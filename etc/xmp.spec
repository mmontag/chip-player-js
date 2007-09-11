Name: xmp
Version: 2.2.0
Release: %mkrel 0.pre6
Summary: A multi-format module player
Group: Sound
Source: %{name}-%{version}-pre6.tar.gz
License: GPL
URL: http://xmp.sourceforge.net/
Buildrequires: libalsa2-devel >= 1.0.12
BuildRoot: %{_tmppath}/%{name}-buildroot

%description
The Extended Module Player is a modplayer for Unix-like systems that plays
over 70 mainstream and obscure module formats from Amiga, Atari, Acorn and
PC, including Protracker (MOD), Scream Tracker 3 (S3M), Fast Tracker II (XM)
and Impulse Tracker (IT) files.

%prep
%setup -q -n %{name}-%{version}-pre4

%build
%configure
%make

%install
rm -rf %buildroot
#make install DESTDIR=%{buildroot}
install -D src/main/xmp %{buildroot}%{_bindir}/xmp
install -D etc/xmp.conf %{buildroot}%{_sysconfdir}/xmp/xmp.conf
install -D etc/xmp-modules.conf %{buildroot}%{_sysconfdir}/xmp/xmp-modules.conf
install -D docs/xmp.1 %{buildroot}%{_mandir}/man1/xmp.1

%clean
rm -rf %buildroot

%files
%defattr(0755,root,root,0755)
%{_bindir}/*
%defattr(0644,root,root,0755)
%{_sysconfdir}/*
%{_mandir}/*
%doc README docs/COPYING docs/README.* docs/ChangeLog docs/CREDITS

