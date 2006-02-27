-------------------------------------
--MD2/TAG Exporter
--Version 1.4 for max 4
--Herby - Email herby@planetquake.com
--Adam Barton - Email adam@add3d.co.uk
-------------------------------------

global lastfile=""
global MD2TAGExport
global nlist=#([-0.525731, 0.000000, 0.850651],[-0.442863, 0.238856, 0.864188],[-0.295242, 0.000000, 0.955423],[-0.309017, 0.500000, 0.809017],[-0.162460, 0.262866, 0.951056],[0.000000, 0.000000, 1.000000],[0.000000, 0.850651, 0.525731],[-0.147621, 0.716567, 0.681718],[0.147621, 0.716567, 0.681718],[0.000000, 0.525731, 0.850651],[0.309017, 0.500000, 0.809017],[0.525731, 0.000000, 0.850651],[0.295242, 0.000000, 0.955423],[0.442863, 0.238856, 0.864188],[0.162460, 0.262866, 0.951056],[-0.681718, 0.147621, 0.716567],[-0.809017, 0.309017, 0.500000],[-0.587785, 0.425325, 0.688191],[-0.850651, 0.525731, 0.000000],[-0.864188, 0.442863, 0.238856],[-0.716567, 0.681718, 0.147621],[-0.688191, 0.587785, 0.425325],[-0.500000, 0.809017, 0.309017],[-0.238856, 0.864188, 0.442863],[-0.425325, 0.688191, 0.587785],[-0.716567, 0.681718, -0.147621],[-0.500000, 0.809017, -0.309017],[-0.525731, 0.850651, 0.000000],[0.000000, 0.850651, -0.525731],[-0.238856, 0.864188, -0.442863],[0.000000, 0.955423, -0.295242],[-0.262866, 0.951056, -0.162460],[0.000000, 1.000000, 0.000000],[0.000000, 0.955423, 0.295242],[-0.262866, 0.951056, 0.162460],[0.238856, 0.864188, 0.442863],[0.262866, 0.951056, 0.162460],[0.500000, 0.809017, 0.309017],[0.238856, 0.864188, -0.442863],[0.262866, 0.951056, -0.162460],[0.500000, 0.809017, -0.309017],[0.850651, 0.525731, 0.000000],[0.716567, 0.681718, 0.147621],[0.716567, 0.681718, -0.147621],[0.525731, 0.850651, 0.000000],[0.425325, 0.688191, 0.587785],[0.864188, 0.442863, 0.238856],[0.688191, 0.587785, 0.425325],[0.809017, 0.309017, 0.500000],[0.681718, 0.147621, 0.716567],[0.587785, 0.425325, 0.688191],[0.955423, 0.295242, 0.000000],[1.000000, 0.000000, 0.000000],[0.951056, 0.162460, 0.262866],[0.850651, -0.525731, 0.000000],[0.955423, -0.295242, 0.000000],[0.864188, -0.442863, 0.238856],[0.951056, -0.162460, 0.262866],[0.809017, -0.309017, 0.500000],[0.681718, -0.147621, 0.716567],[0.850651, 0.000000, 0.525731],[0.864188, 0.442863, -0.238856],[0.809017, 0.309017, -0.500000],[0.951056, 0.162460, -0.262866],[0.525731, 0.000000, -0.850651],[0.681718, 0.147621, -0.716567],[0.681718, -0.147621, -0.716567],[0.850651, 0.000000, -0.525731],[0.809017, -0.309017, -0.500000],[0.864188, -0.442863, -0.238856],[0.951056, -0.162460, -0.262866],[0.147621, 0.716567, -0.681718],[0.309017, 0.500000, -0.809017],[0.425325, 0.688191, -0.587785],[0.442863, 0.238856, -0.864188],[0.587785, 0.425325, -0.688191],[0.688191, 0.587785, -0.425325],[-0.147621, 0.716567, -0.681718],[-0.309017, 0.500000, -0.809017],[0.000000, 0.525731, -0.850651],[-0.525731, 0.000000, -0.850651],[-0.442863, 0.238856, -0.864188],[-0.295242, 0.000000, -0.955423],[-0.162460, 0.262866, -0.951056],[0.000000, 0.000000, -1.000000],[0.295242, 0.000000, -0.955423],[0.162460, 0.262866, -0.951056],[-0.442863, -0.238856, -0.864188],[-0.309017, -0.500000, -0.809017],[-0.162460, -0.262866, -0.951056],[0.000000, -0.850651, -0.525731],[-0.147621, -0.716567, -0.681718],[0.147621, -0.716567, -0.681718],[0.000000, -0.525731, -0.850651],[0.309017, -0.500000, -0.809017],[0.442863, -0.238856, -0.864188],[0.162460, -0.262866, -0.951056],[0.238856, -0.864188, -0.442863],[0.500000, -0.809017, -0.309017],[0.425325, -0.688191, -0.587785],[0.716567, -0.681718, -0.147621],[0.688191, -0.587785, -0.425325],[0.587785, -0.425325, -0.688191],[0.000000, -0.955423, -0.295242],[0.000000, -1.000000, 0.000000],[0.262866, -0.951056, -0.162460],[0.000000, -0.850651, 0.525731],[0.000000, -0.955423, 0.295242],[0.238856, -0.864188, 0.442863],[0.262866, -0.951056, 0.162460],[0.500000, -0.809017, 0.309017],[0.716567, -0.681718, 0.147621],[0.525731, -0.850651, 0.000000],[-0.238856, -0.864188, -0.442863],[-0.500000, -0.809017, -0.309017],[-0.262866, -0.951056, -0.162460],[-0.850651, -0.525731, 0.000000],[-0.716567, -0.681718, -0.147621],[-0.716567, -0.681718, 0.147621],[-0.525731, -0.850651, 0.000000],[-0.500000, -0.809017, 0.309017],[-0.238856, -0.864188, 0.442863],[-0.262866, -0.951056, 0.162460],[-0.864188, -0.442863, 0.238856],[-0.809017, -0.309017, 0.500000],[-0.688191, -0.587785, 0.425325],[-0.681718, -0.147621, 0.716567],[-0.442863, -0.238856, 0.864188],[-0.587785, -0.425325, 0.688191],[-0.309017, -0.500000, 0.809017],[-0.147621, -0.716567, 0.681718],[-0.425325, -0.688191, 0.587785],[-0.162460, -0.262866, 0.951056],[0.442863, -0.238856, 0.864188],[0.162460, -0.262866, 0.951056],[0.309017, -0.500000, 0.809017],[0.147621, -0.716567, 0.681718],[0.000000, -0.525731, 0.850651],[0.425325, -0.688191, 0.587785],[0.587785, -0.425325, 0.688191],[0.688191, -0.587785, 0.425325],[-0.955423, 0.295242, 0.000000],[-0.951056, 0.162460, 0.262866],[-1.000000, 0.000000, 0.000000],[-0.850651, 0.000000, 0.525731],[-0.955423, -0.295242, 0.000000],[-0.951056, -0.162460, 0.262866],[-0.864188, 0.442863, -0.238856],[-0.951056, 0.162460, -0.262866],[-0.809017, 0.309017, -0.500000],[-0.864188, -0.442863, -0.238856],[-0.951056, -0.162460, -0.262866],[-0.809017, -0.309017, -0.500000],[-0.681718, 0.147621, -0.716567],[-0.681718, -0.147621, -0.716567],[-0.850651, 0.000000, -0.525731],[-0.688191, 0.587785, -0.425325],[-0.587785, 0.425325, -0.688191],[-0.425325, 0.688191, -0.587785],[-0.425325, -0.688191, -0.587785],[-0.587785, -0.425325, -0.688191],[-0.688191, -0.587785, -0.425325])

fn addcheck cb=
(
if classof cb==string do messagebox cb title:"MD2/TAG Export Error"
)

------------------------------------
-- MD2 Export Stuff
------------------------------------

struct MD2_Tex
(
	s,t
)	

struct MD2_Vertex
(
	pos, lightNormalIndex
)

struct MD2_Frame
(
	scale, translate, name,
	vList,vNorm	
)

struct MD2_Tri
(
	vx,
	st
)

struct MD2_model
(
	skinlist,
	texlist,
	trilist,
	framelist,
	glcmdlist
)

------------------------------------
-- GL COMMAND GENERATION
------------------------------------
global strip_vx
global strip_st
global strip_tris
global used
global cmds
global skins = #()
	
------------------------------------
-- STRIPLENGTH
------------------------------------
fn striplength md2 it iv=
(
	strip_vx=#()
	strip_st=#()
	strip_tris=#()

	tri=md2.trilist[it]
	
	append strip_vx tri.vx[(mod (iv-1) 3) + 1]
	append strip_vx tri.vx[(mod (iv+0) 3) + 1]
	append strip_vx tri.vx[(mod (iv+1) 3) + 1]

	append strip_st tri.st[(mod (iv-1) 3) + 1]
	append strip_st tri.st[(mod (iv+0) 3) + 1]
	append strip_st tri.st[(mod (iv+1) 3) + 1]

	append strip_tris it
	stripcount = 1
	
	mvx1=tri.vx[(mod (iv+1) 3) + 1]
	mst1=tri.st[(mod (iv+1) 3) + 1]
	mvx2=tri.vx[(mod iv 3) + 1]
	mst2=tri.st[(mod iv 3) + 1]
	
	done=false
	
	do
	(
		nexttri=false
		
		for i=it+1 to md2.trilist.count do
		(
			tri = md2.trilist[i]

			if tri.vx[1]==mvx1 do if tri.st[1]==mst1 do if tri.vx[2]==mvx2 do if tri.st[2]==mst2 do
			(				
				if used[i]>0 do
				(
					done=true
					exit
				)

				if (mod stripcount 2) == 1 then
				(
					mvx2=tri.vx[3]
					mst2=tri.st[3]
					append strip_vx mvx2
					append strip_st mst2
				) else (
					mvx1=tri.vx[3]
					mst1=tri.st[3]
					append strip_vx mvx1
					append strip_st mst1
				)
				append strip_tris i
				
				stripcount+=1
				used[i]=2
				
				nexttri=true
				exit
			)

			if tri.vx[2]==mvx1 do if tri.st[2]==mst1 do if tri.vx[3]==mvx2 do if tri.st[3]==mst2 do
			(				
				if used[i]>0 do
				(
					done=true
					exit
				)

				if (mod stripcount 2) == 1 then
				(
					mvx2=tri.vx[1]
					mst2=tri.st[1]
					append strip_vx mvx2
					append strip_st mst2
				) else (
					mvx1=tri.vx[1]
					mst1=tri.st[1]
					append strip_vx mvx1
					append strip_st mst1
				)
				append strip_tris i
				
				stripcount+=1
				used[i]=2
				
				nexttri=true
				exit
			)

			if tri.vx[3]==mvx1 do if tri.st[3]==mst1 do if tri.vx[1]==mvx2 do if tri.st[1]==mst2 do
			(				
				if used[i]>0 do
				(
					done=true
					exit
				)

				if (mod stripcount 2) == 1 then
				(
					mvx2=tri.vx[2]
					mst2=tri.st[2]
					append strip_vx mvx2
					append strip_st mst2
				) else (
					mvx1=tri.vx[2]
					mst1=tri.st[2]
					append strip_vx mvx1
					append strip_st mst1
				)
				append strip_tris i
				
				stripcount+=1
				used[i]=2
				
				nexttri=true
				exit
			)
		)
		if not done and not nexttri do exit
	) while not done
	
	for i=it+1 to md2.trilist.count do
		if used[i]==2 do used[i]=0
	
	return stripcount
)


------------------------------------
-- FANLENGTH
------------------------------------
fn fanlength md2 it iv=
(
	strip_vx=#()
	strip_st=#()
	strip_tris=#()

	tri=md2.trilist[it]
	
	append strip_vx tri.vx[(mod (iv-1) 3) + 1]
	append strip_vx tri.vx[(mod (iv+0) 3) + 1]
	append strip_vx tri.vx[(mod (iv+1) 3) + 1]

	append strip_st tri.st[(mod (iv-1) 3) + 1]
	append strip_st tri.st[(mod (iv+0) 3) + 1]
	append strip_st tri.st[(mod (iv+1) 3) + 1]

	append strip_tris it
	stripcount = 1
	
	mvx1=tri.vx[(mod (iv-1) 3) + 1]
	mst1=tri.st[(mod (iv-1) 3) + 1]
	mvx2=tri.vx[(mod (iv+1) 3) + 1]
	mst2=tri.st[(mod (iv+1) 3) + 1]
	
	done=false
	
	do
	(
		nexttri=false
	
		for i=it+1 to md2.trilist.count do
		(
			tri = md2.trilist[i]

			if tri.vx[1]==mvx1 do if tri.st[1]==mst1 do if tri.vx[2]==mvx2 do if tri.st[2]==mst2 do
			(				
				if used[i]>0 do
				(
					done=true
					exit
				)

				mvx2=tri.vx[3]
				mst2=tri.st[3]
				
				append strip_vx mvx2
				append strip_st mst2
				append strip_tris i
				
				stripcount+=1
				used[i]=2
				
				nexttri=true
				exit
			)

			if tri.vx[2]==mvx1 do if tri.st[2]==mst1 do if tri.vx[3]==mvx2 do if tri.st[3]==mst2 do
			(				
				if used[i]>0 do
				(
					done=true
					exit
				)

				mvx2=tri.vx[1]
				mst2=tri.st[1]
				
				append strip_vx mvx2
				append strip_st mst2
				append strip_tris i
				
				stripcount+=1
				used[i]=2
				
				nexttri=true
				exit
			)
			
			if tri.vx[3]==mvx1 do if tri.st[3]==mst1 do if tri.vx[1]==mvx2 do if tri.st[1]==mst2 do
			(				
				if used[i]>0 do
				(
					done=true
					exit
				)

				mvx2=tri.vx[2]
				mst2=tri.st[2]
				
				append strip_vx mvx2
				append strip_st mst2
				append strip_tris i
				
				stripcount+=1
				used[i]=2
				
				nexttri=true
				exit
			)
		)
		if not done and not nexttri do exit
	) while not done
	
	for i=it+1 to md2.trilist.count do
		if used[i]==2 do used[i]=0
	
	return stripcount
)


------------------------------------
-- MAKEMD2
------------------------------------
fn makemd2 object=
(
	if MD2TAGexport.useanim.state==1 then 
		(
		astart=animationrange.start
		aend=animationrange.end
		)
	else
		(
		astart=MD2TAGexport.animstart.value
		aend=MD2TAGexport.animend.value
		)
	if not(MD2TAGexport.oneframe.state) do 
		(
		astart=1
		aend=1
		)
		
	obj=(object)
	floor=getNodeByName "tag_floor" exact:true
	
	md2=md2_model skinlist:skins texlist:#() trilist:#() framelist:#() glcmdlist:#()
	
	------------------------------------
	-- build texture vert list
	------------------------------------
	for i=1 to getnumtverts obj do
	(
		local s=(((getTVert obj i).x)*256) as integer
		local t=((-(getTVert obj i).y+1)*256) as integer
		append md2.texlist (md2_tex s:s t:t)
	)
	------------------------------------
	-- build triangle list
	------------------------------------
	for i=1 to obj.numfaces do
	(
		local vx=#()
		append vx (((getFace obj i).z) as integer)
		append vx (((getFace obj i).y) as integer)
		append vx (((getFace obj i).x) as integer)
		local st=#()
		append st (((getTVFace obj i).z) as integer)
		append st (((getTVface obj i).y) as integer)
		append st (((getTVface obj i).x) as integer)
		append md2.trilist (md2_Tri vx st)
	)
	-------------------------------------
	-- build frame list
	-------------------------------------
	--print astart
	--print aend

	-------------------------------------
	-- get transformation matrix
	-------------------------------------
	local trafo=matrix3 1
	local traforot=matrix3 1
	if (classof floor)!=UndefinedClass then
	(
		trafo.row1 = normalize floor.transform.row1
		trafo.row2 = normalize floor.transform.row2
		trafo.row3 = normalize floor.transform.row3
		trafo.row4 = floor.transform.row4
		trafo.row4.z += 32
		trafo = inverse trafo

		traforot = copy trafo
		traforot.row4 = [0,0,0]
	)

	for f=astart to aend by MD2TAGexport.stepframe.value do 
	(
		local vertlist=#()
		local normlist=#()
		frame=MD2_frame scale:[1,1,1] translate:[0,0,0] name:"FRAME 000......"       
		vmin=[0,0,0]
		vmax=[0,0,0]

		if MD2TAGexport.unibbox.state then
		(
			for curObj in objects do
				for i=1 to curObj.numverts do at time f
				(
					local v=(getvert curObj i) * trafo
					if v.x<vmin.x do vmin.x=v.x
					if v.y<vmin.y do vmin.y=v.y
					if v.z<vmin.z do vmin.z=v.z
					if v.x>vmax.x do vmax.x=v.x
					if v.y>vmax.y do vmax.y=v.y
					if v.z>vmax.z do vmax.z=v.z	
				)
		) else (
			for i=1 to obj.numverts do at time f
			(
				local v=(getvert obj i) * trafo
				if v.x<vmin.x do vmin.x=v.x
				if v.y<vmin.y do vmin.y=v.y
				if v.z<vmin.z do vmin.z=v.z
				if v.x>vmax.x do vmax.x=v.x
				if v.y>vmax.y do vmax.y=v.y
				if v.z>vmax.z do vmax.z=v.z	
			)
		)
			
		frame.translate=vmin
		frame.scale=(vmax-vmin)/[255,255,255]
	
		for i=1 to obj.numverts do at time f
		(
			local v=(getvert obj i) * trafo
			v-=frame.translate
			v/=frame.scale
			append vertlist v
			------------------------------------
			-- do normals
			------------------------------------
			local vn=(getnormal obj i) * traforot
			local maxdot=-99999.0
			local normindex=0
			if MD2TAGexport.donormals.state do
				for j=1 to nlist.count do
				(
					local dotvalue
					dotvalue=dot nlist[j] vn
					if dotvalue>maxdot do
					(
						maxdot=dotvalue
						normindex=j-1
					)
				)
			append normlist normindex 
		)
		
		frame.vList=vertlist
		frame.vNorm=normlist
		append md2.framelist frame
	)	
	
	------------------------------------
	-- Z-sort triangle list
	------------------------------------
	if MD2TAGexport.dozsort.state do
	(
		fn zsort t1 t2 ff: = 
		( (ff.vList[t1.vx[1]].z+ff.vList[t1.vx[2]].z+ff.vList[t1.vx[3]].z) - (ff.vList[t2.vx[1]].z+ff.vList[t2.vx[2]].z+ff.vList[t2.vx[3]].z) )

		qsort md2.trilist zsort ff:md2.framelist[1]
	)
	
	------------------------------------
	-- build GL command list
	------------------------------------
	if MD2TAGexport.doglcmds.state do
	(
		if cmds==undefined or not MD2TAGexport.sameglcmds.state do
		(
			cmds=#()
			used=#()
			for i=1 to md2.trilist.count do append used 0
			
			best_vx=#()
			best_st=#()
			best_tris=#()
	
			for i=1 to md2.trilist.count do
			(
				if used[i]>0 do continue
				
				bestlen=0
				besttype=0
				
				for type=1 to 2 do
				(
					for sv=1 to 3 do
					(
						if type==1 then len=striplength md2 i sv
						else len=fanlength md2 i sv
						
						if len>bestlen do
						(
							besttype=type
							bestlen=len
							best_vx=#()
							best_st=#()
							best_tris=#()
	
							for j=1 to bestlen+2 do
							(
								append best_vx strip_vx[j]
								append best_st strip_st[j]
							)
							for j=1 to bestlen do
								append best_tris strip_tris[j]
						)
					)
				)
				
				for j=1 to bestlen do
					used[best_tris[j]]=1
					
				if besttype==1 then append cmds (bestlen+2)
				else append cmds -(bestlen+2)
				
				for j=1 to bestlen+2 do
				(
					k=best_st[j]
					
					s=md2.texlist[k].s as float
					t=md2.texlist[k].t as float
					
					s=(s+0.5)/256;
					t=(t+0.5)/256;
					
					append cmds s
					append cmds t
					append cmds (best_vx[j]-1)   -- (C Style: -1)
				)		
			)		
			append cmds 0
			
			if not MD2TAGexport.sameglcmds.enabled do
				MD2TAGexport.sameglcmds.enabled=true
		)
		
		md2.glcmdlist=cmds
	)
	
	md2
)

------------------------------------
-- WRITEMD2
------------------------------------
fn writemd2 object md2file=
(
	if object.count==0 do return "No object selected"
	if object.count>1 do return "More than one object selected"
	obj=object[1]
	if (classof obj)!=editable_mesh do return "Object is not a mesh"
	model=makemd2 obj
	
	f=fopen md2file "wb"
	
	----------------------------------------
	-- write the header
	----------------------------------------
	
	magic=844121161
	version=8
	skinw=256
	skinh=256
	framenum=model.framelist.count
	skinnum=model.skinlist.count
	vertnum=model.framelist[1].vlist.count
	texnum=model.texlist.count
	trinum=model.trilist.count
	glnum=model.glcmdlist.count
	
	texsize=4
	trisize=12
	framesize=40+(4*vertnum)
	
	headersize=68				-- bytes in header
	offsetskins=headersize
	offsettexcoords=offsetskins+(skinnum*64)
	offsettris=offsettexcoords+(texnum*texsize)
	offsetframes=offsettris+(trinum*trisize)
	offsetgl=offsetframes+(framenum*framesize)
	offsetend=offsetgl+(glnum*4)
	
	writelong f magic
	writelong f version
	writelong f skinw				--(blitz dont care)
	writelong f skinh				--(blitz dont care)
	writelong f framesize
	writelong f skinnum
	writelong f vertnum
	writelong f texnum
	writelong f trinum
	writelong f glnum
	writelong f framenum
	writelong f offsetskins		--offset to skins
	writelong f offsettexcoords	--offset to texture coords
	writelong f offsettris		--offset to triangles
	writelong f offsetframes		--offset to frames
	writelong f offsetgl			--offset to gl commands
	writelong f offsetend			--offset to end

-----------------------------------------------------
-- end of header
-----------------------------------------------------
-- write out the skin names
-----------------------------------------------------
	for i in model.skinlist do
	(
		if i.count>63 do return "Skin name too long"
		writestring f i
		for j=i.count to 62 do writebyte f 0
	)

-----------------------------------------------------
-- write texture coordinate list
-----------------------------------------------------
	for i in model.texlist do
	(
		writeshort f i.s #unsigned
		writeshort f i.t #unsigned
	)
	
-----------------------------------------------------
--write triangles (C style: -1)
-----------------------------------------------------	
	for i in model.trilist do
	(
		writeshort f (i.vx[1]-1) #unsigned
		writeshort f (i.vx[2]-1) #unsigned
		writeshort f (i.vx[3]-1) #unsigned
		writeshort f (i.st[1]-1) #unsigned
		writeshort f (i.st[2]-1) #unsigned
		writeshort f (i.st[3]-1) #unsigned
	)

-----------------------------------------------------
-- write frames
-----------------------------------------------------
	for i in model.framelist do
	(
		writefloat f i.scale.x
		writefloat f i.scale.y
		writefloat f i.scale.z
		writefloat f i.translate.x
		writefloat f i.translate.y
		writefloat f i.translate.z
		writestring f i.name
		k=0
		for j=1 to i.vlist.count do
		(
			writebyte f i.vlist[j].x
			writebyte f i.vlist[j].y
			writebyte f i.vlist[j].z
			writebyte f i.vNorm[j]
		)
	)
-----------------------------------------------------
-- write gl commands
-----------------------------------------------------
	for i in model.glcmdlist do
	(
		if (classof i)==integer then writelong f i
		else if (classof i)==float then writefloat f i
		else return "Unknown glcmd data type"			
	)
	
------------------------------------------------------
-- end of file writing
------------------------------------------------------
	msgstr = "MD2: " + md2file + " ->\n" + obj.name + " exported"
	messagebox msgstr title:"MD2 Export Success"

	fclose f
)


------------------------------------
-- TAG Export Stuff
------------------------------------

struct TAG_single
(
	matl
)

struct TAG_file
(
	framenum,
	namel,
	tagl
)

------------------------------------
-- MAKETAGS
------------------------------------
fn maketags objlist=
(
	-------------------------------------
	-- get frame range
	-------------------------------------
	if MD2TAGexport.useanim.state==1 then 
		(
		astart=animationrange.start
		aend=animationrange.end
		)
	else
		(
		astart=MD2TAGexport.animstart.value
		aend=MD2TAGexport.animend.value
		)
	if not(MD2TAGexport.oneframe.state) do 
		(
		astart=1
		aend=1
		)
		
	num=aend-astart+1
	tags=TAG_file framenum:num namel:#() tagl:#()
	floor=getNodeByName "tag_floor" exact:true
		
	-------------------------------------
	-- get transformation matrix
	-------------------------------------
	local trafo=matrix3 1
	if (classof floor)!=UndefinedClass then
	(
		trafo.row1 = normalize floor.transform.row1
		trafo.row2 = normalize floor.transform.row2
		trafo.row3 = normalize floor.transform.row3
		trafo.row4 = floor.transform.row4
		trafo.row4.z += 32
		trafo = inverse trafo		
	)

	-------------------------------------
	-- go through all objects
	-------------------------------------
	for obj in objlist do
	(
		-------------------------------------
		-- build frame list
		-------------------------------------
		local tag=TAG_single matl:#()
		
		for f=astart to aend by MD2TAGexport.stepframe.value do at time f
		(
			local mat=matrix3 0
			
			mat = obj.transform * trafo
			mat.row1 = normalize mat.row1
			mat.row2 = normalize mat.row2
			mat.row3 = normalize mat.row3
			
			append tag.matl mat
		)
		
		append tags.namel obj.name
		append tags.tagl tag
	)
	
	tags
)

------------------------------------
-- WRITETAGS
------------------------------------
fn writetags objlist tagfile=
(
	if objlist.count==0 do return "No objects selected"
	tags=maketags objlist	
	
	f=fopen tagfile "wb"
	
	----------------------------------------
	-- write the header
	----------------------------------------
	
	magic=844121162
	version=1
	tagnum=tags.namel.count
	framenum=tags.framenum
	
	headersize=32
	offsetnames=headersize
	offsettags=offsetnames+(tagnum*64)
	offsetend=offsettags+(tagnum*framenum*48)
	offsetextractend=offsettags+(tagnum*framenum*64)
	
	writelong f magic
	writelong f version
	writelong f tagnum
	writelong f framenum
	writelong f offsetnames
	writelong f offsettags
	writelong f offsetend
	writelong f offsetextractend

	-----------------------------------------------------
	-- end of header
	-----------------------------------------------------
	-- write out the tag names
	-----------------------------------------------------
	for i in tags.namel do
	(
		if i.count>63 do return "Object name too long"
		writestring f i
		for j=i.count to 62 do writebyte f 0
	)

	-----------------------------------------------------
	-- write positions and matrices
	-----------------------------------------------------
	for i in tags.tagl do
	(
		for j=1 to i.matl.count do
		(
			writefloat f i.matl[j].row1.x
			writefloat f i.matl[j].row1.y
			writefloat f i.matl[j].row1.z
			writefloat f i.matl[j].row2.x
			writefloat f i.matl[j].row2.y
			writefloat f i.matl[j].row2.z
			writefloat f i.matl[j].row3.x
			writefloat f i.matl[j].row3.y
			writefloat f i.matl[j].row3.z
			writefloat f i.matl[j].row4.x
			writefloat f i.matl[j].row4.y
			writefloat f i.matl[j].row4.z
		)
	)
	
	------------------------------------------------------
	-- end of file writing
	------------------------------------------------------
	msgstr = "TAG: " + tagfile + " ->";
	for obj in objlist do msgstr += "\n  " + obj.name
	messagebox msgstr title:"TAG Export Success"

	fclose f	
)


------------------------------------
-- Utility Stuff
------------------------------------

utility MD2TAGExport "MD2/TAG Exporter"
(
	group "Animation"
	(
		checkbox oneframe "Save Animation" checked:false align:#center
		spinner stepframe "Frame Step" range:[1,200,1] fieldwidth:40 type:#integer
		radiobuttons useanim labels:#("Active Time Segment","Custom Time Segment")
		spinner animstart "from" range:[0,1000,0] type:#integer fieldwidth:40 across:2
		spinner animend "to" range:[0,1000,40] type:#integer fieldwidth:40
	)
	group "Skins"
	(
		edittext skinText
		listbox skinBox items:#() height:4
		button baddSkin "Add" across:2
		button bdelSkin "Delete"
	)
	group "MD2 Export"
	(
		checkbox dozsort "Z-Sort Triangles" align:#center checked:true
		checkbox donormals "Generate Normals" align:#center checked:true
		checkbox doglcmds "Add GL Commands" align:#center checked:true
		checkbox unibbox "Unified Bounding Box" align:#center checked:false
		checkbox sameglcmds "Use same GLCmds" align:#center checked:false enabled:false
		button bexportMD2 "     Export MD2    "
	)
	group "TAG Export"
	(
		button bexportTAG "     Export TAG    "
	)
	group "About"
	(
		label titleLabel	"MD2/TAG Exporter v1.4"
		HyperLink credits1 "by Herby" align:#center address:"mailto:herby@planetquake.com" color:(color 0 100 0) hoverColor:(color 0 0 100)
		label credits2 "based on code"
		HyperLink credits3 "by Adam Barton" align:#center address:"mailto:adam.add3d.co.uk" color:(color 0 100 0) hoverColor:(color 0 0 100)
	)
	
	on baddSkin pressed do
	(
		append skins skinText.text
		skinBox.items = skins
	)
	
	on bdelSkin pressed do
	(
		if skinBox.selection > 0 then deleteItem skins skinBox.selection
		skinBox.items = skins
	)
	
	on skinBox selected num do
	(
		skinText.text = skinBox.selected
	)
	
--	on skinText entered text do
--	(
--		if skinBox.selection > 0 then skins[skinBox.selection] = skinText.text
--		skinBox.items = skins
--	)
		
	on bexportMD2 pressed do
	( 
		if heapSize < 32000000 then heapSize = 32000000
		
		md2filename=getsavefilename caption:"Save MD2" \
			filename:(lastfile+".md2") \
			types:"Blitz MD2 (*.md2)|*.md2|All Files (*.*)|*.*|"
		if md2filename!=undefined do 
		(				
			addcheck (writemd2 selection md2filename)
			lastfile=substring md2filename 1 (md2filename.count-4)
		)
	)

	on bexportTAG pressed do
	( 
		if heapSize < 32000000 then heapSize = 32000000

		tagfilename=getsavefilename caption:"Save TAG" \
			filename:(lastfile+".tag") \
			types:"Herby TAG (*.tag)|*.tag|All Files (*.*)|*.*|"
		if tagfilename!=undefined do 
		(				
			addcheck (writetags selection tagfilename)
			lastfile=substring tagfilename 1 (tagfilename.count-4)
		)
	)
)
