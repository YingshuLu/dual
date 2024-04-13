gcc  -o proxy proxy.c socks5.c ip_geo.c http.c picohttpparser.c site.c record.c refer.c -I ./includes -I. -L./lib -lcask -lmaxminddb -lpthread -lcurl -lsqlite3 -ldl

mkdir run

cwd=`pwd`

mv -f  $cwd/proxy                 $cwd/run/
cp -rf $cwd/lib                   $cwd/run/
cp -f  $cwd/data/*                $cwd/run/
cp -f  $cwd/script/*              $cwd/run/