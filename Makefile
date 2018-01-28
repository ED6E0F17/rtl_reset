all:
	gcc -Wall -Wextra rtl_reset.c -lusb-1.0 -o rtl_reset
