#---------------------------------------------------------------------------------
# Yoi Themes - Makefile (PSL1GHT) - versao offline
#---------------------------------------------------------------------------------
include $(PSL1GHT)/ppu_rules

TARGET   := YoiThemes
BUILD    := build
SOURCES  := .
INCLUDES := .
LIBS     := -lsysutil -lrsx -lgcm_sys -lio -lm

APP_TITLE  := Yoi Themes
APP_ID     := YOIT00001
CONTENT_ID := UP0001-YOIT00001_00-YOITHEMES000001
PKG_DIR    := pkg
THEMES_SRC := themes

#---------------------------------------------------------------------------------
# Gera o .pkg: EBOOT.BIN + PARAM.SFO + pasta themes/ dentro de USRDIR
#---------------------------------------------------------------------------------
pkg: $(TARGET).self
	rm -rf $(PKG_DIR)
	mkdir -p $(PKG_DIR)/USRDIR/themes
	cp $(TARGET).self $(PKG_DIR)/USRDIR/EBOOT.BIN
	cp $(THEMES_SRC)/*.p3t $(PKG_DIR)/USRDIR/themes/ 2>/dev/null || true
	$(PS3DEV)/bin/sfo.py --title "$(APP_TITLE)" --appid $(APP_ID) $(PKG_DIR)/PARAM.SFO
	$(PS3DEV)/bin/pkg.py --contentid $(CONTENT_ID) $(PKG_DIR) $(TARGET).pkg
	@echo "Gerado: $(TARGET).pkg"

clean-all: clean
	rm -rf $(PKG_DIR) $(TARGET).pkg

.PHONY: pkg clean-all
