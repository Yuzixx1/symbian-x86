APP := PROMPT
SRCS := main.c
SDK := ../..

all:
	python3 "$(SDK)/tools/buildapp.py" --sdk "$(SDK)" --app $(APP) $(SRCS:%=--src %)

clean:
	rm -rf build $(APP).E32

.PHONY: all clean
