## =====================================================
# Copyright © hk. 2022-2022. All rights reserved.
# File name: Makefile
# Author   : fanhua
# Description: Makefile文件
## =====================================================
# 
	
TARGET1=serverFTP
TARGET2=clientFTP
CC = gcc

# 链接库
SO_LIB = 
A_LIB = 

DEBUG = -g -O2 -Wall
CFLAGS += $(DEBUG)

# 所有.c文件去掉后缀
TARGET_LIST = ${patsubst %.c, %, ${wildcard *.c}} 
OBJ_LIST = ${patsubst %.c, %.o, ${wildcard *.c}} 


all : $(TARGET1) $(TARGET2)
	
$(TARGET1): serverFTP.o
	$(CC) $(CFLAGS) $< -o $@ $(A_LIB)

$(TARGET2): clientFTP.o
	$(CC) $(CFLAGS) $< -o $@ $(A_LIB)

$(OBJ_LIST): %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean clean_o clean_out
clean : clean_o clean_out
	@rm -vf $(TARGET_LIST) $(TARGET1) $(TARGET2) 
	
clean_o :
	@rm -vf *.o
	
clean_out :
	@rm -vf *.out