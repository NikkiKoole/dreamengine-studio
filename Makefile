.PHONY: start install help

# default target
start:
	cd editor && bash -c 'source ~/.nvm/nvm.sh && nvm use 22 && npm start'

install:
	cd editor && bash -c 'source ~/.nvm/nvm.sh && nvm use 22 && npm install'

help:
	@echo ""
	@echo "  make          start the editor (Vite + Electron)"
	@echo "  make start    same as above"
	@echo "  make install  install npm dependencies"
	@echo ""
