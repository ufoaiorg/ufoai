--------------------------------------------------------------------
-- Quake III MD3 Importer for gmax
-- Copyright (c) Chris Cookson, 2002
--
-- cjcookson@hotmail.com
--
-- Thanx to Acrid
--
-- All bug reports & feature requests to cjcookson@hotmail.com
--
-- For gmax Scripts, News, Articles and More check out:
--
-- http://gmax.digitalarenas.com
--
-- Version history:
-- 	beta0.1 Public beta - unsupported test version
--
-- Known issues:
--	* Auto-attach doesn't always work. Open upper.md3 first for best results
--	* Freeze when animated objects selected under Win98?
--  * Slow for huge models, but hey, it's only a script!
--------------------------------------------------------------------


utility MD3Import "Quake3 MD3 Import" silentErrors:false
(
	-- Useful model file information
	local g_modelFilename
	local g_modelName
	local g_modelPath

	-- Quake3 Image Extensions
	local g_q3ImageExts = #( ".tga", ".jpg", ".jpeg" )

	local appName = "Quake3 MD3 Import"

	-- Texture Cache
	local g_textureNames = #()
	local g_textures = #()


	--------------------
	-- User Interface --
	--------------------

	group "About:"
	(
		label titleLabel "Quake III MD3 Import"
		label betaLabel "Public Beta v0.1"
		HyperLink addy "by Chris Cookson" align:#center address:"mailto:cjcookson@hotmail.com" color:(color 0 0 170) hoverColor:(color 170 0 0)
	)

	group "Import:"
	(
		radiobuttons tagAction "Tags:" labels:#("Ignore", "Load all", "Auto attach") default:1
		checkbox loadAnimations "Load Animations" checked:true
		checkbox promptForMissing "Prompt for missing textures" checked:false

		button importButton "Import MD3..."	tooltip:"Import a Quake III .MD3 model"
	)

	group "Tools:"
	(
		button applySkin "Apply Skin..." tooltip:"Apply a .skin file to models in the scene"
		button createTag "Create New Tag" tooltip:"Create a new tag triangle"
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


	-- Convert a forward-slash style PK3 path to a windows one
	fn ToWindowsPath pakPath=
	(
		local winPath = "\\"
		for i = 1 to pakPath.count do
		(
			if pakPath[i] == "/" then
			(
				winPath += "\\"
			) else
			(
				winPath += pakPath[i]
			)
		)
		winPath
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

	-------------------
	-- Texture Cache --
	-------------------

	-- Clear the texture cache
	fn ClearTextureCache=
	(
		g_textureNames = #()
		g_textures = #()
	)

	-- Add a texture to the cache and return the loaded bitmaptexture
	fn AddTextureToCache name fname=
	(
		local bmpTex = bitmapTexture filename:fname
		-- Don't add invalid names
		if name != "" then
		(
			append g_textureNames name
			append g_textures bmpTex
		)
		bmpTex
	)

	-- Find a texture given a PK3 filename
	fn FindTexture filename=
	(
		-- First of all, check the cache
		local index = findItem g_textureNames filename
		if index != 0 then
		(
			return g_textures[index]
		)

		-- Not in cache so we'll try searching in the model's directory
		local localFilename = g_modelPath + (GetDataPathFilename filename)

		-- Thanks to the Discreet/Tempest guys for the for ext... bit!
		for ext in g_q3ImageExts do
		(
			local imgName = ChangeExt localFilename ext

			if DoesFileReallyExist imgName then
			(
				return (AddTextureToCache filename imgName)
			)
			format "Failed on %\n" imgName
		)

		-- We're out of luck so ask the user to find it for us!
		local foundFilename
		if promptForMissing.checked do
		(
			messageBox ("Can't find texture " + filename + "\nPlease click OK and locate it.")

			foundFilename = getOpenFileName caption:("Find Texture " + filename) filename:localFilename \
						types:"All supported (*.tga;*.jpg;*.jpeg)|*.tga;*.jpg;*.jpeg|All Files (*.*)|*.*|"
		)

		if foundFilename != undefined then
		(
			return (AddTextureToCache filename foundFilename)
		)
		else
		(
			format "Warning: Could not find % or any variant!\n" filename
			append g_textureNames filename
			append g_textures undefined
		)
		undefined
	)


	----------------------
	-- Skin file reader --
	----------------------
	struct Tokenizer
	(
		tokens = #(),

		fn SetString str=
		(
			tokens = filterString str "\t ,"
		),

		fn ReadToken=
		(
			if tokens.count > 0 then
			(
				local tok = tokens[1]
				deleteItem tokens 1
				tok
			) else
			(
				undefined
			)
		),

		fn PeekToken=
		(
			if tokens.count > 0 then tokens[1] else undefined
		)
	)

	struct SkinDef
	(
		name,
		imgFile
	)

	local g_skinDefs = #()

	-- Strip off the _1, _2 level of detail suffixes
	function StripLodSuffix str=
	(
		if str[str.count-1] == "_" then
		(
			return subString str 1 (str.count-2)
		)
		str
	)

	function GetDefaultSkinFile=
	(
		-- Strip LOD suffix from model name
		local strippedName = StripLodSuffix g_modelName

		g_modelPath + strippedName + "_default.skin"
	)

	function LoadSkinFile fname=
	(
		try
		(
			g_skinDefs = #()

			--format "Attempting to load skin file: %\n" fname
			local fp = openFile fname mode:"r"

			if fp == undefined then
			(
				format "Skin file % not found\n" fname
				return false
			)

			local tok = Tokenizer()

			while not (eof fp) do
			(
				local curLine = readLine fp

				--format "%\n" curLine

				tok.SetString curLine

				if tok.PeekToken() != undefined then
				(
					local surfName = tok.ReadToken()
					surfName = ToLowerCase surfName
					local imageFile = tok.ReadToken()
					local newSkinDef = SkinDef name:surfName imgFile:imageFile
					append g_skinDefs newSkinDef
					--format "Got skindef: (%, %)\n" (newSkinDef.name) (newSkinDef.imgFile)
				)
			)

			close fp
		)
		catch ()
		ok
	)

	function GetSkinForSurface surfName=
	(
		--format "Looking for skin for surface %..." surfName
		surfName = ToLowerCase surfName

		for def in g_skinDefs do
		(
			if (def.name == surfName) or (def.name == (StripLodSuffix surfname)) then
			(
				--format "Found %\n" (def.imgFile)
				return def.imgFile
			)
		)
		--format "Not found\n"
		undefined
	)



	--------------------------------------------------------------------
	-- MD3 Importer
	--------------------------------------------------------------------

	-- Surfaces are a group of polys with the same rendering style
	struct MD3_Surface
	(
		ident,
		name,
		flags,
		numFrames,
		numShaders,
		numVertices,
		numTriangles,
		offsetTriangles,
		offsetShaders,
		offsetTexCoords,
		offsetVertices,
		offsetEnd,

		-- Internal
		offsetStart,	-- Set when LoadFromStream called

		fn LoadFromStream bstream=
		(
			offsetStart = ftell bstream

			ident					= ReadLong bstream #unsigned
			name					= ReadFixedString bstream 64
			flags					= ReadLong bstream #unsigned
			numFrames				= ReadLong bstream #unsigned
			numShaders				= ReadLong bstream #unsigned
			numVertices				= ReadLong bstream #unsigned
			numTriangles			= ReadLong bstream #unsigned
			offsetTriangles			= ReadLong bstream #unsigned
			offsetShaders			= ReadLong bstream #unsigned
			offsetTexCoords			= ReadLong bstream #unsigned
			offsetVertices			= ReadLong bstream #unsigned
			offsetEnd				= ReadLong bstream #unsigned
		),

		fn IsValid=
		(
			/*
			if ident != 0x07 then
			(
				return false
			)
			*/
			true
		),

		fn DebugOut=
		(
			format "MD3_Surface\n-----------\n"
			format "ident = %, name = %, flags = %\n" ident name flags
			format "numFrames = %, numShaders = %\n" numFrames numShaders
			format "numVertices = %, numTriangles = %\n" numVertices numTriangles
			format "ofsTris = %, ofsShaders = %, ofsTexCoords = %, ofsVertices = %\n" \
				offsetTriangles offsetShaders offsetTexCoords offsetVertices
		)
	)

	-- Triangle defined as 3 indices
	struct MD3_Triangle
	(
		indices,

		fn LoadFromStream bstream=
		(
			local v1 = ReadLong bstream #unsigned
			local v2 = ReadLong bstream #unsigned
			local v3 = ReadLong bstream #unsigned
			indices = [v3 + 1, v2 + 1, v1 + 1]
		)
	)


	-- Vertex definition
	struct MD3_Vertex
	(
		position,
		normal,

		fn LoadFromStream bstream=
		(
			local x = ReadShort bstream #signed
			local y = ReadShort bstream #signed
			local z = ReadShort bstream #signed
			position = [x as float, y as float, z as float] * (1.0 / 64.0)
			normal = ReadShort bstream #unsigned
		),

		fn GetNormal=
		(
			local unpackAngle = 360.0 / 255.0

			local longitude = ((normal / 256) as float) * unpackAngle
			local latitude = ((bit.and normal 0xff) as float) * unpackAngle

			local norm = [0,0,0]
			norm.x = cos( longitude ) * sin( latitude )
			norm.y = sin( longitude ) * sin( latitude )
			norm.z = cos( latitude )

			norm
		),

		fn GetSize= 8
	)

	-- Texture co-ordinate
	struct MD3_TexCoord
	(
		stu,

		fn LoadFromStream bstream=
		(
			local s = ReadFloat bstream
			local t = ReadFloat bstream
			-- gmax needs the t co-ordinate reversed
			stu = [s, -t, 0.0]
		)
	)

	-- Shader definition
	struct MD3_Shader
	(
		name,
		shaderIndex,	-- Internal to Q3

		fn LoadFromStream bstream=
		(
			name = ReadFixedString bstream 64
			shaderIndex = ReadLong bstream #unsigned
		),

		fn DebugOut=
		(
			format "MD3_Shader: %\n" name
		)
	)

	-- Tag
	struct MD3_Tag
	(
		name,
		origin,
		axisRow1,
		axisRow2,
		axisRow3,
		tagMatrix,

		fn LoadFromStream bstream=
		(
			name = ReadFixedString bstream 64
			local x = ReadFloat bstream
			local y = ReadFloat bstream
			local z = ReadFloat bstream
			origin = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			axisRow1 = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			axisRow2 = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			axisRow3 = [x, y, z]

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
			format "MD3_Tag: %, matrix = %\n" name tagMatrix
		)
	)

	-- Frame:
	struct MD3_Frame
	(
		bboxMin,
		bboxMax,
		localOrigin,
		radius,
		name,			-- often creator

		fn LoadFromStream bstream=
		(
			local x = ReadFloat bstream
			local y = ReadFloat bstream
			local z = ReadFloat bstream
			bboxMin = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			bboxMax = [x, y, z]
			x = ReadFloat bstream
			y = ReadFloat bstream
			z = ReadFloat bstream
			localOrigin = [x, y, z]
			radius = ReadFloat bstream
			name = ReadFixedString bstream 16
		)
	)

	-- Header: found at start of file
	struct MD3_Header
	(
		ident,			-- IDP3
		version,		-- 15
		name,
		flags,
		numFrames,
		numTags,
		numSurfaces,
		numSkins,
		offsetFrames,
		offsetTagNames,
		offsetTags,
		offsetSurfaces,
		offsetEnd,

		fn LoadFromStream bstream=
		(
			ident			= ReadLong bstream #unsigned
			version			= ReadLong bstream #unsigned
			name			= ReadFixedString bstream 64
			flags			= ReadLong bstream #unsigned
			numFrames		= ReadLong bstream #unsigned
			numTags			= ReadLong bstream #unsigned
			numSurfaces		= ReadLong bstream #unsigned
			numSkins		= ReadLong bstream #unsigned
			offsetFrames	= ReadLong bstream #unsigned
			offsetTags		= ReadLong bstream #unsigned
			offsetSurfaces	= ReadLong bstream #unsigned
			offsetEnd		= ReadLong bstream #unsigned
		),

		fn IsValid=
		(
			local identStr = LongToString ident
			if (LongToString ident) != "IDP3" then return false
			if version != 15 then
			(
				ShowError "Incorrect version! Found " + version + " but should be 15"
				return false
			)
			if numFrames < 1 then
			(
				ShowError "MD3 does not have any frames!"
				return false
			)
			true
		),

		fn DebugOut=
		(
			format "MD3_Header\n----------\n"
			format "Ident: %, version: %, name: %\n" ident version name
			format "Flags: %, numFrames: %, numTags: %, numSurfaces: %, numSkins: %\n" flags numFrames numTags numSurfaces numSkins
			format "ofsFrames: %, ofsTags: %, ofsSurfs: %, ofsEnd: %\n" offsetFrames offsetTags offsetSurfaces offsetEnd
			ok
		)
	)


	-- The main MD3 loader object
	struct MD3_Loader
	(
		bstream,

		header,
		surfaceList,
		tags,
		tagObjects,

		frames,

		skinFile,

		fn Open filename=
		(
			g_modelFilename = filename
			g_modelName = getFilenameFile filename
			g_modelPath = getFileNamePath filename
			bstream = fopen filename "rb"

			skinFile = GetDefaultSkinFile()
			ok
		),

		fn Close=
		(
			fclose bstream
			ok
		),

		-- Create a tag object in the form of the standard Quake3 tag triangle
		fn CreateTagObject md3Tag=
		(
			local verts = #([0, 0, 0], [0, -1, 0], [2, 0, 0])
			local tri = #([1, 2, 3])
			local tagObject = mesh name:(md3Tag.name) vertices:verts faces:tri pos:(md3Tag.GetTranslation())

			in coordsys local tagObject.rotation = md3Tag.GetRotation()

			tagObject
		),

		-- Load a surface and all its associated data (eg. animations, skins)
		fn LoadSurface=
		(
			local surface = MD3_Surface()
			surface.LoadFromStream bstream

			--surface.DebugOut()

			if not surface.IsValid() then
			(
				return false
			)

			-- Load triangles
			fseek bstream (surface.offsetStart + surface.offsetTriangles) #seek_set

			local modelTris = #()
			for i = 1 to surface.numTriangles do
			(
				local curTri = MD3_Triangle()
				curTri.LoadFromStream bstream

				append modelTris curTri.indices
			)

			-- Load shaders
			fseek bstream (surface.offsetStart + surface.offsetShaders) #seek_set
			local modelShaders = #()
			for i = 1 to surface.numShaders do
			(
				local curShader = MD3_Shader()
				curShader.LoadFromStream bstream
				append modelShaders curShader.name
				--curShader.DebugOut()
			)

			-- Load texture co-ordinates
			fseek bstream (surface.offsetStart + surface.offsetTexCoords) #seek_set
			local modelTexCoords = #()
			for i = 1 to surface.numVertices do
			(
				local curTC = MD3_TexCoord()
				curTC.LoadFromStream bstream
				append modelTexCoords curTC.stu
			)

			-- Create a material
			local modelMaterial = standardmaterial name:(header.name + " material")

			local skin = GetSkinForSurface surface.name

			if skin == undefined then
			(
				skin = modelShaders[1]
			)

			local diffuseMap = FindTexture skin

			if diffuseMap != undefined then
			(
				modelMaterial.maps[2] = diffuseMap
				modelMaterial.mapEnables[2] = true
				showTextureMap modelMaterial diffuseMap true
			)

			-- Create mesh in gmax
			local gmaxMesh = mesh name:surface.name numVerts:surface.numVertices numFaces:surface.numTriangles \
									position:[0,0,0] scale:[1,1,1] material:modelMaterial

			-- Add faces
			setMesh gmaxMesh faces:modelTris

			-- Add per-vertex tex-coords and texture faces
			setNumTVerts gmaxMesh surface.numVertices
			for i = 1 to surface.numVertices do
			(
				setTVert gmaxMesh i modelTexCoords[i]
			)

			buildTVFaces gmaxMesh
			for i = 1 to surface.numTriangles do
			(
				setTVFace gmaxMesh i modelTris[i]
			)

			-- try and help garbage collection
			modelTexCoords = undefined
			modelTris = undefined
			gc()

			-- Jump over to the vertex data
			fseek bstream (surface.offsetStart + surface.offsetVertices + 0) #seek_set

			local modelNormals = #()
			for i = 1 to surface.numVertices do
			(
				local curVertex = MD3_Vertex()
				curVertex.LoadFromStream bstream
				append modelNormals (curVertex.GetNormal())
				setVert gmaxMesh i (curVertex.position)
			)

			-- For some reason I have to set normals *after* having set all the verts
			-- They also don't animate properly, so I've had to disable them :/
			/*
			for i = 1 to surface.numVertices do
			(
				setNormal gmaxMesh i modelNormals[i]
			)
			modelNormals = #()
			*/

			if loadAnimations.checked then
			(

				-- Make vertices animatable
				animateVertex gmaxMesh #all

				local animVerts = gmaxMesh[4][1]

				-- Construct each subsequent animation frame
				animate on
				(
					local progStr = " (" + (surfaceList.count as string) + "/" + (header.numSurfaces as string) + ")"
					progressStart ("Loading " + surface.name + progStr + "...")

					try
					(
						-- Seek back to start of vertices
						fseek bstream (surface.offsetStart + surface.offsetVertices) #seek_set

						local curVertex = MD3_Vertex()

						for curFrame = 0 to (surface.numFrames - 1) do
						(
							at time curFrame
							(
								for i = 1 to surface.numVertices do
								(
									curVertex.LoadFromStream bstream
									animVerts[i].value = curVertex.position
								)
							)

							-- Don't update too often (pointless to update more than 100 times anyway)
							if (mod curFrame 3) == 0 then
							(
								progressUpdate (100.0 * curFrame / surface.numFrames)
							)

							-- Allow the user to cancel if they get bored or panic
							if getProgressCancel() then
							(
								progressEnd()
								return undefined
							)
						)
					)
					catch
					(
						-- Always call this otherwise gmax kind of locks up
						progressEnd()
						throw
					)

					progressEnd()
				)
			)

			update gmaxMesh

			-- Move to end of surface ready for the next one
			fseek bstream (surface.offsetStart + surface.offsetEnd) #seek_set

			-- Return gmax mesh
			gmaxMesh
		),


		-- Load and construct the entire MD3 model
		fn LoadModel=
		(
			-- Load MD3_Header
			fseek bstream 0 #seek_set

			header = MD3_Header()
			header.LoadFromStream bstream
			--header.DebugOut()

			if not header.IsValid() then
			(
				format "Invalid header, aborting.\n"
				return false;
			)

			-- Process tags only if we're loading or attaching
			if tagAction.state != 1 then
			(
				-- Load tags
				fseek bstream header.offsetTags #seek_set

				tagObjects = #()

				local relativeTag

				for i = 1 to header.numFrames do
				(
					local curTag = MD3_Tag()

					for j = 1 to header.numTags do
					(
						curTag.LoadFromStream bstream

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
			)

			-- Load frames
			fseek bstream header.offsetFrames #seek_set
			frames = #()

			--format "Loading % frames...\n" header.numFrames
			local curFrame = MD3_Frame()
			for i = 1 to header.numFrames do
			(
				curFrame.LoadFromStream bstream
				append frames (copy curFrame)
			)

			-- Load skin file
			LoadSkinFile skinFile

			-- Load surfaces
			surfaceList = #()

			fseek bstream header.offsetSurfaces #seek_set

			for i = 1 to header.numSurfaces do
			(
				-- Load and create the surface model
				local newObject = LoadSurface()
				if newObject == undefined then
				(
					print "MD3 Import Cancelled"
					ClearTextureCache()
					gc()
					return false
				)
				append surfaceList newObject
			)


			-- Fixed models drive a load of tags but aren't controlled by one
			local isFixed = false
			local rootTag
			local rootName

			-- Make all surfaces children of the root tag for this model
			if tagAction.state != 1 then
			(
				/*
				-- Guess our root tag (depends on model, actually unnecessary)
				case (ToLowerCase (StripLodSuffix g_modelName)) of
				(
					"head":					(
						rootName = "tag_head"
						isFixed = false
					)

					"upper":
					(
						rootName = "tag_torso"
						isFixed = false
					)

					"lower":
					(
						rootName = undefined
						isFixed = true
					)


					default:
					(
						rootName = undefined
						isFixed = false
					)
				)


				format "I think the root is %\n" rootName
				*/
				rootName = undefined
				isFixed = false

				if not isFixed then
				(

					-- Find the root tag in our tag objects
					for ourTag in tagObjects do
					(
						if ourTag.name == rootName then
						(
							rootTag = ourTag
							exit
						)

						if rootName == undefined then
						(
							-- Grab the tag at the origin - probably the root
							--format "Checking % pos: %\n" (ourTag.name) (ourTag.pos)
							if ourTag.pos == [0,0,0] then
							(
								rootTag = ourTag
								rootName = rootTag.name
								exit
							)
						)
					)

					if rootTag != undefined then
					(
						format "Root tag is %\n" rootTag.name

						-- Make surfaces children of the root tag
						for surf in surfaceList do
						(
							attachObjects rootTag surf move:false
						)

						-- Make other tags children of the root tag
						for tag in tagObjects do
						(
							if tag != rootTag then
							(
								attachObjects rootTag tag move:false
							)
						)
					)
					else
					(
						format "Root tag undefined\n"
					)

				)
			)


			-- Auto attach to a tag in the scene that matches with one in the MD3
			if tagAction.state == 3 then
			(
				if isFixed then
				(
					-- Attach all matching scene tags to our tags
					for sceneObj in objects do
					(
						for ourTag in tagObjects do
						(
							if (sceneObj != ourTag) and (ourTag.name == sceneObj.name) then
							(
								attachObjects ourTag sceneObj
								exit
							)
						)
					)
				)
				else
				(
					local hookedOnto

					-- Make ourselves the child of the corresponding tag in the scene
					for obj in objects do
					(
						if (findItem tagObjects obj) == 0 then
						(
							-- Hook onto scene tag (we become its child)
							if (ToLowerCase obj.name) == rootName then
							(
								-- Got it. Now attach our tag with our model hierarchy to it

								format "Found %, attaching our tag to it\n" rootName
								attachObjects obj rootTag
								hookedOnto = obj
								exit	-- Only 1 allowed!
							)
						)
					)

					format "Hooked onto = %\n" hookedOnto

					-- Now hook other objects onto us
					-- Must be careful that we're not hooked onto them
					-- Because of this it's only allowed if we haven't got a rootTag
					for obj in objects do
					(
						if (findItem tagObjects obj) == 0 then
						(
							if rootTag == undefined then
							(
								for ourTag in tagObjects do
								(
									if obj.name == ourTag.name then
									(
										format "Attaching scene tag % to our corresponding tag\n" ourTag.name
										attachObjects ourTag obj
										exit
									)
								)
							)
						)
					)

				)
			)


			-- Set animation slider range and FPS
			if header.numFrames > 1 then
			(
				animationRange = interval 0 (amax animationRange.end header.numFrames-1)
				frameRate = 10.0 -- a guess, almost certainly wrong (TODO: read animation.cfg)
			)

			-- All done, try and clean up
			ClearTextureCache()

			gc()
			true
		)
	)


	-------------------
	-- Final UI Bits --
	-------------------

	on importButton pressed do
	(
		local md3Filename = getOpenFileName caption:"Import Quake III Model" filename:(GetMostRecentPath()) \
					types:"Quake III MD3 (*.md3)|*.md3|All Files (*.*)|*.*|"

		if md3Filename != undefined then
		(
			if DoesFileReallyExist md3Filename then
			(
				AddRecentFile md3Filename

				--try
				(
					local loader = MD3_Loader()
					loader.Open md3Filename
					loader.LoadModel()
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

	-- Select a skin file and apply it to scene meshes
	on applySkin pressed do
	(
		local skinFilename = getOpenFileName caption:"Select Skin to Apply" filename:(GetMostRecentPath()) \
					types:"Quake III Skin File (*.skin)|*.skin|All Files (*.*)|*.*|"

		if skinFilename != undefined then
		(
			if DoesFileReallyExist skinFilename then
			(
				try
				(
					LoadSkinFile skinFilename

					-- Apply skin to all appropriate objects
					for obj in objects do
					(
						local skinFile = GetSkinForSurface obj.name

						if skinFile != undefined then
						(
							-- Set up approximate paths for object
							g_modelFilename = skinFilename
							g_modelName = getFilenameFile skinFilename
							g_modelPath = getFileNamePath skinFilename

							-- Create new material

							local newMat = standardMaterial name:(getFilenameFile skinFilename)
							local diffuseMap = FindTexture skinFile

							if diffuseMap != undefined then
							(
								newMat.maps[2] = diffuseMap
								newMat.mapEnables[2] = true
								showTextureMap newMat diffuseMap true
							)

							obj.material = newMat

							update obj
						)
					)

				)
				catch
				(
				)

				max views redraw
			)
		)
	)

	-- Does exactly what it says :)
	on createTag pressed do
	(
		local verts = #([0, 0, 0], [0, -1, 0], [2, 0, 0])
		local tri = #([1, 2, 3])
		local tagObject = mesh prefix:"tag_unnamed" vertices:verts faces:tri pos:[0,0,0]
		update tagObject
		max views redraw
	)
)
