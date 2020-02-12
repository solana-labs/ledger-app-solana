TEST_SRCS := $(wildcard src/*Test.c)
TEST_BINS = $(patsubst src/%.c,obj/%,$(TEST_SRCS))
TEST_OKS = $(addsuffix .ok,$(TEST_BINS))

all: $(TEST_OKS) $(TEST_BINS)

# Note: this executes on the host, not via QEMU
obj/%Test.ok: obj/%Test
	@echo "Testing $<..."
	@$< && touch $@

obj/%Test: src/%.c src/%Test.c
	@mkdir -p $(@D)
	$(CC) $(filter-out $<,$^) -o $@

obj/systemInstructionTest: src/parser.c
