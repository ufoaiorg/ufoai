Name:		ufoai
Version:	2.5
Release:	%mkrel 3
URL:		http://ufoai.org/
Source0:	http://downloads.sourceforge.net/%{name}/%{name}-%{version}-source.tar.bz2
Source1:	%{name}.desktop
Source2:	uforadiant.desktop
License:	GPLv2+
Group:		Games/Strategy
Summary:	UFO: Alien Invasion
Requires:	%{name}-data = %{version}

BuildRequires:	pkgconfig(libcurl)
BuildRequires:	desktop-file-utils
BuildRequires:	pkgconfig(sdl) >= 1.2.10
BuildRequires:	pkgconfig(SDL_image)  >= 1.2.7
BuildRequires:	pkgconfig(SDL_mixer) >= 1.2.7
BuildRequires:	SDL_ttf-devel >= 2.0.7
BuildRequires:	pkgconfig(openal)
BuildRequires:	pkgconfig(gtksourceview-2.0)
BuildRequires:	pkgconfig(gdkglext-x11-1.0)
BuildRequires:	gtk2-devel
BuildRequires:	mxml-devel
# xvid is in tainted, but we can build without support, adding dlopen() support and
# a suggests on it could be done though..
#BuildRequires:	xvid-devel
BuildRequires:	pkgconfig(theoradec)
BuildRequires:	zip
BuildRequires:	texlive

%description
UFO: ALIEN INVASION is a strategy game featuring tactical combat against
hostile alien forces which are about to infiltrate earth at this very
moment. You are in command of a small special unit which has been
founded to face the alien strike force. To be successful on the long
run, you will also have to have a research team study the aliens and
their technologies in order to learn as much as possible about their
technology, their goals and the aliens themselves. 'UFO: Alien Invasion'
is heavily inspired by the 'X-COM' series by Mythos and Microprose.


%package uforadiant
Summary:	UFO: Alien Invasion map editor
Group:		Development/Tools
Requires:	%{name} = %{version}

%description uforadiant
UFO: ALIEN INVASION is a strategy game featuring tactical combat
against hostile alien forces which are about to infiltrate earth at
this very moment.

This package contains the map editor UFORadiant.


%prep
%setup -q -n %{name}-%{version}-source
%patch1 -p0 -b .nostrip~

%build
./configure	--prefix=%{_prefix} \
		--bindir=%{_gamesbindir} \
		--datadir=%{_gamesdatadir}/ufoai \
		--localedir=%{_datadir}/locale \
		--enable-release \
%ifarch x86_64
		--enable-sse \
%endif
		--enable-cgame-campaign \
		--enable-cgame-multiplayer \
		--enable-cgame-skirmish \
		--enable-game \
		--enable-memory \
		--enable-testall \
		--enable-ufo2map \
		--enable-ufoded \
		--enable-ufo \
		--enable-ufomodel \
		--enable-uforadiant \
		--enable-ufoslicer
## Mageia doesn't use "make %{?_smp_mflags}".
## They use "%make" to achieve the same effect.
%make LDFLAGS="%ldflags" uforadiant_LINKER="g++ %ldflags"

%install
%makeinstall_std
# Remove empty data files to avoid file conflict with data package
rm -f %{buildroot}/%{_gamesdatadir}/%{name}/base/*pk3

%find_lang ufoai
desktop-file-install --dir=%{buildroot}%{_datadir}/applications %{SOURCE1}
install -m644 src/ports/linux/ufo.png -D %{buildroot}%{_datadir}/pixmaps/%{name}.png
desktop-file-install --dir=%{buildroot}%{_datadir}/applications %{SOURCE2}

## install uforadiant
mkdir -p %{buildroot}%{_mandir}/man6
install -m 0644 debian/ufo*.6 %{buildroot}%{_mandir}/man6/
mkdir -p -m 0755 %{buildroot}%{_datadir}/%{name}/radiant
cp -p radiant/*.xml %{buildroot}%{_gamesdatadir}/%{name}/radiant/
cp -p radiant/mapdef.template %{buildroot}%{_gamesdatadir}/%{name}/radiant/
cp -pr radiant/bitmaps %{buildroot}%{_gamesdatadir}/%{name}/radiant
cp -pr radiant/prefabs %{buildroot}%{_gamesdatadir}/%{name}/radiant
cp -pr radiant/sourceviewer %{buildroot}%{_gamesdatadir}/%{name}/radiant
cp -pr radiant/i18n/* %{buildroot}%{_datadir}/locale/
%find_lang uforadiant
install -D -m 0644 debian/uforadiant.xpm %{buildroot}%{_datadir}/icons/hicolor/32x32/apps/uforadiant.xpm


%files -f ufoai.lang
%doc LICENSES
%{_gamesbindir}/ufo
%{_gamesbindir}/ufoded
%dir %{_gamesdatadir}/ufoai
%{_gamesdatadir}/ufoai/memory
%{_gamesdatadir}/ufoai/ufo*
%dir %{_gamesdatadir}/ufoai/base
%{_gamesdatadir}/ufoai/base/game.so
%{_datadir}/pixmaps/%{name}.png
%{_datadir}/applications/%{name}.desktop
%{_mandir}/man6/ufo.6*
%{_mandir}/man6/ufoded.6*
%{_mandir}/man6/ufo2map.6*


%files uforadiant -f uforadiant.lang
%{_gamesbindir}/uforadiant
%{_datadir}/applications/uforadiant.desktop
%{_iconsdir}/hicolor/*/apps/uforadiant.xpm
%{_gamesdatadir}/%{name}/radiant
%{_mandir}/man6/uforadiant.6*

