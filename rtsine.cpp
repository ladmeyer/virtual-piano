// rtsine.cpp
#include "SineWave.h"
#include "RtWvOut.h"
#include "Envelope.h"
#include "ADSR.h"
#include "FileWvOut.h"

using namespace stk;

int main() {
  Stk::setSampleRate( 44100.0 );
  Stk::showWarnings( true );
  int nFrames = 100000;
  int releaseCount = (int) (0.9 * nFrames);
  float rampTime = (nFrames - releaseCount) / Stk::sampleRate();
  SineWave sine;
  RtWvOut dac( 1 );
  ADSR env;
  env.keyOn();
  env.setAllTimes( rampTime, rampTime, 0.7, rampTime ); 
  sine.setFrequency( 440.0 );
  StkFloat temp;
  for ( int i=0; i<nFrames; i++ ) {
    temp = env.tick() * sine.tick();
    dac.tick( temp );
    if ( i == releaseCount ) env.keyOff();
  }
}