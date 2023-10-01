# GNU Makefile for poc, mp3cue, mp3cut
#
# Please compile with GNU make.
#
# 2005 bl0rg.net

CFLAGS += -Wall -O2

# Uncomment these flags to add id3 support to mp3cue and mp3cut
#CFLAGS += -DWITH_ID3TAG
#LDFLAGS += -lid3tag
# On MacOSX using fink
#CFLAGS += -I/sw/include
#LDFLAGS += -L/sw/lib

# Uncomment this flag to add ipv6 support to poc
#CFLAGS+=-DWITH_IPV6

# Uncomment these flags to add SSL support to poc
#CFLAGS+=-DWITH_OPENSSL 
#LDFLAGS+=-lssl -lcrypto

# Uncomment these flags to debug
#CFLAGS += -g
#CFLAGS+=-DDEBUG
#CFLAGS+=-DMALLOC -ldmalloc

TEXIFY := ./texify.pl
FLEX=flex
FLEX_LIBS=

# Use these definitions when using bison
YACC=bison -b y

# Use these definitions when using yacc
#YACC=yacc
#LIBS+=-ly

all:  servers clients mp3cue mp3cut mp3length

# Create dependencies
%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@
	$(CC) -MM $(CFLAGS) $< | sed s/\\.o/.d/ >> $@

MP3_OBJS     := mp3-read.o mp3-write.o mp3.o aq.o id3.o
NETWORK_OBJS := network.o network4.o network6.o
RTP_OBJS     := rtp.o rtp-rb.o
UTILS_OBJS   := pack.o bv.o sig_set_handler.o dlist.o file.o buf.o crc32.o misc.o
FEC_OBJS     := galois.o matrix.o fec.o fec-pkt.o fec-rb.o fec-group.o
OGG_OBJS     := ogg.o vorbis.o ogg-read.o ogg-write.o vorbis-read.o

OBJS := $(MP3_OBJS) \
        $(OGG_OBJS) \
        $(NETWORK_OBJS) \
        $(RTP_OBJS) \
        $(UTILS_OBJS) \
        $(FEC_OBJS)
DEPS := $(patsubst %.o,%.d,$(OBJS))
include $(DEPS)

# mp3cue
MP3CUE_OBJS := $(MP3_OBJS) $(UTILS_OBJS) \
               mp3cue_lex.o mp3cue_yacc.o mp3cue-main.o
include mp3cue-main.d

mp3cue_lex.c: tools/mp3cue/mp3cue_lex.l
	$(FLEX) -o$@ $<
mp3cue_lex.o: mp3cue_lex.c mp3cue_yacc.c
	$(CC) -c -o $@ mp3cue_lex.c
mp3cue_yacc.c: tools/mp3cue/mp3cue_yacc.y
	$(YACC) -d -o $@ $<
mp3cue: $(MP3CUE_OBJS)
	$(CC) $(CFLAGS) -o mp3cue $(MP3CUE_OBJS) \
              $(LDFLAGS) $(LIBS) $(FLEX_LIBS)
mp3cue-clean:
	- rm -rf $(MP3CUE_OBJS) \
                 mp3cue_lex.c mp3cue_yacc.c mp3cue_yacc.h \
		 mp3cue mp3cue.exe

# mp3cut
MP3CUT_OBJS := $(MP3_OBJS) $(UTILS_OBJS) mp3cut.o
include mp3cut.d

mp3cut: $(MP3CUT_OBJS)
	$(CC) $(CFLAGS) -o mp3cut $(MP3CUT_OBJS) $(LDFLAGS) $(LIBS)
mp3cut-clean:
	- rm -rf $(MP3CUT_OBJS) mp3cut mp3cut.exe

MP3LENGTH_OBJS := $(MP3_OBJS) $(UTILS_OBJS) mp3length.o
include mp3length.d

mp3length: $(MP3LENGTH_OBJS)
	$(CC) $(CFLAGS) -o mp3length $(MP3LENGTH_OBJS) $(LDFLAGS) $(LIBS)
mp3length-clean:
	- rm -rf $(MP3LENGTH_OBJS) mp3length mp3length.exe

# Servers
SERVERS := poc-2250 \
           poc-3119 \
           poc-2250-ploss \
           poc-3119-ploss \
           poc-fec \
	   poc-fec-ploss \
           poc-http \
           pogg-http
SERVERS_EXE := $(patsubst %,%.exe,$(SERVERS))
SERVERS_OBJS := 

servers: $(SERVERS)

MP3RTP_OBJS := $(NETWORK_OBJS) $(MP3_OBJS) $(RTP_OBJS) $(UTILS_OBJS)

# RFC 2250 protocol
POC_2250_OBJS := $(MP3RTP_OBJS) poc-2250.o
include poc-2250.d
poc-2250: $(POC_2250_OBJS)
	$(CC) $(CFLAGS) -o poc-2250 $(POC_2250_OBJS) $(LDFLAGS) $(LIBS)
POC_2250_PLOSS_OBJS := $(MP3RTP_OBJS) poc-2250-ploss.o
poc-2250-ploss.o: servers/poc-2250.c
	$(CC) $(CFLAGS) -DDEBUG_PLOSS -c -o $@ $<
poc-2250-ploss: $(POC_2250_PLOSS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POC_2250_PLOSS_OBJS) $(LDFLAGS) $(LIBS)
SERVERS_OBJS += $(POC_2250_OBJS) $(POC_2250_PLOSS_OBJS)

# RFC 3119 protocol
POC_3119_OBJS := $(MP3RTP_OBJS) poc-3119.o
include poc-3119.d
poc-3119: $(POC_3119_OBJS)
	$(CC) $(CFLAGS) -o poc-3119 $(POC_3119_OBJS) $(LDFLAGS) $(LIBS)
POC_3119_PLOSS_OBJS := $(MP3RTP_OBJS) poc-3119-ploss.o
poc-3119-ploss.o: servers/poc-3119.c
	$(CC) $(CFLAGS) -DDEBUG_PLOSS -c -o $@ $<
poc-3119-ploss: $(POC_3119_PLOSS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POC_3119_PLOSS_OBJS) $(LDFLAGS) $(LIBS)
SERVERS_OBJS += $(POC_3119_OBJS) $(POC_3119_PLOSS_OBJS)

# FEC protocol
MP3FEC_OBJS := $(MP3_OBJS) $(NETWORK_OBJS) $(UTILS_OBJS) $(FEC_OBJS)
POC_FEC_OBJS := $(MP3FEC_OBJS) poc-fec.o
include poc-fec.d
poc-fec: $(POC_FEC_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POC_FEC_OBJS) $(LDFLAGS) $(LIBS)
POC_FEC_PLOSS_OBJS := $(MP3FEC_OBJS) poc-fec-ploss.o
poc-fec-ploss.o: servers/poc-fec.c
	$(CC) $(CFLAGS) -DDEBUG_PLOSS -c -o $@ $<
poc-fec-ploss: $(POC_FEC_PLOSS_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POC_FEC_PLOSS_OBJS) $(LDFLAGS) $(LIBS)
SERVERS_OBJS += $(POC_FEC_OBJS) $(POC_FEC_PLOSS_OBJS)

# mp3 and ogg HTTP server
POC_HTTP_OBJS := $(MP3_OBJS) $(NETWORK_OBJS) $(UTILS_OBJS) http.o poc-http.o
include poc-http.d
poc-http: $(POC_HTTP_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POC_HTTP_OBJS) $(LDFLAGS) $(LIBS)
SERVERS_OBJS += $(POC_HTTP_OBJS)

POGG_HTTP_OBJS := $(OGG_OBJS) $(NETWORK_OBJS) $(UTILS_OBJS) http.o pogg-http.o
include pogg-http.d
pogg-http: $(POGG_HTTP_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POGG_HTTP_OBJS) $(LDFLAGS) $(LIBS)
SERVERS_OBJS += $(POGG_HTTP_OBJS)

servers-clean:
	- rm -f $(SERVERS) $(SERVERS_EXE) $(SERVERS_OBJS)

# Clients
CLIENTS := pob-fec \
           pob-3119 \
           pob-2250
CLIENTS_EXE := $(patsubst %,%.exe,$(CLIENTS))

clients: $(CLIENTS)

RTP_CLIENT_OBJS := $(RTP_OBJS) $(NETWORK_OBJS) $(UTILS_OBJS) $(MP3_OBJS)

# RFC 2250 client
POB_2250_RB_OBJS := $(RTP_CLIENT_OBJS) pob-2250-rb.o
include pob-2250-rb.d
pob-2250: $(POB_2250_RB_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POB_2250_RB_OBJS) $(LDFLAGS) $(LIBS)
CLIENTS_OBJS += $(POB_2250_OBJS) $(POB_2250_RB_OBJS)

# RFC 3119 client
POB_3119_RB_OBJS := $(RTP_CLIENT_OBJS) pob-3119-rb.o
include pob-3119-rb.d
pob-3119: $(POB_3119_RB_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POB_3119_RB_OBJS) $(LDFLAGS) $(LIBS)
CLIENTS_OBJS += $(POB_3119_OBJS) $(POB_3119_RB_OBJS)

# FEC client
POB_FEC_OBJS := $(NETWORK_OBJS) $(FEC_OBJS) $(UTILS_OBJS) $(MP3_OBJS) pob-fec.o
include pob-fec.d
pob-fec: $(POB_FEC_OBJS)
	$(CC) $(CFLAGS) -o $@ $(POB_FEC_OBJS) $(LDFLAGS) $(LIBS)
CLIENTS_OBJS += $(POB_FEC_OBJS)

clients-clean:
	- rm -f $(CLIENTS) $(CLIENTS_EXE) $(CLIENTS_OBJS)

# Tools
MP3TOADU_OBJS := $(UTILS_OBJS) $(MP3_OBJS) mp3toadu.o
include mp3toadu.d
mp3toadu: $(MP3TOADU_OBJS)
	$(CC) $(CFLAGS) -o $@ $(MP3TOADU_OBJS) $(LDFLAGS) $(LIBS)

ADUTOMP3_OBJS := $(UTILS_OBJS) $(MP3_OBJS) adutomp3.o
include adutomp3.d
adutomp3: $(ADUTOMP3_OBJS)
	$(CC) $(CFLAGS) -o $@ $(ADUTOMP3_OBJS) $(LDFLAGS) $(LIBS)

FIFO_READ_OBJS := misc.o fifo-read.o
include fifo-read.d
fifo-read: $(FIFO_READ_OBJS)
	$(CC) $(CFLAGS) -o $@ $(FIFO_READ_OBJS) $(LDFLAGS) $(LIBS)

FIFO_WRITE_OBJS := misc.o fifo-write.o
include fifo-write.d
fifo-write: $(FIFO_WRITE_OBJS)
	$(CC) $(CFLAGS) -o $@ $(FIFO_WRITE_OBJS) $(LDFLAGS) $(LIBS)

# Tests
bvtest: bv.c bv.h
	$(CC) $(CFLAGS) -o $@ -DBV_TEST bv.c $(LDFLAGS)
crc32test: crc32.h crc32.c
	$(CC) $(CFLAGS) -o $@ -DCRC32_TEST crc32.c $(LDFLAGS)
packtest: lib/pack.c lib/pack.h
	$(CC) $(CFLAGS) -o $@ -DPACK_TEST pack.c $(LDFLAGS)
dlisttest: lib/data-structures/dlist.c lib/data-structures/dlist.h
	$(CC) $(CFLAGS) -o $@ -DDLIST_TEST dlist.c $(LDFLAGS)

galoistest: lib/math/galois.c lib/math/galois.h
	$(CC) $(CFLAGS) -o $@ -DGALOIS_TEST galois.c $(LDFLAGS)
matrixtest: lib/math/matrix.c lib/math/matrix.h galois.o
	$(CC) $(CFLAGS) -o $@ -DMATRIX_TEST matrix.c galois.o $(LDFLAGS)
fectest: lib/fec/fec.c lib/fec/fec.h galois.o matrix.o
	$(CC) $(CFLAGS) -o $@ -DFEC_TEST fec.c matrix.o galois.o $(LDFLAGS)

rtptest: lib/rtp/rtp.c lib/rtp/rtp.h pack.o lib/pack.h
	$(CC) $(CFLAGS) -o $@ -DRTP_TEST rtp.c pack.o $(LDFLAGS)

ogg-readtest: lib/ogg/ogg-read.c ogg.o crc32.o file.o buf.o pack.o
	$(CC) $(CFLAGS) -o $@ -DDEBUG -DOGG_TEST ogg-read.c ogg.o crc32.o \
				file.o buf.o pack.o
ogg-writetest: lib/ogg/ogg-write.c lib/ogg/ogg-read.c ogg.o crc32.o file.o buf.o pack.o
	$(CC) $(CFLAGS) -o $@ -DDEBUG -DOGG_WRITETEST ogg-write.c ogg-read.c\
				ogg.o crc32.o \
				file.o buf.o pack.o
vorbis-readtest: lib/ogg/vorbis-read.c vorbis.o ogg.o crc32.o file.o buf.o pack.o \
				ogg-read.o bv.o
	$(CC) $(CFLAGS) -o $@ -DDEBUG -DVORBIS_TEST vorbis-read.c vorbis.o \
				ogg.o ogg-read.o crc32.o file.o buf.o pack.o \
				bv.o
mp3-readtest: lib/mp3/mp3-read.c lib/mp3/mp3.h bv.o bv.h mp3.o mp3-sf.o
	$(CC) $(CFLAGS) -o $@ -DMP3_TEST mp3-read.c bv.o mp3.o  mp3-sf.o \
		$(LDFLAGS)
mp3-writetest: lib/mp3/mp3-write.c mp3-read.o lib/mp3/mp3.h bv.o bv.h mp3.o mp3-sf.o
	$(CC) $(CFLAGS) -o $@ -DMP3_TEST mp3-write.c bv.o mp3-read.o mp3.o \
					 mp3-sf.o $(LDFLAGS)
mp3-sftest: lib/mp3/mp3-sf.c mp3-write.o mp3-read.o lib/mp3/mp3.h bv.o bv.h mp3.o aq.o dlist.o
	$(CC) $(CFLAGS) -o $@ -DMP3SF_TEST mp3-sf.c mp3-write.o bv.o \
				mp3-read.o mp3.o aq.o dlist.o $(LDFLAGS)
mp3-transtest: lib/mp3/mp3-trans.c mp3-read.o mp3-write.o lib/mp3/mp3.h bv.o bv.h \
	       mp3.o mp3-sf.o 
	$(CC) $(CFLAGS) -o $@ -DMP3_TEST mp3-trans.c bv.o mp3-read.o \
			mp3.o mp3-write.o  mp3-sf.o $(LDFLAGS)
aq1test: lib/mp3/aq.c lib/mp3/aq.h dlist.o lib/data-structures/dlist.h mp3-read.o lib/mp3/mp3.h bv.h bv.o mp3.o mp3-sf.o
	$(CC) $(CFLAGS) -o $@ -DAQ1_TEST aq.c mp3-read.o bv.o mp3.o dlist.o \
		mp3-sf.o $(LDFLAGS)
aq2test: lib/mp3/aq.c lib/mp3/aq.h dlist.o lib/data-structures/dlist.h mp3-read.o lib/mp3/mp3.h bv.h bv.o mp3.o \
		mp3-write.o mp3-sf.o file.o
	$(CC) $(CFLAGS) -o $@ -DAQ2_TEST aq.c mp3-read.o bv.o mp3.o dlist.o \
		mp3-write.o mp3-sf.o file.o $(LDFLAGS)
TESTS = bvtest packtest dlisttest rtptest mp3-readtest mp3-writetest \
	mp3-sftest mp3-transtest aq1test aq2test galoistest matrixtest \
	fectest crc32test ogg-readtest
tests: test.sh $(TESTS)
	./test.sh $(TESTS)
tests-clean:
	- rm -f $(TESTS)

# Tex
tex/aq.tex: lib/mp3/aq.h lib/mp3/aq.c
	$(TEXIFY) aq.h aq.c > $@
tex/bv.tex: bv.h bv.c
	$(TEXIFY) bv.h bv.c > $@
tex/mp3.tex: lib/mp3/mp3.h lib/mp3/mp3.c lib/mp3/mp3-read.c lib/mp3/mp3-write.c
	$(TEXIFY) mp3.h mp3.c mp3-read.c mp3-write.c > $@
tex/rtp.tex: lib/rtp/rtp.h lib/rtp/rtp.c
	$(TEXIFY) rtp.h rtp.c > $@
tex/rtp-rb.tex: lib/rtp/rtp-rb.h lib/rtp/rtp-rb.c
	$(TEXIFY) rtp-rb.h rtp-rb.c > $@
tex/dlist.tex: lib/data-structures/dlist.h lib/data-structures/dlist.c
	$(TEXIFY) dlist.h dlist.c > $@
tex/pack.tex: lib/pack.h lib/pack.c
	$(TEXIFY) pack.h pack.c > $@
tex/network.tex: lib/system/network.h lib/system/network.c
	$(TEXIFY) network.h network.c > $@
tex/errorlog.tex: errorlog2tex.pl errorlog.txt
	./errorlog2tex.pl errorlog.txt > $@
tex/matrix.tex: lib/math/matrix.h lib/math/matrix.c
	$(TEXIFY) matrix.h matrix.c > $@
tex/galois.tex: lib/math/galois.h lib/math/galois.c
	$(TEXIFY) galois.h galois.c > $@
tex/fec.tex: lib/fec/fec.h lib/fec/fec.c
	$(TEXIFY) fec.h fec.c > $@
tex/fec-pkt.tex: lib/fec/fec-pkt.h lib/fec/fec-pkt.c
	$(TEXIFY) fec-pkt.h fec-pkt.c > $@
tex/fec-group.tex: lib/fec/fec-group.h lib/fec/fec-group.c
	$(TEXIFY) fec-group.h fec-group.c > $@
tex/fec-rb.tex: lib/fec/fec-rb.h lib/fec/fec-rb.c
	$(TEXIFY) fec-rb.h fec-rb.c > $@
tex/poc-2250.tex: servers/poc-2250.c
	$(TEXIFY) poc-2250.c > $@
tex/pob-2250.tex: pob-2250.c
	$(TEXIFY) pob-2250.c > $@
tex/pob-2250-rb.tex: clients/pob-2250-rb.c
	$(TEXIFY) pob-2250-rb.c > $@
tex/poc-3119.tex: servers/poc-3119.c
	$(TEXIFY) poc-3119.c > $@
tex/pob-3119.tex: pob-3119.c
	$(TEXIFY) pob-3119.c > $@
tex/pob-3119-rb.tex: clients/pob-3119-rb.c
	$(TEXIFY) pob-3119-rb.c > $@
tex/poc-fec.tex: servers/poc-fec.c
	$(TEXIFY) poc-fec.c > $@
tex/pob-fec.tex: clients/pob-fec.c clients/pob-fec.h
	$(TEXIFY) pob-fec.h pob-fec.c > $@
tex/poc-http.tex: servers/poc-http.c
	$(TEXIFY) poc-http.c > $@
tex/huffman.tex: huffman.pl
	./pod2latex.pl -out $@ $<
TEXS = tex/aq.tex tex/mp3.tex tex/rtp.tex tex/dlist.tex tex/pack.tex \
       tex/network.tex tex/bv.tex tex/galois.tex \
	tex/matrix.tex tex/fec.tex tex/poc-2250.tex tex/pob-2250.tex \
	tex/poc-3119.tex tex/pob-3119.tex tex/poc-fec.tex tex/pob-fec.tex \
	tex/poc-http.tex tex/fec-pkt.tex tex/rtp-rb.tex tex/fec-group.tex \
	tex/fec-rb.tex tex/pob-3119-rb.tex tex/pob-2250-rb.tex tex/poc-http.tex
STUDIENTEXS = tex/einleitung.tex tex/implementation.tex \
         tex/streaming.tex tex/studienarbeit.tex tex/transcoding.tex \
	 tex/uebersicht.tex tex/fecsec.tex tex/zusammenfassung.tex
tex/test.pdf: tex/test.tex $(TEXS)
	cd tex; pdflatex test.tex; cd ..
tex/studienarbeit.pdf: $(STUDIENTEXS) $(TEXS)
	cd tex; pdflatex studienarbeit.tex; cd ..
tex/code.pdf: tex/code.tex $(TEXS)
	cd tex; pdflatex code.tex; cd ..
tex-clean:
	- rm -f $(TEXS) tex/studienarbeit.pdf tex/code.pdf tex/*.aux tex/*.log

dep-clean:
	- rm -f *.d

clean: tests-clean \
       clients-clean \
       servers-clean \
       tex-clean \
       mp3cue-clean \
       mp3cut-clean \
       mp3length-clean \
       dep-clean

USER  ?= root
GROUP ?= root
PREFIX:= usr/local
install: install-man
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 mp3cue    $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 mp3cut    $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 mp3length $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 pob-2250  $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 pob-3119  $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 pob-fec   $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 poc-2250  $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 poc-3119  $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 poc-fec   $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 poc-http  $(DESTDIR)/$(PREFIX)/bin
	install -g "$(GROUP)" -o "$(USER)" -m 0755 pogg-http $(DESTDIR)/$(PREFIX)/bin

install-man:
	mkdir -p $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/mp3cue.1    $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/mp3cut.1    $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/mp3length.1 $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/pob-2250.1  $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/pob-3119.1  $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/pob-fec.1   $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/poc-2250.1  $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/poc-3119.1  $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/poc-fec.1   $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/poc-http.1  $(DESTDIR)/$(PREFIX)/share/man/man1
	install -g "$(GROUP)" -o "$(USER)" -m 0644 man/man1/pogg-http.1 $(DESTDIR)/$(PREFIX)/share/man/man1

