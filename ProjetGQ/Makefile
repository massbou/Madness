
SRCS = httpget.c main.c
DEFINE = -pthread -g
FINAL_TARGET = html

ccoptions=$(shell pkg-config --cflags glib-2.0)
ldoptions=$(shell pkg-config --libs glib-2.0)

CFLAGS = $(DEFINE) -Wall

./$(FINAL_TARGET): $(SRCS:%.c=./%.o)
	gcc $(CFLAGS) $+ -o $@ ${ldoptions}

./%.o: ./%.c
	gcc -c $(CFLAGS) $< ${ccoptions} -o $@

clean:
	rm -f -R -- */ ./*.o ./$(FINAL_TARGET) *.html *.png *.jpg *.jpeg && reset

targ:
	tar -zcvf Bouneffa_Abebe.tar.gz *.c *.h Makefile
