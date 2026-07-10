.PHONY: start install help

# default target
start:
	-git config core.hooksPath .githooks
	-pkill -f electron 2>/dev/null; pkill -f vite 2>/dev/null; sleep 1
	cd editor && bash -c 'source ~/.nvm/nvm.sh && nvm use 22 && npm start'

install:
	-git config core.hooksPath .githooks
	cd editor && bash -c 'source ~/.nvm/nvm.sh && nvm use 22 && npm install'

help:
	@echo ""
	@echo "  make          start the editor (Vite + Electron)"
	@echo "  make start    same as above"
	@echo "  make install  install npm dependencies"
	@echo ""
