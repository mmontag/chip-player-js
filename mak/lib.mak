LIB = libmdxmini.a

OBJFILES =  mdxmini.o mdx2151.o mdxmml_ym2151.o 
OBJFILES += pdxfile.o mdxfile.o pcm8.o ym2151.o

SRCDIR = src
OBJDIR = obj

OBJS = $(addprefix $(OBJDIR)/,$(OBJFILES))

all : $(OBJDIR) $(LIB)

$(OBJDIR) :
	mkdir $(OBJDIR)

$(LIB) : $(OBJS)
	$(AR) r $@ $(OBJS)

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) -o $@ $< -c $(CFLAGS)

clean :
	rm -rf $(OBJDIR)
	rm -f $(LIB) 
