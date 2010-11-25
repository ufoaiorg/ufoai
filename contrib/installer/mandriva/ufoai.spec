%define	oname	ufo

Name:		ufoai
Version:	2.3.1
Release:	%mkrel 1
URL:		http://ufoai.sourceforge.net/
Source0:	%{name}-%{version}-source.tar.bz2
License:	GPL
Group:		Games/Strategy
Summary:	UFO: Alien Invasion
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

%rename ufo

Requires:	%{name}-data = %{version}

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
%setup -q -n %{name}-%{version}-source

%build
./configure --enable-release --prefix=/usr
%make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall
%find_lang ufoai

%clean
rm -rf $RPM_BUILD_ROOT

%files -f ufoai.lang
%defattr(-,root,root)
%doc CONTRIBUTORS README
%{_bindir}/%{oname}
%{_bindir}/%{oname}ded
%{_datadir}/ufoai/*

%changelog
* Tue Mar 31 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- More spec cleanup, after tip from Per Øyvind Karlsen

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
