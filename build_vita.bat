mkdir vita_release
cd vita_release
mkdir ntsc
mkdir pal
mkdir jap
mkdir vpk
cd ntsc
cmake ../.. -G "Unix Makefiles" -DVITA=1
make -j15
cd ../pal
cmake ../.. -G "Unix Makefiles" -DVITA=1 -DROMID=pal-final
make -j15
cd ../jap
cmake ../.. -G "Unix Makefiles" -DVITA=1 -DROMID=jpn-final
make -j15
mkdir vpk
cd ../vpk
cmake ../.. -G "Unix Makefiles" -DVITA=1 -DLAUNCHER=1
make -j15