ifneq ($(RIOTBASE), "")
	# This is the Makefile for RIOT
	MODULE = bw2c
	include $(RIOTBASE)/Makefile.base
endif
