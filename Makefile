LIB_OUT=libAU.a
OBJS=AU.o
SRCS=AU.c

CC_CMD=gcc -pipe -Wall -Wextra -Werror -pedantic -std=c99 -c -g3 \
	-O2

.c.o:
	$(CC_CMD) $<

$(shell $(CC_CMD) -MM $(SRCS) > deps)
include deps

build: $(OBJS)
	ar rc $(LIB_OUT) $(OBJS)
	ranlib $(LIB_OUT)
	rm deps

clean:
	rm -f $(OBJS) deps $(LIB_OUT)
