Summary: Stop Bill from loading his OS into all the computers.
Name: xbill
Version: 2.1
Release: 1
Copyright: GPL
Group: Amusements/Games
Source: ftp://ftp.xbill.org/pub/xbill/%{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-root

%description
The xbill game tests your reflexes as you seek out and destroy all
forms of Bill, establish a new operating system throughout the
universe, and boldly go where no geek has gone before.  Xbill has
become an increasingly attractive option as the Linux Age progresses,
and it is very popular at Red Hat.

%prep
%setup -q

%build
./configure --prefix=/usr --localstatedir=/var/lib/games --disable-motif
make

%install
rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT install

( cd $RPM_BUILD_ROOT
  mkdir -p ./etc/X11/applnk/Games
  cat > ./etc/X11/applnk/Games/xbill.desktop <<EOF
[Desktop Entry]
Name=xbill
Type=Application
Description=Save the world
Exec=xbill
EOF
)

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/usr/bin/xbill
%config(noreplace) /var/lib/games/xbill/scores
/usr/share/xbill
%config /etc/X11/applnk/Games/xbill.desktop

%changelog
* Sun Oct 28 2001 Brian Wellington <bwelling@xbill.org>
- Updated to 2.1

* Sun Jun 24 2001 Elliot Lee <sopwith@redhat.com>
- Bump release + rebuild.

* Fri Apr 27 2001 Bill Nottingham <notting@redhat.com>
- rebuild for C++ exception handling on ia64

* Wed Oct 18 2000 Than Ngo <than@redhat.com>
- rebuilt against gcc-2.96-60

* Tue Jul 18 2000 Than Ngo <than@redhat.de>
- rebuilt with gcc-2.96-4.0

* Thu Jul 13 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Tue Jul  2 2000 Jakub Jelinek <jakub@redhat.com>
- Rebuild with new C++

* Sun Jun 18 2000 Than Ngo <than@redhat.de>
- rebuilt in the new build environment
- use RPM maccros

* Mon May 08 2000 Preston Brown <pbrown@redhat.com>
- fix for gcc 2.95 from t-matsuu@protein.osaka-u.ac.jp.

* Mon Feb 07 2000 Preston Brown <pbrown@redhat.com>
- rebuild with config(noreplace) score file, new description
- replace wmconfig with desktop file

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com> 
- auto rebuild in the new build environment (release 6)

* Thu Dec 17 1998 Michael Maher <mike@redhat.com>
- built pacakge for 6.0

* Fri May 01 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr, tr

* Fri Aug 22 1997 Erik Troan <ewt@redhat.com>
- built against glibc
