#
#
#

# Compile rule for Earthworm version under 7.9
#
ver_709: libs echo_msg_709
	@(cd ./src; make -f makefile.unix ver_709;);

ver_709_sql: libs_all echo_msg_709
	@(cd ./src; make -f makefile.unix ver_709_sql;);

# Compile rule for Earthworm version over 7.10
#
ver_710: libs echo_msg_710
	@(cd ./src; make -f makefile.unix ver_710;);

ver_710_sql: libs_all echo_msg_710
	@(cd ./src; make -f makefile.unix ver_710_sql;);

#
#
cap_set:
	@(cd ./src; make -f makefile.unix cap_set;);
#
#
libs: echo_msg_libraries
	@(cd ./src/libsrc; make -f makefile.unix;);

libs_all: echo_msg_libraries
	@echo "Making libraries";
	@(cd ./src/libsrc; make -f makefile.unix all;);

#
#
echo_msg_709:
	@echo "-----------------------------------";
	@echo "- Making main palert2ew for EW7.9 -";
	@echo "-----------------------------------";
echo_msg_710:
	@echo "------------------------------------";
	@echo "- Making main palert2ew for EW7.10 -";
	@echo "------------------------------------";
echo_msg_libraries:
	@echo "----------------------------------";
	@echo "-        Making libraries        -";
	@echo "----------------------------------";

# Clean-up rules
clean:
	@(cd ./src; make -f makefile.unix clean;);
	@(cd ./src/libsrc; make -f makefile.unix clean; make -f makefile.unix clean_lib;);

clean_bin:
	@(cd ./src; make clean_bin;);
