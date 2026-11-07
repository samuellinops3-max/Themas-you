#---------------------------------------------------------------------------------
# Controle Tester - Makefile (PSL1GHT)
#---------------------------------------------------------------------------------
include $(PSL1GHT)/ppu_rules

TARGET   := ControleTester
BUILD    := build
SOURCES  := .
INCLUDES := .
LIBS     := -lsysutil -lrsx -lgcm_sys -lio -lm

APP_TITLE  := Controle Tester
APP_ID     := CTRLTEST1
CONTENT_ID := UP0001-CTRLTEST1_00-CONTROLETESTER01
PKG_DIR    := pkg

#---------------------------------------------------------------------------------
# Gera o .pkg: EBOOT.BIN + PARAM.SFO (+ ICON0.PNG, se existir em assets/)
#---------------------------------------------------------------------------------
pkg: $(TARGET).self
	rm -rf $(PKG_DIR)
	mkdir -p $(PKG_DIR)/USRDIR
	cp $(TARGET).self $(PKG_DIR)/USRDIR/EBOOT.BIN
	$(PS3DEV)/bin/sfo.py --title "$(APP_TITLE)" --appid $(APP_ID) $(PKG_DIR)/PARAM.SFO
	if [ -f assets/ICON0.PNG ]; then cp assets/ICON0.PNG $(PKG_DIR)/ICON0.PNG; fi
	$(PS3DEV)/bin/pkg.py --contentid $(CONTENT_ID) $(PKG_DIR) $(TARGET).pkg
	@echo "Gerado: $(TARGET).pkg"

clean-all: clean
	rm -rf $(PKG_DIR) $(TARGET).pkg

.PHONY: pkg clean-all
