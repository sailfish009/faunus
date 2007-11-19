  MODEL = gnu
  GROMACS = yes
  OPENMP = no

  ##################################################################

CXX=g++
CLASSDIR=./classes
INCDIR=-I$(CLASSDIR)

ifeq ($(OPENMP), yes)
  OMP=-fopenmp
endif

ifeq ($(GROMACS), yes)
  INCDIR=-I/usr/local/gromacs/include/gromacs/ -I$(CLASSDIR)
  LDFLAGS=-L/usr/local/gromacs/lib/ -lgmx
  GRO=-DGROMACS
endif

ifeq ($(MODEL), debug)
  CXXFLAGS = -O0 -Wextra -Wno-sign-compare -Winline -g $(INCDIR) $(GRO) $(OMP)
endif

ifeq ($(MODEL), gnu)
  CXXFLAGS = -O3 -w -funroll-loops -Winline -g $(INCDIR) $(GRO) $(OMP)
endif

OBJS=$(CLASSDIR)/inputfile.o \
     $(CLASSDIR)/io.o\
     $(CLASSDIR)/titrate.o\
     $(CLASSDIR)/point.o \
     $(CLASSDIR)/physconst.o\
     $(CLASSDIR)/potentials.o\
     $(CLASSDIR)/slump.o\
     $(CLASSDIR)/container.o\
     $(CLASSDIR)/hardsphere.o\
     $(CLASSDIR)/group.o \
     $(CLASSDIR)/particles.o \
     $(CLASSDIR)/analysis.o \
     $(CLASSDIR)/species.o
all:	classes examples

classes:	$(OBJS)
manual:
	doxygen doc/Doxyfile

widom:	examples/widom/widom.C $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) examples/widom/widom.C -o examples/widom/widom
	
ewald:	examples/ewald/ewald.C $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) examples/ewald/ewald.C -o examples/ewald/ewald

twobody:	examples/twobody/twobody.C $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) examples/twobody/twobody.C -o examples/twobody/twobody

pka:	examples/titration/pka.C $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(INCDIR) examples/titration/pka.C -o examples/titration/pka

examples:	widom pka ewald twobody

clean:
	rm -vf $(OBJS) examples/titration/pka examples/widom/widom examples/ewald/ewald

docclean:
	rm -vfR doc/html doc/latex
