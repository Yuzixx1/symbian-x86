# Builds every sample program. Each sample is a standalone app directory
# with its own Makefile; this just fans out to them.
APPS := calc guess wc rev fib mem

.PHONY: all clean
all:
	@for app in $(APPS); do \
		echo "== $$app"; \
		$(MAKE) -C $$app || exit 1; \
	done

clean:
	@for app in $(APPS); do $(MAKE) -C $$app clean; done
