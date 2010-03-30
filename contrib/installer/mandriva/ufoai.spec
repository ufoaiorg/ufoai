%define	name	ufo
%define	version	2.2.1
%define	release	%mkrel 1
%define	Summary	UFO: Alien Invasion

Name:		%{name}
Version:	%{version}
Release:	%{release}
URL:		http://ufoai.sourceforge.net/
Source0:	%{name}ai-%{version}-source.tar.bz2
License:	GPL
Group:		Games/Strategy
Summary:	%{Summary}
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

Requires:	%{name}-data = %{version}
Requires:	libcurl4
Requires:	libjpeg62
Requires:	libSDL1 >= 1.2.10
Requires:	libSDL_image1 >= 1.2.7
Requires:	libSDL_mixer1 >= 1.2.7
Requires:	libSDL_ttf2 >= 2.0.7
Requires:	libvorbis
Requires:	zlib1 >= 1.2.3

BuildRequires:  libcurl-devel
BuildRequires:  libjpeg62-devel
BuildRequires:  libSDL-devel >= 1.2.10
BuildRequires:  libSDL_image-devel >= 1.2.7
BuildRequires:  libSDL_mixer-devel >= 1.2.7
BuildRequires:  libSDL_ttf-devel >= 2.0.7
BuildRequires:  libvorbis-devel
BuildRequires:  zlib1-devel >= 1.2.3

%description
UFO: ALIEN INVASION is a strategy game featuring tactical combat against
hostile alien forces which are about to infiltrate earth at this very
moment. You are in command of a small special unit which has been
founded to face the alien strike force. To be successful on the long
run, you will also have to have a research team study the aliens and
their technologies in order to learn as much as possible about their
technology, their goals and the aliens themselves. 'UFO: Alien Invasion'
is heavily inspired by the 'X-COM' series by Mythos and Microprose.

%prep
%setup -q -n %{name}ai-%{version}-source

%build
./configure --enable-release --prefix=/usr
%make

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

%post
%update_menus

%postun
%clean_menus

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0755,root,root)
%doc CONTRIBUTORS COPYING README
%{_bindir}/%{name}
%{_bindir}/%{name}ded
/usr/share/ufoai/base/game.so
/usr/share/ufoai/base/i18n/cs/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/da/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/de/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/el/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/en/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/es/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/es_ES/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/est/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/fi/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/fr/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/it/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/ja/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/pl/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/pt_BR/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/ru/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/slo/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/sv/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/base/i18n/th/LC_MESSAGES/ufoai.mo
/usr/share/ufoai/ufo
/usr/share/ufoai/ufo2map
/usr/share/ufoai/ufoded

%changelog
* Tue Mar 30 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- Upgraded the package.
- spec cleanup

* Fri Jan 27 2006 Guillaume Rousse <guillomovitch@zarb.org> 0.10-4plf
- %%mkrel 
- spec cleanup

* Sun Nov 28 2004 Guillaume Rousse <guillomovitch@zarb.org> 0.10-3plf
- fix macros 
- PLF reason

* Tue Sep 28 2004 Guillaume Rousse <guillomovitch@zarb.org> 0.10-2plf
- moved to plf, as it is useless without its data

* Tue Mar 02 2004 Per Øyvind Karlsen <peroyvind@linux-mandrake.com> 0.10-2mdk
- buildrequires

* Fri Feb 20 2004 Per Øyvind Karlsen <peroyvind@linux-mandrake.com> 0.10-1mdk
- initial mdk release
