include config.mk

SRC = src/main.c
OBJ = ${SRC:.c=.o}

all: mark

.c.o:
	${CC} -c ${CFLAGS} $< -o $@

mark: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f mark ${OBJ} mark-${VERSION}.tar.gz

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f mark ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/mark

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/mark

.PHONY: all clean install uninstall
