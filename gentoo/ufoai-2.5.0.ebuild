# Copyright 1999-2018 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2

EAPI=6
inherit desktop epatch versionator gnome2-utils

# UFO:AI v2.5.0 was uploaded to SourceForge as 2.5
DIST_VERSION=$(get_version_component_range 1-2)

# Install game data here
DATADIR="${EPREFIX}/usr/share/games/${PN}"

DESCRIPTION="UFO: Alien Invasion - X-COM inspired strategy game"
HOMEPAGE="https://ufoai.org/"
SRC_URI="
	mirror://sourceforge/ufoai/$PN-${DIST_VERSION}-source.tar.bz2
	mirror://sourceforge/ufoai/$PN-${DIST_VERSION}-data.tar

	mapeditor? (
		mirror://sourceforge/ufoai/$PN-${DIST_VERSION}-mappack.tar.bz2
	)
"

# https://ufoai.org/licenses/
LICENSE="GPL-2 GPL-3 public-domain CC-BY-3.0 CC-BY-SA-3.0 MIT"
SLOT="0"
KEYWORDS="~amd64 ~x86"
IUSE="cpu_flags_x86_sse debug server +client mapeditor"
REQUIRED_USE="|| ( server client mapeditor )"

RDEPEND="
	client? (
		media-libs/libogg
		media-libs/libpng
		media-libs/libsdl2[joystick,opengl,sound,threads,video]
		media-libs/libtheora
		media-libs/libvorbis
		media-libs/sdl2-mixer
		media-libs/sdl2-ttf
		media-libs/xvid
		net-misc/curl
		sys-devel/gettext
		sys-libs/zlib
		virtual/jpeg
		virtual/opengl
	)

	server? (
		media-libs/libogg
		media-libs/libsdl2[joystick,opengl,sound,threads,video]
		media-libs/libvorbis
		net-misc/curl
		sys-devel/gettext
		sys-libs/zlib
	)

	mapeditor? (
		media-libs/libogg
		media-libs/libpng
		media-libs/libsdl2[joystick,opengl,sound,threads,video]
		media-libs/libvorbis
		sys-devel/gettext
		sys-libs/zlib

		net-misc/curl
		dev-libs/libxml2
		media-libs/openal
		x11-libs/gtk+:2
		x11-libs/gdk-pixbuf
		x11-libs/gtkglext
		x11-libs/gtksourceview:2.0
		virtual/jpeg
		virtual/glu
	)
"

DEPEND="${RDEPEND}"

S=${WORKDIR}/$PN-${DIST_VERSION}-source

src_unpack() {
	if use mapeditor; then
		unpack $PN-${DIST_VERSION}-mappack.tar.bz2
	fi

	unpack $PN-${DIST_VERSION}-source.tar.bz2
	cd "${S}" || die
	unpack $PN-${DIST_VERSION}-data.tar
}

src_prepare() {
	EPATCH_SOURCE="${FILESDIR}/" EPATCH_SUFFIX="patch" epatch
	default_src_prepare
}

src_configure() {
	# The configure script of UFO:AI is hand crafted and a bit special
	# econf does not work: "invalid option --build=x86_64-pc-linux-gnu"
	local config="
		--disable-paranoid
		--disable-dependency-tracking
		--disable-memory
		--disable-testall
		--disable-ufomodel
		--disable-ufoslicer
		$(use_enable cpu_flags_x86_sse sse)
		$(use_enable !debug release)
		$(use_enable server ufoded)
		$(use_enable client ufo)
		$(use_enable mapeditor uforadiant)
		$(use_enable mapeditor ufo2map)
	"
	if use server; then
		config="${config} --enable-game"
	elif use client; then
		config="${config} --enable-game"
	else
		config="${config} --disable-game"
	fi
	./configure \
		${config} \
		--prefix="${EPREFIX}"/usr/ \
		--datadir="${DATADIR}" || die
}

src_compile() {
	emake
	emake lang
}

src_install() {
	newicon -s 32 src/ports/linux/ufo.png ${PN}.png

	emake DESTDIR="${D}" install

	# Remove shell scripts the installscript created
	rm "${D}/usr/bin/ufo" || die
	rm "${D}/usr/bin/ufoded" || die
	rm "${D}/usr/bin/uforadiant" || die

	if use client; then
		rm "${D}/${DATADIR}/ufo" || die
		dobin ufo
		doman debian/ufo.6
		make_desktop_entry ufo "UFO: Alien Invasion" ${PN}
	fi

	if use server; then
		rm "${D}/${DATADIR}/ufoded" || die
		dobin ufoded
		doman debian/ufoded.6
		make_desktop_entry ufoded "UFO: Alien Invasion Server" ${PN} "Game;StrategyGame" "Terminal=true"
	fi

	if use mapeditor; then
		# Remove binaries installed to the wrong directory
		rm "${D}/${DATADIR}/radiant/uforadiant" || die
		rm "${D}/${DATADIR}/ufo2map" || die
		# Install them to the proper place
		dobin ufo2map
		doman debian/ufo2map.6
		dobin radiant/uforadiant
		doman debian/uforadiant.6

		# Install map editor data (without the binary)
		rm radiant/uforadiant
		insinto "${DATADIR}"
		doins -r radiant

		# Install map sources
		insinto "${DATADIR}/base/maps"
		doins -r "${WORKDIR}/$PN-${DIST_VERSION}-mappack"/*

		make_desktop_entry uforadiant "UFO: Alien Invasion Map editor" ${PN}
	fi
}

pkg_preinst() {
	gnome2_icon_savelist
}

pkg_postinst() {
	gnome2_icon_cache_update
}

pkg_postrm() {
	gnome2_icon_cache_update
}
