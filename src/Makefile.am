INCLUDES = $(all_includes) -I$(top_srcdir)/include
AM_CFLAGS = ${CFLAGS} -fPIC ${SYMBOL_VISIBILITY}

bin_PROGRAMS         = rtl_433

rtl_433_SOURCES      = baseband.c \
                       bitbuffer.c \
                       data.c \
                       pulse_demod.c \
                       pulse_detect.c \
                       rtl_433.c \
                       util.c \
                       devices/fineoffset_wh1080.c
 

rtl_433_LDADD        = $(LIBRTLSDR) $(LIBM)
