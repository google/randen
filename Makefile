override CPPFLAGS += -I../..
override CXXFLAGS += -std=c++11 -Wall -O3 -fno-pic -mavx2 -maes
override LDFLAGS += $(CXXFLAGS)
override CXX = clang++

all: $(addprefix bin/, nanobenchmark_test randen_test randen_benchmark vector128_test)

obj/%.o: %.cc
	@mkdir -p -- $(dir $@)
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $< -o $@

bin/%: obj/%.o obj/nanobenchmark.o obj/randen.o
	@mkdir -p bin
	$(CXX) $(LDFLAGS) $^ -o $@

.DELETE_ON_ERROR:
deps.mk: $(wildcard *.cc) $(wildcard *.h) Makefile
	set -eu; for file in *.cc; do \
		target=obj/$${file##*/}; target=$${target%.*}.o; \
		$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -MM -MT \
		"$$target" "$$file"; \
	done >$@
-include deps.mk

clean:
	[ ! -d obj ] || $(RM) -r -- obj/
	[ ! -d bin ] || $(RM) -r -- bin/
	[ ! -d lib ] || $(RM) -r -- lib/

.PHONY: clean all
