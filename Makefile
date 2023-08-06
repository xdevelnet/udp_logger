.PHONY: all clean
all:
	cc main.c -o udp_logger -lm
clean:
	rm -f udp_logger
