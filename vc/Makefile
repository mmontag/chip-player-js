
VC_DFILES	= Makefile libxmp.sln libxmp.vcproj
VC_PATH		= vc
VC_CLEAN	= libxmp.vcproj

default-vc:
	$(MAKE) -C .. vcproj

dist-vc: vcproj
	mkdir -p $(DIST)/$(VC_PATH)
	cp -RPp $(addprefix $(VC_PATH)/,$(VC_DFILES)) $(DIST)/$(VC_PATH)

vcproj: $(VC_PATH)/libxmp.vcproj

$(VC_PATH)/libxmp.vcproj: Makefile $(SRC_PATH)/Makefile $(LOADERS_PATH)/Makefile $(PROWIZ_PATH)/Makefile $(VC_PATH)/libxmp.vcproj.in
	@echo Generate $@
	@rm -f $@.tmp
	@for i in $(OBJS:.o=.c) $(PROWIZARD_OBJS:.o=.c) $(DEPACKER_OBJS:.o=.c); do \
	    echo -e "\t\t\t<File" >> $@.tmp; \
	    echo -e "\t\t\t\tRelativePath=\"../$$i\""|sed 's!/!\\!g' >> $@.tmp;\
	    echo -e "\t\t\t\t>" >> $@.tmp; \
	    echo -e "\t\t\t</File>" >> $@.tmp; \
	done
	@sed -e '/@SOURCES@/{r $@.tmp' -e 'd' -e '}' < $@.in > $@
	@rm -f $@.tmp
	@unix2dos $@
