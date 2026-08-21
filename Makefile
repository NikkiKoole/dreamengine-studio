.PHONY: start serve install help

# default target
start:
	-git config core.hooksPath .githooks
	@# Kill only THIS clone's stale processes. Every one of them (concurrently, vite,
	@# electron + its helpers) carries $(CURDIR)/editor/node_modules in its argv, so the
	@# path IS the scope — a bare `pkill -f vite` also killed any other vite dev server
	@# on the machine, and `pkill -f electron` any other Electron app.
	@# The [e]/[d] brackets are LOAD-BEARING, not a typo: this recipe's own shell has the
	@# pattern in its command line, and pkill spares itself but NOT its parent — so an
	@# unbracketed pattern makes `make` kill the shell running it. The class matches the
	@# same path while not matching the pattern's own literal text.
	-pkill -f "$(CURDIR)/[e]ditor/node_modules" 2>/dev/null; pkill -f "$(CURDIR)/[d]ata-tools/fmltools/floorplanner.js --serve" 2>/dev/null; sleep 1
	@# floorplan cart id-picker fetch-bridge — only if a Floorplanner credential is set (else the
	@# picker just shows a "run --serve" hint). Backgrounded; logs to build/.fp-serve.log.
	@if [ -f data-tools/fmltools/.token ] || [ -n "$$FP_AUTH_TOKEN" ] || [ -n "$$FP_SESSION" ]; then \
		mkdir -p build; \
		echo "▸ floorplan fetch-bridge running (floorplanner.js --serve → build/.fp-serve.log)"; \
		( bash -c 'source ~/.nvm/nvm.sh && nvm use 22 >/dev/null 2>&1 && node $(CURDIR)/data-tools/fmltools/floorplanner.js --serve' >build/.fp-serve.log 2>&1 & ) ; \
	else \
		echo "(floorplan --serve skipped — no credential; paste one into data-tools/fmltools/.token to auto-fetch ids in the picker)"; \
	fi
	cd editor && bash -c 'source ~/.nvm/nvm.sh && nvm use 22 && npm start'

# just the floorplan cart's fetch-bridge (foreground), for the id picker — run alongside the editor
serve:
	node data-tools/fmltools/floorplanner.js --serve

install:
	-git config core.hooksPath .githooks
	cd editor && bash -c 'source ~/.nvm/nvm.sh && nvm use 22 && npm install'

help:
	@echo ""
	@echo "  make          start the editor (+ floorplan fetch-bridge if a token is set)"
	@echo "  make start    same as above"
	@echo "  make serve    run just the floorplan fetch-bridge (the cart's id picker)"
	@echo "  make install  install npm dependencies"
	@echo ""
