PROJECT             := codeblocks
EXTENSION           := .cbp
WORKSPACE_EXTENSION := .workspace

define BUILD_$(PROJECT)_RULE
ifndef $(1)_DISABLE
ifndef $(1)_IGNORE

COMPILATION_$(PROJECT)_UNITS_$(1) := $(addprefix \t\t<Unit filename='$(SRCDIR)/,$(addsuffix '/>\n,$($(1)_SRCS)))

.PHONY: $(PROJECT)-$(1)
$(PROJECT)-$(1):
	@sed "s#%%COMPILATION_UNITS%%#$$(COMPILATION_$(PROJECT)_UNITS_$(1))#"g $(PROJECTSDIR)/$(PROJECT)/$(PROJECT).in > $(1)$(EXTENSION)
	@sed -i "s#%%CFLAGS%%#$$(CXXFLAGS) $$($(1)_CXXFLAGS)#"g $(1)$(EXTENSION)
	@sed -i "s#(-D.*)#$$(CXXFLAGS) $$($(1)_CXXFLAGS)#"g $(1)$(EXTENSION)
	@sed -i "s#%%LDFLAGS%%#$$($(1)_LDFLAGS) $$(LDFLAGS)#"g $(1)$(EXTENSION)
	@sed -i "s#%%TARGET%%#$$(MODE)-$$(TARGET_OS)#"g $(1)$(EXTENSION)
	@sed -i "s#%%TARGET_OS%%#$$(TARGET_OS)#"g $(1)$(EXTENSION)
	@sed -i "s#%%NAME%%#$(1)#"g $(1)$(EXTENSION)
	@echo '===> [$(PROJECT)] $(1)$(EXTENSION)'
	@sed "s#%%NAME%%#$(1)#"g $(PROJECTSDIR)/$(PROJECT)/$(PROJECT).workspace.in > $(1)$(WORKSPACE_EXTENSION)
	@echo '===> [$(PROJECT) Workspace] $(1)$(WORKSPACE_EXTENSION)'
endif
endif
endef
$(foreach TARGET,$(TARGETS),$(eval $(call BUILD_$(PROJECT)_RULE,$(TARGET))))

.PHONY: $(PROJECT)
$(PROJECT): $(addprefix $(PROJECT)-,$(TARGETS))
