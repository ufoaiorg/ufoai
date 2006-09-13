# =================================
# Client objects
# =================================

# =================================
# Configure build directory
# =================================

build_client?=$(DIR_BUILD)/client
mkdir_dir_list+=$(build_client)

# =================================
# Configure common client headers
# =================================

hdr_client+=$(src_client)/client.h

# =================================
# Objects
# =================================

obj_client+=$(build_client)/cl_actor.o
obj_client+=$(build_client)/cl_aircraft.o
obj_client+=$(build_client)/cl_basemanagement.o
obj_client+=$(build_client)/cl_campaign.o
obj_client+=$(build_client)/cl_employee.o
obj_client+=$(build_client)/cl_fx.o
obj_client+=$(build_client)/cl_input.o
obj_client+=$(build_client)/cl_inventory.o
obj_client+=$(build_client)/cl_le.o
obj_client+=$(build_client)/cl_main.o
obj_client+=$(build_client)/cl_map.o
obj_client+=$(build_client)/cl_market.o
obj_client+=$(build_client)/cl_menu.o
obj_client+=$(build_client)/cl_parse.o
obj_client+=$(build_client)/cl_particle.o
obj_client+=$(build_client)/cl_popup.o
obj_client+=$(build_client)/cl_produce.o
obj_client+=$(build_client)/cl_radar.o
obj_client+=$(build_client)/cl_research.o
obj_client+=$(build_client)/cl_scrn.o
obj_client+=$(build_client)/cl_sequence.o
obj_client+=$(build_client)/cl_shader.o
obj_client+=$(build_client)/cl_team.o
obj_client+=$(build_client)/cl_ufo.o
obj_client+=$(build_client)/cl_ufopedia.o
obj_client+=$(build_client)/cl_view.o
obj_client+=$(build_client)/console.o
obj_client+=$(build_client)/keys.o
obj_client+=$(build_client)/snd_mem.o
obj_client+=$(build_client)/snd_mix.o
obj_client+=$(build_client)/snd_ref.o
obj_client+=$(build_client)/x86.o

# =================================
# Implicit Rules
# =================================

$(build_client)/%.o: $(src_client)/%.c $(src_client)/%.h $(HDR_COMMON) $(hdr_client) $(hdr_game)
	$(CC_CLIENT)

# Fall-back option for source files without headers.

$(build_client)/%.o: $(src_client)/%.c $(HDR_COMMON) $(hdr_client)
	$(CC_CLIENT)

# =================================
# Target-specific Rules
# =================================

$(target_client): $(obj_system) $(obj_system_client) $(obj_common) $(obj_client) $(obj_client_main)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(obj_system) $(obj_system_client) $(obj_common) $(obj_client) $(obj_client_main) -o $(target_client)
