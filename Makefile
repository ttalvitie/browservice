CXX ?= g++
CFLAGS_COMMON := -std=c++17 -D_FILE_OFFSET_BITS=64 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror -Wno-error=deprecated-declarations -fno-threadsafe-statics -fvisibility-inlines-hidden -Wsign-compare -Wno-psabi `pkg-config --cflags pangoft2`
CFLAGS_debug := $(CFLAGS_COMMON) -g -O0
CFLAGS_release := $(CFLAGS_COMMON) -O3 -DNDEBUG -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
LDFLAGS_COMMON := -rdynamic -fPIC -pthread -Wl,--disable-new-dtags -Wl,--fatal-warnings -Wl,-z,noexecstack -Wl,-z,now -Wl,-z,relro -Wl,-rpath,"\$$$$ORIGIN" cef/Release/libcef.so `pkg-config --libs pangoft2` -lX11 -ldl
LDFLAGS_debug := $(LDFLAGS_COMMON) cef/debugbuild/libcef_dll_wrapper/libcef_dll_wrapper.a
LDFLAGS_release := $(LDFLAGS_COMMON) -Wl,-O1 -Wl,--as-needed -Wl,--gc-sections cef/releasebuild/libcef_dll_wrapper/libcef_dll_wrapper.a
SRCS := $(shell find src -name '*.cpp')
CEFFILES_IN := cef/Release/chrome-sandbox cef/Release/libcef.so cef/Release/libEGL.so cef/Release/libGLESv2.so cef/Release/snapshot_blob.bin cef/Release/v8_context_snapshot.bin cef/Resources/resources.pak cef/Resources/chrome_100_percent.pak cef/Resources/chrome_200_percent.pak cef/Resources/icudtl.dat cef/Resources/locales cef/Release/libvk_swiftshader.so cef/Release/libvulkan.so.1 cef/Release/vk_swiftshader_icd.json

define OUTDEFS
OBJS_$(1) := $(SRCS:%.cpp=$(1)/obj/%.o)
DEPS_$(1) := $(SRCS:%.cpp=$(1)/obj/%.d)
CEFFILES_OUT_$(1) := $(addprefix $(1)/bin/,$(notdir $(CEFFILES_IN)))
endef
$(foreach b,debug release,$(eval $(call OUTDEFS,$(b))))

.PHONY: debug release clean default FORCE

default: release

define BUILDRULES

$(1): $(1)/bin/browservice $(1)/bin/retrojsvice.so $(CEFFILES_OUT_$(1))
	@if [ `stat -c '%U:%G:%a' $(1)/bin/chrome-sandbox` != "root:root:4755" ]; \
	then \
	echo; \
	echo "*** Run the following manually to set SUID permissions for chrome-sandbox: ***"; \
	echo "sudo chown root:root $(1)/bin/chrome-sandbox && sudo chmod 4755 $(1)/bin/chrome-sandbox"; \
	fi

$(1)/bin/browservice: $(OBJS_$(1))
	@mkdir -p $(1)/bin
	$(CXX) $(CFLAGS_$(1)) $(OBJS_$(1)) -o $(1)/bin/browservice $(LDFLAGS_$(1))

endef
$(eval $(call BUILDRULES,debug))
$(eval $(call BUILDRULES,release))

define OBJRULE
$(2:%.cpp=$(1)/obj/%.o): $(2) cef/include
	@mkdir -p `dirname $(2:%.cpp=$(1)/obj/%.o)`
	$(CXX) $(CFLAGS_$(1)) -Icef -Isrc -MMD -c $(2) -o $(2:%.cpp=$(1)/obj/%.o)
endef
$(foreach s,$(SRCS),$(eval $(call OBJRULE,debug,$(s))))
$(foreach s,$(SRCS),$(eval $(call OBJRULE,release,$(s))))

define CEFFILE_RULE
$(addprefix $(1)/bin/,$(notdir $(2))): $(2)
	@mkdir -p $(1)/bin
	cp -r $(2) $(addprefix $(1)/bin/,$(notdir $(2)))
endef
$(foreach f,$(CEFFILES_IN),$(eval $(call CEFFILE_RULE,debug,$(f))))
$(foreach f,$(CEFFILES_IN),$(eval $(call CEFFILE_RULE,release,$(f))))

release/bin/retrojsvice.so: viceplugins/retrojsvice/release/lib/retrojsvice.so
	@mkdir -p release/bin
	cp $< $@

debug/bin/retrojsvice.so: viceplugins/retrojsvice/debug/lib/retrojsvice.so
	@mkdir -p debug/bin
	cp $< $@

FORCE: ;

viceplugins/retrojsvice/release/lib/retrojsvice.so: FORCE
	$(MAKE) -C viceplugins/retrojsvice release

viceplugins/retrojsvice/debug/lib/retrojsvice.so: FORCE
	$(MAKE) -C viceplugins/retrojsvice debug

clean:
	rm -rf $(OBJS_debug) $(OBJS_release) $(DEPS_debug) $(DEPS_release) debug/bin/browservice release/bin/browservice debug/bin/retrojsvice.so release/bin/retrojsvice.so $(CEFFILES_OUT_debug) $(CEFFILES_OUT_release)
	$(MAKE) -C viceplugins/retrojsvice clean

-include $(DEPS_debug) $(DEPS_release)
