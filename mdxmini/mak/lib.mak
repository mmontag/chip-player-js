LIB = $(OBJDIR)/libmdxmini.a

OBJFILES =  mdxmini.o mdx2151.o mdxmml_ym2151.o 
OBJFILES += pdxfile.o mdxfile.o pcm8.o ym2151.o nlg.o

CFLAGS += -DUSE_NLG -I.

SRCDIR = src

OBJS = $(addprefix $(OBJDIR)/,$(OBJFILES))

all : $(OBJDIR) $(LIB)

$(OBJDIR) :
	mkdir $(OBJDIR)

$(LIB) : $(OBJS)
	$(AR) rcs $@ $(OBJS)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $< -c
