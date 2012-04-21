# Copyright 1999-2010 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=2
inherit eutils games subversion


DESCRIPTION="UFO: Alien Invasion - X-COM inspired strategy game"
HOMEPAGE="http://ufoai.sourceforge.net/"
ESVN_REPO_URI="https://ufoai.svn.sourceforge.net/svnroot/ufoai/ufoai/trunk"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="-*"
IUSE="debug dedicated doc editor"

# Dependencies and more instructions can be found here:
# http://ufoai.org/wiki/index.php/Compile_for_Linux
RDEPEND="!dedicated? (
		virtual/opengl
		virtual/glu
		media-libs/libsdl
		media-libs/sdl-ttf
		media-libs/sdl-mixer
		media-libs/jpeg
		media-libs/libpng
		media-libs/libogg
		media-libs/libvorbis
		x11-proto/xf86vidmodeproto
	)
	editor? ( media-libs/jpeg )
	net-misc/curl
	sys-devel/gettext"

DEPEND="${RDEPEND}
	doc? ( virtual/latex-base )
	x11-libs/gtkglext
	x11-libs/gtksourceview:2.0
	!dedicated? (
	media-libs/sdl-image[jpeg,png] )
	!games-strategy/ufo-ai
	!=games-strategy/ufoai-2.2*"

S=${WORKDIR}/${P}-source

src_configure() {
	egamesconf \
		$(use_enable !debug release) \
		$(use_enable editor ufo2map) \
		--enable-dedicated \
		$(use_enable !dedicated client) \
		--bindir=${GAMES_BINDIR} \
		--datarootdir=${GAMES_DATADIR_BASE} \
		--datadir=${GAMES_DATADIR}
		--localedir=${GAMES_DATADIR_BASE}/locale
		ewarn "Configuring with $(use_enable !debug release) $(use_enable editor ufo2map) --enable-dedicated $(use_enable !dedicated client) --bindir=${GAMES_BINDIR} --datarootdir=${GAMES_DATADIR_BASE} --datadir=${GAMES_DATADIR} --localedir=${GAMES_DATADIR}/${PN}/base/i18n/"
}

src_compile() {
	if use doc ; then
		emake pdf-manual || die "emake pdf-manual failed (USE=doc)"
	fi

	emake || die "emake failed"
	emake lang || die "emake lang failed"
	if use editor ; then
		emake uforadiant || die "emake uforadiant failed"
	fi
	emake maps-sync || die "emake maps-sync failed"
}

src_install() {
	# server
	emake DESTDIR="${D}" install || die
	newicon contrib/installer/linux/data/ufo.xpm ${PN}.xpm \
		|| die "Failed installing icon"
	make_desktop_entry ${PN}-ded "UFO: Alien Invasion Server"
	if ! use dedicated ; then
		# client
		make_desktop_entry ${PN} "UFO: Alien Invasion"
	fi

	prepgamesdirs
}
