# ApolloCrossDev Makefile Template v1.0
# 28-11-2025
# Willem Drijver

# Define Shell Environment
SHELL = /bin/bash
.SHELLFLAGS = -o pipefail -c
PWD = $(shell pwd)
DIR = $(notdir $(shell pwd))
APOLLO_V4_HOST = "${AMIGAHOST}:Ram Disk/"


# Define ApolloCrossDev Compiler
PREFIX 	= $(PWD)/../../Compilers/GCC-6.50-Latest
TARGET 	= m68k-amigaos
ACP 	= $(PWD)/../../Projects/ApolloExplorer/acp/acp
GDB		= $(PWD)/../../Projects/bgdbserver/bgdbserver
ICONS	= $(PWD)/../../Projects/_icons

#Define Project Name (replace <project-name>)
PROJECT_NAME	= Load_Pic_show_datatype
PROJECT_DIR		= $(PWD)

# Define GNU C-Compiler Type (C = Default, switch comment to change to C++)
C_COMPILER	= $(PREFIX)/bin/m68k-amigaos-gcc
# C_COMPILER	= $(PREFIX)/bin/m68k-amigaos-g++

# Define Standard C-Libraries and Headers (libnix = Default, switch comments to change to clib2)
C_LIB		= -noixemul
# C_LIB 	= -mcrt=clib2
C_INCL_CLB	= $(PREFIX)/$(TARGET)/libnix/include
# C_INCL_CLB  = $(PREFIX)/$(TARGET)/clib2/include

# Define Additional Libraried - SDL, MUI and Mathemetics, switch comments to enable/disable 
# C_LIBS_SDL	= -lSDL -lSDL_ttf -lfreetype
# C_LIBS_MUI	= -lmui
C_LIBS_MATH	= -lm
# C_LIBS_CYBER	= -lcybergraphics (not available in this SDK)
C_LIBS_ALL 	= $(C_LIBS_SDL) $(C_LIBS_MUI) $(C_LIBS_MATH) 

# Define SDK/NDK Headers, switch comments to enable/disable
C_INCL_SYS	= $(PREFIX)/$(TARGET)/sys-include
# C_INCL_NDK  = $(PREFIX)/$(TARGET)/ndk13-include
# C_INCL_NDK  = $(PREFIX)/$(TARGET)/ndk39-include
C_INCL_NDK  = $(PREFIX)/$(TARGET)/ndk32-include/Include_H
# C_INCL_GL	= $(PREFIX)/$(TARGET)/include/GL
# C_INCL_SDL	= $(PREFIX)/$(TARGET)/include/SDL
# C_INCL_MUI	= $(PREFIX)/$(TARGET)/include/MUI
C_INC_I2C	= $(PREFIX)/$(TARGET)/i2c-include
THIRD_PARTY_CJSON_DIR = $(PROJECT_DIR)/ThirdParty/cJSON
C_INCL_ALL 	= -I. -I$(C_INCL_CLB) -I$(C_INCL) -I$(C_INCL_SYS) -I$(C_INCL_NDK) -I$(C_INC_I2C) -I$(THIRD_PARTY_CJSON_DIR)

# Define C Options
C_OPTIONS 	= -O2 -m68080 -m68881 -ffast-math -fomit-frame-pointer -DREGPARM=4
C_DEFINES 	= 
C_DEBUG		= -g
C_FLAGS = $(C_LIB) $(C_OPTIONS) $(C_DEBUG) $(C_DEFINES) $(C_INCL_ALL) $(C_LIBS_ALL)

# Define ASM Compiler
S_COMPILER = $(PREFIX)/bin/vasmm68k_mot

# Define ASM Includes
S_INCLUDE 	= $(PREFIX)/$(TARGET)/DevPac/Include
S_LIBS		= $(PREFIX)/$(TARGET)/DevPac/Libs	

# Define ASM Options
S_DEBUG		= -linedebug -kick1hunks
S_FLAGS 	= -Fhunk -m68080 -m68881 -opt-speed $(S_DEBUG) -I$(S_INCLUDE)

# Define Object Linker
O_LINKER = $(PREFIX)/bin/m68k-amigaos-gcc

# Define Object Filelist
# NOTE: Compile only Load_Pic_show_datatype.c (not test_*.c files)
C_FILES = $(PROJECT_DIR)/Load_Pic_show_datatype.c $(wildcard $(THIRD_PARTY_CJSON_DIR)/*.c)
S_FILES = $(wildcard $(PROJECT_DIR)/*.s) $(wildcard $(PROJECT_DIR)/asm/*.s)
O_FILES = $(patsubst %.c,%.o,$(C_FILES)) $(patsubst %.s,%.o,$(S_FILES))

EXE		= $(PROJECT_DIR)/$(PROJECT_NAME)
DATA_DIR	= $(PROJECT_DIR)/Data

# Make Project (Init -> Build -> Clean -> Exit)
.PHONY: all init cleanbefore build upload upload-only fetch-log cleanafter exit

all: init cleanbefore build upload cleanafter exit

upload-only: upload

fetch-log:
	@set -e; \
	if [ -z "$(AMIGAHOST)" ]; then \
		echo "Fehler: AMIGAHOST ist nicht gesetzt (z.B. export AMIGAHOST=192.168.178.50)"; \
		exit 1; \
	fi; \
	rm -f "$(PROJECT_DIR)/Load_Pic_show_datatype.log"; \
	echo "Hole Logdatei von $(AMIGAHOST):Ram Disk/Load_Pic_show_datatype.log"; \
	$(ACP) "$(AMIGAHOST):Ram Disk/Load_Pic_show_datatype.log" "$(PROJECT_DIR)/"; \
	if [ ! -f "$(PROJECT_DIR)/Load_Pic_show_datatype.log" ]; then \
		echo "Fehler: Keine Logdatei heruntergeladen."; \
		exit 2; \
	fi; \
	echo "Log gespeichert unter: $(PROJECT_DIR)/Load_Pic_show_datatype.log"

init:
	clear
	@echo "\e[37m------------------------------------------------------------------\e[0m"
	@echo "PWD: $(PWD) PROJECT_DIR: $(PROJECT_DIR), PROJECT_NAME: $(PROJECT_NAME) C-INCL: $(C_INC_I2C)"
	@echo "\e[1;31mApollo\e[1;30mCrossDev \e[37mGCC-6.50 Compiler v1.0\e[0m"    
	@echo "\e[36mC-Sources : \e[37m$(C_FILES)\e[0m"
	@echo "\e[36mS-Sources : \e[37m$(S_FILES)\e[0m"
	@echo "\e[36mExecutable: \e[37m$(EXE)\e[0m"

cleanbefore:
	@rm -f $(O_FILES) $(PROJECT_DIR)/$(PROJECT_NAME)*.txt $(EXE)

build: $(EXE)
	@echo "\e[37mStep #2 - \e[31mLinker \e[37mFinished\e[0m"

# Link all Objects to one Executable
$(EXE) : $(O_FILES) 
	$(O_LINKER) $(O_FILES) $(C_FLAGS) -o $(EXE) 2>&1 | tee -a $(PROJECT_DIR)/$(PROJECT_NAME)-log.txt

# Compile all C-Sources to Objects
%.o : %.c
	$(C_COMPILER) -c $(C_FLAGS) -o $@ $< 2>&1 | tee -a $(PROJECT_DIR)/$(PROJECT_NAME)-log.txt

# Compile all ASM-Sources to Objects
%.o : %.s
	@$(S_COMPILER) $(S_FLAGS) -o $@ $< >>/dev/null 2>>$(PROJECT_DIR)/$(PROJECT_NAME)-log.txt

upload:
	@echo "\e[37mStep #3 - \e[31mApollo\e[30mExplorer \e[37mUpload\e[30m"
	@set -e; \
	if [ -z "$(AMIGAHOST)" ]; then \
		echo "Fehler: AMIGAHOST ist nicht gesetzt (z.B. export AMIGAHOST=192.168.178.50)"; \
		exit 1; \
	fi; \
	retry_upload() { \
		local tries=0; \
		local max=4; \
		while true; do \
			tries=$$((tries + 1)); \
			if $(ACP) "$$@" $(APOLLO_V4_HOST); then \
				return 0; \
			fi; \
			if [ $$tries -ge $$max ]; then \
				echo "Upload fehlgeschlagen nach $$tries Versuchen: $$*"; \
				return 1; \
			fi; \
			echo "Retry $$tries/$$max fuer $$* ..."; \
		done; \
	}; \
	retry_upload "$(EXE)"; \
	retry_upload "$(GDB)"; \
	if [ -d "$(DATA_DIR)" ]; then \
		retry_upload -r "$(DATA_DIR)"; \
	fi; \
	cp $(ICONS)/ApolloIcon.info $(PROJECT_DIR)/$(PROJECT_NAME).info; \
	cp $(ICONS)/ApolloIcon_Debug.info $(PROJECT_DIR)/$(PROJECT_NAME)_Debug.info; \
	retry_upload "$(PROJECT_DIR)/$(PROJECT_NAME).info"; \
	retry_upload "$(PROJECT_DIR)/$(PROJECT_NAME)_Debug.info"; \
	echo "bgdbserver $(PROJECT_NAME)" > $(EXE)_Debug; \
	retry_upload "$(EXE)_Debug"

cleanafter:
	@rm -f $(O_FILES) $(PROJECT_DIR)/*.info $(EXE)_Debug

exit:
	@echo "\e[37m------------------------------------------------------------------\e[0m"
	@echo "\e[1;31mApollo\e[1;30mCrossDev \e[32mCompile Successful\e[0m"
	@echo "\e[37m------------------------------------------------------------------\e[0m"
	@echo " "