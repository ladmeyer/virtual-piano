// #define COW_PATCH_FRAMERATE
// #define COW_PATCH_FRAMERATE_SLEEP

#define COW_NO_SOUND
#include "include.cpp"

// stk
#undef PI
#include "SineWave.h"
#include "RtWvOut.h"
#include "BeeThree.h"
#include "Clarinet.h"
#include "Mandolin.h"
#include "Wurley.h"
#include "Bowed.h"
#include "RtAudio.h"
#include "Voicer.h"
#include "Messager.h"
#include "SKINImsg.h"
#include <cstdlib>
#include <algorithm>
using std::min;
using namespace stk;

#define WHITE_KEY_NUM 52
#define BLACK_KEY_NUM 36

#define KEY_FULL 0
#define KEY_LEFT 1
#define KEY_RIGHT 2
#define KEY_CENTER 3
#define KEY_BLACK 4

// The TickData structure holds all the class instances and data that
// are shared by the various processing functions.
struct TickData {
  Voicer voicer;
  Messager messager;
  Skini::Message message;
  int counter;
  bool haveMessage;
  bool done;

  // Default constructor.
  TickData()
    : counter(0), haveMessage(false), done( false ) {}
};

#define DELTA_CONTROL_TICKS 64 // default sample frames between control input checks

// The processMessage() function encapsulates the handling of control
// messages.  It can be easily relocated within a program structure
// depending on the desired scheduling scheme.
void processMessage( TickData* data )
{
  register StkFloat value1 = data->message.floatValues[0];
  register StkFloat value2 = data->message.floatValues[1];

  switch( data->message.type ) {

  case __SK_Exit_:
    data->done = true;
    return;

  case __SK_NoteOn_:
    if ( value2 == 0.0 ) // velocity is zero ... really a NoteOff
      data->voicer.noteOff( value1, 64.0 );
    else { // a NoteOn
      data->voicer.noteOn( value1, value2 );
    }
    break;

  case __SK_NoteOff_:
    data->voicer.noteOff( value1, value2 );
    break;

  case __SK_ControlChange_:
    data->voicer.controlChange( (int) value1, value2 );
    break;

  case __SK_AfterTouch_:
    data->voicer.controlChange( 128, value1 );

  case __SK_PitchChange_:
    data->voicer.setFrequency( value1 );
    break;

  case __SK_PitchBend_:
    data->voicer.pitchBend( value1 );

  } // end of switch

  data->haveMessage = false;
  return;
}

// This tick() function handles sample computation and scheduling of
// control updates.  It will be called automatically when the system
// needs a new buffer of audio samples.
int tick_synth( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
         double streamTime, RtAudioStreamStatus status, void *dataPointer )
{
  TickData *data = (TickData *) dataPointer;
  register StkFloat *samples = (StkFloat *) outputBuffer;
  int counter, nTicks = (int) nBufferFrames;

  while ( nTicks > 0 && !data->done ) {

    if ( !data->haveMessage ) {
      data->messager.popMessage( data->message );
      if ( data->message.type > 0 ) {
        data->counter = (long) (data->message.time * Stk::sampleRate());
        data->haveMessage = true;
      }
      else
        data->counter = DELTA_CONTROL_TICKS;
    }

    counter = min( nTicks, data->counter );
    data->counter -= counter;

    for ( int i=0; i<counter; i++ ) {
      *samples++ = data->voicer.tick();
      nTicks--;
    }
    if ( nTicks == 0 ) break;

    // Process control messages.
    if ( data->haveMessage ) processMessage( data );
  }

  return 0;
}

// REMINDER: translation, then rotation, then scaling

// draw a rectangular prism w/ origin as its lower left corner with specified dimensions
void draw_prism(mat4 P, mat4 V, mat4 M, vec3 origin, vec3 dimensions, vec3 color) {
    // makes "lower-left corner" of prism its origin
    mat4 translation = M4_Translation(V3(dimensions.x / 2, dimensions.y / 2, -dimensions.z / 2) + origin);
    mat4 scaling = M4_Scaling(dimensions.x / 2, dimensions.y / 2, dimensions.z / 2);
    library.meshes.box.draw(P, V, M * translation * scaling, color);
}

// draw a white key that is not bordered by any black keys
void draw_full_white_key(mat4 P, mat4 V, mat4 M, vec3 origin, vec3 color) {
    draw_prism(P, V, M, origin, V3(0.2, 0.15, 1.0), color);
    return;
}

// draw a white key that is to the left of one black key
void draw_left_white_key(mat4 P, mat4 V, mat4 M, vec3 origin, vec3 color) {
    // draws the thicker part of the key
    draw_prism(P, V, M, origin, V3(0.2, 0.15, 0.333), color);
    // draws the thinner part of the key
    draw_prism(P, V, M, origin + V3(0.0, 0.0, -0.333), V3(0.15, 0.15, 0.666), color);
    return;
}

// draw a white key that is to the right of one black key
void draw_right_white_key(mat4 P, mat4 V, mat4 M, vec3 origin, vec3 color) {
    // draws the thicker part of the key
    draw_prism(P, V, M, origin, V3(0.2, 0.15, 0.333), color);
    // draws the thinner part of the key
    draw_prism(P, V, M, origin + V3(0.05, 0.0, -0.333), V3(0.15, 0.15, 0.666), color);
    return;
}

// draw a white key that is between two black keys
void draw_center_white_key(mat4 P, mat4 V, mat4 M, vec3 origin, vec3 color) {
    // draws the thicker part of the key
    draw_prism(P, V, M, origin, V3(0.2, 0.15, 0.333), color);
    // draws the thinner part of the key
    draw_prism(P, V, M, origin + V3(0.05, 0.0, -0.333), V3(0.1, 0.15, 0.666), color);
    return;
}

// draw a black key :)
void draw_black_key(mat4 P, mat4 V, mat4 M, vec3 origin, vec3 color) {
    draw_prism(P, V, M, origin, V3(0.1, 0.25, 0.65), color);
}

void play_white_note(int note_id, long tags[], int octave, StretchyBuffer<vec3> white_key_colors, StretchyBuffer<vec3> white_key_positions) {
    white_key_colors.data[note_id + (7 * octave)] = monokai.yellow;
    white_key_positions.data[note_id + (7 * octave)] -= V3(0.0, 0.1, 0.0);      
}

void stop_white_note(int note_id, long tags[], int octave, StretchyBuffer<vec3> white_key_colors, StretchyBuffer<vec3> white_key_positions) {
    white_key_colors.data[note_id + (7 * octave)] = monokai.white;
    white_key_positions.data[note_id + (7 * octave)] += V3(0.0, 0.1, 0.0);
}

void play_black_note(int note_id, long tags[], int octave, StretchyBuffer<vec3> black_key_colors, StretchyBuffer<vec3> black_key_positions) {
    black_key_colors.data[note_id + (5 * octave)] = monokai.purple;
    black_key_positions.data[note_id + (5 * octave)] -= V3(0.0, 0.1, 0.0);
}

void stop_black_note(int note_id, long tags[], int octave, StretchyBuffer<vec3> black_key_colors, StretchyBuffer<vec3> black_key_positions) {
    black_key_colors.data[note_id + (5 * octave)] = monokai.blue;
    black_key_positions.data[note_id + (5 * octave)] += V3(0.0, 0.1, 0.0);
}

void final_project () {
    // stk setup code
    // set the global sample rate and rawwave path
    Stk::setSampleRate( 44100.0 );
    Stk::setRawwavePath( "stk/rawwaves/" );

    // initialize instrument (we can play 6 simultaneous notes)
    int i;
    TickData data;
    RtAudio dac;
    Instrmnt *instrument[6];
    for ( i=0; i<6; i++ ) { 
        instrument[i] = 0;
    }

    // setup the RtAudio stream
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 1;
    RtAudioFormat format = ( sizeof(StkFloat) == 8 ) ? RTAUDIO_FLOAT64 : RTAUDIO_FLOAT32;
    unsigned int bufferFrames = RT_BUFFER_SIZE;

    dac.openStream( &parameters, NULL, format, (unsigned int)Stk::sampleRate(), &bufferFrames, &tick_synth, (void *)&data );

    // define and load the instruments
    for ( i=0; i<6; i++ ) {
        instrument[i] = new Wurley();
    }

    // add the instruments to the voicer
    for ( i=0; i<6; i++ ) {
        data.voicer.addInstrument( instrument[i] );
    }

    dac.startStream();
    // piano range in MIDI note values is 21 through 108
    long tags[128];

    Camera3D camera = {3.0};

    // fill white key position & color buffers
    StretchyBuffer<vec3> white_key_positions = {};
    StretchyBuffer<vec3> white_key_colors = {};
    for (int i = 0; i < WHITE_KEY_NUM; i++) {
        // since keys are 0.2 units wide, this leaves 0.02 units of space between them
        sbuff_push_back(&white_key_positions, V3(i * 0.22, 0.0, 0.0));        
        sbuff_push_back(&white_key_colors, monokai.white);
    }

    // fill key type buffer to reflect standard 52-key layout
    StretchyBuffer<int> white_key_types = {};
    // low A and B
    { sbuff_push_back(&white_key_types, KEY_LEFT);
    sbuff_push_back(&white_key_types, KEY_RIGHT);
    }
    // 7 full octaves
    for (int i = 0; i < 7; i++) { 
    sbuff_push_back(&white_key_types, KEY_LEFT);
    sbuff_push_back(&white_key_types, KEY_CENTER);
    sbuff_push_back(&white_key_types, KEY_RIGHT);
    sbuff_push_back(&white_key_types, KEY_LEFT);
    sbuff_push_back(&white_key_types, KEY_CENTER);
    sbuff_push_back(&white_key_types, KEY_CENTER);
    sbuff_push_back(&white_key_types, KEY_RIGHT);
    }
    // top C
    sbuff_push_back(&white_key_types, KEY_FULL);

    // fill black key position & color buffers; this process is dependent on white key types
    StretchyBuffer<vec3> black_key_positions = {};
    StretchyBuffer<vec3> black_key_colors = {};
    int black_key_num = BLACK_KEY_NUM;
    for (int i = 0; i < WHITE_KEY_NUM; i++) {
        // if there should be a black key to the right of the current white key...
        if (white_key_types[i] == KEY_LEFT or white_key_types[i] == KEY_CENTER) {
            // add it to the position buffer!
            sbuff_push_back(&black_key_positions, white_key_positions[i] + V3(0.16, 0.0, -0.34));
            sbuff_push_back(&black_key_colors, monokai.blue);
        }
    }

    int octave = 4;

    while (cow_begin_frame()) { 
        mat4 P = camera_get_P(&camera);
        mat4 V = camera_get_V(&camera);
        mat4 M = globals.Identity;

        camera_move(&camera);

        gui_slider("octave", &octave, 0, 7);
        if (globals.key_pressed['n'] && octave != 0) {
            octave -= 1;
        }
        if (globals.key_pressed['m'] && octave != 7) {
            octave += 1;
        }

        // randomize keyboard layout :0
        if (globals.key_pressed[COW_KEY_SPACE]) {
            black_key_num = 0;
            sbuff_free(&white_key_types);

            if (random_sign() > 0) {
                sbuff_push_back(&white_key_types, 0);
            } else {
                sbuff_push_back(&white_key_types, 1);
                black_key_num += 1;
            }

            for (int i = 1; i < WHITE_KEY_NUM; i++) {
                if (white_key_types[i - 1] == KEY_FULL or white_key_types[i - 1] == KEY_RIGHT) {
                    if (random_sign() > 0) {
                        sbuff_push_back(&white_key_types, 0);
                    } else {
                        sbuff_push_back(&white_key_types, 1);
                        black_key_num += 1;
                    }
                } else {
                    if (random_sign() > 0) {
                        sbuff_push_back(&white_key_types, 2);
                    } else {
                        sbuff_push_back(&white_key_types, 3);
                        black_key_num += 1;
                    }
                }
            }

            sbuff_free(&black_key_positions);
            sbuff_free(&black_key_colors);
            for (int i = 0; i < WHITE_KEY_NUM; i++) {
                // if there should be a black key to the right of the current white key...
                if (white_key_types[i] == KEY_LEFT or white_key_types[i] == KEY_CENTER) {
                    // add it to the position buffer!
                    sbuff_push_back(&black_key_positions, white_key_positions[i] + V3(0.16, 0.0, -0.34));
                    sbuff_push_back(&black_key_colors, monokai.blue);
                }
            }
        }
        
        // react to key presses 
        {
        if (globals.key_pressed['a']) {
            tags[21 + 0 + (12 * octave)] = data.voicer.noteOn(21 + 0 + (12 * octave), 64);
            play_white_note(0, tags, octave, white_key_colors, white_key_positions); 
        }
        if (globals.key_released['a']) {
            data.voicer.noteOff(tags[21 + 0 + (12 * octave)], 64);
            stop_white_note(0, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_pressed['s']) {
            tags[21 + 2 + (12 * octave)] = data.voicer.noteOn(21 + 2 + (12 * octave), 64);
            play_white_note(1, tags, octave, white_key_colors, white_key_positions); 
        }
        if (globals.key_released['s']) {
            data.voicer.noteOff(tags[21 + 2 + (12 * octave)], 64);
            stop_white_note(1, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_pressed['d']) {
            tags[21 + 3 + (12 * octave)] = data.voicer.noteOn(21 + 3 + (12 * octave), 64);
            play_white_note(2, tags, octave, white_key_colors, white_key_positions); 
        }
        if (globals.key_released['d']) {
            data.voicer.noteOff(tags[21 + 3 + (12 * octave)], 64);
            stop_white_note(2, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_pressed['f']) {
            tags[21 + 5 + (12 * octave)] = data.voicer.noteOn(21 + 5 + (12 * octave), 64);
            play_white_note(3, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_released['f']) {
            data.voicer.noteOff(tags[21 + 5 + (12 * octave)], 64);
            stop_white_note(3, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_pressed['g']) {
            tags[21 + 7 + (12 * octave)] = data.voicer.noteOn(21 + 7 + (12 * octave), 64);
            play_white_note(4, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_released['g']) {
            data.voicer.noteOff(tags[21 + 7 + (12 * octave)], 64);
            stop_white_note(4, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_pressed['h']) {
            tags[21 + 8 + (12 * octave)] = data.voicer.noteOn(21 + 8 + (12 * octave), 64);
            play_white_note(5, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_released['h']) {
            data.voicer.noteOff(tags[21 + 8 + (12 * octave)], 64);
            stop_white_note(5, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_pressed['j']) {
            tags[21 + 10 + (12 * octave)] = data.voicer.noteOn(21 + 10 + (12 * octave), 64);
            play_white_note(6, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_released['j']) {
            data.voicer.noteOff(tags[21 + 10 + (12 * octave)], 64);
            stop_white_note(6, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_pressed['k']) {
            tags[21 + 12 + (12 * octave)] = data.voicer.noteOn(21 + 12 + (12 * octave), 64);
            play_white_note(7, tags, octave, white_key_colors, white_key_positions);
        }
        if (globals.key_released['k']) {
            data.voicer.noteOff(tags[21 + 12 + (12 * octave)], 64);
            stop_white_note(7, tags, octave, white_key_colors, white_key_positions);
        } 

        if (globals.key_pressed['w']) {
            tags[22 + 0 + (12 * octave)] = data.voicer.noteOn(22 + 0 + (12 * octave), 64);
            play_black_note(0, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_released['w']) {
            data.voicer.noteOff(tags[22 + 0 + (12 * octave)], 64);
            stop_black_note(0, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_pressed['r']) {
            tags[22 + 3 + (12 * octave)] = data.voicer.noteOn(22 + 3 + (12 * octave), 64);
            play_black_note(1, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_released['r']) {
            data.voicer.noteOff(tags[22 + 3 + (12 * octave)], 64);
            stop_black_note(1, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_pressed['t']) {
            tags[22 + 5 + (12 * octave)] = data.voicer.noteOn(22 + 5 + (12 * octave), 64);
            play_black_note(2, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_released['t']) {
            data.voicer.noteOff(tags[22 + 5 + (12 * octave)], 64);
            stop_black_note(2, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_pressed['u']) {
            tags[22 + 8 + (12 * octave)] = data.voicer.noteOn(22 + 8 + (12 * octave), 64);
            play_black_note(3, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_released['u']) {
            data.voicer.noteOff(tags[22 + 8 + (12 * octave)], 64);
            stop_black_note(3, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_pressed['u']) {
            tags[22 + 10 + (12 * octave)] = data.voicer.noteOn(22 + 10 + (12 * octave), 64);
            play_black_note(4, tags, octave, black_key_colors, black_key_positions);
        }
        if (globals.key_released['u']) {
            data.voicer.noteOff(tags[22 + 10 + (12 * octave)], 64);
            stop_black_note(4, tags, octave, black_key_colors, black_key_positions);
        } 
        }

        // draw white keys
        { for (int i = 0; i < WHITE_KEY_NUM; i++) {
            if (white_key_types[i] == KEY_FULL) {
                draw_full_white_key(P, V, M, white_key_positions[i], white_key_colors[i]);
            } else if (white_key_types[i] == KEY_LEFT) {
                draw_left_white_key(P, V, M, white_key_positions[i], white_key_colors[i]);
            } else if (white_key_types[i] == KEY_RIGHT) {
                draw_right_white_key(P, V, M, white_key_positions[i], white_key_colors[i]);
            } else if (white_key_types[i] == KEY_CENTER) {
                draw_center_white_key(P, V, M, white_key_positions[i], white_key_colors[i]);
            }
        } }

        // draw black keys
        for (int i = 0; i < black_key_num; i++) {
            draw_black_key(P, V, M, black_key_positions[i], black_key_colors[i]);
        }
    }
}

// from stk, example crtsine.cpp
// This tick() function handles sample computation only.  It will be
// called automatically when the system needs a new buffer of audio
// samples.
int tick_sine( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
         double streamTime, RtAudioStreamStatus status, void *dataPointer )
{
  SineWave *sine = (SineWave *) dataPointer;
  register StkFloat *samples = (StkFloat *) outputBuffer;

  for ( unsigned int i=0; i<nBufferFrames; i++ )
    *samples++ = sine->tick();

  return 0;
}

void sine() {
    // Set the global sample rate before creating class instances.
    Stk::setSampleRate( 44100.0 );

    SineWave sine;
    RtAudio dac;

    // Figure out how many bytes in an StkFloat and setup the RtAudio stream.
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 1;
    RtAudioFormat format = ( sizeof(StkFloat) == 8 ) ? RTAUDIO_FLOAT64 : RTAUDIO_FLOAT32;
    unsigned int bufferFrames = RT_BUFFER_SIZE;
    
    dac.openStream( &parameters, NULL, format, (unsigned int)Stk::sampleRate(), &bufferFrames, &tick_sine, (void *)&sine );

    real frequency = 440.0;

    while(cow_begin_frame()) {
        gui_slider("frequency", &frequency, 100.0, 600.0);
        sine.setFrequency(frequency);

        if (globals.key_pressed[COW_KEY_SPACE]) {
            dac.startStream();
        }
        if (globals.key_released[COW_KEY_SPACE]) {
            dac.stopStream();
        }
    }
}

void synth()
{
    // Set the global sample rate and rawwave path before creating class instances.
    Stk::setSampleRate( 44100.0 );
    Stk::setRawwavePath( "stk/rawwaves/" );

    int i;
    TickData data;
    RtAudio dac;
    Instrmnt *instrument[6];
    for ( i=0; i<6; i++ ) { 
        instrument[i] = 0;
    }

    // Figure out how many bytes in an StkFloat and setup the RtAudio stream.
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 1;
    RtAudioFormat format = ( sizeof(StkFloat) == 8 ) ? RTAUDIO_FLOAT64 : RTAUDIO_FLOAT32;
    unsigned int bufferFrames = RT_BUFFER_SIZE;

    dac.openStream( &parameters, NULL, format, (unsigned int)Stk::sampleRate(), &bufferFrames, &tick_synth, (void *)&data );

    // Define and load the instruments
    for ( i=0; i<6; i++ ) {
        instrument[i] = new Mandolin(50.0);
    }

    // Add the instruments to the voicer.
    for ( i=0; i<6; i++ ) {
        data.voicer.addInstrument( instrument[i] );
    }

    dac.startStream();
    long tags[128];

    while (cow_begin_frame()) {
        if (globals.key_pressed['a']) {
            tags[60] = data.voicer.noteOn(60, 64);
        }
        if (globals.key_released['a']) {
            data.voicer.noteOff(tags[60], 64);
        }

        if (globals.key_pressed['s']) {
            tags[62] = data.voicer.noteOn(62, 64);
        }
        if (globals.key_released['s']) {
            data.voicer.noteOff(tags[62], 64);
        }

        if (globals.key_pressed['d']) {
            tags[64] = data.voicer.noteOn(64, 64);
        }
        if (globals.key_released['d']) {
            data.voicer.noteOff(tags[64], 64);
        }

        if (globals.key_pressed['f']) {
            tags[65] = data.voicer.noteOn(65, 64);
        }
        if (globals.key_released['f']) {
            data.voicer.noteOff(tags[65], 64);
        }

        if (globals.key_pressed['g']) {
            tags[67] = data.voicer.noteOn(67, 64);
        }
        if (globals.key_released['g']) {
            data.voicer.noteOff(tags[67], 64);
        }

        if (globals.key_pressed['h']) {
            tags[69] = data.voicer.noteOn(69, 64);
        }
        if (globals.key_released['h']) {
            data.voicer.noteOff(tags[69], 64);
        }

        if (globals.key_pressed['j']) {
            tags[71] = data.voicer.noteOn(71, 64);
        }
        if (globals.key_released['j']) {
            data.voicer.noteOff(tags[71], 64);
        }

        if (globals.key_pressed['k']) {
            tags[72] = data.voicer.noteOn(72, 64);
        }
        if (globals.key_released['k']) {
            data.voicer.noteOff(tags[72], 64);
        }
    }
}

void shader() {
    char *fragment_shader_source = _load_file_into_char_array("piano_shader.frag");
    char *vertex_shader_source = R""(
        #version 330 core
        layout (location = 0) in vec3 vertex_position;
        void main() {
            gl_Position = vec4(vertex_position, 1.0);
        }
    )"";

    Shader shader = shader_create(vertex_shader_source, 1, fragment_shader_source);

    IndexedTriangleMesh3D mesh = library.meshes.square;
    Camera3D camera = { 5.0 };
    real iTime = 0.0;

    // stk instrument setup code
    // set the global sample rate and rawwave path
    Stk::setSampleRate( 44100.0 );
    Stk::setRawwavePath( "stk/rawwaves/" );

    int i;
    TickData data;
    RtAudio dac;
    Instrmnt *instrument[6];
    for ( i=0; i<6; i++ ) { 
        instrument[i] = 0;
    }

    // setup the RtAudio stream
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 1;
    RtAudioFormat format = ( sizeof(StkFloat) == 8 ) ? RTAUDIO_FLOAT64 : RTAUDIO_FLOAT32;
    unsigned int bufferFrames = RT_BUFFER_SIZE;

    dac.openStream( &parameters, NULL, format, (unsigned int)Stk::sampleRate(), &bufferFrames, &tick_synth, (void *)&data );

    // define and load the instruments
    for ( i=0; i<6; i++ ) {
        instrument[i] = new Wurley();
    }

    // add the instruments to the voicer
    for ( i=0; i<6; i++ ) {
        data.voicer.addInstrument( instrument[i] );
    }

    dac.startStream();
    long tags[128];

    // initialize key data buffers
    StretchyBuffer<vec3> white_key_pressed = {};
    for (int i = 0; i < 52; i++) {
        sbuff_push_back(&white_key_pressed, V3(0.0));        
    }
    StretchyBuffer<vec3> black_key_pressed = {};
    for (int i = 0; i < 52; i++) {
        sbuff_push_back(&black_key_pressed, V3(0.0));  
    }

    int octave = 4;

    real light_x = 5.6;
    real light_y = 8.7;
    real light_z = 8.8;

    real spotlight_x = 6.0;
    real spotlight_y = 10.0;
    real spotlight_z = 0.0;

    bool autoplay = false;

    while (cow_begin_frame()) {
        camera_move(&camera);
        iTime += 0.0167;
        spotlight_x = (sin(iTime) * 6) + 6;
        spotlight_z = (sin(1.3 * iTime) * 3) - 1;

        // switching octaves
        if(globals.key_pressed['m'] && octave < 7) {
            octave += 1;
        }
        if (globals.key_pressed['n'] && octave > 0) {
            octave -= 1;
        }

        // switching instruments
        { if (globals.key_pressed['1']) {
            for ( i=0; i<6; i++ ) {
                data.voicer.removeInstrument(instrument[i]);
            }
            for ( i=0; i<6; i++ ) {
                instrument[i] = new Wurley();
            }
            for ( i=0; i<6; i++) {
                data.voicer.addInstrument( instrument[i] );
            }
        }
        if (globals.key_pressed['2']) {
            for ( i=0; i<6; i++ ) {
                data.voicer.removeInstrument(instrument[i]);
            }
            for ( i=0; i<6; i++ ) {
                instrument[i] = new Clarinet();
            }
            for ( i=0; i<6; i++) {
                data.voicer.addInstrument( instrument[i] );
            }
        }
        if (globals.key_pressed['3']) {
            for ( i=0; i<6; i++ ) {
                data.voicer.removeInstrument(instrument[i]);
            }
            for ( i=0; i<6; i++ ) {
                instrument[i] = new Mandolin(100);
            }
            for ( i=0; i<6; i++) {
                data.voicer.addInstrument( instrument[i] );
            }
        }
        if (globals.key_pressed['4']) {
            for ( i=0; i<6; i++ ) {
                data.voicer.removeInstrument(instrument[i]);
            }
            for ( i=0; i<6; i++ ) {
                instrument[i] = new Bowed();
            }
            for ( i=0; i<6; i++) {
                data.voicer.addInstrument( instrument[i] );
            }
        } }

        gui_checkbox("play autonomously", &autoplay);

        // generate music using pentatonic scale
        if (autoplay) {
            real val = random_real(0, 10000);
            if (val > 9500) {
                if (octave == 0) {
                    octave += 1;
                } else if (octave == 7) {
                    octave -= 1;
                } else {
                    octave += int(random_sign());
                }
            }
            if (val > 9000) {
                // stop all notes
                for (int i = 0; i < 52; i++) {
                    data.voicer.noteOff(tags[21 + i], 64);
                    white_key_pressed[i] = V3(0.0);
                }
            }
            if (val > 8800) {
                // play a note
                int note = floor(random_real(0, 7.99));
                if (!(note == 1 or note == 5)) {
                    tags[21 + note + (12 * octave)] = data.voicer.noteOn(21 + note + (12 * octave), 64);
                    white_key_pressed[note + (7 * octave)] = V3(1.0);
                }
            }
        }

        // react to key presses 
        {
        if (globals.key_pressed['a']) {
            tags[21 + 0 + (12 * octave)] = data.voicer.noteOn(21 + 0 + (12 * octave), 64);
            white_key_pressed[0 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['a']) {
            data.voicer.noteOff(tags[21 + 0 + (12 * octave)], 64);
            white_key_pressed[0 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['s']) {
            tags[21 + 2 + (12 * octave)] = data.voicer.noteOn(21 + 2 + (12 * octave), 64);
            white_key_pressed[1 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['s']) {
            data.voicer.noteOff(tags[21 + 2 + (12 * octave)], 64);
            white_key_pressed[1 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['d']) {
            tags[21 + 3 + (12 * octave)] = data.voicer.noteOn(21 + 3 + (12 * octave), 64);
            white_key_pressed[2 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['d']) {
            data.voicer.noteOff(tags[21 + 3 + (12 * octave)], 64);
            white_key_pressed[2 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['f'] && octave < 7) {
            tags[21 + 5 + (12 * octave)] = data.voicer.noteOn(21 + 5 + (12 * octave), 64);
            white_key_pressed[3 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['f'] && octave < 7) {
            data.voicer.noteOff(tags[21 + 5 + (12 * octave)], 64);
            white_key_pressed[3 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['g'] && octave < 7) {
            tags[21 + 7 + (12 * octave)] = data.voicer.noteOn(21 + 7 + (12 * octave), 64);
            white_key_pressed[4 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['g'] && octave < 7) {
            data.voicer.noteOff(tags[21 + 7 + (12 * octave)], 64);
            white_key_pressed[4 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['h'] && octave < 7) {
            tags[21 + 8 + (12 * octave)] = data.voicer.noteOn(21 + 8 + (12 * octave), 64);
            white_key_pressed[5 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['h'] && octave < 7) {
            data.voicer.noteOff(tags[21 + 8 + (12 * octave)], 64);
            white_key_pressed[5 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['j'] && octave < 7) {
            tags[21 + 10 + (12 * octave)] = data.voicer.noteOn(21 + 10 + (12 * octave), 64);
            white_key_pressed[6 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['j'] && octave < 7) {
            data.voicer.noteOff(tags[21 + 10 + (12 * octave)], 64);
            white_key_pressed[6 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['k'] && octave < 7) {
            tags[21 + 12 + (12 * octave)] = data.voicer.noteOn(21 + 12 + (12 * octave), 64);
            white_key_pressed[7 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['k'] && octave < 7) {
            data.voicer.noteOff(tags[21 + 12 + (12 * octave)], 64);
            white_key_pressed[7 + (7 * octave)] = V3(0.0);
        } 
        if (globals.key_pressed['w']) {
            tags[22 + 0 + (12 * octave)] = data.voicer.noteOn(22 + 0 + (12 * octave), 64);
            black_key_pressed[0 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['w']) {
            data.voicer.noteOff(tags[22 + 0 + (12 * octave)], 64);
            black_key_pressed[0 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['r'] && octave < 7) {
            tags[22 + 3 + (12 * octave)] = data.voicer.noteOn(22 + 3 + (12 * octave), 64);
            black_key_pressed[2 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['r'] && octave < 7) {
            data.voicer.noteOff(tags[22 + 3 + (12 * octave)], 64);
            black_key_pressed[2 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['t'] && octave < 7) {
            tags[22 + 5 + (12 * octave)] = data.voicer.noteOn(22 + 5 + (12 * octave), 64);
            black_key_pressed[3 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['t'] && octave < 7) {
            data.voicer.noteOff(tags[22 + 5 + (12 * octave)], 64);
            black_key_pressed[3 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['u'] && octave < 7) {
            tags[22 + 8 + (12 * octave)] = data.voicer.noteOn(22 + 8 + (12 * octave), 64);
            black_key_pressed[5 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['u'] && octave < 7) {
            data.voicer.noteOff(tags[22 + 8 + (12 * octave)], 64);
            black_key_pressed[5 + (7 * octave)] = V3(0.0);
        }
        if (globals.key_pressed['i'] && octave < 7) {
            tags[22 + 10 + (12 * octave)] = data.voicer.noteOn(22 + 10 + (12 * octave), 64);
            black_key_pressed[6 + (7 * octave)] = V3(1.0);
        }
        if (globals.key_released['i'] && octave < 7) {
            data.voicer.noteOff(tags[22 + 10 + (12 * octave)], 64);
            black_key_pressed[6 + (7 * octave)] = V3(0.0);
        } 
        }

        shader_set_uniform(&shader, "iTime", iTime);
        shader_set_uniform(&shader, "iResolution", window_get_size());
        shader_set_uniform(&shader, "C", camera_get_C(&camera));

        shader_set_uniform(&shader, "lightPos", V3(light_x, light_y, light_z));
        shader_set_uniform(&shader, "spotlightPos", V3(spotlight_x, spotlight_y, spotlight_z));

        shader_set_uniform(&shader, "white_key_pressed", white_key_pressed.length, white_key_pressed.data);
        shader_set_uniform(&shader, "black_key_pressed", black_key_pressed.length, black_key_pressed.data);
        shader_set_uniform(&shader, "octave", octave);

        shader_pass_vertex_attribute(&shader, mesh.num_vertices, mesh.vertex_positions);
        shader_draw(&shader, mesh.num_triangles, mesh.triangle_indices);
    }
}
  
int main() {
    _cow_init();
    APPS {
        APP(shader);
        APP(final_project);
        APP(sine);
        APP(synth);
    }
    return 0;
}





