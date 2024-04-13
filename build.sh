gcc  -o proxy proxy.c socks5.c ip_geo.c http.c picohttpparser.c site.c record.c refer.c -I ./includes -I. -L./lib -lcask -lmaxminddb -lpthread -lcurl -lsqlite3 -ldl

mkdir run

mv -f  proxy                 run/
cp -rf lib                   run/
cp -f  GeoLite2-Country.mmdb run/
cp -rf script/*.sh           run/