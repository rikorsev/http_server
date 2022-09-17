CC=gcc
CFLAGS=-c -Wall
MBEDTLSDIR=./mbedtls
LDFLAGS=-L$(MBEDTLSDIR)/library
SOURCES=main.c server.c http.c soc.c tls.c $(MBEDTLSDIR)/tests/src/certs.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=server
INCLUDE=-I$(MBEDTLSDIR)/include -I$(MBEDTLSDIR)/tests/include -I$(MBEDTLSDIR)/library
LIBS=-lmbedtls -lmbedx509 -lmbedcrypto

.PHONY: mbedtls

all: $(SOURCES) $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) mbedtls
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBS) -o $@

.c.o:
	$(CC) $(CFLAGS) $(INCLUDE) $< -o $@

mbedtls:
	cmake -B$(MBEDTLSDIR) -S$(MBEDTLSDIR)
	$(MAKE) -C $(MBEDTLSDIR)

clean:
	git submodule foreach git clean -xfd
	git submodule foreach git reset --hard
	rm -rf *.o $(EXECUTABLE)