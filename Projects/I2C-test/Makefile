# ApolloCrossDev Makefile Template v1.0
# 28-11-2025
# Willem Drijver

# Define Shell Environment
PWD = $(shell pwd)
DIR = $(notdir $(shell pwd))
APOLLO_V4_HOST = $(AMIGAHOST):Ram Disk/

# Define ApolloCrossDev Compiler
PREFIX 	= $(PWD)/../../Compilers/GCC-6.50-Latest
TARGET 	= m68k-amigaos
ACP 	= $(PWD)/../../Projects/ApolloExplorer/acp/acp
GDB		= $(PWD)/../../Projects/bgdbserver/bgdbserver
ICONS	= $(PWD)/../../Projects/_icons

#Define Project Name (replace <project-name>)
PROJECT_NAME	= i2cBTdevice
PROJECT_DIR		= $(PWD)

# Define GNU C-Compiler Type (C = Default, remove/add # to enable/disable
C_COMPILER	= $(PREFIX)/bin/m68k-amigaos-gcc
# C_COMPILER	= $(PREFIX)/bin/m68k-amigaos-g++

# Define Standard C-Libraries and Headers (libnix = Default, remove/add # to enable/disable
C_LIB		= -noixemul
# C_LIB 	= -mcrt=clib2
C_INCL_CLB	= $(PREFIX)/$(TARGET)/libnix/include
# C_INCL_CLB  = $(PREFIX)/$(TARGET)/clib2/include

# Define ApolloLib
APOLLOLIB = $(PWD)/../../Projects/ApolloLib

# Define Additional Libraries - SDL, MUI and Mathemetics, remove/add # to enable/disable
# C_LIBS_SDL	= -lSDL -lSDL_ttf -lfreetype
# C_LIBS_MUI	= -lmui
# C_LIBS_MATH	= -lm
C_LIBS_ALL 	= $(C_LIBS_SDL) $(C_LIBS_MUI) $(C_LIBS_MATH)

# Define SDK/NDK Headers, remove/add # to enable/disable
C_INCL_SYS	= $(PREFIX)/$(TARGET)/sys-include
# C_INCL_NDK  = $(PREFIX)/$(TARGET)/ndk13-include
# C_INCL_NDK  = $(PREFIX)/$(TARGET)/ndk39-include
C_INCL_NDK  = $(PREFIX)/$(TARGET)/ndk32-include/Include_H
# C_INCL_GL	= $(PREFIX)/$(TARGET)/include/GL
# C_INCL_SDL	= $(PREFIX)/$(TARGET)/include/SDL
# C_INCL_MUI	= $(PREFIX)/$(TARGET)/include/MUI

# Define I2C Library includes
I2C_LIB_DIR = $(PWD)/i2clib-master
C_INCL_I2C = $(I2C_LIB_DIR)/include

C_INCL_ALL 	= -I. -I$(C_INCL_CLB) -I$(C_INCL) -I$(C_INCL_SYS) -I$(C_INCL_NDK) -I$(C_INCL_I2C)

# Define C Options
C_OPTIONS 	= -O2 -m68080 -m68881 -ffast-math -fomit-frame-pointer -DREGPARM=4
# -O = basic optimization | -O2 = advanced optimization (default and advised) | -O3 = agressive optimization | -Os = small size | =Ofast = speed optimization (same as -O2)
# -m68080 = target for Apollo V4 (default and advised) | -m68881 = enable FPU (default and advised) | -ffast-math = faster but less accurate math = -fomit-frame-pointer = free up register
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

# Define Source and Object Directory
C_SOURCEDIR = $(PROJECT_DIR)
S_SOURCEDIR = $(PROJECT_DIR)
C_OBJECTDIR = $(PROJECT_DIR)
S_OBJECTDIR = $(PROJECT_DIR)

# Define Object Filelist
C_FILES = $(wildcard $(C_SOURCEDIR)/*.c)
S_FILES = $(wildcard $(S_SOURCEDIR)/*.s)
O_FILES = $(patsubst $(C_SOURCEDIR)/%.c,$(C_OBJECTDIR)/%.o,$(C_FILES)) $(patsubst $(S_SOURCEDIR)/*.s,$(S_OBJECTDIR)/%.o,$(S_FILES))

EXE		= $(PROJECT_DIR)/$(PROJECT_NAME)

# Make Project (Init -> Build -> Clean -> Exit)
all: init cleanbefore build upload_all cleanafter exit

debug: init cleanbefore build upload_debug cleanafter exit

clean:
	@rm -f $(C_OBJECTDIR)/*.o $(S_OBJECTDIR)/*.o $(PROJECT_DIR)/$(PROJECT_NAME)*.txt $(EXE)

init:
	clear
	@echo "\e[37m------------------------------------------------------------------\e[0m"
	@echo "\e[1;31mApollo\e[1;30mCrossDev \e[37mGCC-6.50 Compiler v1.0\e[0m"
	@echo "\e[36mC-Sources : \e[37m$(C_FILES)\e[0m"
	@echo "\e[36mS-Sources : \e[37m$(S_FILES)\e[0m"
	@echo "\e[36mExecutable: \e[37m$(EXE)\e[0m"
	@echo "\e[37m------------------------------------------------------------------\e[0m"

cleanbefore:
	@rm -f $(C_OBJECTDIR)/*.o $(S_OBJECTDIR)/*.o $(PROJECT_DIR)/$(PROJECT_NAME)*.txt $(EXE)

build: $(EXE)

$(EXE): $(O_FILES)
	@echo "\e[32mLinking...\e[0m"
	$(O_LINKER) $(C_FLAGS) -o $(EXE) $(O_FILES) -L$(I2C_LIB_DIR)/lib -li2c

$(C_OBJECTDIR)/%.o: $(C_SOURCEDIR)/%.c
	@echo "\e[33mCompiling $<...\e[0m"
	$(C_COMPILER) $(C_FLAGS) -c $< -o $@

$(S_OBJECTDIR)/%.o: $(S_SOURCEDIR)/%.s
	@echo "\e[33mAssembling $<...\e[0m"
	$(S_COMPILER) $(S_FLAGS) $< -o $@

upload_all: $(EXE)
	@echo "\e[35mUploading $(PROJECT_NAME) to Apollo V4...\e[0m"
	$(ACP) "$(EXE)" "$(APOLLO_V4_HOST)"
	$(ACP) "$(ICONS)/ApolloIcon.info" "$(APOLLO_V4_HOST)"

upload_debug: $(EXE)
	@echo "\e[35mUploading $(PROJECT_NAME) to Apollo V4 (Debug)...\e[0m"
	$(ACP) "$(EXE)" "$(APOLLO_V4_HOST)"
	$(ACP) "$(ICONS)/ApolloIcon_Debug.info" "$(APOLLO_V4_HOST)"

cleanafter:
	@echo "\e[31mCleaning up...\e[0m"
	@rm -f $(C_OBJECTDIR)/*.o $(S_OBJECTDIR)/*.o

exit:
	@echo "\e[37m------------------------------------------------------------------\e[0m"
	@echo "\e[1;32mBuild completed successfully!\e[0m"
	@echo "\e[37m------------------------------------------------------------------\e[0m"