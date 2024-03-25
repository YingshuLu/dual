gcc -D CO_DEBUG -o proxy proxy.c socks5.c -I ./includes -I. -L./lib -lcask -lpthread -ldl

