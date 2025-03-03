rm -rf run
mkdir run

cwd=`pwd`

make clean
make

cp -f  $cwd/proxy                 $cwd/run/
cp -rf $cwd/lib                   $cwd/run/
cp -f  $cwd/data/*                $cwd/run/
cp -f  $cwd/script/*              $cwd/run/
cp -f  $cwd/dual.json             $cwd/run/

make clean