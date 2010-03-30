%define	name	ufo-data
%define	version	2.2.1
%define	release	%mkrel 1
%define	Summary	Datafiles for UFO: Alien Invasion

Name:		%{name}
Version:	%{version}
Release:	%{release}
URL:		http://ufoai.sourceforge.net/
Source0:	ufoai-%{version}-data.tar
License:	GPL
Group:		Games/Strategy
Summary:	%{Summary}
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
This is the datafiles needed to run UFO: ALIEN INVASION.

%prep
%setup -q -n base

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/share/ufoai/base
cp -p *pk3 $RPM_BUILD_ROOT/usr/share/ufoai/base/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(0755,root,root)
/usr/share/ufoai/base/0base.pk3
/usr/share/ufoai/base/0maps.pk3
/usr/share/ufoai/base/0media.pk3
/usr/share/ufoai/base/0models.pk3
/usr/share/ufoai/base/0music.pk3
/usr/share/ufoai/base/0pics.pk3
/usr/share/ufoai/base/0snd.pk3
/usr/share/ufoai/base/0ufos.pk3

%changelog
* Tue Mar 30 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- Upgraded the package, which now have free licenses.
- Created new spec file, didn't find an old one.
