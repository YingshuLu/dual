gcc  -o proxy proxy.c socks5.c ip_geo.c http.c picohttpparser.c site.c record.c refer.c -I ./includes -I. -L./lib -lcask -lmaxminddb -lpthread -lcurl -lsqlite3 -ldl

