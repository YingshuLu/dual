gcc  -o proxy proxy.c socks5.c ip_geo.c http.c picohttpparser.c -I ./includes -I. -L./lib -lcask -lmaxminddb -lpthread -ldl

