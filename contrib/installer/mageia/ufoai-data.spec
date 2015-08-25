%define	oname	ufoai
%define subrel	1

Name:		%{oname}-data
Version:	2.5
Release:	%mkrel 1
URL:		http://ufoai.org/
Source0:	http://prdownloads.sourceforge.net/%{oname}/%{oname}-%{version}-data.tar
License:	GPLv2+ and CC-BY and CC-BY-SA and GFDL and MIT and Public Domain
Group:		Games/Strategy
Summary:	Data files for %{oname}
BuildArch:	noarch
Suggests:	%{oname}

%description
Data files needed to run %{oname}.


%prep

%build

%install

mkdir -p %{buildroot}%{_gamesdatadir}/%{oname}
tar -xf %{SOURCE0} -C %{buildroot}%{_gamesdatadir}/%{oname}
install -m 644 %{SOURCE1} %{buildroot}%{_gamesdatadir}/%{oname}/base


%files
%{_gamesdatadir}/%{oname}/*

%changelog
* Sat Dec 27 2014 Johnny A. Solbu <johnny@solbu.net> 2.5-1.1.solbu3
- Bugfix. Add missing maps.

* Sat Jul 26 2014 Johnny A. Solbu <johnny@solbu.net> 2.5-1.solbu3
- New version

* Fri May 04 2012 Johnny A. Solbu <solbu@mandriva.org> 2.4-1.solbu2010.2
- New version

* Mon May 09 2011 Johnny A. Solbu <solbu@mandriva.org> 2.3.1-1mdv
- New version
- License changes. See "LICENSES" file for details.
- Bugfix. One of the maps jail soldiers inside the aircraft.

* Fri Sep 24 2010 Per Øyvind Karlsen <proyvind@mandriva.com> 2.3-2mdv
- Spec cleanup

* Fri Jul 16 2010 Johnny A. Solbu <johnny@solbu.net> 2.3-1mdv
- Upgraded the package.

* Tue Jul 5 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- Bugfix. the shellscripts (/usr/bin/{ufo,ufoded}) tried to launch the game from RPM_BUILD_ROOT.

* Thu Apr 2 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- Added menu entry.

* Tue Mar 31 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- More spec cleanup, after tip from Per Øyvind Karlsen.

* Tue Mar 30 2010 Johnny A. Solbu <johnny@solbu.net> 2.2.1-1mdv
- Upgraded the package.
- spec cleanup.

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
