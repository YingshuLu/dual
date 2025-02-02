gcc  -o proxy proxy.c config.c cJSON.c socks5.c ip_geo.c http.c picohttpparser.c site.c record.c refer.c -I ./includes -I. -L./lib -lcask -lmaxminddb -lpthread -lcurl -lsqlite3 -ldl

rm -rf run
mkdir run

cwd=`pwd`

mv -f  $cwd/proxy                 $cwd/run/
cp -rf $cwd/lib                   $cwd/run/
cp -f  $cwd/data/*                $cwd/run/
cp -f  $cwd/script/*              $cwd/run/
cp -f  $cwd/dual.json             $cwd/run/