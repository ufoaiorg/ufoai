--------------------------------------------------------------------
-- Quake II MD2 Importer
-- v1.1
--
-- Copyright (c) Chris Cookson, 2002
-- (cjcookson@hotmail.com)
--
-- gmax Scripts, News, Articles and More:
--
-- http://gmax.digitalarenas.com
--
-- Thanks to TiCaL
--
-- Version history:
--  1.1 Fixed anim problems and out of memory errors
-- 	1.0 First Release
--------------------------------------------------------------------

utility MD2Import "Quake2 MD2 Importer" silentErrors:false
(
	--------------------
	-- User Interface --
	--------------------

	group "About:"
	(
		label titleLabel	"Quake2 MD2 Importer v1.0"
		HyperLink addy "by Chris Cookson" align:#center address:"mailto:cjcookson@hotmail.com" color:(color 0 0 170) hoverColor:(color 170 0 0)
	)

	group "Import:"
	(
		checkbox animCheck "Load all frames" checked:false align:#center
		button importButton "Import MD2..."	
	)
	
	local appName = "MD2 Importer"
	

	-- Utilities

	-- Error handling
	fn ShowError msg=
	(
		format "*** Error: %\n" msg
		messageBox msg title:appName;
		ok
	)
	
	fn FatalError msg=
	(
		ShowError msg;
		throw (appName + ":" + msg)
	)
	
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
	
	fn ReadFixedString bstream fixedLen=
	(
		local str = ""
		for i = 1 to fixedLen do
		(
			str += bit.intAsChar (ReadByte bstream #unsigned)
		)
		str
	)
	
	-- String utils
	
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
	
	--------------------------------------------------------------------
	-- MD2 Importer
	--------------------------------------------------------------------
	

	struct MD2_Vertex
	(
		pos, lightNormalIndex
	)

	struct MD2_Frame
	(
		scale, translate, name,
		vertListOffset
	)
	
	struct MD2_Tri
	(
		vertIndices,
		texCoordIndices
	)

	struct MD2Reader
	(
		modelFilename, modelName, modelPath,
		bstream,
		skinList,
		triList,
		frameList,
		texCoordList,
		glCmdList,
		numVerts,
				
		fn Open filename=
		(
			modelFilename = filename
			modelName = getFilenameFile filename
			modelPath = getFileNamePath filename
			bstream = fopen filename "rb"
			ok
		),
		
		fn Close=
		(
			fclose bstream
			ok
		),
		
		fn ReadSkins offset numSkins=
		(
			fseek bstream offset #seek_set
			
			skinList = #()
			
			format "Reading % skins..." numSkins
			for i = 1 to numSkins do
			(
				append skinList (ReadFixedString bstream 64)
				format "Skin %: %\n" i skinList[i]
			)
			format "OK\n"
		),
		
		fn ReadTris offset numTris=
		(
			fseek bstream offset #seek_set
			
			triList = #()
			
			format "Reading % tris..." numTris
			
			for i = 1 to numTris do
			(
				local v1 = ReadShort bstream #unsigned
				local v2 = ReadShort bstream #unsigned
				local v3 = ReadShort bstream #unsigned
				
				local uv1 = ReadShort bstream #unsigned
				local uv2 = ReadShort bstream #unsigned
				local uv3 = ReadShort bstream #unsigned
				
				append triList (MD2_Tri vertIndices:[v1, v2, v3] texCoordIndices:[uv1,uv2,uv3])
			)
			
			format "OK\n"
		),
		
		fn ReadFrames offset numFrames=
		(
			fseek bstream offset #seek_set
			frameList = #()
			format "Reading % frames..." numFrames
			for i = 1 to numFrames do
			(
				local sx = ReadFloat bstream
				local sy = ReadFloat bstream
				local sz = ReadFloat bstream
				
				local tx = ReadFloat bstream
				local ty = ReadFloat bstream
				local tz = ReadFloat bstream
				
				local name = ReadFixedString bstream 16
				
				local newFrame = MD2_Frame scale:[sx, sy, sz] translate:[tx,ty, tz] name:name

				-- Just remember where the verts are
				newFrame.vertListOffset = ftell bstream
				
				-- Skip all verts 'cos we'll read 'em later
				SkipBytes bstream (numVerts * 4)
					
				append frameList newFrame
			)
			format "OK\n"
		),
		
		fn ReadTexCoords offset numTexCoords skinWidth skinHeight=
		(
			fseek bstream offset #seek_set
			texCoordList = #()
			format "Reading % texcoords..." numTexCoords
			for i = 1 to numTexCoords do
			(
				local s = (ReadShort bstream #unsigned) as float
				local t = (ReadShort bstream #unsigned) as float
				append texCoordList [s / skinWidth, -t / skinHeight, 0]	
			)
			format "OK\n"
		),
	
		fn LoadModel=
		(
			fseek bstream 0 #seek_set
			
			local magic 			= ReadLong bstream #unsigned
			
			if magic != 0x32504449 then
			(
				FatalError "File does not appear to be an MD2 model!"
			)
			
			local version			= ReadLong bstream #unsigned
			
			if version != 0x8 then
			(
				FatalError "Unsupported version!"
			)
			
			local skinWidth			= ReadLong bstream #unsigned
			local skinHeight 		= ReadLong bstream #unsigned
			local frameSize			= ReadLong bstream #unsigned
			local numSkins			= ReadLong bstream #unsigned
			numVerts				= ReadLong bstream #unsigned
			local numTexCoords		= ReadLong bstream #unsigned
			local numTris			= ReadLong bstream #unsigned
			local numGLCmds			= ReadLong bstream #unsigned
			local numFrames			= ReadLong bstream #unsigned
			local offsetSkins		= ReadLong bstream #unsigned
			local offsetTexCoords	= ReadLong bstream #unsigned
			local offsetTris		= ReadLong bstream #unsigned
			local offsetFrames		= ReadLong bstream #unsigned
			local offsetGLCmds		= ReadLong bstream #unsigned
			local offsetEnd			= ReadLong bstream #unsigned
			
			format "Skin size (%, %)\n" skinWidth skinHeight
			format "Frame size % bytes\n" frameSize
			format "% skins, % verts, % texcoords, % tris, % glCmds, % frames\n" numSkins numVerts numTexCoords numTris numGLCmds numFrames
			
			if not animCheck.checked then
			(
				numFrames = 1
			)
			
			ReadSkins offsetSkins numSkins
			ReadTexCoords offsetTexCoords numTexCoords skinWidth skinHeight
			ReadTris offsetTris numTris
			ReadFrames offsetFrames numFrames
			
		),
		
		fn GetVertsForFrame frameNum=
		(
			local verts = #()
			for i = 1 to ((frameList[frameNum]).vertList).count do
			(
				local theVert = (frameList[frameNum].vertList[i]).pos
				
				theVert *= frameList[frameNum].scale
				theVert += frameList[frameNum].translate
			
				append verts theVert
			)
			verts				
		),
		
		fn CreateMAXModel=
		(
			-- Firstly, create our material
			local skinMaterial = standardMaterial name:(modelName + ".skin")

			-- Sadly, we can't load PCX files in gmax - so no skins! :(			
			local verts = #()
			fseek bstream (frameList[1].vertListOffset) #seek_set
			
			for i = 1 to numVerts do
			(
				local x = ReadByte bstream #unsigned
				local y = ReadByte bstream #unsigned
				local z = ReadByte bstream #unsigned
				local normIndex = ReadByte bstream #unsigned
				
				local theVert = ([x, y, z] * (frameList[1].scale)) + (frameList[1].translate)
			
				append verts theVert
			)
			
			local tris = #()
			for i = 1 to triList.count do
			(
				local v = triList[i].vertIndices + [1,1,1]
				append tris [v.z, v.y, v.x]
				
			)
			
			local theMesh = mesh name:(modelName + ".md2") vertices:verts faces:tris\
								material:skinMaterial tverts:texCoordList

			-- Set texcoord faces (so user just needs to load a converted skin PCX file)
			buildTVFaces theMesh false
			for i = 1 to triList.count do
			(
				local tcVert = triList[i].texCoordIndices + [1,1,1]
				setTVFace theMesh i [tcVert.z, tcVert.y, tcVert.x]
			)

			-- Set vertices up for animation
			animateVertex theMesh #all
			local animVerts = theMesh[4][1]
											
			-- Set animation frames			
			progressStart "Loading frames"
			animate on
			(
				-- Now create animation frames
				for i = 1 to frameList.count do
				(
					local frameScale = frameList[i].scale
					local frameTranslate = frameList[i].translate

					fseek bstream (frameList[i].vertListOffset) #seek_set
					
					at time i
					(
						for j = 1 to numVerts do
						(
							local x = ReadByte bstream #unsigned
							local y = ReadByte bstream #unsigned
							local z = ReadByte bstream #unsigned
							local normIndex = ReadByte bstream #unsigned
							
							animVerts[j].value = ([x, y, z] * frameScale) + frameTranslate
						)
					)
					if (mod i 5) == 0 then
					(
						progressUpdate ((100 * i) / frameList.count)
					)
				)
			)
			progressEnd()
			
			update theMesh
			
			-- Set animation slider range and FPS
			if frameList.count > 1 then
			(
				animationRange = interval 1 frameList.count
			)
			frameRate = 10.0
			
			gc()
		)
	)

	on importButton pressed do
	(
		local modelFilename = getOpenFileName caption:"Import MD2 Model" \
							types:"Quake2 MD2 (*.md2)|*.md2|All Files (*.*)|*.*|"

		if modelFilename != undefined then
		(
			if DoesFileExist modelFilename then
			(
				--try
				(
					local reader = MD2Reader()
					local fileType = getFileNameType modelFileName
					
					reader.Open modelFilename
					reader.LoadModel()
					reader.CreateMAXModel()
					reader.Close()
				)
				--catch
				(
					--throw
					--FatalError "Could 
				)
				
				max views redraw
			)
		)
	)
)


