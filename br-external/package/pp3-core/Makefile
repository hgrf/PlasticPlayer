OBJS = bt.o icons.o librespot.o tagreader.o ui.o wifistatus.o

INC_DIRS := -I$(STAGING_DIR)/usr/include \
			-I$(STAGING_DIR)/usr/include/freetype2 \
			-I$(STAGING_DIR)/usr/include/glib-2.0 \
			-I$(STAGING_DIR)/usr/lib/glib-2.0/include

LIBS := -L$(TARGET_DIR)/usr/lib \
		-lfreetype \
		-lgio-2.0 \
		-lglib-2.0 \
		-lgobject-2.0 \
		-lgpiod \
		-lndef \
		-l:libntag21x.so.1.0.0 \
		-lmenu \
		-lncurses

$(OBJS): %.o: %.c
	$(CC) -c $^ $(INC_DIRS) -o $@
	
pp3-core: $(OBJS)
	$(CXX) $(LIBS) $^ main.cpp -o $@
