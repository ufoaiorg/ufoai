%define oname   ufoai

Name:		ufoai-data
Version:	2.3.1
Release:	%mkrel 1
URL:		http://ufoai.sourceforge.net/
Source0:	%{oname}-%{version}-data.tar
License:	GPL
Group:		Games/Strategy
Summary:	Datafiles for %{oname}
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot

Requires:	%{oname}

%description
Datafiles needed to run ufoai


%prep
%setup -q -n base

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/share/ufoai/base
cp -p *pk3 $RPM_BUILD_ROOT/usr/share/ufoai/base/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_datadir}/ufoai/*

%changelog
* Tue Mar 31 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- Renamed to ufoai.
- Some spec cleanup

* Tue Mar 30 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- Upgraded the package, which now have free licenses.

* Fri Feb 20 2004 Per Øyvind Karlsen <peroyvind@sintrax.net> 0.10-1plf
- initial plf release
