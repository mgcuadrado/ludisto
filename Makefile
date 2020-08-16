SHELL := /bin/bash
VARIANT := opt
SRCDIR := .
DEPDIR := .depend.$(VARIANT)
OBJDIR := .object.$(VARIANT)
include $(SRCDIR)/Makefile.common
include $(SRCDIR)/.makefile.config
include $(SRCDIR)/Makevariant.$(VARIANT)
include $(SRCDIR)/Makefile.def
# include $(SRCDIR)/Makefile.syntax
include $(SRCDIR)/Makefile.spe
