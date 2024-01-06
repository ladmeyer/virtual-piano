g++ -std=c++11 -o rtsine -Istk/include/ -Lstk/src/ -D__MACOSX_CORE__ rtsine.cpp -lstk -lpthread -framework CoreAudio -framework CoreMIDI -framework CoreFoundation
./rtsine
