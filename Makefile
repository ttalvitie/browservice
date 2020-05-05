CXX ?= g++
CFLAGS_COMMON := -std=c++17 -D_FILE_OFFSET_BITS=64 -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -fno-strict-aliasing -fPIC -fstack-protector -funwind-tables -fvisibility=hidden --param=ssp-buffer-size=4 -pipe -pthread -Wall -Werror -fno-threadsafe-statics -fvisibility-inlines-hidden -Wsign-compare -Wno-psabi `pkg-config --cflags pangoft2`
CFLAGS_debug := $(CFLAGS_COMMON) -g -O0
CFLAGS_release := $(CFLAGS_COMMON) -O3 -DNDEBUG -fdata-sections -ffunction-sections -fno-ident -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
LDFLAGS_COMMON := -rdynamic -fPIC -pthread -Wl,--disable-new-dtags -Wl,--fatal-warnings -Wl,-rpath,. -Wl,-z,noexecstack -Wl,-z,now -Wl,-z,relro -Wl,-rpath,"\$$$$ORIGIN" cef/Release/libcef.so -lPocoFoundation -lPocoNet `pkg-config --libs pangoft2` -ljpeg -lz
LDFLAGS_debug := $(LDFLAGS_COMMON) cef/debugbuild/libcef_dll_wrapper/libcef_dll_wrapper.a
LDFLAGS_release := $(LDFLAGS_COMMON) -Wl,-O1 -Wl,--as-needed -Wl,--gc-sections cef/releasebuild/libcef_dll_wrapper/libcef_dll_wrapper.a
SRCS := $(shell find src -name '*.cpp') gen/html.cpp
HTMLS := $(shell find html -name '*.html')
CEFFILES_IN := cef/Release/chrome-sandbox cef/Release/libcef.so cef/Release/libEGL.so cef/Release/libGLESv2.so cef/Release/snapshot_blob.bin cef/Release/v8_context_snapshot.bin cef/Release/swiftshader cef/Resources/cef.pak cef/Resources/cef_100_percent.pak cef/Resources/cef_200_percent.pak cef/Resources/cef_extensions.pak cef/Resources/devtools_resources.pak cef/Resources/icudtl.dat cef/Resources/locales

define OUTDEFS
OBJS_$(1) := $(SRCS:%.cpp=$(1)/obj/%.o)
DEPS_$(1) := $(SRCS:%.cpp=$(1)/obj/%.d)
CEFFILES_OUT_$(1) := $(addprefix $(1)/bin/,$(notdir $(CEFFILES_IN)))
endef
$(foreach b,debug release,$(eval $(call OUTDEFS,$(b))))

.PHONY: debug release clean default

default: release

define BUILDRULES

$(1): $(1)/bin/baas $(CEFFILES_OUT_$(1))
	@if [ `stat -c '%U:%G:%a' $(1)/bin/chrome-sandbox` != "root:root:4755" ]; \
	then \
	echo; \
	echo "*** Run the following manually to set SUID permissions for chrome-sandbox: ***"; \
	echo "sudo chown root:root $(1)/bin/chrome-sandbox && sudo chmod 4755 $(1)/bin/chrome-sandbox"; \
	fi

$(1)/bin/baas: $(OBJS_$(1))
	@mkdir -p $(1)/bin
	$(CXX) $(CFLAGS_$(1)) $(OBJS_$(1)) -o $(1)/bin/baas $(LDFLAGS_$(1))

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

gen/html.cpp: $(HTMLS) gen_html_header.py
	@mkdir -p gen
	./gen_html_header.py > gen/html.cpp

define CEFFILE_RULE
$(addprefix $(1)/bin/,$(notdir $(2))): $(2)
	@mkdir -p $(1)/bin
	cp -r $(2) $(addprefix $(1)/bin/,$(notdir $(2)))
endef
$(foreach f,$(CEFFILES_IN),$(eval $(call CEFFILE_RULE,debug,$(f))))
$(foreach f,$(CEFFILES_IN),$(eval $(call CEFFILE_RULE,release,$(f))))

clean:
	rm -rf $(OBJS_debug) $(OBJS_release) $(DEPS_debug) $(DEPS_release) debug/bin/baas release/bin/baas $(CEFFILES_OUT_debug) $(CEFFILES_OUT_release)

-include $(DEPS_debug) $(DEPS_release)
