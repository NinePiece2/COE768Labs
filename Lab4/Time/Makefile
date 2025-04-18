#
# Internetworking with TCP/IP, Volume III example code Makefile
#
#	David L Stevens, Internetworking Research Group at Purdue
#	Tue Sep 17 19:40:42 EST 1991
#

CC = gcc

INCLUDE =


DEFS =
CFLAGS = ${DEFS} ${INCLUDE}

time_client:
	${CC} -o time_client time_client.c

time_server:
	${CC} -o time_server time_server.c


clean: FRC
	rm -f Makefile.bak a.out core errs lint.errs ${PROGS} *.o

depend: ${HDR} ${CSRC} ${SSRC} ${TNSRC} FRC
	maketd -a ${DEFS} ${INCLUDE} ${CSRC} ${SSRC} ${TNSRC}

install: all FRC
	@echo "Your installation instructions here."

lint: ${HDR} ${XSRC} ${CSRC} ${SSRC} FRC
	lint ${DEFS} ${INCLUDE} ${CSRC} ${SSRC} ${CXSRC} ${SXSRC}

print: Makefile ${SRC} FRC
	lpr Makefile ${CSRC} ${SSRC} ${CXSRC} ${SXSRC}

spotless: clean FRC
	rcsclean Makefile ${HDR} ${SRC}

tags: ${CSRC} ${SSRC} ${CXSRC} ${SXSRC}
	ctags ${CSRC} ${SSRC} ${CXSRC} ${SXSRC}

${HDR} ${CSRC} ${CXSRC} ${SSRC} ${SXSRC}:
	co $@

TCPecho: TCPecho.o
TCPdaytime: TCPdaytime.o
TCPtecho: TCPtecho.o
UDPecho: UDPecho.o
UDPtime: UDPtime.o
TCPdaytimed: TCPdaytimed.o
TCPechod: TCPechod.o
TCPmechod: TCPmechod.o
UDPtimed: UDPtimed.o
daytimed: daytimed.o
superd: superd.o
sonnet: sonnet.o
sonnetd: sonnetd.o

FRC:
	
# DO NOT DELETE THIS LINE - maketd DEPENDS ON IT
S=/usr/include/sys
I=/usr/include

TCPecho.o: $I/stdio.h TCPecho.c

TCPdaytime.o: $I/stdio.h TCPdaytime.c

TCPtecho.o: $I/sgtty.h $I/signal.h $I/stdio.h $S/ioctl.h \
	$S/param.h $S/time.h $S/ttychars.h $S/ttydev.h $S/types.h TCPtecho.c

UDPecho.o: $I/stdio.h UDPecho.c

UDPtime.o: $I/stdio.h UDPtime.c

TCPdaytimed.o: $I/netinet/in.h $I/stdio.h $S/types.h TCPdaytimed.c

TCPechod.o: $I/netinet/in.h $I/stdio.h $S/errno.h $S/signal.h $S/types.h \
	$S/wait.h TCPechod.c

TCPmechod.o: $I/netinet/in.h $I/stdio.h $S/types.h TCPmechod.c

UDPtimed.o: $I/netinet/in.h $I/stdio.h $S/types.h UDPtimed.c

daytimed.o: $I/netinet/in.h $I/stdio.h $S/types.h daytimed.c

superd.o: $I/netinet/in.h $I/signal.h $I/stdio.h $S/errno.h \
	$S/param.h $S/signal.h $S/types.h $S/wait.h superd.c

sonnetd.o: $I/netinet/in.h $I/signal.h $I/stdio.h $S/errno.h \
	$S/param.h $S/signal.h $S/types.h $S/wait.h sonnetd.c
		 
sonnet.o: $I/netinet/in.h $I/signal.h $I/stdio.h $S/errno.h \
	$S/param.h $S/signal.h $S/types.h $S/wait.h sonnet.c


# *** Do not add anything here - It will go away. ***
