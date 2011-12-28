#! /usr/bin/env python
# -*- coding: utf-8
#
# Extension for fontforge which keep a set of glyths and remove all others.
# Move it to  ~/.FontForge/python/

import fontforge;

ufoaiLanguages = u"""
Български
中文
česky
dansk
Nederlands
English
eesti
Français
Deutsch
Ελληνικά
Magyar
Italiano
日本語
한국어
Norsk (bokmål)
polski
Português
Português (Brasil)
Русский
slovenščina
español
español (España)
svenska
ไทย
Türkçe
suomi
українська
"""

allLanguages = u"""
Ποντιακά
Afrikaans
Akan
Alemannisch
አማርኛ
Aragonés
Anglo-Saxon
العربية
ܐܪܡܝܐ
مصرى
Asturianu
Aymar aru
Azərbaycan
Башҡорт
Boarisch
Žemaitėška
Беларуская
Беларуская (тарашкевіца)
Български
Bislama
Bamanankan
বাংলা
ইমার ঠার/বিষ্ণুপ্রিয়া মণিপুরী
Brezhoneg
Bosanski
ᨅᨔ ᨕᨘᨁᨗ
Català
Mìng-dĕ̤ng-ngṳ̄
Cebuano
Chamoru
ᏣᎳᎩ
Corsu
Qırımtatarca
Česky
Kaszëbsczi
Словѣ́ньскъ / ⰔⰎⰑⰂⰡⰐⰠⰔⰍⰟ
Чăвашла
Cymraeg
Dansk
Deutsch
Zazaki
Dolnoserbski
Ελληνικά
English
Esperanto
Español
Eesti
Euskara
Estremeñu
فارسی
Fulfulde
Suomi
Võro
Føroyskt
Arpetan
Furlan
Frysk
Gaeilge
贛語
Gàidhlig
Galego
Avañe'ẽ
ગુજરાતી
Gaelg
Hak-kâ-fa
Hawai`i
עברית
हिन्दी
Hrvatski
Hornjoserbsce
Kreyòl ayisyen
Magyar
Հայերեն
Interlingua
Bahasa Indonesia
Igbo
Iñupiak
Ilokano
Ido
Íslenska
Italiano
ᐃᓄᒃᑎᑐᑦ/inuktitut
日本語
Lojban
Basa Jawa
ქართული
Qaraqalpaqsha
Taqbaylit
Қазақша
Kalaallisut
ភាសាខ្មែរ
ಕನ್ನಡ
한국어
कश्मीरी - (كشميري)
Ripoarisch
Kurdî / كوردی
Kernewek
Latina
Ladino
Lëtzebuergesch
Лакку
Limburgs
Líguru
Lumbaart
Lingála
ລາວ
Lietuvių
Latviešu
Basa Banyumasan
Мокшень
Malagasy
Māori
Македонски
മലയാളം
Монгол
मराठी
Bahasa Melayu
Malti
Myanmasa
Эрзянь
مَزِروني
Dorerin Naoero
Nāhuatl
Nnapulitano
Plattdüütsch
Nedersaksisch
नेपाली
Oshiwambo
Nederlands
Norsk (nynorsk)
Norsk (bokmål)
Novial
Nouormand
Diné bizaad
Occitan
Иронау
ਪੰਜਾਬੀ
Pangasinan
Kapampangan
Papiamentu
Polski
Piemontèis
Português
Runa Simi
Rumantsch
Romani
Română
Armãneashce
Tarandíne
Русский
संस्कृत
Саха тыла
Sardu
Sicilianu
Scots
سنڌي
Sámegiella
Srpskohrvatski / Српскохрватски
සිංහල
Simple English
Slovenčina
Slovenščina
Soomaaliga
Shqip
Српски / Srpski
Seeltersk
Basa Sunda
Svenska
Kiswahili
Ślůnski
தமிழ்
తెలుగు
Tetun
Тоҷикӣ
ไทย
Türkmençe
Tagalog
Setswana
Tok Pisin
Türkçe
Xitsonga
Tatarça/Татарча
Twi
Удмурт
Uyghurche‎ / ئۇيغۇرچە
Українська
اردو
O'zbek
Vèneto
Tiếng Việt
West-Vlams
Walon
Winaray
Wolof
吴语
ייִדיש
Yorùbá
Zeêuws
中文
文言
Bân-lâm-gú
粵語
"""

def getUnicodeChars(languages):
	chars = {}

	for c in languages:
		chars["" + c] = True

	chars = chars.keys()
	chars.sort()

	line = ""
	for c in chars:
		line += c
	#print "Need %d glyph(s)" % len(chars)
	#print line.encode('utf-8')

	return chars

def removeUselessGlyphs(text, font):
	charsNeed = getUnicodeChars(text)
	font.selection.none()
	existingChars = 0
	removed = 0
	missingGlyphs = ""

	for uni in charsNeed:
		code = ord(uni)
		name = fontforge.nameFromUnicode(code)
		font.selection.select(("more", None), name)
		if name in font:
			existingChars += 1
		else:
			missingGlyphs += uni
	font.selection.invert()
	for glyph in font.selection:
		#TODO check existance instead of using undocumented exception
		try:
			font.removeGlyph(glyph)
			removed += 1
		except ValueError:
			pass

	font.selection.none()
	missingGlyphs = missingGlyphs.strip()
	print "Need %d glyph(s), found %d glyph(s), removed %d glyph(s)" % (len(charsNeed), existingChars, removed)
	print "Missing glyphs: %d" % len(missingGlyphs)
	for c in missingGlyphs:
		print "    %s (%s)" % (c.encode('utf-8'), hex(ord(c)))

fontforge.registerMenuItem(removeUselessGlyphs, None, ufoaiLanguages, "Font", None, "Keep glyphs need to display UFOAI languages");
fontforge.registerMenuItem(removeUselessGlyphs, None, allLanguages, "Font", None, "Keep glyphs need to display all languages");
