--------------------------------------------------------------------
-- UFOAI MD2 TAG Importer
--------------------------------------------------------------------

utility MD2TAGImport "UFOAI MD2 Tag Import" silentErrors:false
(
	local appName = "UFOAI Tag Import"

	--------------------
	-- User Interface --
	--------------------

	group "Import:"
	(
		button importButton "Import Tag..."	tooltip:"Import the tag file"
	)

	-----------------------------------------------------
	-- Recent file list (sadly I can't save to an INI) --
	-----------------------------------------------------

	local recentFile

	fn AddRecentFile fname=
	(
		recentFile = fname
		ok
	)

	fn GetMostRecentPath=
	(
		if recentFile != undefined then
		(
			return getFileNamePath recentFile
		) else if g_gameDataPath != undefined then
		(
			return g_gameDataPath + "\\models\\"
		) else
		(
			return ""
		)
	)

	---------------
	-- Utilities --
	---------------

	-- Replacement for doesFileExist which seems to give false positives
	fn DoesFileReallyExist filename=
	(
		local temp = fopen filename "rb"
		if temp != undefined then
		(
			fclose temp
			true
		)
		else
		(
			false
		)
	)

	-- Error message box
	fn ShowError msg=
	(
		format "*** Error: %\n" msg
		messageBox msg title:appName;
		ok
	)

	-- Error message box and exception
	fn FatalError msg=
	(
		ShowError msg;
		throw (appName + ":" + msg)
	)

	-- Quick check, error if fail
	fn Check condition msg =
	(
		if not condition do
		(
			if msg == unsupplied do msg = "Check failed"
			format "*** Check failed: %\n" msg
			FatalError msg
		)
	)

	-- Binstream tools

	-- Skip over a number of bytes
	fn SkipBytes bstream count=
	(
		local unknown
		case count of
		(
			2: unknown = ReadShort bstream #unsigned
			4: unknown = ReadLong bstream #unsigned
			default:
			(
				for i = 1 to count do
				(
					unknown = ReadByte bstream #unsigned
				)
			)
		)
	)

	-- Read a string of fixedLen chars
	fn ReadFixedString bstream fixedLen=
	(
		local str = ""
		for i = 1 to fixedLen do
		(
			local ch = ReadByte bstream #unsigned
			str += bit.intAsChar ch
			if ch == 0 then
			(
				-- Ignore junk
				SkipBytes bstream (fixedLen - i)
				exit
			)
		)
		str
	)

	-- String utils

	-- Get the string a long represents
	fn LongToString num=
	(
		local str = ""
		for i = 1 to 4 do
		(
			str += bit.intAsChar (bit.and num 0xff)
			-- num = bit.shift num -8
			num /= 256
		)
		str
	)

	-- Convert to lowercase
	fn ToLowerCase instr=
	(
		local outstr = copy instr
		local upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		local lower = "abcdefghijklmnopqrstuvwxyz"

		for i =1 to outstr.count do
		(
			local pos = findString upper outstr[i]
			if pos != undefined do outstr[i] = lower[pos]
		)
		outstr
	)

	-- Swap a file's extension
	fn ChangeExt filename newExt=
	(
		(getFilenamePath filename) + (getFilenameFile filename) + newExt
	)

	-- Get the game data path set by the user, or a default one
	fn GetDataPath=
	(
		if g_gameDataPath != undefined then
		(
			g_gameDataPath
		)
		else
		(
			g_defaultGameDataPath
		)
	)

	-- Extract the filename of a data file from a PK3 filename
	fn GetDataPathFilename filename=
	(
		local splitPath = filterString filename "\\/"

		if splitPath.count > 0 then
		(
			splitPath[splitPath.count]
		)
		else
		(
			""
		)
	)

	--------------------------------------------------------------------
	-- TAG Importer
	--------------------------------------------------------------------

	-- Tag
	struct MD2_Tag
	(
		name,
		origin,
		axisRow1,
		axisRow2,
		axisRow3,
		tagMatrix,

		fn LoadNameFromStream bstream=
		(
			name = ReadFixedString bstream 64
		),

		fn LoadFrameFromStream bstream=
		(
			local x = ReadFloat bstream
			local y = ReadFloat bstream
			local z = ReadFloat bstream
			axisRow1 = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			axisRow2 = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			axisRow3 = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			origin = [x, y, z]

			tagMatrix = matrix3 axisRow1 axisRow2 axisRow3 origin
		),

		fn GetTranslation=
		(
			tagMatrix.translationpart
		),

		fn GetRotation=
		(
			tagMatrix.rotationPart
		),

		fn GetMatrix=
		(
			tagMatrix
		),

		fn DebugOut=
		(
			format "MD2_Tag: %, matrix = %\n" name tagMatrix
		)
	)

	-- Header: found at start of file
	struct TAG_Header
	(
		ident,			-- JDP2
		version,		-- 1
		numTags,
		numFrames,
		offsetTagNames,
		offsetTags,
		offsetEnd,
		offsetExtractEnd,

		fn LoadFromStream bstream=
		(
			ident			= ReadLong bstream #unsigned
			version			= ReadLong bstream #unsigned
			numTags			= ReadLong bstream #unsigned
			numFrames		= ReadLong bstream #unsigned
			offsetTagNames	= ReadLong bstream #unsigned
			offsetTags		= ReadLong bstream #unsigned
			offsetEnd		= ReadLong bstream #unsigned
			offsetExtractEnd= ReadLong bstream #unsigned
		),

		fn IsValid=
		(
			local identStr = LongToString ident
			if (LongToString ident) != "JDP2" then return false
			if version != 1 then
			(
				ShowError "Incorrect version! Found " + version + " but should be 1"
				return false
			)
			if numFrames < 1 then
			(
				ShowError "TAG does not have any frames!"
				return false
			)
			true
		),

		fn DebugOut=
		(
			format "Header\n----------\n"
			format "Ident: %, version: %, name: %\n" ident version name
			format "numFrames: %, numTags: %\n" numFrames numTags
			format "ofsTags: %, ofsEnd: %\n" offsetTags offsetEnd
			ok
		)
	)


	-- The main TAG loader object
	struct TAG_Loader
	(
		bstream,

		header,
		tags,
		tagObjects,

		fn Open filename=
		(
			bstream = fopen filename "rb"
			ok
		),

		fn Close=
		(
			fclose bstream
			ok
		),

		-- Create a tag object
		fn CreateTagObject md2Tag=
		(
			local verts = #([0, 0, 0], [0, -1, 0], [2, 0, 0])
			local tri = #([1, 2, 3])
			local tagObject = mesh name:(md2Tag.name) vertices:verts faces:tri pos:(md2Tag.GetTranslation())

			in coordsys local tagObject.rotation = md2Tag.GetRotation()

			tagObject
		),

		-- Load and construct the entire tag object
		fn LoadTAG=
		(
			-- Load TAG_Header
			fseek bstream 0 #seek_set

			header = TAG_Header()
			header.LoadFromStream bstream
			--header.DebugOut()

			if not header.IsValid() then
			(
				format "Invalid header, aborting.\n"
				return false;
			)

			tagObjects = #()

			local relativeTag

			for j = 1 to header.numTags do
			(
				local curTag = MD2_Tag()

				namesOffset = (header.offsetTagNames + (j - 1) * 64)
				fseek bstream namesOffset #seek_set
				curTag.LoadNameFromStream bstream

				for i = 1 to header.numFrames do
				(
					tagOffset = (j - 1) * header.numFrames * 12 * 4
					frameOffset = (i - 1) * 12 * 4
					tagFrameOffset = header.offsetTags + tagOffset + frameOffset
					fseek bstream tagFrameOffset #seek_set
					curTag.LoadFrameFromStream bstream

					if i == 1 then
					(
						local tagObject = CreateTagObject curTag
						append tagObjects tagObject

						in coordsys world tagObject.transform = curTag.GetMatrix()

						animate on
						(
							at time 0
							(
								in coordsys world tagObject.transform = curTag.GetMatrix()
							)
						)
					)
					else
					(
						animate on
						(
							at time (i-1)
							(
								in coordsys world tagObjects[j].transform = curTag.GetMatrix()
							)
						)
					)
				)
			)

			-- Set animation slider range and FPS
			if header.numFrames > 1 then
			(
				animationRange = interval 0 (amax animationRange.end header.numFrames-1)
				frameRate = 10.0
			)

			gc()
			true
		)
	)


	-------------------
	-- Final UI Bits --
	-------------------

	on importButton pressed do
	(
		local tagFilename = getOpenFileName caption:"Import MD2 TAG File" filename:(GetMostRecentPath()) \
					types:"UFOAI MD2 TAG (*.tag)|*.tag|All Files (*.*)|*.*|"

		if tagFilename != undefined then
		(
			if DoesFileReallyExist tagFilename then
			(
				AddRecentFile tagFilename

				--try
				(
					local loader = TAG_Loader()
					loader.Open tagFilename
					loader.LoadTAG()
					loader.Close()
					format "OK\n"
				)
				--catch
				--(
				--)

				max views redraw
			)
		)
	)
)
