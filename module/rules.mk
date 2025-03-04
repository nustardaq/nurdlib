# nurdlib, NUstar ReaDout LIBrary
#
# Copyright (C) 2015-2017, 2019-2021, 2023-2024
# Bastian Löher
# Michael Munch
# Hans Toshihide Törnqvist
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA  02110-1301  USA

NAME:=module
DIR_$(NAME):=module
C_$(NAME):=$(wildcard $(DIR_$(NAME))/*.c)
CGEN_$(NAME):=$(BUILD_DIR)/module/genlist.c

include gmake/c.mk
include gmake/cgen.mk
include gmake/close.mk

MODULE_OBJ_LIST:=$(foreach mod,$(MODULE_LIST),$(BUILD_DIR)/module/$(mod)/$(mod).o)

$(BUILD_DIR)/module/genlist.h: Makefile module/rules.mk
	$(QUIET)echo "LSTH  $@" &&\
	$(MKDIR) &&\
	echo "#ifndef MODULE_LIST_H" > $@.tmp;\
	echo "#define MODULE_LIST_H" >> $@.tmp;\
	echo "#include <module/module.h>" >> $@.tmp;\
	echo "#include <module/reggen/registerlist.h>" >> $@.tmp;\
	echo "extern struct ModuleListEntry const c_module_list_[];" >> $@.tmp;\
	echo "struct ModuleRegisterListEntryClient {" >> $@.tmp;\
	echo "	enum Keyword type;" >> $@.tmp;\
	echo "	struct RegisterEntryClient const *list;" >> $@.tmp;\
	echo "};" >> $@.tmp;\
	echo "struct ModuleRegisterListEntryServer {" >> $@.tmp;\
	echo "	enum Keyword type;" >> $@.tmp;\
	echo "	struct RegisterEntryServer const *list;" >> $@.tmp;\
	echo "};" >> $@.tmp;\
	echo "extern struct ModuleRegisterListEntryClient const c_module_register_list_client_[];" >> $@.tmp;\
	echo "extern struct ModuleRegisterListEntryServer const c_module_register_list_server_[];" >> $@.tmp;\
	echo "#endif" >> $@.tmp;\
	mv -f $@.tmp $@

$(BUILD_DIR)/module/genlist.c: $(BUILD_DIR)/module/genlist.h
	$(QUIET)echo "LSTC  $@" &&\
	$(MKDIR) &&\
	echo "#include <module/genlist.h>" > $@.tmp;\
	for i in $(MODULE_LIST); do \
		if [ -f module/$$i/$$i.h ]; then \
			echo "#include <module/$$i/$$i.h>" >> $@.tmp;\
		fi;\
	done;\
	echo "#include <nurdlib/keyword.h>" >> $@.tmp;\
	echo "struct ModuleListEntry const c_module_list_[] = {" >> $@.tmp;\
	for i in $(MODULE_LIST); do \
		if [ -f module/$$i/$$i.h ]; then \
			up=`echo $$i | tr '[:lower:]' '[:upper:]'`;\
			down=`echo $$i | tr '[:upper:]' '[:lower:]'`;\
			echo "	{KW_$$up, $${down}_setup_, $${down}_create_, \"$$down.cfg\"}," >> $@.tmp;\
		fi;\
	done;\
	echo "	{KW_NONE, NULL, NULL, NULL}" >> $@.tmp;\
	echo "};" >> $@.tmp;\
	for i in $(MODULE_LIST); do \
		if [ -f module/$$i/register_list.txt ]; then \
			down=`echo $$i | tr '[:upper:]' '[:lower:]'`;\
			echo "extern struct RegisterEntryClient const c_$${down}_register_list_client_[];" >> $@.tmp;\
			echo "extern struct RegisterEntryServer const c_$${down}_register_list_server_[];" >> $@.tmp;\
		fi;\
	done;\
	echo "struct ModuleRegisterListEntryClient const c_module_register_list_client_[] = {" >> $@.tmp;\
	for i in $(MODULE_LIST); do \
		if [ -f module/$$i/$$i.h -a -f module/$$i/register_list.txt ]; then \
			up=`echo $$i | tr '[:lower:]' '[:upper:]'`;\
			down=`echo $$i | tr '[:upper:]' '[:lower:]'`;\
			echo "	{KW_$$up, c_$${down}_register_list_client_}," >> $@.tmp;\
		fi;\
	done;\
	echo "	{KW_NONE, NULL}" >> $@.tmp;\
	echo "};" >> $@.tmp;\
	echo "struct ModuleRegisterListEntryServer const c_module_register_list_server_[] = {" >> $@.tmp;\
	for i in $(MODULE_LIST); do \
		if [ -f module/$$i/$$i.h -a -f module/$$i/register_list.txt ]; then \
			up=`echo $$i | tr '[:lower:]' '[:upper:]'`;\
			down=`echo $$i | tr '[:upper:]' '[:lower:]'`;\
			echo "	{KW_$$up, c_$${down}_register_list_server_}," >> $@.tmp;\
		fi;\
	done;\
	echo "	{KW_NONE, NULL}" >> $@.tmp;\
	echo "};" >> $@.tmp;\
	mv -f $@.tmp $@

$(BUILD_DIR)/module/genlist.o: $(BUILD_DIR)/module/genlist.h

MODULE_HEADER_LIST:=$(wildcard $(foreach mod,$(MODULE_LIST),module/$(mod)/$(mod).h))
MODULE_REGLIST_LIST:=$(wildcard $(addsuffix register_list.txt,$(dir $(MODULE_HEADER_LIST))))
MODULE_DUMPABLE_C:=$(patsubst %,$(BUILD_DIR)/%registerlist.c,$(dir $(MODULE_REGLIST_LIST)))
MODULE_DUMPABLE_O:=$(MODULE_DUMPABLE_C:.c=.o)

$(BUILD_DIR)/module/module.d: $(BUILD_DIR)/module/genlist.h $(MODULE_DUMPABLE_O)

include module/map/rules.mk
include module/reggen/rules.mk
include module/reggen2/rules.mk
