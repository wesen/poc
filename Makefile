CFLAGS?=-Wall -g
#CFLAGS+=-DWITH_OPENSSL 
#CFLAGS+=-DWITH_IPV6
#LDFLAGS+=-lssl -lcrypto
#CFLAGS+=-DDEBUG
#CFLAGS+=-DMALLOC -ldmalloc
#CFLAGS += -O2
TEXIFY?=./texify.pl
FLEX=flex
LIBS=-lfl

# bison
YACC=bison -b y

#YACC
#YACC=yacc
#LIBS+=-ly

all:  servers clients mp3cue mp3cut

mp3cue-lex.yy.c: mp3cue.l
	$(FLEX) -o$@ $<
mp3cue-lex.yy.o: mp3cue-lex.yy.c mp3cue-y.tab.c
	$(CC) -c -o $@ mp3cue-lex.yy.c
mp3cue-y.tab.c: mp3cue.y
	$(YACC) -d -o $@ $<
mp3cue-y.tab.o: mp3cue-y.tab.c
	$(CC) -c -o $@ $<

mp3cue: mp3-read.o mp3-write.o mp3.o mp3cue-main.c bv.o mp3.h bv.h \
        mp3cue-lex.yy.o mp3cue-y.tab.o aq.o file.o \
	dlist.o
	$(CC) $(CFLAGS) -o $@  mp3cue-main.c mp3-read.o mp3-write.o mp3.o \
	 bv.o mp3cue-lex.yy.o mp3cue-y.tab.o aq.o file.o \
	 dlist.o $(LDFLAGS) $(LIBS)

mp3cut: mp3-read.o mp3-write.o mp3.o mp3cut.o bv.o mp3.h bv.h aq.o file.o dlist.o
	$(CC) $(CFLAGS) -o $@  mp3-read.o mp3-write.o mp3.o mp3cut.o bv.o aq.o file.o dlist.o \
        $(LDFLAGS) $(LIBS)

aq.o: aq.c aq.h conf.h
buf.o: buf.c buf.h conf.h
bv.o: bv.c bv.h conf.h
crc32.o: crc32.c crc32.h conf.h
dlist.o: dlist.c dlist.h conf.h
fec-group.o: fec-group.c fec-group.h fec.h conf.h
fec-pkt.o: fec-pkt.c pack.h fec-pkt.h conf.h
fec-rb.o: fec-rb.c fec-group.h conf.h
fec.o: fec.c fec.h matrix.h conf.h
file.o: file.h file.c conf.h
galois.o: galois.c galois.h
huffman-read.o: huffman-read.c bv.h
matrix.o: matrix.c matrix.h
mp3-huffman.o: mp3-huffman.o bv.h mp3.h conf.h
mp3-read.o: mp3-read.c mp3.h bv.h conf.h
mp3-sf.o: mp3-sf.c bv.h mp3.h conf.h
mp3-trans.o: mp3-trans.c mp3.h conf.h
mp3-write.o: mp3-write.c mp3.h bv.h conf.h
mp3.o: mp3.c mp3.h conf.h
network.o: network.h network.c conf.h
network4.o: network.h pack.h network4.c conf.h
network6.o: network6.c pack.h network.h conf.h
ogg.o: ogg.c ogg.h crc32.h buf.h conf.h pack.h
ogg-read.o: ogg-read.c ogg.h file.h pack.h crc32.h conf.h
ogg-write.o: ogg-write.c ogg.h pack.h file.h crc32.h conf.h
pack.o: pack.c pack.h conf.h
pob-2250-rb.o: pob-2250-rb.c conf.h rtp.h rtp-rb.h network.h
pob-2250.o: pob-2250.c conf.h rtp.h network.h dlist.h
pob-3119-rb.o: pob-3119-rb.c conf.h rtp.h rtp-rb.h aq.h network.h
pob-3119.o: pob-3119.c conf.h rtp.h aq.h network.h dlist.h
pob-fec.o: pob-fec.c conf.h fec-pkt.h aq.h fec.h network.h
poc-2250.o: poc-2250.c conf.h mp3.h rtp.h signal.h file.h
poc-3319.o: poc-3119.c conf.h mp3.h aq.h network.h rtp.h signal.h file.h
poc-fec.o: poc-fec.c conf.h mp3.h network.h fec-pkt.h pack.h aq.h signal.h
poc-http.o: poc-http.c conf.h file.h mp3.h network.h signal.h
pogg-http.o: pogg-http.c conf.h file.h vorbis.h network.h ogg.h signal.h
rtp-rb.o: rtp-rb.c rtp.h conf.h
rtp.o: rtp.c pack.h rtp.h conf.h
signal.o: signal.c signal.h conf.h
vorbis.o: vorbis.c ogg.h buf.h vorbis.h conf.h
vorbis-read.o: vorbis-read.c ogg.h file.h buf.h vorbis.h pack.h bv.h

NETWORK_O=network.o network4.o network6.o
MP3_O=mp3.o mp3-read.o mp3-write.o aq.o
RTP_O=rtp.o rtp-rb.o
UTILS_O=pack.o bv.o signal.o dlist.o file.o buf.o crc32.o
FEC_O=galois.o matrix.o fec.o fec-pkt.o fec-rb.o fec-group.o
OGG_O=ogg.o vorbis.o ogg-read.o ogg-write.o vorbis-read.o

# Servers
servers: poc-2250 poc-3119 poc-2250-ploss poc-3119-ploss poc-fec \
	poc-fec-ploss poc-http pogg-http

servers-clean:
	- rm -f poc-2250 poc-3119 poc-2250-ploss poc-3119-ploss poc-fec \
	poc-fec-ploss poc-http pogg-http

poc-2250: poc-2250.o $(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ poc-2250.o \
	$(MP3_O) $(RTP_O) $(UTILS_O) $(NETWORK_O) \
		$(LDFLAGS)

poc-3119: poc-3119.o $(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ poc-3119.o \
	$(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O) \
		$(LDFLAGS)

poc-2250-ploss: poc-2250.c $(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O)
	$(CC) -DDEBUG_PLOSS $(CFLAGS) -o $@ poc-2250.c \
	$(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O) \
		$(LDFLAGS)

poc-3119-ploss: poc-3119.c $(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O)
	$(CC) -DDEBUG_PLOSS $(CFLAGS) -o $@ poc-3119.c \
	$(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O) \
		$(LDFLAGS)

poc-fec: poc-fec.o $(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O) $(FEC_O)
	$(CC) $(CFLAGS) -o $@ poc-fec.o \
	$(NETWORK_O) $(MP3_O) $(UTILS_O) $(FEC_O) \
		$(LDFLAGS)

poc-fec-ploss: poc-fec.c $(NETWORK_O) $(MP3_O) $(RTP_O) $(UTILS_O) $(FEC_O)
	$(CC) -DDEBUG_PLOSS $(CFLAGS) -o $@ poc-fec.c \
	$(NETWORK_O) $(MP3_O) $(UTILS_O) $(FEC_O) \
		$(LDFLAGS)

poc-http: poc-http.o $(NETWORK_O) $(MP3_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ poc-http.o \
	$(NETWORK_O) $(MP3_O) $(UTILS_O) $(LDFLAGS)

pogg-http: pogg-http.o $(NETWORK_O) $(OGG_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ pogg-http.o \
	$(NETWORK_O) $(OGG_O) $(UTILS_O) $(LDFLAGS)

# Clients
clients: pob-2250 pob-3119 pob-fec pob-3119-rb pob-2250-rb

clients-clean:
	- rm -f pob-2250 pob-3119 pob-3119-rb pob-2250-rb pob-fec

pob-2250: pob-2250.o $(RTP_O) $(NETWORK_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ pob-2250.c \
	$(RTP_O) $(NETWORK_O) $(UTILS_O) \
			$(LDFLAGS)

pob-3119: pob-3119.o $(RTP_O) $(NETWORK_O) $(MP3_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ pob-3119.c \
		$(RTP_O) $(MP3_O) $(NETWORK_O) $(UTILS_O) \
		$(LDFLAGS)

pob-3119-rb: pob-3119-rb.o $(RTP_O) $(NETWORK_O) $(MP3_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ pob-3119-rb.c \
		$(RTP_O) $(MP3_O) $(NETWORK_O) $(UTILS_O) \
		$(LDFLAGS)

pob-2250-rb: pob-2250-rb.o $(RTP_O) $(NETWORK_O) $(MP3_O) $(UTILS_O)
	$(CC) $(CFLAGS) -o $@ pob-2250-rb.c \
		$(RTP_O) $(MP3_O) $(NETWORK_O) $(UTILS_O) \
		$(LDFLAGS)

pob-fec: pob-fec.o $(RTP_O) $(NETWORK_O) $(MP3_O) $(UTILS_O) $(FEC_O)
	$(CC) $(CFLAGS) -o $@ pob-fec.c \
		$(RTP_O) $(MP3_O) $(NETWORK_O) $(UTILS_O) $(FEC_O) \
		$(LDFLAGS)

# Tests
bvtest: bv.c bv.h
	$(CC) $(CFLAGS) -o $@ -DBV_TEST bv.c $(LDFLAGS)
crc32test: crc32.h crc32.c
	$(CC) $(CFLAGS) -o $@ -DCRC32_TEST crc32.c $(LDFLAGS)
packtest: pack.c pack.h 
	$(CC) $(CFLAGS) -o $@ -DPACK_TEST pack.c $(LDFLAGS)
dlisttest: dlist.c dlist.h
	$(CC) $(CFLAGS) -o $@ -DDLIST_TEST dlist.c $(LDFLAGS)

galoistest: galois.c galois.h
	$(CC) $(CFLAGS) -o $@ -DGALOIS_TEST galois.c $(LDFLAGS)
matrixtest: matrix.c matrix.h galois.o
	$(CC) $(CFLAGS) -o $@ -DMATRIX_TEST matrix.c galois.o $(LDFLAGS)
fectest: fec.c fec.h galois.o matrix.o
	$(CC) $(CFLAGS) -o $@ -DFEC_TEST fec.c matrix.o galois.o $(LDFLAGS)

rtptest: rtp.c rtp.h pack.o pack.h
	$(CC) $(CFLAGS) -o $@ -DRTP_TEST rtp.c pack.o $(LDFLAGS)

ogg-readtest: ogg-read.c ogg.o crc32.o file.o buf.o pack.o
	$(CC) $(CFLAGS) -o $@ -DDEBUG -DOGG_TEST ogg-read.c ogg.o crc32.o \
				file.o buf.o pack.o
ogg-writetest: ogg-write.c ogg-read.c ogg.o crc32.o file.o buf.o pack.o
	$(CC) $(CFLAGS) -o $@ -DDEBUG -DOGG_WRITETEST ogg-write.c ogg-read.c\
				ogg.o crc32.o \
				file.o buf.o pack.o
vorbis-readtest: vorbis-read.c vorbis.o ogg.o crc32.o file.o buf.o pack.o \
				ogg-read.o bv.o
	$(CC) $(CFLAGS) -o $@ -DDEBUG -DVORBIS_TEST vorbis-read.c vorbis.o \
				ogg.o ogg-read.o crc32.o file.o buf.o pack.o \
				bv.o
mp3-readtest: mp3-read.c mp3.h bv.o bv.h mp3.o mp3-sf.o
	$(CC) $(CFLAGS) -o $@ -DMP3_TEST mp3-read.c bv.o mp3.o  mp3-sf.o \
		$(LDFLAGS)
mp3-writetest: mp3-write.c mp3-read.o mp3.h bv.o bv.h mp3.o mp3-sf.o
	$(CC) $(CFLAGS) -o $@ -DMP3_TEST mp3-write.c bv.o mp3-read.o mp3.o \
					 mp3-sf.o $(LDFLAGS)
mp3-sftest: mp3-sf.c mp3-write.o mp3-read.o mp3.h bv.o bv.h mp3.o aq.o dlist.o
	$(CC) $(CFLAGS) -o $@ -DMP3SF_TEST mp3-sf.c mp3-write.o bv.o \
				mp3-read.o mp3.o aq.o dlist.o $(LDFLAGS)
mp3-transtest: mp3-trans.c mp3-read.o mp3-write.o mp3.h bv.o bv.h \
	       mp3.o mp3-sf.o 
	$(CC) $(CFLAGS) -o $@ -DMP3_TEST mp3-trans.c bv.o mp3-read.o \
			mp3.o mp3-write.o  mp3-sf.o $(LDFLAGS)
aq1test: aq.c aq.h dlist.o dlist.h mp3-read.o mp3.h bv.h bv.o mp3.o mp3-sf.o
	$(CC) $(CFLAGS) -o $@ -DAQ1_TEST aq.c mp3-read.o bv.o mp3.o dlist.o \
		mp3-sf.o $(LDFLAGS)
aq2test: aq.c aq.h dlist.o dlist.h mp3-read.o mp3.h bv.h bv.o mp3.o \
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
tex/aq.tex: aq.h aq.c
	$(TEXIFY) aq.h aq.c > $@
tex/bv.tex: bv.h bv.c
	$(TEXIFY) bv.h bv.c > $@
tex/mp3.tex: mp3.h mp3.c mp3-read.c mp3-write.c 
	$(TEXIFY) mp3.h mp3.c mp3-read.c mp3-write.c > $@
tex/rtp.tex: rtp.h rtp.c
	$(TEXIFY) rtp.h rtp.c > $@
tex/rtp-rb.tex: rtp-rb.h rtp-rb.c
	$(TEXIFY) rtp-rb.h rtp-rb.c > $@
tex/dlist.tex: dlist.h dlist.c
	$(TEXIFY) dlist.h dlist.c > $@
tex/pack.tex: pack.h pack.c
	$(TEXIFY) pack.h pack.c > $@
tex/network.tex: network.h network.c
	$(TEXIFY) network.h network.c > $@
tex/errorlog.tex: errorlog2tex.pl errorlog.txt
	./errorlog2tex.pl errorlog.txt > $@
tex/matrix.tex: matrix.h matrix.c
	$(TEXIFY) matrix.h matrix.c > $@
tex/galois.tex: galois.h galois.c
	$(TEXIFY) galois.h galois.c > $@
tex/fec.tex: fec.h fec.c
	$(TEXIFY) fec.h fec.c > $@
tex/fec-pkt.tex: fec-pkt.h fec-pkt.c
	$(TEXIFY) fec-pkt.h fec-pkt.c > $@
tex/fec-group.tex: fec-group.h fec-group.c
	$(TEXIFY) fec-group.h fec-group.c > $@
tex/fec-rb.tex: fec-rb.h fec-rb.c
	$(TEXIFY) fec-rb.h fec-rb.c > $@
tex/poc-2250.tex: poc-2250.c
	$(TEXIFY) poc-2250.c > $@
tex/pob-2250.tex: pob-2250.c
	$(TEXIFY) pob-2250.c > $@
tex/pob-2250-rb.tex: pob-2250-rb.c
	$(TEXIFY) pob-2250-rb.c > $@
tex/poc-3119.tex: poc-3119.c
	$(TEXIFY) poc-3119.c > $@
tex/pob-3119.tex: pob-3119.c
	$(TEXIFY) pob-3119.c > $@
tex/pob-3119-rb.tex: pob-3119-rb.c
	$(TEXIFY) pob-3119-rb.c > $@
tex/poc-fec.tex: poc-fec.c
	$(TEXIFY) poc-fec.c > $@
tex/pob-fec.tex: pob-fec.c pob-fec.h
	$(TEXIFY) pob-fec.h pob-fec.c > $@
tex/poc-http.tex: poc-http.c
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

clean: tests-clean clients-clean servers-clean tex-clean
	- rm -f *.o *.exe mp3cue-lex.yy.c mp3cue-y.tab.c mp3cue-y.tab.h mp3cue

# Generate Huffman table
huffman_read.c: huffman.pl huffman-table
	./huffman.pl huffman-table
	indent $@
