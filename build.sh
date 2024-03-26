gcc -D CO_DEBUG -o proxy proxy.c socks5.c ip_geo.c -I ./includes -I. -L./lib -lcask -lmaxminddb -lpthread -ldl

