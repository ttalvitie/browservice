CXX ?= g++
CFLAGS_COMMON := -std=c++17 -fPIC -fvisibility=hidden -Wall -Werror -Wno-error=deprecated-declarations -Wsign-compare
CFLAGS_debug := $(CFLAGS_COMMON) -g -O0
CFLAGS_debug_png := $(CFLAGS_COMMON) -g -O3
CFLAGS_release := $(CFLAGS_COMMON) -O3 -DNDEBUG
CFLAGS_release_png := $(CFLAGS_release)
LDFLAGS_COMMON := -shared -fPIC -pthread -lPocoFoundation -lPocoNet -ljpeg -lz -latomic -lcrypto
LDFLAGS_debug := $(LDFLAGS_COMMON)
LDFLAGS_release := $(LDFLAGS_COMMON)
SRCS := $(shell find src -name '*.cpp') gen/html.cpp
HTMLS := $(shell find html -name '*.html')

define OUTDEFS
OBJS_$(1) := $(SRCS:%.cpp=$(1)/obj/%.o)
DEPS_$(1) := $(SRCS:%.cpp=$(1)/obj/%.d)
endef
$(foreach b,debug release,$(eval $(call OUTDEFS,$(b))))

.PHONY: debug release clean default

default: release

define BUILDRULES

$(1): $(1)/lib/retrojsvice.so

$(1)/lib/retrojsvice.so: $(OBJS_$(1))
	@mkdir -p $(1)/lib
	$(CXX) $(CFLAGS_$(1)) $(OBJS_$(1)) -o $(1)/lib/retrojsvice.so $(LDFLAGS_$(1))

endef
$(eval $(call BUILDRULES,debug))
$(eval $(call BUILDRULES,release))

define OBJRULE
$(2:%.cpp=$(1)/obj/%.o): $(2)
	@mkdir -p `dirname $(2:%.cpp=$(1)/obj/%.o)`
	$(CXX) $(if $(filter src/png.cpp,$(2)),$(CFLAGS_$(1)_png),$(CFLAGS_$(1))) -Isrc -MMD -c $(2) -o $(2:%.cpp=$(1)/obj/%.o)
endef
$(foreach s,$(SRCS),$(eval $(call OBJRULE,debug,$(s))))
$(foreach s,$(SRCS),$(eval $(call OBJRULE,release,$(s))))

gen/html.cpp: $(HTMLS) gen_html_cpp.py
	@mkdir -p gen
	./gen_html_cpp.py > gen/html.cpp.tmp
	mv gen/html.cpp.tmp gen/html.cpp

clean:
	rm -rf $(OBJS_debug) $(OBJS_release) $(DEPS_debug) $(DEPS_release) debug/lib/retrojsvice.so release/lib/retrojsvice.so gen/html.cpp gen/html.cpp.tmp

-include $(DEPS_debug) $(DEPS_release)
