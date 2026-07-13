.PHONY: start serve install help

# default target
start:
	-git config core.hooksPath .githooks
	-pkill -f electron 2>/dev/null; pkill -f vite 2>/dev/null; pkill -f 'floorplanner.js --serve' 2>/dev/null; sleep 1
	@# floorplan cart id-picker fetch-bridge — only if a Floorplanner credential is set (else the
	@# picker just shows a "run --serve" hint). Backgrounded; logs to build/.fp-serve.log.
	@if [ -f data-tools/fmltools/.token ] || [ -n "$$FP_AUTH_TOKEN" ] || [ -n "$$FP_SESSION" ]; then \
		mkdir -p build; \
		echo "▸ floorplan fetch-bridge running (floorplanner.js --serve → build/.fp-serve.log)"; \
		( bash -c 'source ~/.nvm/nvm.sh && nvm use 22 >/dev/null 2>&1 && node data-tools/fmltools/floorplanner.js --serve' >build/.fp-serve.log 2>&1 & ) ; \
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
