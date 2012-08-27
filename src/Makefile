OBJS = ibutton/crcutil.o ibutton/ds2480ut.o \
ibutton/linuxlnk.o ibutton/owerr.o \
ibutton/owllu.o ibutton/ownetu.o \
ibutton/owsesu.o ibutton/owtrnu.o \
ibutton/sha18.o ibutton/shaib.o \
ibutton-brute.c ibutton.c

ibutton-brute: $(OBJS)
	gcc $^ -o $@

%.o: %.c
	gcc -c -o $@ $<

all: ibutton-brute

clean:
	rm -f *.o
