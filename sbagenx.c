//
//	SBaGenX - Sequenced Brainwave Generator
//
//	Original version (c) 1999-2011 Jim Peters <jim@uazu.net>
//	This fork maintained by Ruan <https://ruan.sh/>
//
//	For latest version see http://sbagen.sf.net/ or
//	http://uazu.net/sbagen/. Released under the GNU GPL version 2.
//	Use at your own risk.
//
//	" This program is free software; you can redistribute it and/or modify
//	  it under the terms of the GNU General Public License as published by
//	  the Free Software Foundation, version 2.
//	  
//	  This program is distributed in the hope that it will be useful,
//	  but WITHOUT ANY WARRANTY; without even the implied warranty of
//	  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	  GNU General Public License for more details. "
//
//	See the file COPYING for details of this license.
//	
//	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
//
//	Some code fragments in the Win32 audio handling are based on
//	code from PLIB (c) 2001 by Steve Baker, originally released
//	under the LGPL (slDSP.cxx and sl.h).  For the original source,
//	see the PLIB project: http://plib.sf.net
//
//	The code for the Mac audio output was based on code from the
//	FINK project's patches to ESounD, by Shawn Hsiao and Masanori
//	Sekino.  See: http://fink.sf.net

#define VERSION __VERSION__

// This should be built with one of the following target macros
// defined, which selects options for that platform, or else with some
// of the individual named flags #defined as listed later.
//
//  T_LINUX	To build the LINUX version with ALSA support
//  T_MINGW	To build for Windows using MinGW and Win32 calls
//  T_MSVC	To build for Windows using MSVC and Win32 calls
//  T_MACOSX	To build for MacOSX using CoreAudio
//  T_POSIX	To build for simple file output on any Posix-compliant OS
//
// Ogg, MP3 and FLAC support are handled separately from the T_* macros.

// Define ALSA_AUDIO to use ALSA for audio output
// Define WIN_AUDIO to use Win32 calls
// Define MAC_AUDIO to use Mac CoreAudio calls
// Define NO_AUDIO if no audio output device is usable
// Define UNIX_TIME to use UNIX calls for getting time
// Define WIN_TIME to use Win32 calls for getting time
// Define ANSI_TTY to use ANSI sequences to clear/redraw lines
// Define UNIX_MISC to use UNIX calls for various miscellaneous things
// Define WIN_MISC to use Windows calls for various miscellaneous things
// Define EXIT_KEY to require the user to hit RETURN before exiting after error

// Define OGG_DECODE to include OGG support code
// Define MP3_DECODE to include MP3 support code
// Define FLAC_DECODE to include FLAC support code
//
// Output encoding (selected from -o filename extension):
//   *.mp3  -> MP3 (libmp3lame, dynamically loaded by default)
//   *.ogg  -> OGG/Vorbis (libsndfile, dynamically loaded by default)
//   *.flac -> FLAC (libsndfile, dynamically loaded by default)
// Define STATIC_OUTPUT_ENCODERS to link output encoders at build-time
// (used by Windows build script when static encoder libs are available)

#ifdef T_LINUX
#define ALSA_AUDIO
#define UNIX_TIME
#define UNIX_MISC
#define ANSI_TTY
#endif

#ifdef T_MINGW
#define WIN_AUDIO
#define WIN_TIME
#define WIN_MISC
#define EXIT_KEY
#endif

#ifdef T_MSVC
#define WIN_AUDIO
#define WIN_TIME
#define WIN_MISC
#define EXIT_KEY
#endif

#ifdef T_MACOSX
#define MAC_AUDIO
#define UNIX_TIME
#define UNIX_MISC
#define ANSI_TTY
#endif

#ifdef T_POSIX
#define NO_AUDIO
#define UNIX_TIME
#define UNIX_MISC
#endif

// Make sure NO_AUDIO is set if necessary
#ifndef MAC_AUDIO
#ifndef WIN_AUDIO
#ifndef ALSA_AUDIO
#define NO_AUDIO
#endif
#endif
#endif

// Make sure one of the _TIME macros is set
#ifndef UNIX_TIME
#ifndef WIN_TIME
#error UNIX_TIME or WIN_TIME not defined.  Maybe you did not define one of T_LINUX/T_MINGW/T_MACOSX/etc ?
#endif
#endif

// Make sure one of the _MISC macros is set
#ifndef UNIX_MISC
#ifndef WIN_MISC
#error UNIX_MISC or WIN_MISC not defined.  Maybe you did not define one of T_LINUX/T_MINGW/T_MACOSX/etc ?
#endif
#endif

#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef T_MSVC
 #include <io.h>
 #define write _write
 #define vsnprintf _vsnprintf
 typedef long long S64;		// I have no idea if this is correct for MSVC
#else
 #include <unistd.h>
 #include <sys/time.h>
 typedef long long S64;
#endif

#ifdef T_MINGW
 #define vsnprintf _vsnprintf
#endif

#ifdef T_MACOSX
 #include <mach-o/dyld.h>
#endif

#ifdef ALSA_AUDIO
 #include <alsa/asoundlib.h>
#endif
#ifdef WIN_AUDIO
 #include <windows.h>
 #include <mmsystem.h>
#endif
#ifdef MAC_AUDIO
#include <CoreAudio/CoreAudio.h>
#endif
#ifdef UNIX_TIME
 #include <sys/ioctl.h>
 #include <sys/times.h>
#endif
#ifdef UNIX_MISC
 #include <pthread.h>
 #include <dlfcn.h>
#endif
#include "libs/sndfile.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"
#include "libs/tinyexpr.h"
#include "sbagenxlib.h"
#include "sbagenxlib_dsp.h"

typedef struct Channel Channel;
typedef struct Voice Voice;
typedef struct Period Period;
typedef struct NameDef NameDef;
typedef struct BlockDef BlockDef;
typedef unsigned char uchar;

int inbuf_loop(void *vp) ;
int inbuf_read(int *dst, int dlen) ;
void inbuf_start(int(*rout)(int*,int), int len) ;
int t_per24(int t0, int t1) ;
int t_per0(int t0, int t1) ;
int t_mid(int t0, int t1) ;
int main(int argc, char **argv) ;
void status(char *) ;
void statusSbx(char *) ;
void dispCurrPer( FILE* ) ;
void init_sin_table() ;
void debug(char *fmt, ...) ;
void warn(char *fmt, ...) ;
void * Alloc(size_t len) ;
char * StrDup(char *str) ;
int calcNow() ;
void loop() ;
void outChunk() ;
void outChunkSbx() ;
void corrVal(int ) ;
int readLine() ;
char * getWord() ;
void badSeq() ;
int sbx_try_readSeqImm_runtime(int ac, char **av);
int sbx_try_readSeq_runtime(int ac, char **av);
void readSeqImm(int ac, char **av) ;
void readSeq(int ac, char **av) ;
void readPreProg(int ac, char **av) ;
void correctPeriods();
void setup_device(void) ;
void readNameDef();
void readTimeLine();
int voicesEq(Voice *, Voice *);
void error(char *fmt, ...) ;
int sprintTime(char *, int);
int sprintVoice(char *, Voice *, Voice *);
int readTime(char *, int *);
void writeWAV();
void writeOut(char *, int);
void sinc_interpolate(double *, int, int *);
inline int userTime();
void find_wav_data_start(FILE *in);
int raw_mix_in(int *dst, int dlen);
int scanOptions(int *acp, char ***avp);
void handleOptions(char *p);
void setupOptC(char *spec) ;
extern int out_rate, out_rate_def;
void create_drop(int ac, char **av);
void create_sigmoid(int ac, char **av);
void create_curve(int ac, char **av);
void create_slide(int ac, char **av);
void create_libseq(int ac, char **av, int sbg_timing);
int is_mix_mod_option_spec(const char *spec);
void parse_mix_mod_option_spec(const char *spec);
int is_iso_gate_option_spec(const char *spec);
void parse_iso_gate_option_spec(const char *spec);
int is_mixam_env_option_spec(const char *spec);
void parse_mixam_env_option_spec(const char *spec);
void clear_mix_mod_curve(void);
void setup_mix_mod_curve(double delta, double epsilon, double k_min, double end_level,
			 double main_len_min, double wake_len_min, int wake_enabled);
double mix_mod_multiplier(int now_ms);
void normalizeAmplitude(Voice *voices, int numChannels, const char *line, int lineNum);
void printSequenceDuration();
void checkMixInSequence(); // Check if mix/<amp> is specified
void create_noise_spin_effect(
  int typ,
  int amp,
  int spin_position,
  int *left,
  int *right
); // Create a spin effect
void init_mixbeat_hilbert(void);
void reset_mixbeat_channel_state(int ch_index);
double mixbeat_hilbert_sample(int ch_index, double x);
void detect_output_encoder();
void init_output_encoder();
void output_encoder_write(short *pcm, int frames);
void finish_output_encoder();
void write_sigmoid_graph_png(const char *fmt, double level,
			     char depth_ch, int len0, int len1, int len2,
			     double beat_start, double beat_target,
			     double sig_l, double sig_h,
			     double sig_a, double sig_b);
void write_drop_graph_png(const char *fmt, double level,
			  char depth_ch, int len0, int len1, int len2,
			  double beat_start, double beat_target,
			  int slide, int n_step, int steplen,
			  int isisochronic, int ismono);
void write_curve_graph_png(const char *fmt, double level,
			   char depth_ch, int len0, int len1, int len2,
			   int slide, int n_step, int steplen,
			   int isisochronic, int ismono,
			   double beat_start, double beat_target);
void write_iso_cycle_graph_png_from_sequence(void);
void write_mixam_cycle_graph_png(void);
int try_external_sigmoid_graph_png(const char *out_fname,
				   int len0_sec,
				   double beat_start, double beat_target,
				   double sig_l, double sig_h,
				   double sig_a, double sig_b);
int try_external_drop_graph_png(const char *out_fname,
				int len0_sec,
				double beat_start, double beat_target,
				int slide, int n_step, int steplen_sec,
				int mode_kind);
int try_external_curve_graph_png(const char *out_fname,
				 int len0_sec,
				 double beat_start, double beat_target,
				 int mode_kind,
				 int slide, int n_step, int steplen_sec,
				 const char *sample_file);
int try_external_iso_cycle_graph_png(const char *out_fname,
				     double carr_hz, double pulse_hz,
				     double amp_pct, int waveform);
int try_external_mixam_cycle_graph_png(const char *out_fname,
				       int m, double s, double d, double a, double r,
				       int e, double f);
static void emit_periods_from_sbx_context(SbxContext *ctx, int loop_requested);

#define ALLOC_ARR(cnt, type) ((type*)Alloc((cnt) * sizeof(type)))
#define uint unsigned int

#ifdef OGG_DECODE
#include "oggdec.c"
#endif
#ifdef MP3_DECODE
#include "mp3dec.c"
#endif
#ifdef FLAC_DECODE
#include "flacdec.c"
#endif

// Tiny expression parser/evaluator used for .sbgf custom curves.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
#include "libs/tinyexpr.c"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#ifdef WIN_AUDIO
void CALLBACK win32_audio_callback(HWAVEOUT, UINT, DWORD, DWORD, DWORD);
#endif
#ifdef MAC_AUDIO
OSStatus mac_callback(AudioDeviceID, const AudioTimeStamp *, const AudioBufferList *, 
		      const AudioTimeStamp *, AudioBufferList *, const AudioTimeStamp *, 
		      void *inClientData);

void init_mac_audio();
#endif

#define NL "\n"

void 
help() {
   printf("SBaGenX - Sequenced Brainwave Generator, version " VERSION
     NL "Original SBaGen version (c) 1999-2011 Jim Peters, http://uazu.net/"
     NL "SBaGen+ fork by Ruan Klein, https://ruan.sh/"
     NL "This fork (SBaGenX) maintained by Lech Madrzyk, https://www.sbagenx.com/"
		  NL "Released under the GNU GPL v2. See file COPYING."
	  NL 
	  NL "Usage: sbagenx [options] seq-file ..."
	  NL "       sbagenx [options] -i tone-specs ..."
	  NL "       sbagenx [options] -p pre-programmed-sequence-specs ..."
	  NL
	  NL "Options:  -h        Display this help-text"
	  NL "          -Q        Quiet - don't display running status"
	  NL "          -D        Display the full interpreted sequence instead of playing it"
	  NL "          -P        Plot one-cycle envelope/waveform PNG and exit"
	  NL "                     With -H: plots mixam envelope/gain cycle"
	  NL "                     With -p ... and mixam:<...> extras: plots mixam cycle"
	  NL "                     Otherwise: plots one-cycle isochronic waveform"
	  NL "                     Compatibility: with -p drop/-p sigmoid/-p curve and no"
	  NL "                     cycle target, falls back to program beat/pulse graph"
	  NL "          -G        Plot program beat/pulse-vs-time PNG and exit"
	  NL "                     Supported with -p drop/-p sigmoid/-p curve"
	  NL "          -i        Immediate.  Take the remainder of the command line to be"
	  NL "                     tone-specifications, and play them continuously"
	  NL "          -p        Pre-programmed sequence.  Take the remainder of the command"
	  NL "                     line to be a type and arguments, e.g. \"drop 00ds+\""
	  NL "                     or \"curve path/to/file.sbgf 00ls:l=0.2:h=0\""
	  NL "                     or \"libseq file.sbxseq [loop]\""
	  NL "                     or \"libsbg file.sbg [loop]\""
	  NL "          -q mult   Quick.  Run through quickly (real time x 'mult') from the"
	  NL "                     start time, rather than wait for real time to pass"
	  NL
	  NL "          -r rate   Select the output rate (default is 44100 Hz, or from -m)"
#ifndef MAC_AUDIO
	  NL "          -b bits   Select the number bits for output (8 or 16, default 16)"
#else
	  NL "          -B size   Force buffer size (in samples) for audio output."
    NL "                     (e.g. 1024, 2048, 4096, etc.)"
#endif
	  NL "          -N        Disable automatic amplitude normalization (allow clipping)"
	  NL "          -V        Set the global volume level (Min 1, Max 100. Default 100)"
    NL "          -w type   Set the global waveform type (sine, square, triangle, sawtooth; default sine)"
	  NL "          -L time   Select the length of time (hh:mm or hh:mm:ss) to output"
	  NL "                     for.  Default is to output forever."
	  NL "          -S        Output from the first tone-set in the sequence (Start),"
	  NL "                     instead of working in real-time.  Equivalent to '-q 1'."
	  NL "          -E        Output until the last tone-set in the sequence (End),"
	  NL "                     instead of outputting forever."
	  NL "          -T time   Start at the given clock-time (hh:mm)"
	  NL
		  NL "          -o file   Output raw data to the given file instead of default device"
		  NL "                     (or MP3/OGG/FLAC if file extension is .mp3/.ogg/.flac)"
		  NL "          -K kbps   MP3 bitrate in kbps (8-320, default 192)"
		  NL "          -J q      MP3 quality (0-9, lower is better, default 2)"
		  NL "          -X q      MP3 VBR quality (0-9, lower is better; enables VBR)"
		  NL "          -U q      OGG Vorbis quality (0-10, default 6)"
		  NL "          -Z level  FLAC compression level (0-12, default library setting)"
		  NL "          -O        Output raw data to the standard output"
	  NL "          -W        Output a WAV-format file instead of raw data"
	  NL "          -m file   Read audio data from the given file and mix it with"
	  NL "                      generated brainwave tones; may be "
#ifdef OGG_DECODE
	  "ogg/"
#endif
#ifdef MP3_DECODE
		  "mp3/"
#endif
#ifdef FLAC_DECODE
		  "flac/"
#endif
		  "wav/raw format"
	  NL "          -M        Read raw audio data from the standard input and mix it"
	  NL "                      with the generated brainwave tones (raw only)"
	  NL "          -A [spec] Enable mix amplitude modulation for -p drop/-p sigmoid"
	  NL "                      with mix input; spec is d=<v>:e=<v>:k=<v>:E=<v>"
	  NL "                      defaults: d=0.3:e=0.3:k=10:E=0.7"
	  NL "          -I [spec] Customize isochronic (@) pulse envelope; spec is"
	  NL "                      s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>"
	  NL "                      defaults s=0.0485:d=0.4030:a=0.5:r=0.5:e=2"
	  NL "          -H [spec] Customize global mixam envelope for mixam:<pulse|beat>"
	  NL "                      spec is m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>"
	  NL "                      order-independent; omitted params use defaults"
	  NL "                      defaults m=cos:s=0:d=0.5:a=0.5:r=0.5:e=3:f=0.45"
	  NL "                      if d/a/r/e are set without m=..., pulse mode is implied"
	  NL "                      in m=cos mode, d/a/r/e are ignored; s/f apply"
	  NL
	  NL "          -R rate   Select rate in Hz that frequency changes are recalculated"
	  NL "                     (for file/pipe output only, default is 10Hz)"
	  NL "          -F fms    Fade in/out time in ms (default 60000ms, or 1min)"
#ifdef ALSA_AUDIO
	  NL "          -d dev    Select a different ALSA device instead of 'default'"
#endif
	  NL "          -c spec   Compensate for low-frequency headphone roll-off; see docs"
	  NL
	  NL "Environment:"
	  NL "          SBAGENX_SEQ_BACKEND=auto|legacy|sbagenxlib"
	  NL "                     Select seq-file backend (default auto)."
	  NL "                     auto: try sbagenxlib subset then fallback to legacy;"
	  NL "                     legacy: force legacy parser/runtime;"
	  NL "                     sbagenxlib: require subset compatibility."
	  NL "                     (alias: sbagenlib)"
	  NL
	  );
   exit(0);
}

void 
usage() {
  error("SBaGenX - Sequenced Brainwave Generator, version " VERSION 
		NL "Original SBaGen version (c) 1999-2011 Jim Peters, http://uazu.net/"
		NL "SBaGen+ fork by Ruan Klein, https://ruan.sh/"
		NL "This fork (SBaGenX) maintained by Lech Madrzyk, https://www.sbagenx.com/"
		NL "Released under the GNU GPL v2. See file COPYING."
	NL 
		NL "Usage: sbagenx [options] seq-file ..."
		NL "       sbagenx [options] -i tone-specs ..."
		NL "       sbagenx [options] -p pre-programmed-sequence-specs ..."
		NL
		NL "SBaGenX is a fork of SBaGen+, which is a fork of the original SBaGen."
		NL "Use inline 'M' (where '@' is used) for monaural beats."
		NL "For full usage help, type 'sbagenx -h'."
#ifdef EXIT_KEY
	NL
	NL "Windows users please note that this utility is designed to be run as the"
	NL "associated application for SBG files.  This should have been set up for you by"
	NL "the installer.  You can run all the SBG files directly from the desktop by"
	NL "double-clicking on them, and edit them using NotePad from the right-click menu."
	NL "Alternatively, SBaGenX may be run from the command line, or from"
	NL "BAT/PS1 files.  SBaGenX is powerful software -- it is worth the effort of figuring"
	NL "all this out.  See SBAGENX.TXT for the full documentation."
#endif
	NL);
}


#define DEBUG_CHK_UTIME 0	// Check how much user time is being consumed
#define DEBUG_DUMP_WAVES 0	// Dump out wave tables (to plot with gnuplot)
#define DEBUG_DUMP_AMP 0	// Dump output amplitude to stdout per chunk
#define N_CH 16			// Number of channels

struct Voice {
  int typ;			// Voice type: 0 off, 1 binaural, 2 pink noise, 3 bell, 4 spin, 5 mix, 6 mixspin, 7 mixpulse, 8 isochronic, 9 white noise, 10 brown noise, 11 bspin, 12 wspin, 13 mixbeat, -1 to -100 wave00 to wave99
  double amp;			// Amplitude level (0-4096 for 0-100%)
  double carr;			// Carrier freq (for binaural/bell/isochronic), width (for spin)
  double res;			// Resonance freq (-ve or +ve) (for binaural/spin/isochronic)
  int waveform; // 0=sine, 1=square, 2=triangle, 3=sawtooth
};

struct Channel {
  Voice v;			// Current voice setting (updated from current period)
  int typ;			// Current type: 0 off, 1 binaural, 2 pink noise, 3 bell, 4 spin, 5 mix, 6 mixspin, 7 mixpulse, 8 isochronic, 9 white noise, 10 brown noise, 11 bspin, 12 wspin, 13 mixbeat, -1 to -100 wave00 to wave99
  int amp, amp2;		// Current state, according to current type
  int inc1, off1;		//  ::  (for binaural tones, offset + increment into sine 
  int inc2, off2;		//  ::   table * 65536)
  int inc3, off3;		//  ::   extra phase for custom waveform envelope
};

struct Period {
  Period *nxt, *prv;		// Next/prev in chain
  int tim;			// Start time (end time is ->nxt->tim)
  Voice v0[N_CH], v1[N_CH];	// Start and end voices
  int fi, fo;			// Temporary: Fade-in, fade-out modes
};

struct NameDef {
  NameDef *nxt;
  char *name;			// Name of definition
  BlockDef *blk;		// Non-zero for block definition
  Voice vv[N_CH];		// Voice-set for it (unless a block definition)
};

struct BlockDef {
  BlockDef *nxt;		// Next in chain
  char *lin;			// StrDup'd line
};

#define ST_AMP 0x7FFFF		// Amplitude of wave in sine-table
#define NS_ADJ 12		// Noise is generated internally with amplitude ST_AMP<<NS_ADJ
#define NS_DITHER 16		// How many bits right to shift the noise for dithering
#define NS_AMP (ST_AMP<<NS_ADJ)
#define ST_SIZ 16384		// Number of elements in sine-table (power of 2)
// int *sin_table;
int *sin_tables[4]; // 0=sine, 1=square, 2=triangle, 3=sawtooth
#define AMP_DA(pc) (40.96 * (pc))	// Display value (%age) to ->amp value
#define AMP_AD(amp) ((amp) / 40.96)	// Amplitude value to display %age
int *waves[100];		// Pointers are either 0 or point to a sin_table[]-style array of int
#define MIXBEAT_HILBERT_TAPS 31
double mixbeat_hilbert_coeff[MIXBEAT_HILBERT_TAPS];
double mixbeat_hist[N_CH][MIXBEAT_HILBERT_TAPS];
int mixbeat_hist_pos[N_CH];

Channel chan[N_CH];		// Current channel states
int now;			// Current time (milliseconds from midnight)
Period *per= 0;			// Current period
NameDef *nlist;			// Full list of name definitions

int *tmp_buf;			// Temporary buffer for 20-bit mix values
short *out_buf;			// Output buffer
int out_bsiz;			// Output buffer size (bytes)
int out_blen;			// Output buffer length (samples) (1.0* or 0.5* out_bsiz)
int out_bps;			// Output bytes per sample (2 or 4)
int out_buf_ms;			// Time to output a buffer-ful in ms
int out_buf_lo;			// Time to output a buffer-ful, fine-tuning in ms/0x10000
int out_fd;			// Output file descriptor
int out_rate= 44100;		// Sample rate
int out_rate_def= 1;		// Sample rate is default value, not set by user
int out_mode= 1;		// Output mode: 0 unsigned char[2], 1 short[2], 2 swapped short[2]
int out_prate= 10;		// Rate of parameter change (for file and pipe output only)
int fade_int= 60000;		// Fade interval (ms)
FILE *in;			// Input sequence file
int in_lin;			// Current input line
char buf[4096];			// Buffer for current line
char buf_copy[4096];		// Used to keep unmodified copy of line
char *lin;			// Input line (uses buf[])
char *lin_copy;			// Copy of input line
double spin_carr_max;		// Maximum 'carrier' value for spin (really max width in us)

#define NS_BIT 10
int ns_tbl[1<<NS_BIT];
int ns_off= 0;

int fast_tim0= -1;		// First time mentioned in the sequence file (for -q and -S option)
int fast_tim1= -1;		// Last time mentioned in the sequence file (for -E option)
int fast_mult= 0;		// 0 to sync to clock (adjusting as necessary), or else sync to
				//  output rate, with the multiplier indicated
S64 byte_count= -1;		// Number of bytes left to output, or -1 if unlimited
int tty_erase;			// Chars to erase from current line (for ESC[K emulation)

int opt_Q;			// Quiet mode
int opt_D;
int opt_G;			// Program beat/pulse graph flag (-G)
int opt_P;			// Cycle plot flag (-P)
int opt_P_sigmoid;		// Program graph requested for -p sigmoid
int opt_P_drop;		// Program graph requested for -p drop
int opt_P_curve;		// Program graph requested for -p curve
int opt_M, opt_S, opt_E;
char *opt_o, *opt_m;
int opt_O;
int opt_W;
#ifdef ALSA_AUDIO
char *opt_d= "default";	// Output device to ALSA
#endif
int opt_L= -1;			// Length of WAV file in ms
int opt_T= -1;			// Time to start at (for -S option)
#ifdef MAC_AUDIO
int opt_B= -1;		// Buffer size override (-1 = auto)
#endif
int opt_N= 1;			// Enable automatic amplitude normalization (default)
int opt_V= 100;			// Global volume level (default 100%)
int opt_w= 0;			// Waveform type (0 = sine, 1 = square, 2 = triangle, 3 = sawtooth)
int opt_A;			// Enable mix amplitude modulation for pre-programmed sequences
double opt_A_d= 0.3;		// d parameter for mix modulation
double opt_A_e= 0.3;		// e parameter for mix modulation
double opt_A_k= 10.0;		// k parameter (minutes) for mix modulation
double opt_A_E= 0.7;		// E parameter for mix modulation
int opt_I;			// Enable custom isochronic gate for @ tones
double opt_I_s= 0.048493;	// Gate start as cycle proportion (legacy-equivalent default)
double opt_I_d= 0.403014;	// Gate duty as cycle proportion (legacy-equivalent default)
double opt_I_a= 0.5;		// Attack as fraction of ON window
double opt_I_r= 0.5;		// Release as fraction of ON window
int opt_I_e= 2;			// Edge shape: 0 hard, 1 linear, 2 smoothstep, 3 smootherstep
int opt_H;			// Enable global mixam envelope override
int opt_H_m= SBX_MIXAM_MODE_COS;	// mixam model: 0 pulse, 1 raised-cosine
double opt_H_s= 0.0;		// mixam start-cycle (phase offset)
double opt_H_d= 0.5;		// mixam duty
double opt_H_a= 0.5;		// mixam attack share
double opt_H_r= 0.5;		// mixam release share
int opt_H_e= 3;			// mixam edge shape: 0 hard, 1 linear, 2 smoothstep, 3 smootherstep
double opt_H_f= 0.45;		// mixam floor gain
char *waveform_name[] = {"sine", "square", "triangle", "sawtooth"}; // To be used for messages

FILE *mix_in;			// Input stream for mix sound data, or 0
int mix_cnt;			// Version number from mix filename (#<digits>), or -1
int bigendian;			// Is this platform Big-endian?
int mix_flag= 0;		// Has 'mix/*' been used in the sequence?
double *mix_amp= NULL; // Amplitude of mix sound data to use with mixspin/mixpulse/mixbeat. Default is 100%
double mix_amp_current= 4096.0; // Current mix/<amp> value (0-4096)

typedef struct {
   int active;
   double delta;		// d
   double epsilon;		// e
   double period_min;		// k (minutes)
   double end_level;		// E
   double main_len_min;		// T, includes hold if present, excludes wake
   double wake_len_min;		// U
   int wake_enabled;
} MixModCurve;
MixModCurve mix_mod_curve;

typedef enum {
   OUT_ENC_NONE= 0,
   OUT_ENC_MP3,
   OUT_ENC_OGG,
   OUT_ENC_FLAC
} OutEncFmt;

OutEncFmt out_enc_fmt= OUT_ENC_NONE;
int out_enc_active= 0;
int out_enc_finished= 0;
int out_enc_atexit= 0;
int opt_mp3_bitrate= 192;	// MP3 CBR bitrate in kbps
int opt_mp3_quality= 2;		// LAME quality (0 best .. 9 fastest)
int opt_mp3_bitrate_set;
int opt_mp3_quality_set;
double opt_mp3_vbr_quality= 0.0;	// LAME VBR quality scale 0..9 (lower is better)
int opt_mp3_vbr_quality_set;
double opt_ogg_quality= 6.0;	// Vorbis quality scale 0..10
int opt_ogg_quality_set;
double opt_flac_compression= 0.0; // FLAC compression level scale 0..12
int opt_flac_compression_set;

#ifdef WIN_MISC
typedef HMODULE DLibHandle;
#else
typedef void *DLibHandle;
#endif

typedef struct {
   DLibHandle lib;
   SNDFILE *snd;
   SNDFILE *(*sf_open_fn)(const char*, int, SF_INFO*);
   sf_count_t (*sf_writef_short_fn)(SNDFILE*, const short*, sf_count_t);
   int (*sf_close_fn)(SNDFILE*);
   const char *(*sf_strerror_fn)(SNDFILE*);
   int (*sf_command_fn)(SNDFILE*, int, void*, int);
} SndEncState;
SndEncState snd_enc;

typedef struct lame_global_flags lame_global_flags;
typedef lame_global_flags *lame_t;

#ifdef STATIC_OUTPUT_ENCODERS
extern lame_t lame_init(void);
extern int lame_set_in_samplerate(lame_t gfp, int in_samplerate);
extern int lame_set_num_channels(lame_t gfp, int num_channels);
extern int lame_set_quality(lame_t gfp, int quality);
extern int lame_set_VBR(lame_t gfp, int vbr_mode);
extern int lame_set_VBR_q(lame_t gfp, int quality);
extern int lame_set_VBR_quality(lame_t gfp, float quality);
extern int lame_set_brate(lame_t gfp, int bitrate);
extern int lame_init_params(lame_t gfp);
extern int lame_encode_buffer_interleaved(lame_t gfp, short int pcm[], int num_samples,
					  unsigned char *mp3buf, int mp3buf_size);
extern int lame_encode_flush(lame_t gfp, unsigned char *mp3buf, int size);
extern int lame_close(lame_t gfp);
#endif

typedef struct {
   DLibHandle lib;
   lame_t gfp;
   lame_t (*lame_init_fn)(void);
   int (*lame_set_in_samplerate_fn)(lame_t, int);
   int (*lame_set_num_channels_fn)(lame_t, int);
   int (*lame_set_quality_fn)(lame_t, int);
   int (*lame_set_VBR_fn)(lame_t, int);
   int (*lame_set_VBR_q_fn)(lame_t, int);
   int (*lame_set_VBR_quality_fn)(lame_t, float);
   int (*lame_set_brate_fn)(lame_t, int);
   int (*lame_init_params_fn)(lame_t);
   int (*lame_encode_buffer_interleaved_fn)(lame_t, short int*, int, unsigned char*, int);
   int (*lame_encode_flush_fn)(lame_t, unsigned char*, int);
   int (*lame_close_fn)(lame_t);
   unsigned char *buf;
   int buflen;
} Mp3EncState;
Mp3EncState mp3_enc;

#define CURVE_MAX_PARAMS 32
#define CURVE_MAX_PIECES 64
#define CURVE_NAME_MAX 64
#define CURVE_EXPR_MAX 1024
#define CURVE_FILE_MAX 1024
#define CURVE_MAX_SOLVE_UNK 8
#define CURVE_MAX_SOLVE_EQ CURVE_MAX_SOLVE_UNK

typedef struct {
   int active;			// Function-driven modulation enabled?
   int mode;			// 1 exponential drop, 2 sigmoid, 3 custom expression
   int chan;			// Channel index to apply to
   int chan2;			// Optional second channel (used for monaural twin)
   int typ;			// Voice type to apply to (1 binaural, 8 isochronic)
   int monaural;		// 1 => chan/chan2 are f1/f2 mono components
   int start_ms;		// Start time (ms from midnight)
   int end_ms;			// End time for function-driven carrier path
   double carr0, carr1;	// Carrier start/end (Hz)
   double carr_span_s;		// Carrier transition time in seconds
   double beat0, beat1;	// Beat/pulse start/end (Hz)
   double beat_span_s;		// Beat transition time in seconds
   double beat_log_ratio;	// Precomputed log(beat1/beat0)
   double sig_a, sig_b;	// Sigmoid coefficients (beat = a*tanh(..)+b)
   double sig_l, sig_h;	// Sigmoid shape parameters
   double sig_d_min;		// Sigmoid drop duration (minutes)
   double beat_amp0_pct;	// Base beat/iso/mono amplitude (%)
   double mix_amp0_pct;	// Base mix amplitude (%) from mix/<amp>
   char src_file[CURVE_FILE_MAX];	// Source .sbgf file for mode 3
   char beat_expr_src[CURVE_EXPR_MAX];
   char carr_expr_src[CURVE_EXPR_MAX];
   char amp_expr_src[CURVE_EXPR_MAX];
   char mixamp_expr_src[CURVE_EXPR_MAX];
   int have_carr_expr;
   int have_amp_expr;
   int have_mixamp_expr;
   int beat_piece_count;
   int carr_piece_count;
   int amp_piece_count;
   int mixamp_piece_count;
   char beat_piece_cond_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   char beat_piece_expr_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   char carr_piece_cond_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   char carr_piece_expr_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   char amp_piece_cond_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   char amp_piece_expr_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   char mixamp_piece_cond_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   char mixamp_piece_expr_src[CURVE_MAX_PIECES][CURVE_EXPR_MAX];
   int have_solve;
   int solve_unknown_count;
   int solve_eq_count;
   char solve_unknown_names[CURVE_MAX_SOLVE_UNK][CURVE_NAME_MAX];
   int solve_unknown_param_idx[CURVE_MAX_SOLVE_UNK];
   char solve_eq_lhs_src[CURVE_MAX_SOLVE_EQ][CURVE_EXPR_MAX];
   char solve_eq_rhs_src[CURVE_MAX_SOLVE_EQ][CURVE_EXPR_MAX];
   int param_count;
   char param_names[CURVE_MAX_PARAMS][CURVE_NAME_MAX];
   double param_values[CURVE_MAX_PARAMS];
   te_expr *beat_expr;
   te_expr *carr_expr;
   te_expr *amp_expr;
   te_expr *mixamp_expr;
   te_expr *beat_piece_cond[CURVE_MAX_PIECES];
   te_expr *beat_piece_expr[CURVE_MAX_PIECES];
   te_expr *carr_piece_cond[CURVE_MAX_PIECES];
   te_expr *carr_piece_expr[CURVE_MAX_PIECES];
   te_expr *amp_piece_cond[CURVE_MAX_PIECES];
   te_expr *amp_piece_expr[CURVE_MAX_PIECES];
   te_expr *mixamp_piece_cond[CURVE_MAX_PIECES];
   te_expr *mixamp_piece_expr[CURVE_MAX_PIECES];
   double ev_t, ev_m;		// Runtime variables for expression evaluation
   double ev_D, ev_H, ev_T, ev_U;
   double ev_b0, ev_b1, ev_c0, ev_c1;
   double ev_a0, ev_m0;
} FuncCurve;
FuncCurve func_curve;		// Runtime function curve for pre-programmed sequences

typedef struct {
   int have_mix;
   double mix_amp_pct;
   SbxToneSpec aux_tones[SBX_MAX_AUX_TONES];
   size_t aux_count;
   SbxMixFxSpec mix_fx[SBX_MAX_AUX_TONES];
   size_t mix_fx_count;
   int unsupported;
   char bad_token[128];
} SbxRuntimeExtraSpec;

static void sbx_runtime_clear(void);
static void sbx_runtime_activate_from_keyframes(const SbxProgramKeyframe *kfs, size_t n, int loop_flag,
						double mix_amp_pct,
						const SbxToneSpec *aux_tones, size_t aux_count,
						const SbxMixFxSpec *mix_fx, size_t mix_fx_count,
						const SbxMixAmpKeyframe *mkf, size_t mkf_n);
static int sbx_parse_runtime_extra_tokens(const char *extra, SbxRuntimeExtraSpec *spec);

int sbx_runtime_active= 0;
double sbx_runtime_total_sec= 0.0;
SbxContext *sbx_runtime_ctx= 0;
float *sbx_runtime_fbuf= 0;
size_t sbx_runtime_fcap= 0;

int opt_c;			// Number of -c option points provided (max 16)
struct AmpAdj { 
   double freq, adj;
} ampadj[16];			// List of maximum 16 (freq,adj) pairs, freq-increasing order

char *pdir;			// Program directory (used as second place to look for -m files)

#ifdef WIN_AUDIO
 #define BUFFER_COUNT 8
 #define BUFFER_SIZE 8192*4
 HWAVEOUT aud_handle;
 WAVEHDR *aud_head[BUFFER_COUNT];
 int aud_current;		// Current header
 int aud_cnt;			// Number of headers in use
#endif

#ifdef MAC_AUDIO
 #define BUFFER_COUNT 8
 #define BUFFER_SIZE 4096*4
 char *aud_buf[BUFFER_COUNT];
 int aud_rd;	// Next buffer to read out of list (to send to device)
 int aud_wr;	// Next buffer to write.  aud_rd==aud_wr means empty buffer list
 static AudioDeviceID aud_dev;
 static AudioDeviceIOProcID proc_id; // New: store the procedure ID
#endif

//
//	Delay for a short period of time (in ms)
//

#ifdef UNIX_MISC
void 
delay(int ms) {
   struct timespec ts;
   ts.tv_sec= ms / 1000;
   ts.tv_nsec= (ms % 1000) * 1000000;
   nanosleep(&ts, 0);
}
#endif
#ifdef WIN_MISC
void 
delay(int ms) {
   Sleep(ms);
}
#endif


//
//	WAV/OGG/MP3 input data buffering
//

int *inbuf;		// Buffer for input data (as 20-bit samples)
int ib_len;		// Length of input buffer (in ints)
volatile int ib_rd;	// Read-offset in inbuf
volatile int ib_wr;	// Write-offset in inbuf
volatile int ib_eof;	// End of file flag
int ib_cycle= 100;	// Time in ms for a complete loop through the buffer
int (*ib_read)(int*,int);  // Routine to refill buffer

int 
inbuf_loop(void *vp) {
   int now= -1;
   int waited= 0;	// Used to bail out if the main thread dies for some reason
   int a;

   while (1) {
      int rv;
      int rd= ib_rd;
      int wr= ib_wr;
      int cnt= (rd-1-wr) & (ib_len-1);
      if (cnt > ib_len-wr) cnt= ib_len-wr;
      if (cnt > ib_len/8) cnt= ib_len/8;

      // Choose to only work in ib_len/8 units, although this is not
      // 100% necessary
      if (cnt < ib_len/8) {
	 // Wait a little while for the buffer to empty (minimum 1ms)
	 if (waited > 10000 + ib_cycle)
	    error("Mix stream halted for more than 10 seconds; aborting");
	 delay(a= 1+ib_cycle/4);
	 waited += a;
	 continue;
      }
      waited= 0;
      
      rv= ib_read(inbuf+wr, cnt);
      //debug("ib_read %d-%d (%d) -> %d", wr, wr+cnt-1, cnt, rv);
      if (rv != cnt) {
	 ib_eof= 1;
	 return 0;
      }

      ib_wr= (wr + rv) & (ib_len-1);

      // Whenever we roll over, recalculate 'ib_cycle'
      if (ib_wr < wr) {
	 int prev= now;
	 now= calcNow();
	 if (prev >= 0 && now > prev)
	    ib_cycle= now - prev;
	 //debug("Input buffer cycle duration is now %dms", ib_cycle);
      }
   }
   return 0;
}

//
//	Read a chunk of int data from the input buffer.  This will
//	always return enough data unless we have hit the end of the
//	file, in which case it returns a lower number or 0.  If not
//	enough data has been read by the input thread, then this
//	thread pauses until data is ready -- but this should hopefully
//	never happen.
//

int 
inbuf_read(int *dst, int dlen) {
   int rv= 0;
   int waited= 0;	// As a precaution, bail out if other thread hangs for some reason
   int a;

   while (dlen > 0) {
      int rd= ib_rd;
      int wr= ib_wr;
      int avail= (wr-rd) & (ib_len-1);
      int toend= ib_len-rd;
      if (avail > toend) avail= toend;
      if (avail > dlen) avail= dlen;

      if (avail == 0) {
	 if (ib_eof) return rv;

	 // Necessary to wait for incoming mix data.  This should
	 // never happen in normal running, though, unless we are
	 // outputting to a file
	 if (waited > 10000) 
	    error("Mix stream problem; waited more than 10 seconds for data; aborting");
	 //debug("Waiting for input thread (%d)", ib_eof);
	 delay(a= ib_cycle/4 > 100 ? 100 : 1+ib_cycle/4);
	 waited += a;
	 continue;
      }
      waited= 0;
      
      memcpy(dst, inbuf+rd, avail * sizeof(int));
      dst += avail;
      dlen -= avail;
      rv += avail;
      ib_rd= (rd + avail) & (ib_len-1);
   }
   return rv;
}

//
//	Start off the thread that fills the buffer
//

void 
inbuf_start(int(*rout)(int*,int), int len) {
   if (0 != (len & (len-1)))
      error("inbuf_start() called with length not a power of two");

   ib_read= rout;
   ib_len= len;
   inbuf= ALLOC_ARR(ib_len, int);
   ib_rd= 0;
   ib_wr= 0;
   ib_eof= 0;
   if (!opt_Q) warn("Initialising %d-sample buffer for mix stream", ib_len/2);

   // Preload 75% of the buffer -- or at least attempt to do so;
   // errors/eof/etc will be picked up in the inbuf_loop() routine
   ib_wr= ib_read(inbuf, ib_len*3/4);

   // Start the thread off
#ifdef UNIX_MISC
   {
      pthread_t thread;
      if (0 != pthread_create(&thread, NULL, (void*)&inbuf_loop, NULL))
	 error("Failed to start input buffering thread");
   }
#endif
#ifdef WIN_MISC
   {
      DWORD tmp;
      if (0 == CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&inbuf_loop, 0, 0, &tmp))
	 error("Failed to start input buffering thread");
   }
#endif
}

#ifdef ALSA_AUDIO
snd_pcm_t *alsa_handle;         // ALSA PCM handle
snd_pcm_hw_params_t *alsa_params; // ALSA hardware parameters

// Function to clean up ALSA resources
void cleanup_alsa() {
  if (alsa_handle) {
    snd_pcm_close(alsa_handle);
    alsa_handle = NULL;
  }
}
#endif

#ifdef MAC_AUDIO
void cleanup_mac_audio() {
    if (proc_id) {
        AudioDeviceStop(aud_dev, proc_id);
        AudioDeviceDestroyIOProcID(aud_dev, proc_id);
        proc_id = 0;
    }
    
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (aud_buf[i]) {
            free(aud_buf[i]);
            aud_buf[i] = NULL;
        }
    }
}

void init_mac_audio() {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        aud_buf[i] = (char *)Alloc(BUFFER_SIZE);
    }
}
#endif

//
//	Time-keeping functions
//

#define H24 (86400000)			// 24 hours
#define H12 (43200000)			// 12 hours

int t_per24(int t0, int t1) {		// Length of period starting at t0, ending at t1.
   int td= t1 - t0;				// NB for t0==t1 this gives 24 hours, *NOT 0*
   return td > 0 ? td : td + H24;
}
int t_per0(int t0, int t1) {		// Length of period starting at t0, ending at t1.
   int td= t1 - t0;				// NB for t0==t1 this gives 0 hours
   return td >= 0 ? td : td + H24;
}
int t_mid(int t0, int t1) {		// Midpoint of period from t0 to t1
   return ((t1 < t0) ? (H24 + t0 + t1) / 2 : (t0 + t1) / 2) % H24;
}

static int
curve_name_char(int c, int first) {
   if (first) return isalpha(c) || c == '_';
   return isalnum(c) || c == '_';
}

static int
curve_name_eq(const char *a, const char *b) {
   return strcmp(a, b) == 0;
}

static int
curve_name_ieq(const char *a, const char *b) {
   while (*a && *b) {
      if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
	 return 0;
      a++; b++;
   }
   return !*a && !*b;
}

static char *
curve_trim(char *s) {
   char *e;
   while (*s && isspace((unsigned char)*s)) s++;
   if (!*s) return s;
   e= s + strlen(s);
   while (e > s && isspace((unsigned char)e[-1])) e--;
   *e= 0;
   return s;
}

static int
curve_has_sbgf_ext(const char *path) {
   const char *dot= strrchr(path, '.');
   return dot && curve_name_ieq(dot, ".sbgf");
}

static int
curve_name_reserved(const char *name) {
   static const char *reserved[]= {
      "t", "m", "D", "H", "T", "U",
      "b0", "b1", "c0", "c1", "a0", "m0",
      "amp", "mixamp", "beat", "carrier",
      "solve",
      "pi", "e",
      "ifelse", "step", "clamp", "lerp", "ramp",
      "smoothstep", "smootherstep",
      "between", "pulse", "lt", "le", "gt", "ge", "eq", "ne",
      "seg", "min2", "max2",
      0
   };
   int i;
   for (i= 0; reserved[i]; i++)
      if (curve_name_eq(name, reserved[i])) return 1;
   return 0;
}

static int
curve_param_index(const char *name) {
   int i;
   for (i= 0; i<func_curve.param_count; i++)
      if (curve_name_eq(name, func_curve.param_names[i])) return i;
   return -1;
}

static void
curve_set_param_value(const char *name, double value, int allow_new) {
   int idx= curve_param_index(name);
   if (idx < 0) {
      if (!allow_new)
	 error("Unknown .sbgf parameter override: %s", name);
      if (func_curve.param_count >= CURVE_MAX_PARAMS)
	 error("Too many .sbgf parameters (max %d)", CURVE_MAX_PARAMS);
      if (curve_name_reserved(name))
	 error("Parameter name '%s' is reserved in .sbgf", name);
      idx= func_curve.param_count++;
      strncpy(func_curve.param_names[idx], name, CURVE_NAME_MAX-1);
      func_curve.param_names[idx][CURVE_NAME_MAX-1]= 0;
   }
   func_curve.param_values[idx]= value;
}

static double curve_fn_ifelse(double cond, double a, double b) {
   return cond != 0.0 ? a : b;
}

static double curve_fn_step(double x) {
   return x >= 0.0 ? 1.0 : 0.0;
}

static double curve_fn_clamp(double x, double lo, double hi) {
   double t;
   if (lo > hi) { t= lo; lo= hi; hi= t; }
   return sbx_dsp_clamp(x, lo, hi);
}

static double curve_fn_lerp(double a, double b, double u) {
   return a + (b-a) * u;
}

static double curve_fn_ramp(double x, double x0, double x1) {
   if (x0 == x1) return x >= x1 ? 1.0 : 0.0;
   return curve_fn_clamp((x-x0)/(x1-x0), 0.0, 1.0);
}

static double curve_fn_smoothstep(double edge0, double edge1, double x) {
   double u= curve_fn_ramp(x, edge0, edge1);
   return sbx_dsp_smoothstep01(u);
}

static double curve_fn_smootherstep(double edge0, double edge1, double x) {
   double u= curve_fn_ramp(x, edge0, edge1);
   return sbx_dsp_smootherstep01(u);
}

static double curve_fn_between(double x, double a, double b) {
   if (a > b) { double t= a; a= b; b= t; }
   return (x >= a && x <= b) ? 1.0 : 0.0;
}

static double curve_fn_pulse(double x, double start, double end) {
   return curve_fn_between(x, start, end);
}

static double curve_fn_lt(double a, double b) {
   return a < b ? 1.0 : 0.0;
}

static double curve_fn_le(double a, double b) {
   return a <= b ? 1.0 : 0.0;
}

static double curve_fn_gt(double a, double b) {
   return a > b ? 1.0 : 0.0;
}

static double curve_fn_ge(double a, double b) {
   return a >= b ? 1.0 : 0.0;
}

static double curve_fn_eq(double a, double b) {
   return a == b ? 1.0 : 0.0;
}

static double curve_fn_ne(double a, double b) {
   return a != b ? 1.0 : 0.0;
}

static double curve_fn_seg(double x, double x0, double x1, double y0, double y1) {
   if (x <= x0) return y0;
   if (x >= x1) return y1;
   if (x1 == x0) return y1;
   return y0 + (y1-y0) * (x-x0) / (x1-x0);
}

static double curve_fn_min2(double a, double b) {
   return a < b ? a : b;
}

static double curve_fn_max2(double a, double b) {
   return a > b ? a : b;
}

static void
curve_parse_param_line(char *line, int lno, const char *path) {
   char name[CURVE_NAME_MAX];
   char *p= line;
   char *q;
   char *endp;
   double v;
   double mult= 1.0;
   int i= 0;

   while (*p && isspace((unsigned char)*p)) p++;
   if (!*p) error("%s:%d: expecting parameter name after 'param'", path, lno);
   if (!curve_name_char((unsigned char)*p, 1))
      error("%s:%d: bad parameter name near '%s'", path, lno, p);
   while (*p && curve_name_char((unsigned char)*p, i == 0)) {
      if (i >= CURVE_NAME_MAX-1)
	 error("%s:%d: parameter name too long", path, lno);
      name[i++]= *p++;
   }
   name[i]= 0;

   while (*p && isspace((unsigned char)*p)) p++;
   if (*p != '=')
      error("%s:%d: expected '=' after parameter name '%s'", path, lno, name);
   p++;
   while (*p && isspace((unsigned char)*p)) p++;
   if (!*p)
      error("%s:%d: parameter '%s' needs a numeric value", path, lno, name);

   v= strtod(p, &endp);
   if (endp == p)
      error("%s:%d: parameter '%s' requires a numeric value (optional unit suffix: m or s)",
	    path, lno, name);
   q= endp;
   while (*q && isspace((unsigned char)*q)) q++;
   if (*q) {
      if ((q[0] == 'm' || q[0] == 'M') && q[1] == 0) {
	 mult= 1.0;		// Canonical unit is minutes
      } else if ((q[0] == 's' || q[0] == 'S') && q[1] == 0) {
	 mult= 1.0/60.0;	// Convert seconds -> minutes
      } else {
	 error("%s:%d: trailing text after parameter '%s' value: %s"
	       NL "       (expected optional unit suffix 'm' or 's')",
	       path, lno, name, q);
      }
   }

   curve_set_param_value(name, v * mult, 1);
}

static void
curve_parse_expr_line(char *p, int lno, const char *path,
		      const char *kind, char *dst, int dsz) {
   char *eq= strchr(p, '=');
   char *expr;
   if (!eq)
      error("%s:%d: %s line must be '<name> = <expression>'", path, lno, kind);
   expr= curve_trim(eq + 1);
   if (!*expr)
      error("%s:%d: empty expression for '%s'", path, lno, kind);
   if ((int)strlen(expr) >= dsz)
      error("%s:%d: expression for '%s' is too long", path, lno, kind);
   strcpy(dst, expr);
}

static int
curve_parse_piece_line(char *s, int lno, const char *path,
		       const char *kind, int klen,
		       char cond_store[][CURVE_EXPR_MAX],
		       char expr_store[][CURVE_EXPR_MAX],
		       int *piece_count) {
   char *p, *eq, *rhs, *expr;
   const char *fn= 0;
   int idx;

   if (strncmp(s, kind, klen) != 0) return 0;
   p= s + klen;
   while (*p && isspace((unsigned char)*p)) p++;

   if (*p == '<') {
      p++;
      fn= (*p == '=') ? (p++, "le") : "lt";
   } else if (*p == '>') {
      p++;
      fn= (*p == '=') ? (p++, "ge") : "gt";
   } else {
      return 0;
   }

   while (*p && isspace((unsigned char)*p)) p++;
   eq= strchr(p, '=');
   if (!eq)
      error("%s:%d: %s piecewise line must be '%s<expr = <expression>'",
	    path, lno, kind, kind);

   *eq= 0;
   rhs= curve_trim(p);
   expr= curve_trim(eq + 1);
   if (!*rhs)
      error("%s:%d: missing threshold expression in %s piecewise line", path, lno, kind);
   if (!*expr)
      error("%s:%d: empty expression in %s piecewise line", path, lno, kind);
   if (*piece_count >= CURVE_MAX_PIECES)
      error("%s:%d: too many %s piecewise lines (max %d)", path, lno, kind, CURVE_MAX_PIECES);

   idx= (*piece_count)++;
   if (snprintf(cond_store[idx], CURVE_EXPR_MAX, "%s(m,%s)", fn, rhs) >= CURVE_EXPR_MAX)
      error("%s:%d: %s piecewise condition is too long", path, lno, kind);
   if ((int)strlen(expr) >= CURVE_EXPR_MAX)
      error("%s:%d: %s piecewise expression is too long", path, lno, kind);
   strcpy(expr_store[idx], expr);

   return 1;
}

static void
curve_parse_solve_line(char *line, int lno, const char *path) {
   char *p= line;
   char *colon;
   char *eqs;
   int nunk= 0, neq= 0;

   if (func_curve.have_solve)
      error("%s:%d: only one 'solve ...' line is supported per .sbgf file", path, lno);

   while (*p && isspace((unsigned char)*p)) p++;
   if (!*p)
      error("%s:%d: solve line requires unknown names and equations", path, lno);

   colon= strchr(p, ':');
   if (!colon)
      error("%s:%d: solve line must use ':' between unknown names and equations", path, lno);
   *colon= 0;
   p= curve_trim(p);
   eqs= curve_trim(colon + 1);
   if (!*p)
      error("%s:%d: solve line is missing unknown names before ':'", path, lno);
   if (!*eqs)
      error("%s:%d: solve line is missing equations after ':'", path, lno);

   while (*p) {
      char name[CURVE_NAME_MAX];
      int i= 0, a;

      while (*p && isspace((unsigned char)*p)) p++;
      if (!*p) break;
      if (!curve_name_char((unsigned char)*p, 1))
	 error("%s:%d: bad solve unknown name near '%s'", path, lno, p);
      while (*p && curve_name_char((unsigned char)*p, i == 0)) {
	 if (i >= CURVE_NAME_MAX-1)
	    error("%s:%d: solve unknown name is too long", path, lno);
	 name[i++]= *p++;
      }
      name[i]= 0;
      for (a= 0; a<nunk; a++)
	 if (curve_name_eq(name, func_curve.solve_unknown_names[a]))
	    error("%s:%d: duplicate solve unknown '%s'", path, lno, name);
      if (curve_name_reserved(name))
	 error("%s:%d: solve unknown '%s' uses a reserved name", path, lno, name);
      if (nunk >= CURVE_MAX_SOLVE_UNK)
	 error("%s:%d: too many solve unknowns (max %d)", path, lno, CURVE_MAX_SOLVE_UNK);
      strcpy(func_curve.solve_unknown_names[nunk], name);
      nunk++;
      if (curve_param_index(name) < 0)
	 curve_set_param_value(name, 0.0, 1);

      while (*p && isspace((unsigned char)*p)) p++;
      if (!*p) break;
      if (*p != ',')
	 error("%s:%d: expected ',' between solve unknown names near '%s'", path, lno, p);
      p++;
   }

   while (*eqs) {
      char *semi= strchr(eqs, ';');
      char *seg;
      char *eq;
      char *lhs;
      char *rhs;

      if (semi) *semi= 0;
      seg= curve_trim(eqs);
      if (*seg) {
	 if (neq >= CURVE_MAX_SOLVE_EQ)
	    error("%s:%d: too many solve equations (max %d)", path, lno, CURVE_MAX_SOLVE_EQ);
	 eq= strchr(seg, '=');
	 if (!eq)
	    error("%s:%d: solve equation #%d must contain '='", path, lno, neq+1);
	 *eq= 0;
	 lhs= curve_trim(seg);
	 rhs= curve_trim(eq + 1);
	 if (!*lhs || !*rhs)
	    error("%s:%d: solve equation #%d must have both lhs and rhs expressions", path, lno, neq+1);
	 if ((int)strlen(lhs) >= CURVE_EXPR_MAX || (int)strlen(rhs) >= CURVE_EXPR_MAX)
	    error("%s:%d: solve equation #%d is too long", path, lno, neq+1);
	 strcpy(func_curve.solve_eq_lhs_src[neq], lhs);
	 strcpy(func_curve.solve_eq_rhs_src[neq], rhs);
	 neq++;
      }
      if (!semi) break;
      eqs= semi + 1;
   }

   if (nunk <= 0)
      error("%s:%d: solve line must declare at least one unknown", path, lno);
   if (neq <= 0)
      error("%s:%d: solve line must include at least one equation", path, lno);
   if (neq != nunk)
      error("%s:%d: solve currently requires a square system (#equations == #unknowns)"
	    NL "       unknowns=%d equations=%d",
	    path, lno, nunk, neq);

   func_curve.have_solve= 1;
   func_curve.solve_unknown_count= nunk;
   func_curve.solve_eq_count= neq;
}

static double
curve_vec_norm_inf(int n, const double *v) {
   int i;
   double m= 0.0;
   for (i= 0; i<n; i++) {
      double a= fabs(v[i]);
      if (a > m) m= a;
   }
   return m;
}

static int
curve_linear_solve(int n,
		   double a[CURVE_MAX_SOLVE_UNK][CURVE_MAX_SOLVE_UNK],
		   double b[CURVE_MAX_SOLVE_UNK],
		   double x[CURVE_MAX_SOLVE_UNK]) {
   int i, j, k;
   for (i= 0; i<n; i++) {
      int piv= i;
      double pivv= fabs(a[i][i]);
      for (j= i+1; j<n; j++) {
	 double v= fabs(a[j][i]);
	 if (v > pivv) { pivv= v; piv= j; }
      }
      if (pivv < 1e-14) return 0;
      if (piv != i) {
	 double tb= b[i];
	 b[i]= b[piv];
	 b[piv]= tb;
	 for (k= i; k<n; k++) {
	    double ta= a[i][k];
	    a[i][k]= a[piv][k];
	    a[piv][k]= ta;
	 }
      }
      for (j= i+1; j<n; j++) {
	 double f= a[j][i] / a[i][i];
	 if (f == 0.0) continue;
	 a[j][i]= 0.0;
	 for (k= i+1; k<n; k++)
	    a[j][k]-= f * a[i][k];
	 b[j]-= f * b[i];
      }
   }
   for (i= n-1; i>=0; i--) {
      double s= b[i];
      for (j= i+1; j<n; j++)
	 s-= a[i][j] * x[j];
      if (fabs(a[i][i]) < 1e-14) return 0;
      x[i]= s / a[i][i];
   }
   return 1;
}

static void
curve_set_unknown_vector(int n, const double *x) {
   int i;
   for (i= 0; i<n; i++) {
      int idx= func_curve.solve_unknown_param_idx[i];
      if (idx >= 0 && idx < func_curve.param_count)
	 func_curve.param_values[idx]= x[i];
   }
}

static int
curve_eval_solve_residuals(int n, te_expr **lhs, te_expr **rhs, double *resid) {
   int i;
   for (i= 0; i<n; i++) {
      double lv= te_eval(lhs[i]);
      double rv= te_eval(rhs[i]);
      if (!isfinite(lv) || !isfinite(rv)) return 0;
      resid[i]= lv - rv;
   }
   return 1;
}

static void
curve_apply_solve(te_variable *vars, int vcnt) {
   te_expr *lhs[CURVE_MAX_SOLVE_EQ];
   te_expr *rhs[CURVE_MAX_SOLVE_EQ];
   double x[CURVE_MAX_SOLVE_UNK];
   double r0[CURVE_MAX_SOLVE_EQ];
   double r1[CURVE_MAX_SOLVE_EQ];
   double r2[CURVE_MAX_SOLVE_EQ];
   double jac[CURVE_MAX_SOLVE_UNK][CURVE_MAX_SOLVE_UNK];
   int n, i, j, iter, ok= 0;
   double best= 0.0;
   int err= 0;

   if (!func_curve.have_solve) return;
   n= func_curve.solve_unknown_count;
   if (n <= 0 || n > CURVE_MAX_SOLVE_UNK)
      error("%s: internal solve setup error (unknown count=%d)", func_curve.src_file, n);
   if (func_curve.solve_eq_count != n)
      error("%s: solve requires equal equation/unknown counts", func_curve.src_file);

   memset(lhs, 0, sizeof(lhs));
   memset(rhs, 0, sizeof(rhs));

   for (i= 0; i<n; i++) {
      int idx= curve_param_index(func_curve.solve_unknown_names[i]);
      if (idx < 0)
	 error("%s: solve unknown '%s' is not a defined parameter",
	       func_curve.src_file, func_curve.solve_unknown_names[i]);
      func_curve.solve_unknown_param_idx[i]= idx;
      x[i]= func_curve.param_values[idx];
   }
   curve_set_unknown_vector(n, x);

   for (i= 0; i<n; i++) {
      err= 0;
      lhs[i]= te_compile(func_curve.solve_eq_lhs_src[i], vars, vcnt, &err);
      if (!lhs[i])
	 error("%s: solve equation #%d lhs failed near column %d",
	       func_curve.src_file, i+1, err);
      err= 0;
      rhs[i]= te_compile(func_curve.solve_eq_rhs_src[i], vars, vcnt, &err);
      if (!rhs[i])
	 error("%s: solve equation #%d rhs failed near column %d",
	       func_curve.src_file, i+1, err);
   }

   for (iter= 0; iter<40; iter++) {
      double step[CURVE_MAX_SOLVE_UNK];
      double anorm;
      double snorm;
      int accepted= 0;
      double alpha;

      curve_set_unknown_vector(n, x);
      if (!curve_eval_solve_residuals(n, lhs, rhs, r0))
	 error("%s: solve evaluation failed at iteration %d (non-finite residual)",
	       func_curve.src_file, iter+1);
      best= curve_vec_norm_inf(n, r0);
      if (best < 1e-9) { ok= 1; break; }

      for (j= 0; j<n; j++) {
	 double xj= x[j];
	 double h= 1e-6 * (fabs(xj) + 1.0);
	 double xph= xj + h;
	 double xmh= xj - h;
	 int good_plus= 0, good_minus= 0;

	 x[j]= xph;
	 curve_set_unknown_vector(n, x);
	 good_plus= curve_eval_solve_residuals(n, lhs, rhs, r1);

	 x[j]= xmh;
	 curve_set_unknown_vector(n, x);
	 good_minus= curve_eval_solve_residuals(n, lhs, rhs, r2);

	 x[j]= xj;
	 curve_set_unknown_vector(n, x);

	 if (good_plus && good_minus) {
	    for (i= 0; i<n; i++)
	       jac[i][j]= (r1[i] - r2[i]) / (2.0 * h);
	 } else if (good_plus) {
	    for (i= 0; i<n; i++)
	       jac[i][j]= (r1[i] - r0[i]) / h;
	 } else {
	    error("%s: solve Jacobian failed for unknown '%s' at iteration %d",
		  func_curve.src_file, func_curve.solve_unknown_names[j], iter+1);
	 }
      }

      {
	 double a[CURVE_MAX_SOLVE_UNK][CURVE_MAX_SOLVE_UNK];
	 double b[CURVE_MAX_SOLVE_UNK];
	 for (i= 0; i<n; i++) {
	    for (j= 0; j<n; j++)
	       a[i][j]= jac[i][j];
	    b[i]= -r0[i];
	    step[i]= 0.0;
	 }
	 if (!curve_linear_solve(n, a, b, step))
	    error("%s: solve Jacobian is singular/ill-conditioned at iteration %d",
		  func_curve.src_file, iter+1);
      }

      snorm= curve_vec_norm_inf(n, step);
      alpha= 1.0;
      for (i= 0; i<12; i++) {
	 double trial[CURVE_MAX_SOLVE_UNK];
	 double trial_norm;
	 for (j= 0; j<n; j++)
	    trial[j]= x[j] + alpha * step[j];
	 curve_set_unknown_vector(n, trial);
	 if (!curve_eval_solve_residuals(n, lhs, rhs, r1)) {
	    alpha *= 0.5;
	    continue;
	 }
	 trial_norm= curve_vec_norm_inf(n, r1);
	 if (trial_norm < best || trial_norm < 1e-9) {
	    for (j= 0; j<n; j++)
	       x[j]= trial[j];
	    best= trial_norm;
	    accepted= 1;
	    break;
	 }
	 alpha *= 0.5;
      }
      if (!accepted) {
	 if (snorm < 1e-12 && best < 1e-7) {
	    ok= 1;
	    break;
	 }
	 error("%s: solve failed to find a descent step at iteration %d"
	       NL "       try better initial guesses (param values) or a different model",
	       func_curve.src_file, iter+1);
      }
      anorm= alpha * snorm;
      if (best < 1e-9 || anorm < 1e-12) {
	 ok= 1;
	 break;
      }
   }

   if (!ok)
      error("%s: solve did not converge after 40 iterations", func_curve.src_file);

   curve_set_unknown_vector(n, x);
   if (!opt_Q) {
      fprintf(stderr, " Solved constants:");
      for (i= 0; i<n; i++)
	 fprintf(stderr, " %s=%g", func_curve.solve_unknown_names[i], x[i]);
      fprintf(stderr, "\n");
   }

   for (i= 0; i<n; i++) {
      if (lhs[i]) te_free(lhs[i]);
      if (rhs[i]) te_free(rhs[i]);
   }
}

static void
load_curve_file(const char *path) {
   FILE *fp;
   char line[4096];
   int lno= 0;
   int have_beat= 0;

   if (!curve_has_sbgf_ext(path))
      error("Curve file must use .sbgf extension: %s", path);

   fp= fopen(path, "r");
   if (!fp)
      error("Cannot open curve file: %s (%s)", path, strerror(errno));

   strncpy(func_curve.src_file, path, sizeof(func_curve.src_file)-1);
   func_curve.src_file[sizeof(func_curve.src_file)-1]= 0;
   func_curve.beat_expr_src[0]= 0;
   func_curve.carr_expr_src[0]= 0;
   func_curve.amp_expr_src[0]= 0;
   func_curve.mixamp_expr_src[0]= 0;
   func_curve.have_carr_expr= 0;
   func_curve.have_amp_expr= 0;
   func_curve.have_mixamp_expr= 0;
   func_curve.beat_piece_count= 0;
   func_curve.carr_piece_count= 0;
   func_curve.amp_piece_count= 0;
   func_curve.mixamp_piece_count= 0;
   func_curve.have_solve= 0;
   func_curve.solve_unknown_count= 0;
   func_curve.solve_eq_count= 0;
   func_curve.param_count= 0;

   while (fgets(line, sizeof(line), fp)) {
      char *s;
      lno++;
      if (strchr(line, '\n')) *strchr(line, '\n')= 0;
      if (strchr(line, '\r')) *strchr(line, '\r')= 0;
      s= curve_trim(line);
      if (!*s || *s == '#' || *s == ';') continue;
      if (s[0] == '/' && s[1] == '/') continue;

      if (!strncmp(s, "param", 5) && isspace((unsigned char)s[5])) {
	 curve_parse_param_line(s + 5, lno, path);
	 continue;
      }
      if (!strncmp(s, "solve", 5) && isspace((unsigned char)s[5])) {
	 curve_parse_solve_line(s + 5, lno, path);
	 continue;
      }
      if (!strncmp(s, "beat", 4) &&
	  (isspace((unsigned char)s[4]) || s[4] == '=' || s[4] == '<' || s[4] == '>')) {
	 if (curve_parse_piece_line(s, lno, path, "beat", 4,
				    func_curve.beat_piece_cond_src,
				    func_curve.beat_piece_expr_src,
				    &func_curve.beat_piece_count)) {
	    if (func_curve.beat_expr_src[0])
	       error("%s:%d: cannot mix 'beat = ...' with piecewise beat<... lines", path, lno);
	    have_beat= 1;
	    continue;
	 }
	 if (func_curve.beat_piece_count > 0)
	    error("%s:%d: cannot mix piecewise beat<... lines with 'beat = ...'", path, lno);
	 curve_parse_expr_line(s, lno, path, "beat", func_curve.beat_expr_src, sizeof(func_curve.beat_expr_src));
	 have_beat= 1;
	 continue;
      }
      if (!strncmp(s, "carrier", 7) &&
	  (isspace((unsigned char)s[7]) || s[7] == '=' || s[7] == '<' || s[7] == '>')) {
	 if (curve_parse_piece_line(s, lno, path, "carrier", 7,
				    func_curve.carr_piece_cond_src,
				    func_curve.carr_piece_expr_src,
				    &func_curve.carr_piece_count)) {
	    if (func_curve.have_carr_expr)
	       error("%s:%d: cannot mix 'carrier = ...' with piecewise carrier<... lines", path, lno);
	    continue;
	 }
	 if (func_curve.carr_piece_count > 0)
	    error("%s:%d: cannot mix piecewise carrier<... lines with 'carrier = ...'", path, lno);
	 curve_parse_expr_line(s, lno, path, "carrier", func_curve.carr_expr_src, sizeof(func_curve.carr_expr_src));
	 func_curve.have_carr_expr= 1;
	 continue;
      }
      if (!strncmp(s, "amp", 3) &&
	  (isspace((unsigned char)s[3]) || s[3] == '=' || s[3] == '<' || s[3] == '>')) {
	 if (curve_parse_piece_line(s, lno, path, "amp", 3,
				    func_curve.amp_piece_cond_src,
				    func_curve.amp_piece_expr_src,
				    &func_curve.amp_piece_count)) {
	    if (func_curve.have_amp_expr)
	       error("%s:%d: cannot mix 'amp = ...' with piecewise amp<... lines", path, lno);
	    continue;
	 }
	 if (func_curve.amp_piece_count > 0)
	    error("%s:%d: cannot mix piecewise amp<... lines with 'amp = ...'", path, lno);
	 curve_parse_expr_line(s, lno, path, "amp", func_curve.amp_expr_src, sizeof(func_curve.amp_expr_src));
	 func_curve.have_amp_expr= 1;
	 continue;
      }
      if (!strncmp(s, "mixamp", 6) &&
	  (isspace((unsigned char)s[6]) || s[6] == '=' || s[6] == '<' || s[6] == '>')) {
	 if (curve_parse_piece_line(s, lno, path, "mixamp", 6,
				    func_curve.mixamp_piece_cond_src,
				    func_curve.mixamp_piece_expr_src,
				    &func_curve.mixamp_piece_count)) {
	    if (func_curve.have_mixamp_expr)
	       error("%s:%d: cannot mix 'mixamp = ...' with piecewise mixamp<... lines", path, lno);
	    continue;
	 }
	 if (func_curve.mixamp_piece_count > 0)
	    error("%s:%d: cannot mix piecewise mixamp<... lines with 'mixamp = ...'", path, lno);
	 curve_parse_expr_line(s, lno, path, "mixamp", func_curve.mixamp_expr_src, sizeof(func_curve.mixamp_expr_src));
	 func_curve.have_mixamp_expr= 1;
	 continue;
      }

      error("%s:%d: unknown line in .sbgf (expected 'param', 'solve', 'beat', 'beat<...>',"
	    " 'carrier', 'carrier<...>', 'amp', 'amp<...>', 'mixamp', or 'mixamp<...>'): %s",
	    path, lno, s);
   }
   fclose(fp);

   if (!have_beat)
      error("%s: missing required beat definition in .sbgf (use 'beat = ...' or piecewise beat<... lines)", path);
}

static int
setup_custom_func_curve(int chan, int typ, int start_ms,
			double carr0, double carr1, double carr_span_s,
			double beat0, double beat1, double beat_span_s,
			double hold_min, double total_min, double wake_min,
			double beat_amp0_pct, double mix_amp0_pct) {
   te_variable vars[CURVE_MAX_PARAMS + 40];
   int vcnt= 0, err= 0, i;

   if (carr_span_s <= 0 || beat_span_s <= 0) return 0;
   if (!func_curve.beat_expr_src[0] && func_curve.beat_piece_count == 0) return 0;

#define ADD_VAR(name_, ptr_) do { \
      vars[vcnt].name= (name_); \
      vars[vcnt].address= (ptr_); \
      vars[vcnt].type= TE_VARIABLE; \
      vars[vcnt].context= 0; \
      vcnt++; \
   } while (0)
#define ADD_FN1(name_, fn_) do { \
      vars[vcnt].name= (name_); \
      vars[vcnt].address= (const void*)(fn_); \
      vars[vcnt].type= TE_FUNCTION1 | TE_FLAG_PURE; \
      vars[vcnt].context= 0; \
      vcnt++; \
   } while (0)
#define ADD_FN2(name_, fn_) do { \
      vars[vcnt].name= (name_); \
      vars[vcnt].address= (const void*)(fn_); \
      vars[vcnt].type= TE_FUNCTION2 | TE_FLAG_PURE; \
      vars[vcnt].context= 0; \
      vcnt++; \
   } while (0)
#define ADD_FN3(name_, fn_) do { \
      vars[vcnt].name= (name_); \
      vars[vcnt].address= (const void*)(fn_); \
      vars[vcnt].type= TE_FUNCTION3 | TE_FLAG_PURE; \
      vars[vcnt].context= 0; \
      vcnt++; \
   } while (0)
#define ADD_FN5(name_, fn_) do { \
      vars[vcnt].name= (name_); \
      vars[vcnt].address= (const void*)(fn_); \
      vars[vcnt].type= TE_FUNCTION5 | TE_FLAG_PURE; \
      vars[vcnt].context= 0; \
      vcnt++; \
   } while (0)

   func_curve.active= 1;
   func_curve.mode= 3;
   func_curve.chan= chan;
   func_curve.chan2= -1;
   func_curve.typ= typ;
   func_curve.monaural= 0;
   func_curve.start_ms= start_ms;
   func_curve.end_ms= (start_ms + (int)(1000.0 * carr_span_s + 0.5)) % H24;
   func_curve.carr0= carr0;
   func_curve.carr1= carr1;
   func_curve.carr_span_s= carr_span_s;
   func_curve.beat0= beat0;
   func_curve.beat1= beat1;
   func_curve.beat_span_s= beat_span_s;
   func_curve.beat_amp0_pct= beat_amp0_pct;
   func_curve.mix_amp0_pct= mix_amp0_pct;
   func_curve.ev_D= beat_span_s / 60.0;
   func_curve.ev_H= hold_min;
   func_curve.ev_T= total_min;
   func_curve.ev_U= wake_min;
   func_curve.ev_b0= beat0;
   func_curve.ev_b1= beat1;
   func_curve.ev_c0= carr0;
   func_curve.ev_c1= carr1;
   func_curve.ev_a0= beat_amp0_pct;
   func_curve.ev_m0= mix_amp0_pct;

   ADD_VAR("t", &func_curve.ev_t);
   ADD_VAR("m", &func_curve.ev_m);
   ADD_VAR("D", &func_curve.ev_D);
   ADD_VAR("H", &func_curve.ev_H);
   ADD_VAR("T", &func_curve.ev_T);
   ADD_VAR("U", &func_curve.ev_U);
   ADD_VAR("b0", &func_curve.ev_b0);
   ADD_VAR("b1", &func_curve.ev_b1);
   ADD_VAR("c0", &func_curve.ev_c0);
   ADD_VAR("c1", &func_curve.ev_c1);
   ADD_VAR("a0", &func_curve.ev_a0);
   ADD_VAR("m0", &func_curve.ev_m0);

   ADD_FN3("ifelse", curve_fn_ifelse);
   ADD_FN1("step", curve_fn_step);
   ADD_FN3("clamp", curve_fn_clamp);
   ADD_FN3("lerp", curve_fn_lerp);
   ADD_FN3("ramp", curve_fn_ramp);
   ADD_FN3("smoothstep", curve_fn_smoothstep);
   ADD_FN3("smootherstep", curve_fn_smootherstep);
   ADD_FN3("between", curve_fn_between);
   ADD_FN3("pulse", curve_fn_pulse);
   ADD_FN2("lt", curve_fn_lt);
   ADD_FN2("le", curve_fn_le);
   ADD_FN2("gt", curve_fn_gt);
   ADD_FN2("ge", curve_fn_ge);
   ADD_FN2("eq", curve_fn_eq);
   ADD_FN2("ne", curve_fn_ne);
   ADD_FN5("seg", curve_fn_seg);
   ADD_FN2("min2", curve_fn_min2);
   ADD_FN2("max2", curve_fn_max2);

   for (i= 0; i<func_curve.param_count; i++)
      ADD_VAR(func_curve.param_names[i], &func_curve.param_values[i]);

   curve_apply_solve(vars, vcnt);

   if (func_curve.beat_piece_count > 0) {
      for (i= 0; i<func_curve.beat_piece_count; i++) {
	 err= 0;
	 func_curve.beat_piece_cond[i]= te_compile(func_curve.beat_piece_cond_src[i], vars, vcnt, &err);
	 if (!func_curve.beat_piece_cond[i])
	    error("%s: error in beat piece condition #%d near column %d", func_curve.src_file, i+1, err);
	 err= 0;
	 func_curve.beat_piece_expr[i]= te_compile(func_curve.beat_piece_expr_src[i], vars, vcnt, &err);
	 if (!func_curve.beat_piece_expr[i])
	    error("%s: error in beat piece expression #%d near column %d", func_curve.src_file, i+1, err);
      }
   } else {
      func_curve.beat_expr= te_compile(func_curve.beat_expr_src, vars, vcnt, &err);
      if (!func_curve.beat_expr)
	 error("%s: error in beat expression near column %d", func_curve.src_file, err);
   }

   if (func_curve.carr_piece_count > 0) {
      for (i= 0; i<func_curve.carr_piece_count; i++) {
	 err= 0;
	 func_curve.carr_piece_cond[i]= te_compile(func_curve.carr_piece_cond_src[i], vars, vcnt, &err);
	 if (!func_curve.carr_piece_cond[i])
	    error("%s: error in carrier piece condition #%d near column %d", func_curve.src_file, i+1, err);
	 err= 0;
	 func_curve.carr_piece_expr[i]= te_compile(func_curve.carr_piece_expr_src[i], vars, vcnt, &err);
	 if (!func_curve.carr_piece_expr[i])
	    error("%s: error in carrier piece expression #%d near column %d", func_curve.src_file, i+1, err);
      }
   } else if (func_curve.have_carr_expr) {
      err= 0;
      func_curve.carr_expr= te_compile(func_curve.carr_expr_src, vars, vcnt, &err);
      if (!func_curve.carr_expr)
	 error("%s: error in carrier expression near column %d", func_curve.src_file, err);
   }

   if (func_curve.amp_piece_count > 0) {
      for (i= 0; i<func_curve.amp_piece_count; i++) {
	 err= 0;
	 func_curve.amp_piece_cond[i]= te_compile(func_curve.amp_piece_cond_src[i], vars, vcnt, &err);
	 if (!func_curve.amp_piece_cond[i])
	    error("%s: error in amp piece condition #%d near column %d", func_curve.src_file, i+1, err);
	 err= 0;
	 func_curve.amp_piece_expr[i]= te_compile(func_curve.amp_piece_expr_src[i], vars, vcnt, &err);
	 if (!func_curve.amp_piece_expr[i])
	    error("%s: error in amp piece expression #%d near column %d", func_curve.src_file, i+1, err);
      }
   } else if (func_curve.have_amp_expr) {
      err= 0;
      func_curve.amp_expr= te_compile(func_curve.amp_expr_src, vars, vcnt, &err);
      if (!func_curve.amp_expr)
	 error("%s: error in amp expression near column %d", func_curve.src_file, err);
   }

   if (func_curve.mixamp_piece_count > 0) {
      for (i= 0; i<func_curve.mixamp_piece_count; i++) {
	 err= 0;
	 func_curve.mixamp_piece_cond[i]= te_compile(func_curve.mixamp_piece_cond_src[i], vars, vcnt, &err);
	 if (!func_curve.mixamp_piece_cond[i])
	    error("%s: error in mixamp piece condition #%d near column %d", func_curve.src_file, i+1, err);
	 err= 0;
	 func_curve.mixamp_piece_expr[i]= te_compile(func_curve.mixamp_piece_expr_src[i], vars, vcnt, &err);
	 if (!func_curve.mixamp_piece_expr[i])
	    error("%s: error in mixamp piece expression #%d near column %d", func_curve.src_file, i+1, err);
      }
   } else if (func_curve.have_mixamp_expr) {
      err= 0;
      func_curve.mixamp_expr= te_compile(func_curve.mixamp_expr_src, vars, vcnt, &err);
      if (!func_curve.mixamp_expr)
	 error("%s: error in mixamp expression near column %d", func_curve.src_file, err);
   }

#undef ADD_VAR
#undef ADD_FN1
#undef ADD_FN2
#undef ADD_FN3
#undef ADD_FN5
   return 1;
}

static int
eval_custom_curve_point(double pos_s,
			double *beat_out, double *carr_out,
			double *amp_pct_out, double *mixamp_pct_out) {
   int i, matched;
   double condv;
   double beat, carr, amp_pct, mixamp_pct;
   double t= pos_s;
   if (!func_curve.beat_expr && func_curve.beat_piece_count == 0) return 0;
   if (t < 0.0) t= 0.0;
   if (t > func_curve.carr_span_s) t= func_curve.carr_span_s;
   func_curve.ev_t= t;
   func_curve.ev_m= t / 60.0;

   if (func_curve.beat_piece_count > 0) {
      matched= 0;
      for (i= 0; i<func_curve.beat_piece_count; i++) {
	 if (!func_curve.beat_piece_cond[i] || !func_curve.beat_piece_expr[i]) return 0;
	 condv= te_eval(func_curve.beat_piece_cond[i]);
	 if (!isfinite(condv)) return 0;
	 if (condv != 0.0) {
	    beat= te_eval(func_curve.beat_piece_expr[i]);
	    matched= 1;
	    break;
	 }
      }
      if (!matched)
	 beat= te_eval(func_curve.beat_piece_expr[func_curve.beat_piece_count-1]);
   } else {
      beat= te_eval(func_curve.beat_expr);
   }
   if (!isfinite(beat)) return 0;

   if (func_curve.carr_piece_count > 0) {
      matched= 0;
      for (i= 0; i<func_curve.carr_piece_count; i++) {
	 if (!func_curve.carr_piece_cond[i] || !func_curve.carr_piece_expr[i]) return 0;
	 condv= te_eval(func_curve.carr_piece_cond[i]);
	 if (!isfinite(condv)) return 0;
	 if (condv != 0.0) {
	    carr= te_eval(func_curve.carr_piece_expr[i]);
	    matched= 1;
	    break;
	 }
      }
      if (!matched)
	 carr= te_eval(func_curve.carr_piece_expr[func_curve.carr_piece_count-1]);
      if (!isfinite(carr))
	 carr= func_curve.carr0 + (func_curve.carr1 - func_curve.carr0) *
	      (func_curve.carr_span_s > 0.0 ? (t / func_curve.carr_span_s) : 0.0);
   } else if (func_curve.have_carr_expr && func_curve.carr_expr) {
      carr= te_eval(func_curve.carr_expr);
      if (!isfinite(carr))
	 carr= func_curve.carr0 + (func_curve.carr1 - func_curve.carr0) *
	      (func_curve.carr_span_s > 0.0 ? (t / func_curve.carr_span_s) : 0.0);
   } else {
      carr= func_curve.carr0 + (func_curve.carr1 - func_curve.carr0) *
	   (func_curve.carr_span_s > 0.0 ? (t / func_curve.carr_span_s) : 0.0);
   }

   amp_pct= func_curve.beat_amp0_pct;
   if (func_curve.amp_piece_count > 0) {
      matched= 0;
      for (i= 0; i<func_curve.amp_piece_count; i++) {
	 if (!func_curve.amp_piece_cond[i] || !func_curve.amp_piece_expr[i]) return 0;
	 condv= te_eval(func_curve.amp_piece_cond[i]);
	 if (!isfinite(condv)) return 0;
	 if (condv != 0.0) {
	    amp_pct= te_eval(func_curve.amp_piece_expr[i]);
	    matched= 1;
	    break;
	 }
      }
      if (!matched)
	 amp_pct= te_eval(func_curve.amp_piece_expr[func_curve.amp_piece_count-1]);
   } else if (func_curve.have_amp_expr && func_curve.amp_expr) {
      amp_pct= te_eval(func_curve.amp_expr);
   }
   if (!isfinite(amp_pct))
      amp_pct= func_curve.beat_amp0_pct;
   if (amp_pct < 0.0) amp_pct= 0.0;

   mixamp_pct= func_curve.mix_amp0_pct;
   if (func_curve.mixamp_piece_count > 0) {
      matched= 0;
      for (i= 0; i<func_curve.mixamp_piece_count; i++) {
	 if (!func_curve.mixamp_piece_cond[i] || !func_curve.mixamp_piece_expr[i]) return 0;
	 condv= te_eval(func_curve.mixamp_piece_cond[i]);
	 if (!isfinite(condv)) return 0;
	 if (condv != 0.0) {
	    mixamp_pct= te_eval(func_curve.mixamp_piece_expr[i]);
	    matched= 1;
	    break;
	 }
      }
      if (!matched)
	 mixamp_pct= te_eval(func_curve.mixamp_piece_expr[func_curve.mixamp_piece_count-1]);
   } else if (func_curve.have_mixamp_expr && func_curve.mixamp_expr) {
      mixamp_pct= te_eval(func_curve.mixamp_expr);
   }
   if (!isfinite(mixamp_pct))
      mixamp_pct= func_curve.mix_amp0_pct;
   if (mixamp_pct < 0.0) mixamp_pct= 0.0;

   *beat_out= beat;
   *carr_out= carr;
   if (amp_pct_out) *amp_pct_out= amp_pct;
   if (mixamp_pct_out) *mixamp_pct_out= mixamp_pct;
   return 1;
}

static int
curve_parse_mix_amp_token(const char *tok, double *amp_out) {
   char *q;
   double v;
   if (!tok || strncmp(tok, "mix/", 4) != 0) return 0;
   v= strtod(tok + 4, &q);
   if (q == tok + 4 || *q) return 0;
   if (amp_out) *amp_out= v;
   return 1;
}

static int
curve_find_mix_amp_in_extra(const char *extra, double *amp_out) {
   const char *p= extra;
   while (p && *p) {
      const char *s= p;
      const char *e;
      int n;
      char tok[256];
      while (*s && isspace((unsigned char)*s)) s++;
      if (!*s) break;
      e= s;
      while (*e && !isspace((unsigned char)*e)) e++;
      n= (int)(e-s);
      if (n >= (int)sizeof(tok)) n= sizeof(tok)-1;
      memcpy(tok, s, n);
      tok[n]= 0;
      if (curve_parse_mix_amp_token(tok, amp_out))
	 return 1;
      p= e;
   }
   return 0;
}

// Clear any runtime function-driven curve override.
static void
clear_func_curve() {
   int i;
   if (func_curve.beat_expr) {
      te_free(func_curve.beat_expr);
      func_curve.beat_expr= 0;
   }
   if (func_curve.carr_expr) {
      te_free(func_curve.carr_expr);
      func_curve.carr_expr= 0;
   }
   if (func_curve.amp_expr) {
      te_free(func_curve.amp_expr);
      func_curve.amp_expr= 0;
   }
   if (func_curve.mixamp_expr) {
      te_free(func_curve.mixamp_expr);
      func_curve.mixamp_expr= 0;
   }
   for (i= 0; i<CURVE_MAX_PIECES; i++) {
      if (func_curve.beat_piece_cond[i]) {
	 te_free(func_curve.beat_piece_cond[i]);
	 func_curve.beat_piece_cond[i]= 0;
      }
      if (func_curve.beat_piece_expr[i]) {
	 te_free(func_curve.beat_piece_expr[i]);
	 func_curve.beat_piece_expr[i]= 0;
      }
      if (func_curve.carr_piece_cond[i]) {
	 te_free(func_curve.carr_piece_cond[i]);
	 func_curve.carr_piece_cond[i]= 0;
      }
      if (func_curve.carr_piece_expr[i]) {
	 te_free(func_curve.carr_piece_expr[i]);
	 func_curve.carr_piece_expr[i]= 0;
      }
      if (func_curve.amp_piece_cond[i]) {
	 te_free(func_curve.amp_piece_cond[i]);
	 func_curve.amp_piece_cond[i]= 0;
      }
      if (func_curve.amp_piece_expr[i]) {
	 te_free(func_curve.amp_piece_expr[i]);
	 func_curve.amp_piece_expr[i]= 0;
      }
      if (func_curve.mixamp_piece_cond[i]) {
	 te_free(func_curve.mixamp_piece_cond[i]);
	 func_curve.mixamp_piece_cond[i]= 0;
      }
      if (func_curve.mixamp_piece_expr[i]) {
	 te_free(func_curve.mixamp_piece_expr[i]);
	 func_curve.mixamp_piece_expr[i]= 0;
      }
   }
   memset(&func_curve, 0, sizeof(func_curve));
   func_curve.chan2= -1;
}

void
clear_mix_mod_curve(void) {
   memset(&mix_mod_curve, 0, sizeof(mix_mod_curve));
}

void
setup_mix_mod_curve(double delta, double epsilon, double k_min, double end_level,
		    double main_len_min, double wake_len_min, int wake_enabled) {
   mix_mod_curve.active= 1;
   mix_mod_curve.delta= delta;
   mix_mod_curve.epsilon= epsilon;
   mix_mod_curve.period_min= k_min;
   mix_mod_curve.end_level= end_level;
   mix_mod_curve.main_len_min= main_len_min;
   mix_mod_curve.wake_len_min= wake_len_min;
   mix_mod_curve.wake_enabled= wake_enabled ? 1 : 0;
}

double
mix_mod_multiplier(int now_ms) {
   double t_min, mod_2k, x, g, v, two_k;

   if (!mix_mod_curve.active || mix_mod_curve.main_len_min <= 0.0 || fast_tim0 < 0)
      return 1.0;

   t_min= t_per0(fast_tim0, now_ms) / 60000.0;

   if (t_min < mix_mod_curve.main_len_min) {
      two_k= 2.0 * mix_mod_curve.period_min;
      mod_2k= fmod(t_min, two_k);
      if (mod_2k < 0.0) mod_2k += two_k;
      x= mod_2k - mix_mod_curve.period_min;
      g= 1.0 - mix_mod_curve.delta * exp(-mix_mod_curve.epsilon * x * x);
      v= 1.0 - ((1.0 - mix_mod_curve.end_level) / mix_mod_curve.main_len_min) * t_min;
      if (g < 0.0) g= 0.0;
      if (v < 0.0) v= 0.0;
      return g * v;
   }

   if (mix_mod_curve.wake_enabled && mix_mod_curve.wake_len_min > 0.0) {
      double tw= t_min - mix_mod_curve.main_len_min;
      double w;
      if (tw < 0.0) tw= 0.0;
      if (tw <= mix_mod_curve.wake_len_min) {
	 w= (1.0 - mix_mod_curve.end_level) + (mix_mod_curve.end_level / mix_mod_curve.wake_len_min) * tw;
	 if (w < 0.0) w= 0.0;
	 return w;
      }
   }

   return 1.0;
}

int
is_mix_mod_option_spec(const char *spec) {
   const char *p= spec;
   if (!p || !*p) return 0;
   if (*p == '-') return 0;
   if (!strchr(p, '=')) return 0;
   if (*p == ':') p++;
   return (*p == 'd' || *p == 'e' || *p == 'k' || *p == 'E');
}

void
parse_mix_mod_option_spec(const char *spec) {
   char tmp[256];
   char *p, *q;
   if (!spec || !*spec) return;
   if (strlen(spec) >= sizeof(tmp))
      error("-A spec is too long");
   strcpy(tmp, spec);
   p= tmp;

   while (*p) {
      while (*p == ':') p++;
      if (!*p) break;
      switch (*p++) {
       case 'd':
	  if (*p++ != '=') error("-A expects d=<val>:e=<val>:k=<val>:E=<val>");
	  opt_A_d= strtod(p, &q);
	  if (q == p) error("-A parameter d requires a numeric value");
	  p= q;
	  break;
       case 'e':
	  if (*p++ != '=') error("-A expects d=<val>:e=<val>:k=<val>:E=<val>");
	  opt_A_e= strtod(p, &q);
	  if (q == p) error("-A parameter e requires a numeric value");
	  p= q;
	  break;
       case 'k':
	  if (*p++ != '=') error("-A expects d=<val>:e=<val>:k=<val>:E=<val>");
	  opt_A_k= strtod(p, &q);
	  if (q == p) error("-A parameter k requires a numeric value");
	  p= q;
	  break;
       case 'E':
	  if (*p++ != '=') error("-A expects d=<val>:e=<val>:k=<val>:E=<val>");
	  opt_A_E= strtod(p, &q);
	  if (q == p) error("-A parameter E requires a numeric value");
	  p= q;
	  break;
       default:
	  error("-A only supports d=, e=, k= and E= parameters");
      }

      if (*p == ':') p++;
      else if (*p) error("-A expects colon-separated parameters");
   }

   if (opt_A_d < 0.0 || opt_A_d > 1.0)
      error("-A parameter d must be in range 0..1");
   if (opt_A_e <= 0.0)
      error("-A parameter e must be > 0");
   if (opt_A_k <= 0.0)
      error("-A parameter k must be > 0");
   if (opt_A_E < 0.0 || opt_A_E > 1.0)
      error("-A parameter E must be in range 0..1");
}

int
is_iso_gate_option_spec(const char *spec) {
   const char *p= spec;
   if (!p || !*p) return 0;
   if (*p == '-') return 0;
   if (!strchr(p, '=')) return 0;
   if (*p == ':') p++;
   return (*p == 's' || *p == 'd' || *p == 'a' || *p == 'r' || *p == 'e');
}

void
parse_iso_gate_option_spec(const char *spec) {
   char tmp[256];
   char *p, *q, *q2;
   long edge;
   if (!spec || !*spec) return;
   if (strlen(spec) >= sizeof(tmp))
      error("-I spec is too long");
   strcpy(tmp, spec);
   p= tmp;

   while (*p) {
      while (*p == ':') p++;
      if (!*p) break;
      switch (*p++) {
       case 's':
	  if (*p++ != '=') error("-I expects s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>");
	  opt_I_s= strtod(p, &q);
	  if (q == p) error("-I parameter s requires a numeric value");
	  p= q;
	  break;
       case 'd':
	  if (*p++ != '=') error("-I expects s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>");
	  opt_I_d= strtod(p, &q);
	  if (q == p) error("-I parameter d requires a numeric value");
	  p= q;
	  break;
       case 'a':
	  if (*p++ != '=') error("-I expects s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>");
	  opt_I_a= strtod(p, &q);
	  if (q == p) error("-I parameter a requires a numeric value");
	  p= q;
	  break;
       case 'r':
	  if (*p++ != '=') error("-I expects s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>");
	  opt_I_r= strtod(p, &q);
	  if (q == p) error("-I parameter r requires a numeric value");
	  p= q;
	  break;
       case 'e':
	  if (*p++ != '=') error("-I expects s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>");
	  edge= strtol(p, &q, 10);
	  if (q == p) error("-I parameter e requires an integer value");
	  q2= q;
	  while (isspace((unsigned char)*q2)) q2++;
	  if (*q2 && *q2 != ':')
	     error("-I parameter e must be an integer in range 0..3");
	  opt_I_e= (int)edge;
	  p= q;
	  break;
       default:
	  error("-I only supports s=, d=, a=, r= and e= parameters");
      }

      if (*p == ':') p++;
      else if (*p) error("-I expects colon-separated parameters");
   }

   if (opt_I_s < 0.0 || opt_I_s >= 1.0)
      error("-I parameter s must be in range [0,1)");
   if (opt_I_d <= 0.0 || opt_I_d > 1.0)
      error("-I parameter d must be in range (0,1]");
   if (opt_I_a < 0.0 || opt_I_a > 1.0)
      error("-I parameter a must be in range [0,1]");
   if (opt_I_r < 0.0 || opt_I_r > 1.0)
      error("-I parameter r must be in range [0,1]");
   if (opt_I_a + opt_I_r > 1.0)
      error("-I parameters a+r must be <= 1");
   if (opt_I_e < 0 || opt_I_e > 3)
      error("-I parameter e must be in range 0..3");
}

int
is_mixam_env_option_spec(const char *spec) {
   const char *p= spec;
   if (!p || !*p) return 0;
   if (*p == '-') return 0;
   if (!strchr(p, '=')) return 0;
   if (*p == ':') p++;
   return (*p == 'm' || *p == 's' || *p == 'd' || *p == 'a' || *p == 'r' || *p == 'e' || *p == 'f');
}

void
parse_mixam_env_option_spec(const char *spec) {
   char tmp[256];
   char *p, *q, *q2;
   long edge;
   int saw_mode= 0;
   int saw_pulse_shape= 0;
   if (!spec || !*spec) return;
   if (strlen(spec) >= sizeof(tmp))
      error("-H spec is too long");
   strcpy(tmp, spec);
   p= tmp;

   while (*p) {
      while (*p == ':') p++;
      if (!*p) break;
      switch (*p++) {
       case 'm':
	  if (*p++ != '=') error("-H expects m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
	  q= p;
	  while (*q && *q != ':' && !isspace((unsigned char)*q)) q++;
	  if (q == p) error("-H parameter m requires a value (pulse or cos)");
	  {
	     size_t n= (size_t)(q - p);
	     char mtok[16];
	     if (n >= sizeof(mtok)) error("-H parameter m value is too long");
	     memcpy(mtok, p, n);
	     mtok[n]= 0;
	     if (0 == strcasecmp(mtok, "pulse"))
		opt_H_m= SBX_MIXAM_MODE_PULSE;
	     else if (0 == strcasecmp(mtok, "cos"))
		opt_H_m= SBX_MIXAM_MODE_COS;
	     else
		error("-H parameter m must be 'pulse' or 'cos'");
	     saw_mode= 1;
	  }
	  p= q;
	  break;
       case 's':
	  if (*p++ != '=') error("-H expects m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
	  opt_H_s= strtod(p, &q);
	  if (q == p) error("-H parameter s requires a numeric value");
	  p= q;
	  break;
       case 'd':
	  if (*p++ != '=') error("-H expects m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
	  opt_H_d= strtod(p, &q);
	  if (q == p) error("-H parameter d requires a numeric value");
	  saw_pulse_shape= 1;
	  p= q;
	  break;
       case 'a':
	  if (*p++ != '=') error("-H expects m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
	  opt_H_a= strtod(p, &q);
	  if (q == p) error("-H parameter a requires a numeric value");
	  saw_pulse_shape= 1;
	  p= q;
	  break;
       case 'r':
	  if (*p++ != '=') error("-H expects m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
	  opt_H_r= strtod(p, &q);
	  if (q == p) error("-H parameter r requires a numeric value");
	  saw_pulse_shape= 1;
	  p= q;
	  break;
       case 'e':
	  if (*p++ != '=') error("-H expects m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
	  edge= strtol(p, &q, 10);
	  if (q == p) error("-H parameter e requires an integer value");
	  q2= q;
	  while (isspace((unsigned char)*q2)) q2++;
	  if (*q2 && *q2 != ':')
	     error("-H parameter e must be an integer in range 0..3");
	  opt_H_e= (int)edge;
	  saw_pulse_shape= 1;
	  p= q;
	  break;
       case 'f':
	  if (*p++ != '=') error("-H expects m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
	  opt_H_f= strtod(p, &q);
	  if (q == p) error("-H parameter f requires a numeric value");
	  p= q;
	  break;
       default:
	  error("-H only supports m=, s=, d=, a=, r=, e= and f= parameters");
      }

      if (*p == ':') p++;
      else if (*p) error("-H expects colon-separated parameters");
   }

   if (!saw_mode && saw_pulse_shape)
      opt_H_m= SBX_MIXAM_MODE_PULSE;

   if (opt_H_s < 0.0 || opt_H_s > 1.0)
      error("-H parameter s must be in range [0,1]");
   if (opt_H_m == SBX_MIXAM_MODE_PULSE) {
      if (opt_H_d < 0.0 || opt_H_d > 1.0)
	 error("-H parameter d must be in range [0,1] in m=pulse mode");
      if (opt_H_a < 0.0 || opt_H_a > 1.0)
	 error("-H parameter a must be in range [0,1] in m=pulse mode");
      if (opt_H_r < 0.0 || opt_H_r > 1.0)
	 error("-H parameter r must be in range [0,1] in m=pulse mode");
      if (opt_H_a + opt_H_r > 1.0)
	 error("-H parameters a+r must be <= 1 in m=pulse mode");
      if (opt_H_e < 0 || opt_H_e > 3)
	 error("-H parameter e must be in range 0..3");
   }
   if (opt_H_f < 0.0 || opt_H_f > 1.0)
      error("-H parameter f must be in range [0,1]");
}

static double
isochronic_mod_factor_phase_custom(double phase,
				   double start, double duty,
				   double attack, double release,
				   int edge_mode) {
   return sbx_dsp_iso_mod_factor_custom(phase, start, duty, attack, release, edge_mode);
}

static double
wave_sample_phase(int waveform, double phase) {
   int idx;
   phase= sbx_dsp_wrap_unit(phase);
   idx= (int)(phase * ST_SIZ);
   if (idx >= ST_SIZ) idx= ST_SIZ-1;
   return sin_tables[waveform][idx] / (double)ST_AMP;
}

static double
isochronic_mod_factor_phase_legacy(double phase, int waveform) {
   double wave= wave_sample_phase(waveform, phase);
   double mod_factor= 0.0;
   double threshold= 0.3;
   if (wave > threshold) {
      mod_factor= (wave - threshold) / (1.0 - threshold);
      mod_factor= sbx_dsp_smoothstep01(mod_factor);
   }
   return mod_factor;
}

static double
isochronic_mod_factor(Channel *ch) {
   double phase= ch->off2 / (double)(ST_SIZ * 65536.0);
   if (opt_I)
      return isochronic_mod_factor_phase_custom(phase, opt_I_s, opt_I_d, opt_I_a, opt_I_r, opt_I_e);
   return isochronic_mod_factor_phase_legacy(phase, ch->v.waveform);
}

// Configure monaural twin routing for the registered curve.
// Each ear receives both tones: carr-beat/2 and carr+beat/2.
static void
setup_func_curve_monaural_pair(int chan0, int chan1) {
   if (!func_curve.active) return;
   func_curve.monaural= 1;
   func_curve.chan= chan0;
   func_curve.chan2= chan1;
   func_curve.typ= 1;		// Monaural is built from two plain sine tones
}

// Apply the registered curve at the current runtime position.
static void
apply_func_curve(int now_ms, int chan, Voice *vv) {
   double pos_s, carr_s, pos_min;
   double beat= 0.0, carr= 0.0;
   double amp_pct= 0.0, mixamp_pct= 0.0;
   int elapsed_ms, total_ms;
   int have_amp_curve, have_mixamp_curve;

   if (!func_curve.active) return;
   elapsed_ms= t_per0(func_curve.start_ms, now_ms);
   total_ms= t_per0(func_curve.start_ms, func_curve.end_ms);
   if (elapsed_ms > total_ms) return;

   pos_s= elapsed_ms * 0.001;
   carr_s= pos_s;
   if (carr_s > func_curve.carr_span_s) carr_s= func_curve.carr_span_s;

   have_amp_curve= func_curve.have_amp_expr || func_curve.amp_piece_count > 0;
   have_mixamp_curve= func_curve.have_mixamp_expr || func_curve.mixamp_piece_count > 0;

   if (func_curve.mode == 3) {
      if (!eval_custom_curve_point(pos_s, &beat, &carr, &amp_pct, &mixamp_pct))
	 return;

      // mixamp targets mix/<amp> voices only.
      if (vv->typ == 5) {
	 if (have_mixamp_curve)
	    vv->amp= AMP_DA(mixamp_pct);
	 return;
      }

      if (func_curve.monaural) {
	 if ((chan != func_curve.chan && chan != func_curve.chan2) || vv->typ != 1) return;
      } else {
	 if (chan != func_curve.chan || vv->typ != func_curve.typ) return;
      }

      if (func_curve.typ == 8 || func_curve.monaural) {
	 if (beat < 0.0) beat= -beat;
	 if (beat < 1e-6) beat= 1e-6;
      }

      if (have_amp_curve)
	 vv->amp= AMP_DA(amp_pct);
   } else if (func_curve.mode == 2) {
      if (func_curve.monaural) {
	 if ((chan != func_curve.chan && chan != func_curve.chan2) || vv->typ != 1) return;
      } else {
	 if (chan != func_curve.chan || vv->typ != func_curve.typ) return;
      }
      if (pos_s >= func_curve.beat_span_s) beat= func_curve.beat1;
      else {
	 pos_min= pos_s / 60.0;
	 beat= func_curve.sig_a *
	       tanh(func_curve.sig_l * (pos_min - func_curve.sig_d_min/2 - func_curve.sig_h)) +
	       func_curve.sig_b;
      }
      carr= func_curve.carr0 + (func_curve.carr1 - func_curve.carr0) * carr_s / func_curve.carr_span_s;
   } else {
      if (func_curve.monaural) {
	 if ((chan != func_curve.chan && chan != func_curve.chan2) || vv->typ != 1) return;
      } else {
	 if (chan != func_curve.chan || vv->typ != func_curve.typ) return;
      }
      if (pos_s >= func_curve.beat_span_s) beat= func_curve.beat1;
      else beat= func_curve.beat0 * exp(func_curve.beat_log_ratio * pos_s / func_curve.beat_span_s);
      carr= func_curve.carr0 + (func_curve.carr1 - func_curve.carr0) * carr_s / func_curve.carr_span_s;
   }

   if (func_curve.monaural) {
      if (chan == func_curve.chan) {
	 vv->carr= carr - beat/2.0;
      } else {
	 vv->carr= carr + beat/2.0;
      }
      vv->res= 0.0;
   } else {
      vv->carr= carr;
      vv->res= beat;
   }
}

static void
plot_set_px(unsigned char *img, int w, int h, int x, int y,
	    unsigned char r, unsigned char g, unsigned char b) {
   int o;
   if (x < 0 || y < 0 || x >= w || y >= h) return;
   o= (y*w + x) * 3;
   img[o+0]= r;
   img[o+1]= g;
   img[o+2]= b;
}

static void
plot_fill(unsigned char *img, int w, int h,
	  unsigned char r, unsigned char g, unsigned char b) {
   int a, o;
   for (a= 0, o= 0; a<w*h; a++, o += 3) {
      img[o+0]= r;
      img[o+1]= g;
      img[o+2]= b;
   }
}

static void
plot_fill_rect(unsigned char *img, int w, int h,
	       int x0, int y0, int x1, int y1,
	       unsigned char r, unsigned char g, unsigned char b) {
   int x, y, t;
   if (x0 > x1) { t= x0; x0= x1; x1= t; }
   if (y0 > y1) { t= y0; y0= y1; y1= t; }
   if (x0 < 0) x0= 0;
   if (y0 < 0) y0= 0;
   if (x1 >= w) x1= w-1;
   if (y1 >= h) y1= h-1;
   if (x0 > x1 || y0 > y1) return;
   for (y= y0; y<=y1; y++) {
      for (x= x0; x<=x1; x++) {
	 plot_set_px(img, w, h, x, y, r, g, b);
      }
   }
}

static void
plot_line(unsigned char *img, int w, int h,
	  int x0, int y0, int x1, int y1,
	  unsigned char r, unsigned char g, unsigned char b) {
   int dx= abs(x1-x0), sx= x0 < x1 ? 1 : -1;
   int dy= -abs(y1-y0), sy= y0 < y1 ? 1 : -1;
   int err= dx + dy;

   while (1) {
      plot_set_px(img, w, h, x0, y0, r, g, b);
      if (x0 == x1 && y0 == y1) break;
      {
	 int e2= 2 * err;
	 if (e2 >= dy) { err += dy; x0 += sx; }
	 if (e2 <= dx) { err += dx; y0 += sy; }
      }
   }
}

static void
plot_line_thick(unsigned char *img, int w, int h,
		int x0, int y0, int x1, int y1, int thick,
		unsigned char r, unsigned char g, unsigned char b) {
   int rad, ox, oy;
   if (thick <= 1) {
      plot_line(img, w, h, x0, y0, x1, y1, r, g, b);
      return;
   }
   rad= thick / 2;
   for (oy= -rad; oy<=rad; oy++) {
      for (ox= -rad; ox<=rad; ox++) {
	 if (ox*ox + oy*oy <= rad*rad)
	    plot_line(img, w, h, x0+ox, y0+oy, x1+ox, y1+oy, r, g, b);
      }
   }
}

static void
plot_hline(unsigned char *img, int w, int h, int x0, int x1, int y,
	   unsigned char r, unsigned char g, unsigned char b) {
   int x;
   if (x0 > x1) { int t= x0; x0= x1; x1= t; }
   for (x= x0; x<=x1; x++)
      plot_set_px(img, w, h, x, y, r, g, b);
}

static void
plot_vline(unsigned char *img, int w, int h, int x, int y0, int y1,
	   unsigned char r, unsigned char g, unsigned char b) {
   int y;
   if (y0 > y1) { int t= y0; y0= y1; y1= t; }
   for (y= y0; y<=y1; y++)
      plot_set_px(img, w, h, x, y, r, g, b);
}

static unsigned char
plot_clamp_u8(int v) {
   if (v < 0) return 0;
   if (v > 255) return 255;
   return (unsigned char)v;
}

static unsigned char *
plot_downsample_box(unsigned char *src, int sw, int sh, int ss) {
   int dw= sw / ss;
   int dh= sh / ss;
   int x, y, xx, yy;
   int area= ss * ss;
   unsigned char *dst= ALLOC_ARR(dw*dh*3, unsigned char);

   for (y= 0; y<dh; y++) {
      for (x= 0; x<dw; x++) {
	 int sumr= 0, sumg= 0, sumb= 0;
	 int doff= (y*dw + x) * 3;
	 for (yy= 0; yy<ss; yy++) {
	    int sy= y*ss + yy;
	    int row= sy * sw;
	    for (xx= 0; xx<ss; xx++) {
	       int sx= x*ss + xx;
	       int soff= (row + sx) * 3;
	       sumr += src[soff+0];
	       sumg += src[soff+1];
	       sumb += src[soff+2];
	    }
	 }
	 dst[doff+0]= plot_clamp_u8((sumr + area/2) / area);
	 dst[doff+1]= plot_clamp_u8((sumg + area/2) / area);
	 dst[doff+2]= plot_clamp_u8((sumb + area/2) / area);
      }
   }

   return dst;
}

static void
font5x7_glyph(char ch, unsigned char g[7]) {
   int a;
   for (a= 0; a<7; a++) g[a]= 0;

   switch (toupper((unsigned char)ch)) {
    case '0':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x13; g[3]= 0x15;
      g[4]= 0x19; g[5]= 0x11; g[6]= 0x0E; break;
    case '1':
      g[0]= 0x04; g[1]= 0x0C; g[2]= 0x04; g[3]= 0x04;
      g[4]= 0x04; g[5]= 0x04; g[6]= 0x0E; break;
    case '2':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x01; g[3]= 0x02;
      g[4]= 0x04; g[5]= 0x08; g[6]= 0x1F; break;
    case '3':
      g[0]= 0x1E; g[1]= 0x01; g[2]= 0x01; g[3]= 0x0E;
      g[4]= 0x01; g[5]= 0x01; g[6]= 0x1E; break;
    case '4':
      g[0]= 0x02; g[1]= 0x06; g[2]= 0x0A; g[3]= 0x12;
      g[4]= 0x1F; g[5]= 0x02; g[6]= 0x02; break;
    case '5':
      g[0]= 0x1F; g[1]= 0x10; g[2]= 0x10; g[3]= 0x1E;
      g[4]= 0x01; g[5]= 0x01; g[6]= 0x1E; break;
    case '6':
      g[0]= 0x0E; g[1]= 0x10; g[2]= 0x10; g[3]= 0x1E;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x0E; break;
    case '7':
      g[0]= 0x1F; g[1]= 0x01; g[2]= 0x02; g[3]= 0x04;
      g[4]= 0x08; g[5]= 0x08; g[6]= 0x08; break;
    case '8':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x0E;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x0E; break;
    case '9':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x0F;
      g[4]= 0x01; g[5]= 0x01; g[6]= 0x0E; break;
    case '-':
      g[0]= 0x00; g[1]= 0x00; g[2]= 0x00; g[3]= 0x1F;
      g[4]= 0x00; g[5]= 0x00; g[6]= 0x00; break;
    case '.':
      g[0]= 0x00; g[1]= 0x00; g[2]= 0x00; g[3]= 0x00;
      g[4]= 0x00; g[5]= 0x06; g[6]= 0x06; break;
    case 'A':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x1F;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x11; break;
    case 'B':
      g[0]= 0x1E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x1E;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x1E; break;
    case 'C':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x10; g[3]= 0x10;
      g[4]= 0x10; g[5]= 0x11; g[6]= 0x0E; break;
    case 'D':
      g[0]= 0x1E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x11;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x1E; break;
    case 'E':
      g[0]= 0x1F; g[1]= 0x10; g[2]= 0x10; g[3]= 0x1E;
      g[4]= 0x10; g[5]= 0x10; g[6]= 0x1F; break;
    case 'F':
      g[0]= 0x1F; g[1]= 0x10; g[2]= 0x10; g[3]= 0x1E;
      g[4]= 0x10; g[5]= 0x10; g[6]= 0x10; break;
    case 'G':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x10; g[3]= 0x17;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x0E; break;
    case 'H':
      g[0]= 0x11; g[1]= 0x11; g[2]= 0x11; g[3]= 0x1F;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x11; break;
    case 'I':
      g[0]= 0x1F; g[1]= 0x04; g[2]= 0x04; g[3]= 0x04;
      g[4]= 0x04; g[5]= 0x04; g[6]= 0x1F; break;
    case 'J':
      g[0]= 0x01; g[1]= 0x01; g[2]= 0x01; g[3]= 0x01;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x0E; break;
    case 'K':
      g[0]= 0x11; g[1]= 0x12; g[2]= 0x14; g[3]= 0x18;
      g[4]= 0x14; g[5]= 0x12; g[6]= 0x11; break;
    case 'L':
      g[0]= 0x10; g[1]= 0x10; g[2]= 0x10; g[3]= 0x10;
      g[4]= 0x10; g[5]= 0x10; g[6]= 0x1F; break;
    case 'M':
      g[0]= 0x11; g[1]= 0x1B; g[2]= 0x15; g[3]= 0x15;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x11; break;
    case 'N':
      g[0]= 0x11; g[1]= 0x19; g[2]= 0x15; g[3]= 0x13;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x11; break;
    case 'O':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x11;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x0E; break;
    case 'P':
      g[0]= 0x1E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x1E;
      g[4]= 0x10; g[5]= 0x10; g[6]= 0x10; break;
    case 'Q':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x11;
      g[4]= 0x15; g[5]= 0x12; g[6]= 0x0D; break;
    case 'R':
      g[0]= 0x1E; g[1]= 0x11; g[2]= 0x11; g[3]= 0x1E;
      g[4]= 0x14; g[5]= 0x12; g[6]= 0x11; break;
    case 'S':
      g[0]= 0x0F; g[1]= 0x10; g[2]= 0x10; g[3]= 0x0E;
      g[4]= 0x01; g[5]= 0x01; g[6]= 0x1E; break;
    case 'T':
      g[0]= 0x1F; g[1]= 0x04; g[2]= 0x04; g[3]= 0x04;
      g[4]= 0x04; g[5]= 0x04; g[6]= 0x04; break;
    case 'U':
      g[0]= 0x11; g[1]= 0x11; g[2]= 0x11; g[3]= 0x11;
      g[4]= 0x11; g[5]= 0x11; g[6]= 0x0E; break;
    case 'V':
      g[0]= 0x11; g[1]= 0x11; g[2]= 0x11; g[3]= 0x11;
      g[4]= 0x0A; g[5]= 0x0A; g[6]= 0x04; break;
    case 'W':
      g[0]= 0x11; g[1]= 0x11; g[2]= 0x11; g[3]= 0x15;
      g[4]= 0x15; g[5]= 0x15; g[6]= 0x0A; break;
    case 'X':
      g[0]= 0x11; g[1]= 0x11; g[2]= 0x0A; g[3]= 0x04;
      g[4]= 0x0A; g[5]= 0x11; g[6]= 0x11; break;
    case 'Y':
      g[0]= 0x11; g[1]= 0x11; g[2]= 0x0A; g[3]= 0x04;
      g[4]= 0x04; g[5]= 0x04; g[6]= 0x04; break;
    case 'Z':
      g[0]= 0x1F; g[1]= 0x01; g[2]= 0x02; g[3]= 0x04;
      g[4]= 0x08; g[5]= 0x10; g[6]= 0x1F; break;
    case ':':
      g[0]= 0x00; g[1]= 0x06; g[2]= 0x06; g[3]= 0x00;
      g[4]= 0x06; g[5]= 0x06; g[6]= 0x00; break;
    case '/':
      g[0]= 0x01; g[1]= 0x02; g[2]= 0x04; g[3]= 0x08;
      g[4]= 0x10; g[5]= 0x00; g[6]= 0x00; break;
    case '=':
      g[0]= 0x00; g[1]= 0x1F; g[2]= 0x00; g[3]= 0x1F;
      g[4]= 0x00; g[5]= 0x00; g[6]= 0x00; break;
    case '+':
      g[0]= 0x00; g[1]= 0x04; g[2]= 0x04; g[3]= 0x1F;
      g[4]= 0x04; g[5]= 0x04; g[6]= 0x00; break;
    case '_':
      g[0]= 0x00; g[1]= 0x00; g[2]= 0x00; g[3]= 0x00;
      g[4]= 0x00; g[5]= 0x00; g[6]= 0x1F; break;
    case ',':
      g[0]= 0x00; g[1]= 0x00; g[2]= 0x00; g[3]= 0x00;
      g[4]= 0x06; g[5]= 0x06; g[6]= 0x04; break;
    case '%':
      g[0]= 0x19; g[1]= 0x19; g[2]= 0x02; g[3]= 0x04;
      g[4]= 0x08; g[5]= 0x13; g[6]= 0x13; break;
    case '@':
      g[0]= 0x0E; g[1]= 0x11; g[2]= 0x17; g[3]= 0x15;
      g[4]= 0x17; g[5]= 0x10; g[6]= 0x0E; break;
    default:
      break;
   }
}

static void
font5x7_draw_char(unsigned char *img, int w, int h, int x, int y, char ch,
		  int scale, int rot90,
		  unsigned char r, unsigned char g, unsigned char b) {
   unsigned char glyph[7];
   int row, col, sx, sy;

   if (scale < 1) scale= 1;
   font5x7_glyph(ch, glyph);

   for (row= 0; row<7; row++) {
      for (col= 0; col<5; col++) {
	 if (!(glyph[row] & (1 << (4-col)))) continue;
	 for (sy= 0; sy<scale; sy++) {
	    for (sx= 0; sx<scale; sx++) {
	       int px, py;
	       if (!rot90) {
		  px= x + col*scale + sx;
		  py= y + row*scale + sy;
	       } else {
		  int rx= 6-row;
		  int ry= col;
		  px= x + rx*scale + sx;
		  py= y + ry*scale + sy;
	       }
	       plot_set_px(img, w, h, px, py, r, g, b);
	    }
	 }
      }
   }
}

static int
font5x7_text_width(const char *txt, int scale) {
   int n= (int)strlen(txt);
   if (n <= 0) return 0;
   if (scale < 1) scale= 1;
   return n * 6 * scale - scale;
}

static int
font5x7_text_height_rot90(const char *txt, int scale) {
   int n= (int)strlen(txt);
   if (n <= 0) return 0;
   if (scale < 1) scale= 1;
   return n * 6 * scale - scale;
}

static void
font5x7_draw_text(unsigned char *img, int w, int h, int x, int y,
		  const char *txt, int scale, int rot90,
		  unsigned char r, unsigned char g, unsigned char b) {
   int a;
   int cx= x, cy= y;
   int adv= 6 * (scale < 1 ? 1 : scale);

   for (a= 0; txt[a]; a++) {
      char ch= txt[a];
      if (ch != ' ')
	 font5x7_draw_char(img, w, h, cx, cy, ch, scale, rot90, r, g, b);
      if (!rot90) cx += adv;
      else cy += adv;
   }
}

static void
format_tick_value(double val, char *out, int out_sz) {
   char tmp[64];
   int len;

   if (out_sz < 2) return;
   if (fabs(val) < 0.0005) val= 0.0;
   snprintf(tmp, sizeof(tmp), "%.2f", val);

   len= (int)strlen(tmp);
   while (len > 0 && tmp[len-1] == '0')
      tmp[--len]= 0;
   if (len > 0 && tmp[len-1] == '.')
      tmp[--len]= 0;
   if (!strcmp(tmp, "-0")) {
      tmp[0]= '0';
      tmp[1]= 0;
   }

   strncpy(out, tmp, out_sz-1);
   out[out_sz-1]= 0;
}

static void
sanitize_filename_token(const char *in, char *out, int out_sz) {
   int a, b= 0, und= 0;
   if (out_sz < 2) return;
   for (a= 0; in[a] && b < out_sz-1; a++) {
      unsigned char c= (unsigned char)in[a];
      if (isalnum(c) || c == '.' || c == '_' || c == '-') {
	 out[b++]= in[a];
	 und= 0;
      } else {
	 if (!und && b < out_sz-1) {
	    out[b++]= '_';
	    und= 1;
	 }
      }
   }
   while (b > 0 && out[b-1] == '_') b--;
   if (!b) out[b++]= 'x';
   out[b]= 0;
}

static void
double_to_token(double val, char *out, int out_sz) {
   char tmp[64];
   int a, b= 0;
   if (out_sz < 2) return;
   snprintf(tmp, sizeof(tmp), "%.6g", val);
   for (a= 0; tmp[a] && b < out_sz-1; a++) {
      char c= tmp[a];
      if (isdigit((unsigned char)c) || c == 'e' || c == 'E')
	 out[b++]= c;
      else if (c == '.')
	 out[b++]= 'p';
      else if (c == '-')
	 out[b++]= 'm';
      else if (c == '+')
	 ;
      else if (c == '_')
	 out[b++]= '_';
   }
   if (!b) out[b++]= '0';
   out[b]= 0;
}

static int
str_ieq(const char *a, const char *b) {
   while (*a && *b) {
      if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
	 return 0;
      a++;
      b++;
   }
   return *a == 0 && *b == 0;
}

static int
file_exists_regular(const char *path) {
   struct stat st;
   if (!path || !*path) return 0;
   if (stat(path, &st) != 0) return 0;
#ifdef _S_IFREG
   return (st.st_mode & _S_IFMT) == _S_IFREG;
#else
   return S_ISREG(st.st_mode);
#endif
}

static int
plot_cmd_append(char *cmd, int cmd_sz, const char *txt) {
   int len= (int)strlen(cmd);
   int add= (int)strlen(txt);
   if (len + add >= cmd_sz) return 0;
   memcpy(cmd + len, txt, add + 1);
   return 1;
}

#ifndef T_MINGW
static int
plot_cmd_append_quoted(char *cmd, int cmd_sz, const char *arg) {
   const char *p= arg;
   if (!plot_cmd_append(cmd, cmd_sz, " \"")) return 0;
   while (*p) {
      char ch= *p++;
      if (ch == '"') {
	 if (!plot_cmd_append(cmd, cmd_sz, "\\\"")) return 0;
      } else {
	 char tmp[2];
	 tmp[0]= ch;
	 tmp[1]= 0;
	 if (!plot_cmd_append(cmd, cmd_sz, tmp)) return 0;
      }
   }
   if (!plot_cmd_append(cmd, cmd_sz, "\"")) return 0;
   return 1;
}
#endif

#ifdef T_MINGW
static int
plot_cmd_append_quoted_raw(char *cmd, int cmd_sz, const char *arg) {
   const char *p= arg;
   if (!plot_cmd_append(cmd, cmd_sz, "\"")) return 0;
   while (*p) {
      char ch= *p++;
      if (ch == '"') {
	 if (!plot_cmd_append(cmd, cmd_sz, "\\\"")) return 0;
      } else {
	 char tmp[2];
	 tmp[0]= ch;
	 tmp[1]= 0;
	 if (!plot_cmd_append(cmd, cmd_sz, tmp)) return 0;
      }
   }
   if (!plot_cmd_append(cmd, cmd_sz, "\"")) return 0;
   return 1;
}
#endif

static int
plot_find_script(char *script, int script_sz) {
   const char *env_script= getenv("SBAGENX_PLOT_SCRIPT");
   if (env_script && file_exists_regular(env_script)) {
      strncpy(script, env_script, script_sz-1);
      script[script_sz-1]= 0;
      return 1;
   }

   if (pdir && *pdir) {
#ifdef T_MINGW
      snprintf(script, script_sz, "%sscripts\\sbagenx_plot.py", pdir);
#else
      snprintf(script, script_sz, "%sscripts/sbagenx_plot.py", pdir);
#endif
      if (file_exists_regular(script))
	 return 1;
   }

#ifdef T_MINGW
   snprintf(script, script_sz, "scripts\\sbagenx_plot.py");
#else
   snprintf(script, script_sz, "scripts/sbagenx_plot.py");
#endif
   if (file_exists_regular(script))
      return 1;

   return 0;
}

static int
plot_external_disabled(void) {
   const char *mode= getenv("SBAGENX_PLOT_BACKEND");
   if (!mode || !*mode) return 0;
   return str_ieq(mode, "internal") || str_ieq(mode, "builtin") || str_ieq(mode, "c");
}

static int
plot_external_force(void) {
   const char *mode= getenv("SBAGENX_PLOT_BACKEND");
   if (!mode || !*mode) return 0;
   return str_ieq(mode, "python") || str_ieq(mode, "cairo") || str_ieq(mode, "external");
}

static int
plot_external_debug(void) {
   const char *dbg= getenv("SBAGENX_PLOT_DEBUG");
   if (!dbg || !*dbg) return 0;
   return !str_ieq(dbg, "0") && !str_ieq(dbg, "false") && !str_ieq(dbg, "off");
}

static int
plot_try_external_cmd(const char *script,
		      const char *out_fname,
		      const char *arg_tail) {
   const char *env_py= getenv("SBAGENX_PLOT_PYTHON");
   const char *candidates[8];
   char py_embedded[4][PATH_MAX];
   int n= 0, i;
   int ok= 0;
   int force_external= plot_external_force();

   if (plot_external_disabled())
      return 0;

   if (!script || !*script || !file_exists_regular(script))
      return 0;

   if (env_py && *env_py)
      candidates[n++]= env_py;

   if (pdir && *pdir) {
#ifdef T_MINGW
      snprintf(py_embedded[0], sizeof(py_embedded[0]), "%spython\\python.exe", pdir);
      snprintf(py_embedded[1], sizeof(py_embedded[1]), "%spython-win64\\python.exe", pdir);
      snprintf(py_embedded[2], sizeof(py_embedded[2]), "%spython-win32\\python.exe", pdir);
      snprintf(py_embedded[3], sizeof(py_embedded[3]), "%spython\\bin\\python3.exe", pdir);
#else
      snprintf(py_embedded[0], sizeof(py_embedded[0]), "%spython/python.exe", pdir);
      snprintf(py_embedded[1], sizeof(py_embedded[1]), "%spython-win64/python.exe", pdir);
      snprintf(py_embedded[2], sizeof(py_embedded[2]), "%spython-win32/python.exe", pdir);
      snprintf(py_embedded[3], sizeof(py_embedded[3]), "%spython/bin/python3", pdir);
#endif

      for (i= 0; i<4; i++) {
	 if (file_exists_regular(py_embedded[i]))
	    candidates[n++]= py_embedded[i];
      }
   }

   candidates[n++]= "python3";
   candidates[n++]= "python";

   for (i= 0; i<n; i++) {
      char cmd[8192];
      int rc;
      const char *py= candidates[i];
      int looks_like_path= strchr(py, '/') || strchr(py, '\\') || strchr(py, ':');

      if (looks_like_path && !file_exists_regular(py))
	 continue;

      cmd[0]= 0;
#ifdef T_MINGW
      // Use cmd /c ""..."" wrapping so quoted executable + quoted script
      // paths resolve reliably on Windows.
      if (!plot_cmd_append(cmd, sizeof(cmd), "cmd /c \"")) continue;
      if (!plot_cmd_append_quoted_raw(cmd, sizeof(cmd), py)) continue;
      if (!plot_cmd_append(cmd, sizeof(cmd), " ")) continue;
      if (!plot_cmd_append_quoted_raw(cmd, sizeof(cmd), script)) continue;
      if (!plot_cmd_append(cmd, sizeof(cmd), arg_tail)) continue;
      if (!plot_cmd_append(cmd, sizeof(cmd), " >NUL 2>NUL\"")) continue;
#else
      if (!plot_cmd_append_quoted(cmd, sizeof(cmd), py)) continue;
      if (!plot_cmd_append_quoted(cmd, sizeof(cmd), script)) continue;
      if (!plot_cmd_append(cmd, sizeof(cmd), arg_tail)) continue;
      if (!plot_cmd_append(cmd, sizeof(cmd), " >/dev/null 2>&1")) continue;
#endif

      if (plot_external_debug())
	 warn("Plot backend try: py='%s' script='%s' cmd=%s",
	      py, script, cmd);
      rc= system(cmd);
      if (plot_external_debug())
	 warn("Plot backend rc=%d out='%s' exists=%d",
	      rc, out_fname, file_exists_regular(out_fname));
      if (rc == 0 && file_exists_regular(out_fname)) {
	 ok= 1;
	 break;
      }
   }

   if (!ok && force_external)
      error("External Python/Cairo plot backend requested but unavailable or failed (set SBAGENX_PLOT_BACKEND=internal to force built-in plotting)");

   return ok;
}

int
try_external_sigmoid_graph_png(const char *out_fname,
			       int len0_sec,
			       double beat_start, double beat_target,
			       double sig_l, double sig_h,
			       double sig_a, double sig_b) {
   char script[PATH_MAX];
   char args[2048];
   int force_external= plot_external_force();

   if (!plot_find_script(script, sizeof(script))) {
      if (force_external)
	 error("External Python/Cairo plot backend requested but plot script was not found (set SBAGENX_PLOT_SCRIPT or use SBAGENX_PLOT_BACKEND=internal)");
      return 0;
   }

   snprintf(args, sizeof(args),
	    " sigmoid --out \"%s\" --drop-min %.12g --beat-start %.12g --beat-target %.12g --sig-l %.12g --sig-h %.12g --sig-a %.12g --sig-b %.12g",
	    out_fname, len0_sec / 60.0, beat_start, beat_target, sig_l, sig_h, sig_a, sig_b);

   return plot_try_external_cmd(script, out_fname, args);
}

int
try_external_drop_graph_png(const char *out_fname,
			    int len0_sec,
			    double beat_start, double beat_target,
			    int slide, int n_step, int steplen_sec,
			    int mode_kind) {
   char script[PATH_MAX];
   char args[2048];
   int force_external= plot_external_force();

   if (!plot_find_script(script, sizeof(script))) {
      if (force_external)
	 error("External Python/Cairo plot backend requested but plot script was not found (set SBAGENX_PLOT_SCRIPT or use SBAGENX_PLOT_BACKEND=internal)");
      return 0;
   }

   snprintf(args, sizeof(args),
	    " drop --out \"%s\" --drop-min %.12g --beat-start %.12g --beat-target %.12g --slide %d --n-step %d --step-len-sec %d --mode-kind %d",
	    out_fname, len0_sec / 60.0, beat_start, beat_target,
	    slide ? 1 : 0, n_step, steplen_sec, mode_kind);

   return plot_try_external_cmd(script, out_fname, args);
}

static int curve_plot_tmp_counter= 0;

static int
write_curve_plot_samples_file(const double *samples, int n,
			      char *out_path, int out_path_sz) {
   FILE *fp;
   int i;
   long now= (long)time(0);

   if (!samples || n < 2 || !out_path || out_path_sz < 8)
      return 0;

#ifdef T_MINGW
   {
      char tdir[PATH_MAX];
      char tfile[PATH_MAX];
      DWORD len= GetTempPathA((DWORD)sizeof(tdir), tdir);
      UINT trv= 0;
      if (len > 0 && len < sizeof(tdir)-1) {
	 trv= GetTempFileNameA(tdir, "sgx", 0, tfile);
      }
      if (trv != 0 && tfile[0]) {
	 int ncpy= (int)strlen(tfile);
	 if (ncpy >= out_path_sz) ncpy= out_path_sz-1;
	 memcpy(out_path, tfile, ncpy);
	 out_path[ncpy]= 0;
      } else {
	 snprintf(out_path, out_path_sz, "sbagenx_curve_%ld_%d.dat",
		  now, curve_plot_tmp_counter++);
      }
   }
#else
   snprintf(out_path, out_path_sz, "/tmp/sbagenx_curve_%ld_%ld_%d.dat",
	    (long)getpid(), now, curve_plot_tmp_counter++);
#endif

   fp= fopen(out_path, "w");
   if (!fp)
      return 0;
   for (i= 0; i<n; i++) {
      if (fprintf(fp, "%.12g\n", samples[i]) < 0) {
	 fclose(fp);
	 remove(out_path);
	 return 0;
      }
   }
   if (fclose(fp) != 0) {
      remove(out_path);
      return 0;
   }
   return 1;
}

int
try_external_curve_graph_png(const char *out_fname,
			     int len0_sec,
			     double beat_start, double beat_target,
			     int mode_kind,
			     int slide, int n_step, int steplen_sec,
			     const char *sample_file) {
   char script[PATH_MAX];
   char args[2048];
   int force_external= plot_external_force();

   if (!plot_find_script(script, sizeof(script))) {
      if (force_external)
	 error("External Python/Cairo plot backend requested but plot script was not found (set SBAGENX_PLOT_SCRIPT or use SBAGENX_PLOT_BACKEND=internal)");
      return 0;
   }

   if (!sample_file || !*sample_file || !file_exists_regular(sample_file))
      return 0;

   snprintf(args, sizeof(args),
	    " curve --out \"%s\" --drop-min %.12g --beat-start %.12g --beat-target %.12g --mode-kind %d --slide %d --n-step %d --step-len-sec %d --sample-file \"%s\"",
	    out_fname, len0_sec / 60.0, beat_start, beat_target, mode_kind,
	    slide ? 1 : 0, n_step, steplen_sec, sample_file);

   return plot_try_external_cmd(script, out_fname, args);
}

int
try_external_iso_cycle_graph_png(const char *out_fname,
				 double carr_hz, double pulse_hz,
				 double amp_pct, int waveform) {
   char script[PATH_MAX];
   char args[2048];
   int force_external= plot_external_force();

   if (!plot_find_script(script, sizeof(script))) {
      if (force_external)
	 error("External Python/Cairo plot backend requested but plot script was not found (set SBAGENX_PLOT_SCRIPT or use SBAGENX_PLOT_BACKEND=internal)");
      return 0;
   }

   snprintf(args, sizeof(args),
	    " iso-cycle --out \"%s\" --carrier-hz %.12g --pulse-hz %.12g --amp-pct %.12g --waveform %d --opt-i %d --i-s %.12g --i-d %.12g --i-a %.12g --i-r %.12g --i-e %d",
	    out_fname, carr_hz, pulse_hz, amp_pct, waveform, opt_I,
	    opt_I_s, opt_I_d, opt_I_a, opt_I_r, opt_I_e);

   return plot_try_external_cmd(script, out_fname, args);
}

int
try_external_mixam_cycle_graph_png(const char *out_fname,
				   int m, double s, double d, double a, double r,
				   int e, double f) {
   char script[PATH_MAX];
   char args[2048];
   int force_external= plot_external_force();

   if (!plot_find_script(script, sizeof(script))) {
      if (force_external)
	 error("External Python/Cairo plot backend requested but plot script was not found (set SBAGENX_PLOT_SCRIPT or use SBAGENX_PLOT_BACKEND=internal)");
      return 0;
   }

   snprintf(args, sizeof(args),
	    " mixam-cycle --out \"%s\" --h-m %d --h-s %.12g --h-d %.12g --h-a %.12g --h-r %.12g --h-e %d --h-f %.12g",
	    out_fname, m, s, d, a, r, e, f);

   return plot_try_external_cmd(script, out_fname, args);
}

static double
sigmoid_eval(double t_min, double d_min, double beat_target,
	     double sig_l, double sig_h, double sig_a, double sig_b) {
   if (t_min >= d_min) return beat_target;
   return sig_a * tanh(sig_l * (t_min - d_min/2 - sig_h)) + sig_b;
}

static int
build_integer_axis_ticks(double max_v, double *ticks, int max_ticks) {
   double step, xv;
   int n= 0;
   if (max_ticks < 2) return 0;
   if (max_v <= 0.0) {
      ticks[0]= 0.0;
      ticks[1]= 1.0;
      return 2;
   }
   step= floor(max_v / 10.0 + 0.5);
   if (step < 1.0) step= 1.0;
   xv= 0.0;
   while (xv <= max_v + 1e-9 && n < max_ticks) {
      ticks[n++]= xv;
      xv += step;
   }
   if (n < 1) ticks[n++]= 0.0;
   if (n < max_ticks && fabs(ticks[n-1] - max_v) > 1e-9)
      ticks[n++]= max_v;
   return n;
}

static double
drop_eval(double t_min, double d_min, double beat_start, double beat_target,
	  int slide, int n_step, int steplen_sec) {
   if (d_min <= 0.0) return beat_target;
   if (t_min < 0.0) t_min= 0.0;
   if (t_min > d_min) t_min= d_min;
   if (!(beat_start > 0.0))
      beat_start= 10.0;

   if (!slide && n_step > 1 && steplen_sec > 0) {
      int idx= (int)(t_min * 60.0 / steplen_sec);
      if (idx < 0) idx= 0;
      if (idx > n_step-1) idx= n_step-1;
      return beat_start * exp(log(beat_target/beat_start) * idx / (double)(n_step-1));
   }

   return beat_start * exp(log(beat_target/beat_start) * t_min / d_min);
}

static const char *
drop_mode_label(int mode_kind) {
   if (mode_kind == 2) return "monaural beat";
   if (mode_kind == 1) return "pulse";
   return "binaural beat";
}

void
write_drop_graph_png(const char *fmt, double level,
		     char depth_ch, int len0, int len1, int len2,
		     double beat_start, double beat_target,
		     int slide, int n_step, int steplen,
		     int isisochronic, int ismono) {
   int w= 1200, h= 700, ss= 4;
   int hw= w * ss, hh= h * ss;
   int ml= 150 * ss, mr= 40 * ss, mt= 40 * ss, mb= 120 * ss;
   int pw= hw - ml - mr, ph= hh - mt - mb;
   int a, gy;
   unsigned char *img_hi;
   unsigned char *img;
   double d_min= len0 / 60.0;
   double y_min= 1e30, y_max= -1e30;
   double y_pad, y_span;
   double y_tick_step, y_tick_first, y_tick_last;
   int start_y= -1, end_y= -1;
   int label_scale= 3 * ss;
   int tick_scale= 2 * ss;
   int param_scale= (3 * ss) / 2;
   int prev_x= -1, prev_y= -1;
   char fmt_tok[256], lvl_tok[64];
   char tick_txt[64];
   char ptxt1[256], ptxt2[256];
   char fname[512];
   char mode_ch, curve_ch;
   int mode_kind= ismono ? 2 : (isisochronic ? 1 : 0);
   const char *mode_label= drop_mode_label(mode_kind);
   double x_ticks[64];
   int nx;
   const char *x_label= "TIME MIN";
   const char *y_label= "FREQ HZ";

   if (pw < 10 || ph < 10)
      error("Graph dimensions are invalid");
   if (d_min <= 0.0)
      error("Drop graph requires a drop-time > 0");

   sanitize_filename_token(fmt, fmt_tok, sizeof(fmt_tok));
   double_to_token(level, lvl_tok, sizeof(lvl_tok));
   mode_ch= ismono ? 'm' : (isisochronic ? 'p' : 'b');
   curve_ch= slide ? 's' : 'k';
   snprintf(fname, sizeof(fname),
	    "drop_%s_L%s_d%c_t%d_%d_%d_%c%c.png",
	    fmt_tok, lvl_tok, tolower((unsigned char)depth_ch),
	    len0/60, len1/60, len2/60, mode_ch, curve_ch);

   if (try_external_drop_graph_png(fname, len0, beat_start, beat_target,
				   slide, n_step, steplen, mode_kind)) {
      if (!opt_Q)
	 warn("Drop graph saved to: %s (Python/Cairo)", fname);
      return;
   }

   img_hi= ALLOC_ARR(hw*hh*3, unsigned char);
   plot_fill(img_hi, hw, hh, 255, 255, 255);

   nx= build_integer_axis_ticks(d_min, x_ticks, sizeof(x_ticks)/sizeof(x_ticks[0]));
   if (nx < 2) {
      x_ticks[0]= 0.0;
      x_ticks[1]= d_min;
      nx= 2;
   }
   for (a= 0; a<nx; a++) {
      int gx= ml + (int)((pw-1) * x_ticks[a] / d_min + 0.5);
      plot_vline(img_hi, hw, hh, gx, mt, mt+ph-1, 236, 236, 236);
   }

   for (a= 0; a<=2000; a++) {
      double t_min= d_min * a / 2000.0;
      double y= drop_eval(t_min, d_min, beat_start, beat_target, slide, n_step, steplen);
      if (y < y_min) y_min= y;
      if (y > y_max) y_max= y;
   }
   if (beat_start < y_min) y_min= beat_start;
   if (beat_start > y_max) y_max= beat_start;
   if (beat_target < y_min) y_min= beat_target;
   if (beat_target > y_max) y_max= beat_target;

   if (y_max - y_min < 1e-6) {
      y_min -= 1.0;
      y_max += 1.0;
   }
   y_pad= (y_max - y_min) * 0.08;
   if (y_pad < 0.1) y_pad= 0.1;
   y_min -= y_pad;
   y_max += y_pad;
   y_span= y_max - y_min;
   y_tick_step= floor(y_span / 8.0 + 0.5);
   if (y_tick_step < 1.0) y_tick_step= 1.0;
   y_tick_first= ceil((y_min - 1e-9) / y_tick_step) * y_tick_step;
   y_tick_last= floor((y_max + 1e-9) / y_tick_step) * y_tick_step;

   {
      double yv;
      for (yv= y_tick_first; yv <= y_tick_last + y_tick_step*0.25; yv += y_tick_step) {
	 gy= mt + (int)((y_max - yv) * (ph-1) / y_span + 0.5);
	 plot_hline(img_hi, hw, hh, ml, ml+pw-1, gy, 236, 236, 236);
      }
   }

   plot_hline(img_hi, hw, hh, ml, ml+pw-1, mt, 90, 90, 90);
   plot_hline(img_hi, hw, hh, ml, ml+pw-1, mt+ph-1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml, mt, mt+ph-1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml+pw-1, mt, mt+ph-1, 90, 90, 90);

   for (a= 0; a<pw; a++) {
      double t_min= d_min * a / (double)(pw-1);
      double y= drop_eval(t_min, d_min, beat_start, beat_target, slide, n_step, steplen);
      int px= ml + a;
      int py= mt + (int)((y_max - y) * (ph-1) / y_span + 0.5);
      if (a == 0) start_y= py;
      if (a == pw-1) end_y= py;
      if (prev_x >= 0)
	 plot_line_thick(img_hi, hw, hh, prev_x, prev_y, px, py, ss+1, 34, 94, 224);
      prev_x= px;
      prev_y= py;
   }

   for (a= 0; a<nx; a++) {
      int gx= ml + (int)((pw-1) * x_ticks[a] / d_min + 0.5);
      int tw, tx, ty;
      format_tick_value(x_ticks[a], tick_txt, sizeof(tick_txt));
      plot_vline(img_hi, hw, hh, gx, mt+ph-1, mt+ph+4*ss, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= gx - tw/2;
      ty= mt + ph + 10*ss;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   {
      double yv;
      for (yv= y_tick_first; yv <= y_tick_last + y_tick_step*0.25; yv += y_tick_step) {
	 gy= mt + (int)((y_max - yv) * (ph-1) / y_span + 0.5);
	 int tw, tx, ty;
	 format_tick_value(yv, tick_txt, sizeof(tick_txt));
	 plot_hline(img_hi, hw, hh, ml-4*ss, ml, gy, 90, 90, 90);
	 tw= font5x7_text_width(tick_txt, tick_scale);
	 tx= ml - 8*ss - tw;
	 ty= gy - (7 * tick_scale) / 2;
	 font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
      }
   }

   for (a= -4*ss; a<=4*ss; a++) {
      plot_set_px(img_hi, hw, hh, ml + a, start_y, 220, 40, 40);
      plot_set_px(img_hi, hw, hh, ml + pw - 1 + a, end_y, 220, 40, 40);
   }

   {
      int xw= font5x7_text_width(x_label, label_scale);
      int x0= ml + (pw - xw) / 2;
      int y0= mt + ph + 24*ss;
      font5x7_draw_text(img_hi, hw, hh, x0, y0, x_label, label_scale, 0, 30, 30, 30);
   }

   {
      int yh= font5x7_text_height_rot90(y_label, label_scale);
      int x0= 20*ss;
      int y0= mt + (ph - yh) / 2;
      font5x7_draw_text(img_hi, hw, hh, x0, y0, y_label, label_scale, 1, 30, 30, 30);
   }

   snprintf(ptxt1, sizeof(ptxt1), "start=%.3fHz target=%.3fHz D=%dmin",
	    beat_start, beat_target, len0/60);
   if (slide)
      snprintf(ptxt2, sizeof(ptxt2), "%s mode: continuous exponential (s)", mode_label);
   else
      snprintf(ptxt2, sizeof(ptxt2), "%s mode: stepped exponential (k/default), step=%ds n=%d",
	       mode_label, steplen, n_step);
   {
      int tw1= font5x7_text_width(ptxt1, param_scale);
      int tw2= font5x7_text_width(ptxt2, param_scale);
      int tx1= ml + (pw - tw1) / 2;
      int tx2= ml + (pw - tw2) / 2;
      int phh= 7 * param_scale;
      int pgap= 4 * ss;
      int ty2= hh - (8 * ss + phh);
      int ty1= ty2 - (phh + pgap);
      font5x7_draw_text(img_hi, hw, hh, tx1, ty1, ptxt1, param_scale, 0, 30, 30, 30);
      font5x7_draw_text(img_hi, hw, hh, tx2, ty2, ptxt2, param_scale, 0, 30, 30, 30);
   }

   img= plot_downsample_box(img_hi, hw, hh, ss);
   free(img_hi);

   if (!stbi_write_png(fname, w, h, 3, img, w*3))
      error("Failed to write drop graph PNG file");
   if (!opt_Q)
      warn("Drop graph saved to: %s", fname);

   free(img);
}

void
write_curve_graph_png(const char *fmt, double level,
		      char depth_ch, int len0, int len1, int len2,
		      int slide, int n_step, int steplen,
		      int isisochronic, int ismono,
		      double beat_start, double beat_target) {
   int w= 1200, h= 700, ss= 4;
   int hw= w * ss, hh= h * ss;
   int ml= 150 * ss, mr= 40 * ss, mt= 40 * ss, mb= 120 * ss;
   int pw= hw - ml - mr, ph= hh - mt - mb;
   int a, gy;
   unsigned char *img_hi;
   unsigned char *img;
   double d_min= len0 / 60.0;
   double y_min= 1e30, y_max= -1e30;
   double y_pad, y_span;
   double y_tick_step, y_tick_first, y_tick_last;
   int start_y= -1, end_y= -1;
   int label_scale= 3 * ss;
   int tick_scale= 2 * ss;
   int param_scale= (3 * ss) / 2;
   int prev_x= -1, prev_y= -1;
   char fmt_tok[256], lvl_tok[64];
   char tick_txt[64];
   char ptxt1[256], ptxt2[256];
   char fname[512];
   char mode_ch, curve_ch;
   int mode_kind= ismono ? 2 : (isisochronic ? 1 : 0);
   const char *mode_label= drop_mode_label(mode_kind);
   double x_ticks[64];
   int nx;
   const char *x_label= "TIME MIN";
   const char *y_label= "FREQ HZ";
   int n_curve= 2001;
   double *curve_y= 0;
   char sample_file[PATH_MAX];

   if (pw < 10 || ph < 10)
      error("Graph dimensions are invalid");
   if (d_min <= 0.0)
      error("Curve graph requires a drop-time > 0");

   curve_y= ALLOC_ARR(n_curve, double);
   for (a= 0; a<n_curve; a++) {
      double tim= len0 * a / (double)(n_curve-1);
      double tim_eval= tim;
      double beatv, carrv, ampv, mixv;
      if (!slide && n_step > 1 && steplen > 0) {
	 int idx= (int)(tim / steplen);
	 if (idx < 0) idx= 0;
	 if (idx > n_step-1) idx= n_step-1;
	 tim_eval= idx * len0 / (double)(n_step-1);
      }
      if (!eval_custom_curve_point(tim_eval, &beatv, &carrv, &ampv, &mixv)) {
	 free(curve_y);
	 error("Custom curve evaluation failed at t=%g seconds while plotting", tim_eval);
      }
      curve_y[a]= beatv;
      if (beatv < y_min) y_min= beatv;
      if (beatv > y_max) y_max= beatv;
   }

   sanitize_filename_token(fmt, fmt_tok, sizeof(fmt_tok));
   double_to_token(level, lvl_tok, sizeof(lvl_tok));
   mode_ch= ismono ? 'm' : (isisochronic ? 'p' : 'b');
   curve_ch= slide ? 's' : 'k';
   snprintf(fname, sizeof(fname),
	    "curve_%s_L%s_d%c_t%d_%d_%d_%c%c.png",
	    fmt_tok, lvl_tok, tolower((unsigned char)depth_ch),
	    len0/60, len1/60, len2/60, mode_ch, curve_ch);

   sample_file[0]= 0;
   if (write_curve_plot_samples_file(curve_y, n_curve, sample_file, sizeof(sample_file))) {
      if (try_external_curve_graph_png(fname, len0,
				       beat_start, beat_target,
				       mode_kind, slide, n_step, steplen,
				       sample_file)) {
	 if (!opt_Q)
	    warn("Curve graph saved to: %s (Python/Cairo)", fname);
	 remove(sample_file);
	 free(curve_y);
	 return;
      }
      remove(sample_file);
   }

   if (beat_start < y_min) y_min= beat_start;
   if (beat_start > y_max) y_max= beat_start;
   if (beat_target < y_min) y_min= beat_target;
   if (beat_target > y_max) y_max= beat_target;
   if (y_max - y_min < 1e-6) {
      y_min -= 1.0;
      y_max += 1.0;
   }

   y_pad= (y_max - y_min) * 0.08;
   if (y_pad < 0.1) y_pad= 0.1;
   y_min -= y_pad;
   y_max += y_pad;
   y_span= y_max - y_min;
   y_tick_step= floor(y_span / 8.0 + 0.5);
   if (y_tick_step < 1.0) y_tick_step= 1.0;
   y_tick_first= ceil((y_min - 1e-9) / y_tick_step) * y_tick_step;
   y_tick_last= floor((y_max + 1e-9) / y_tick_step) * y_tick_step;

   img_hi= ALLOC_ARR(hw*hh*3, unsigned char);
   plot_fill(img_hi, hw, hh, 255, 255, 255);

   nx= build_integer_axis_ticks(d_min, x_ticks, sizeof(x_ticks)/sizeof(x_ticks[0]));
   if (nx < 2) {
      x_ticks[0]= 0.0;
      x_ticks[1]= d_min;
      nx= 2;
   }
   for (a= 0; a<nx; a++) {
      int gx= ml + (int)((pw-1) * x_ticks[a] / d_min + 0.5);
      plot_vline(img_hi, hw, hh, gx, mt, mt+ph-1, 236, 236, 236);
   }

   {
      double yv;
      for (yv= y_tick_first; yv <= y_tick_last + y_tick_step*0.25; yv += y_tick_step) {
	 gy= mt + (int)((y_max - yv) * (ph-1) / y_span + 0.5);
	 plot_hline(img_hi, hw, hh, ml, ml+pw-1, gy, 236, 236, 236);
      }
   }

   plot_hline(img_hi, hw, hh, ml, ml+pw-1, mt, 90, 90, 90);
   plot_hline(img_hi, hw, hh, ml, ml+pw-1, mt+ph-1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml, mt, mt+ph-1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml+pw-1, mt, mt+ph-1, 90, 90, 90);

   for (a= 0; a<pw; a++) {
      double pos= (n_curve-1) * a / (double)(pw-1);
      int i0= (int)floor(pos);
      int i1= i0 + 1;
      double rat, yv;
      int px= ml + a;
      int py;
      if (i0 < 0) i0= 0;
      if (i1 >= n_curve) i1= n_curve-1;
      rat= pos - i0;
      yv= curve_y[i0] + (curve_y[i1] - curve_y[i0]) * rat;
      py= mt + (int)((y_max - yv) * (ph-1) / y_span + 0.5);
      if (a == 0) start_y= py;
      if (a == pw-1) end_y= py;
      if (prev_x >= 0)
	 plot_line_thick(img_hi, hw, hh, prev_x, prev_y, px, py, ss+1, 34, 94, 224);
      prev_x= px;
      prev_y= py;
   }

   for (a= 0; a<nx; a++) {
      int gx= ml + (int)((pw-1) * x_ticks[a] / d_min + 0.5);
      int tw, tx, ty;
      format_tick_value(x_ticks[a], tick_txt, sizeof(tick_txt));
      plot_vline(img_hi, hw, hh, gx, mt+ph-1, mt+ph+4*ss, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= gx - tw/2;
      ty= mt + ph + 10*ss;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   {
      double yv;
      for (yv= y_tick_first; yv <= y_tick_last + y_tick_step*0.25; yv += y_tick_step) {
	 gy= mt + (int)((y_max - yv) * (ph-1) / y_span + 0.5);
	 int tw, tx, ty;
	 format_tick_value(yv, tick_txt, sizeof(tick_txt));
	 plot_hline(img_hi, hw, hh, ml-4*ss, ml, gy, 90, 90, 90);
	 tw= font5x7_text_width(tick_txt, tick_scale);
	 tx= ml - 8*ss - tw;
	 ty= gy - (7 * tick_scale) / 2;
	 font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
      }
   }

   for (a= -4*ss; a<=4*ss; a++) {
      plot_set_px(img_hi, hw, hh, ml + a, start_y, 220, 40, 40);
      plot_set_px(img_hi, hw, hh, ml + pw - 1 + a, end_y, 220, 40, 40);
   }

   {
      int xw= font5x7_text_width(x_label, label_scale);
      int x0= ml + (pw - xw) / 2;
      int y0= mt + ph + 24*ss;
      font5x7_draw_text(img_hi, hw, hh, x0, y0, x_label, label_scale, 0, 30, 30, 30);
   }

   {
      int yh= font5x7_text_height_rot90(y_label, label_scale);
      int x0= 20*ss;
      int y0= mt + (ph - yh) / 2;
      font5x7_draw_text(img_hi, hw, hh, x0, y0, y_label, label_scale, 1, 30, 30, 30);
   }

   snprintf(ptxt1, sizeof(ptxt1), "start=%.3fHz target=%.3fHz D=%dmin",
	    beat_start, beat_target, len0/60);
   if (slide)
      snprintf(ptxt2, sizeof(ptxt2), "%s mode: custom function curve (.sbgf), continuous (s)",
	       mode_label);
   else
      snprintf(ptxt2, sizeof(ptxt2), "%s mode: sampled custom curve (k/default), step=%ds n=%d",
	       mode_label, steplen, n_step);
   {
      int tw1= font5x7_text_width(ptxt1, param_scale);
      int tw2= font5x7_text_width(ptxt2, param_scale);
      int tx1= ml + (pw - tw1) / 2;
      int tx2= ml + (pw - tw2) / 2;
      int phh= 7 * param_scale;
      int pgap= 4 * ss;
      int ty2= hh - (8 * ss + phh);
      int ty1= ty2 - (phh + pgap);
      font5x7_draw_text(img_hi, hw, hh, tx1, ty1, ptxt1, param_scale, 0, 30, 30, 30);
      font5x7_draw_text(img_hi, hw, hh, tx2, ty2, ptxt2, param_scale, 0, 30, 30, 30);
   }

   img= plot_downsample_box(img_hi, hw, hh, ss);
   free(img_hi);

   if (!stbi_write_png(fname, w, h, 3, img, w*3))
      error("Failed to write curve graph PNG file");
   if (!opt_Q)
      warn("Curve graph saved to: %s", fname);

   free(img);
   free(curve_y);
}

void
write_sigmoid_graph_png(const char *fmt, double level,
			char depth_ch, int len0, int len1, int len2,
			double beat_start, double beat_target,
			double sig_l, double sig_h,
			double sig_a, double sig_b) {
   int w= 1200, h= 700, ss= 4;
   int hw= w * ss, hh= h * ss;
   int ml= 150 * ss, mr= 40 * ss, mt= 40 * ss, mb= 120 * ss;
   int pw= hw - ml - mr, ph= hh - mt - mb;
   int a, gy;
   unsigned char *img_hi;
   unsigned char *img;
   double d_min= len0 / 60.0;
   double y_min= 1e30, y_max= -1e30;
   double y_pad, y_span;
   double y_tick_step, y_tick_first, y_tick_last;
   int start_y= -1, end_y= -1;
   int label_scale= 3 * ss;
   int tick_scale= 2 * ss;
   int param_scale= (3 * ss) / 2;
   int prev_x= -1, prev_y= -1;
   char fmt_tok[256], lvl_tok[64], l_tok[64], h_tok[64];
   char tick_txt[64];
   char ptxt1[256], ptxt2[256];
   char fname[512];
   const char *x_label= "TIME MIN";
   const char *y_label= "FREQ HZ";

   if (pw < 10 || ph < 10)
      error("Graph dimensions are invalid");

   sanitize_filename_token(fmt, fmt_tok, sizeof(fmt_tok));
   double_to_token(level, lvl_tok, sizeof(lvl_tok));
   double_to_token(sig_l, l_tok, sizeof(l_tok));
   double_to_token(sig_h, h_tok, sizeof(h_tok));
   snprintf(fname, sizeof(fname),
	    "sigmoid_%s_L%s_d%c_t%d_%d_%d_l%s_h%s.png",
	    fmt_tok, lvl_tok, tolower((unsigned char)depth_ch),
	    len0/60, len1/60, len2/60, l_tok, h_tok);

   if (try_external_sigmoid_graph_png(fname, len0, beat_start, beat_target, sig_l, sig_h, sig_a, sig_b)) {
      if (!opt_Q)
	 warn("Sigmoid graph saved to: %s (Python/Cairo)", fname);
      return;
   }

   img_hi= ALLOC_ARR(hw*hh*3, unsigned char);
   plot_fill(img_hi, hw, hh, 255, 255, 255);

   for (a= 0; a<=10; a++) {
      int gx= ml + (pw-1) * a / 10;
      plot_vline(img_hi, hw, hh, gx, mt, mt+ph-1, 236, 236, 236);
   }

   for (a= 0; a<=2000; a++) {
      double t_min= d_min * a / 2000.0;
      double y= sigmoid_eval(t_min, d_min, beat_target, sig_l, sig_h, sig_a, sig_b);
      if (y < y_min) y_min= y;
      if (y > y_max) y_max= y;
   }
   if (beat_start < y_min) y_min= beat_start;
   if (beat_start > y_max) y_max= beat_start;
   if (beat_target < y_min) y_min= beat_target;
   if (beat_target > y_max) y_max= beat_target;

   if (y_max - y_min < 1e-6) {
      y_min -= 1.0;
      y_max += 1.0;
   }
   y_pad= (y_max - y_min) * 0.08;
   if (y_pad < 0.1) y_pad= 0.1;
   y_min -= y_pad;
   y_max += y_pad;
   y_span= y_max - y_min;
   // Use whole-number Y ticks for readability (match Python/Cairo renderer).
   y_tick_step= floor(y_span / 8.0 + 0.5);
   if (y_tick_step < 1.0) y_tick_step= 1.0;
   y_tick_first= ceil((y_min - 1e-9) / y_tick_step) * y_tick_step;
   y_tick_last= floor((y_max + 1e-9) / y_tick_step) * y_tick_step;

   {
      double yv;
      for (yv= y_tick_first; yv <= y_tick_last + y_tick_step*0.25; yv += y_tick_step) {
	 gy= mt + (int)((y_max - yv) * (ph-1) / y_span + 0.5);
	 plot_hline(img_hi, hw, hh, ml, ml+pw-1, gy, 236, 236, 236);
      }
   }

   plot_hline(img_hi, hw, hh, ml, ml+pw-1, mt, 90, 90, 90);
   plot_hline(img_hi, hw, hh, ml, ml+pw-1, mt+ph-1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml, mt, mt+ph-1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml+pw-1, mt, mt+ph-1, 90, 90, 90);

   for (a= 0; a<pw; a++) {
      double t_min= d_min * a / (double)(pw-1);
      double y= sigmoid_eval(t_min, d_min, beat_target, sig_l, sig_h, sig_a, sig_b);
      int px= ml + a;
      int py= mt + (int)((y_max - y) * (ph-1) / y_span + 0.5);
      if (a == 0) start_y= py;
      if (a == pw-1) end_y= py;
      if (prev_x >= 0)
	 plot_line_thick(img_hi, hw, hh, prev_x, prev_y, px, py, ss+1, 34, 94, 224);
      prev_x= px;
      prev_y= py;
   }

   for (a= 0; a<=10; a++) {
      int gx= ml + (pw-1) * a / 10;
      int tw, tx, ty;
      double xv= d_min * a / 10.0;
      format_tick_value(xv, tick_txt, sizeof(tick_txt));
      plot_vline(img_hi, hw, hh, gx, mt+ph-1, mt+ph+4*ss, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= gx - tw/2;
      ty= mt + ph + 10*ss;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   {
      double yv;
      for (yv= y_tick_first; yv <= y_tick_last + y_tick_step*0.25; yv += y_tick_step) {
	 gy= mt + (int)((y_max - yv) * (ph-1) / y_span + 0.5);
	 int tw, tx, ty;
	 format_tick_value(yv, tick_txt, sizeof(tick_txt));
	 plot_hline(img_hi, hw, hh, ml-4*ss, ml, gy, 90, 90, 90);
	 tw= font5x7_text_width(tick_txt, tick_scale);
	 tx= ml - 8*ss - tw;
	 ty= gy - (7 * tick_scale) / 2;
	 font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
      }
   }

   for (a= -4*ss; a<=4*ss; a++) {
      plot_set_px(img_hi, hw, hh, ml + a, start_y, 220, 40, 40);
      plot_set_px(img_hi, hw, hh, ml + pw - 1 + a, end_y, 220, 40, 40);
   }

   {
      int xw= font5x7_text_width(x_label, label_scale);
      int x0= ml + (pw - xw) / 2;
      int y0= mt + ph + 24*ss;
      font5x7_draw_text(img_hi, hw, hh, x0, y0, x_label, label_scale, 0, 30, 30, 30);
   }

   {
      int yh= font5x7_text_height_rot90(y_label, label_scale);
      int x0= 20*ss;
      int y0= mt + (ph - yh) / 2;
      font5x7_draw_text(img_hi, hw, hh, x0, y0, y_label, label_scale, 1, 30, 30, 30);
   }

   snprintf(ptxt1, sizeof(ptxt1), "start=%.3fHz target=%.3fHz D=%dmin",
	    beat_start, beat_target, len0/60);
   snprintf(ptxt2, sizeof(ptxt2), "l=%.4f h=%.4f a=%.4f b=%.4f",
	    sig_l, sig_h, sig_a, sig_b);
   {
      int tw1= font5x7_text_width(ptxt1, param_scale);
      int tw2= font5x7_text_width(ptxt2, param_scale);
      int tx1= ml + (pw - tw1) / 2;
      int tx2= ml + (pw - tw2) / 2;
      int phh= 7 * param_scale;
      int pgap= 4 * ss;
      int ty2= hh - (8 * ss + phh);
      int ty1= ty2 - (phh + pgap);
      font5x7_draw_text(img_hi, hw, hh, tx1, ty1, ptxt1, param_scale, 0, 30, 30, 30);
      font5x7_draw_text(img_hi, hw, hh, tx2, ty2, ptxt2, param_scale, 0, 30, 30, 30);
   }

   img= plot_downsample_box(img_hi, hw, hh, ss);
   free(img_hi);

   if (!stbi_write_png(fname, w, h, 3, img, w*3))
      error("Failed to write sigmoid graph PNG file");
   if (!opt_Q)
      warn("Sigmoid graph saved to: %s", fname);

   free(img);
}

void
write_mixam_cycle_graph_png(void) {
   int w= 1500, h= 900, ss= 4;
   int hw= w * ss, hh= h * ss;
   int ml= 130 * ss, mr= 40 * ss, mt= 40 * ss, mb= 160 * ss;
   int gap= 70 * ss;
   int pw= hw - ml - mr;
   int ph= (hh - mt - mb - gap) / 2;
   int top_y0= mt, top_y1= mt + ph - 1;
   int bot_y0= mt + ph + gap, bot_y1= mt + ph + gap + ph - 1;
   int tick_scale= 2 * ss;
   int label_scale= 3 * ss;
   int param_scale= 3 * ss;
   int a, i, prev_ex= -1, prev_ey= -1, prev_gx= -1, prev_gy= -1;
   unsigned char *img_hi;
   unsigned char *img;
   char s_tok[64], d_tok[64], a_tok[64], r_tok[64], f_tok[64], fname[512];
   char tick_txt[64];
   char ptxt1[256], ptxt2[256];
   const char *mode_name= (opt_H_m == SBX_MIXAM_MODE_COS) ? "cos" : "pulse";
   const char *title= (opt_H_m == SBX_MIXAM_MODE_COS)
      ? "MIXAM SINGLE-CYCLE CONTINUOUS-AM PLOT"
      : "MIXAM SINGLE-CYCLE ENVELOPE PLOT";

   sanitize_filename_token("mixam_cycle", fname, sizeof(fname));
   double_to_token(opt_H_s, s_tok, sizeof(s_tok));
   double_to_token(opt_H_d, d_tok, sizeof(d_tok));
   double_to_token(opt_H_a, a_tok, sizeof(a_tok));
   double_to_token(opt_H_r, r_tok, sizeof(r_tok));
   double_to_token(opt_H_f, f_tok, sizeof(f_tok));
   snprintf(fname, sizeof(fname),
	    "mixam_cycle_m%s_s%s_d%s_a%s_r%s_e%d_f%s.png",
	    mode_name, s_tok, d_tok, a_tok, r_tok, opt_H_e, f_tok);

   if (try_external_mixam_cycle_graph_png(fname,
					  opt_H_m,
					  opt_H_s, opt_H_d, opt_H_a, opt_H_r,
					  opt_H_e, opt_H_f)) {
      if (!opt_Q)
	 warn("Mixam cycle graph saved to: %s (Python/Cairo)", fname);
      return;
   }

   img_hi= ALLOC_ARR(hw*hh*3, unsigned char);
   plot_fill(img_hi, hw, hh, 255, 255, 255);
   plot_fill_rect(img_hi, hw, hh, ml, top_y0, ml+pw-1, top_y1, 250, 250, 250);
   plot_fill_rect(img_hi, hw, hh, ml, bot_y0, ml+pw-1, bot_y1, 250, 250, 250);

   for (i= 0; i<=10; i++) {
      int gx= ml + (pw-1) * i / 10;
      plot_vline(img_hi, hw, hh, gx, top_y0, top_y1, 236, 236, 236);
      plot_vline(img_hi, hw, hh, gx, bot_y0, bot_y1, 236, 236, 236);
   }
   for (i= 0; i<=10; i++) {
      int gy= top_y0 + (ph-1) * i / 10;
      plot_hline(img_hi, hw, hh, ml, ml+pw-1, gy, 236, 236, 236);
   }
   for (i= 0; i<=10; i++) {
      int gy= bot_y0 + (ph-1) * i / 10;
      plot_hline(img_hi, hw, hh, ml, ml+pw-1, gy, 236, 236, 236);
   }

   plot_hline(img_hi, hw, hh, ml, ml+pw-1, top_y0, 90, 90, 90);
   plot_hline(img_hi, hw, hh, ml, ml+pw-1, top_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml, top_y0, top_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml+pw-1, top_y0, top_y1, 90, 90, 90);

   plot_hline(img_hi, hw, hh, ml, ml+pw-1, bot_y0, 90, 90, 90);
   plot_hline(img_hi, hw, hh, ml, ml+pw-1, bot_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml, bot_y0, bot_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml+pw-1, bot_y0, bot_y1, 90, 90, 90);

   for (a= 0; a<pw; a++) {
      int ex= ml + a;
      double phase= a / (double)(pw-1);
      double env;
      if (opt_H_m == SBX_MIXAM_MODE_COS) {
	 double phx= sbx_dsp_wrap_unit(phase + opt_H_s);
	 env= 0.5 * (1.0 + cos(2.0 * M_PI * phx));
      } else {
	 env= sbx_dsp_iso_mod_factor_custom(phase, opt_H_s, opt_H_d,
					    opt_H_a, opt_H_r, opt_H_e);
      }
      double gain= opt_H_f + (1.0 - opt_H_f) * env;
      int ey= top_y0 + (int)((1.0 - env) * (ph-1) + 0.5);
      int gy= bot_y0 + (int)((1.0 - gain) * (ph-1) + 0.5);

      if (prev_ex >= 0) {
	 plot_line_thick(img_hi, hw, hh, prev_ex, prev_ey, ex, ey, ss+1, 220, 40, 40);
	 plot_line_thick(img_hi, hw, hh, prev_gx, prev_gy, ex, gy, ss+1, 34, 94, 224);
      }
      prev_ex= ex; prev_ey= ey;
      prev_gx= ex; prev_gy= gy;
   }

   for (i= 0; i<=10; i++) {
      int gx= ml + (pw-1) * i / 10;
      double xv= i / 10.0;
      int tx, tw;
      format_tick_value(xv, tick_txt, sizeof(tick_txt));
      plot_vline(img_hi, hw, hh, gx, bot_y1, bot_y1 + 4*ss, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= gx - tw/2;
      font5x7_draw_text(img_hi, hw, hh, tx, bot_y1 + 10*ss, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   for (i= 0; i<=10; i++) {
      double yv= 1.0 - i * 0.1;
      int gy= top_y0 + (int)((1.0 - yv) * (ph-1) + 0.5);
      int tw, tx, ty;
      format_tick_value(yv, tick_txt, sizeof(tick_txt));
      plot_hline(img_hi, hw, hh, ml - 4*ss, ml, gy, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= ml - 8*ss - tw;
      ty= gy - (7 * tick_scale) / 2;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   for (i= 0; i<=10; i++) {
      double yv= 1.0 - i * 0.1;
      int gy= bot_y0 + (int)((1.0 - yv) * (ph-1) + 0.5);
      int tw, tx, ty;
      format_tick_value(yv, tick_txt, sizeof(tick_txt));
      plot_hline(img_hi, hw, hh, ml - 4*ss, ml, gy, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= ml - 8*ss - tw;
      ty= gy - (7 * tick_scale) / 2;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   {
      int tw= font5x7_text_width(title, label_scale);
      int tx= ml + (pw - tw) / 2;
      font5x7_draw_text(img_hi, hw, hh, tx, 8*ss, title, label_scale, 0, 25, 25, 25);
   }
   {
      const char *x_label= "CYCLE";
      int tw= font5x7_text_width(x_label, label_scale);
      int tx= ml + (pw - tw) / 2;
      int ty= bot_y1 + 32*ss;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, x_label, label_scale, 0, 30, 30, 30);
   }
   {
      const char *y1_label= "ENVELOPE";
      const char *y2_label= "GAIN";
      int yh1= font5x7_text_height_rot90(y1_label, label_scale);
      int yh2= font5x7_text_height_rot90(y2_label, label_scale);
      int x0= 18 * ss;
      int y01= top_y0 + (ph - yh1) / 2;
      int y02= bot_y0 + (ph - yh2) / 2;
      font5x7_draw_text(img_hi, hw, hh, x0, y01, y1_label, label_scale, 1, 30, 30, 30);
      font5x7_draw_text(img_hi, hw, hh, x0, y02, y2_label, label_scale, 1, 30, 30, 30);
   }

   if (opt_H_m == SBX_MIXAM_MODE_COS)
      snprintf(ptxt1, sizeof(ptxt1), "H:m=cos s=%.4f f=%.3f", opt_H_s, opt_H_f);
   else
      snprintf(ptxt1, sizeof(ptxt1), "H:m=pulse s=%.4f d=%.4f a=%.2f r=%.2f e=%d f=%.3f",
	       opt_H_s, opt_H_d, opt_H_a, opt_H_r, opt_H_e, opt_H_f);
   snprintf(ptxt2, sizeof(ptxt2), "mixam cycle gain = f + (1-f)*envelope");
   {
      int tw1= font5x7_text_width(ptxt1, param_scale);
      int tw2= font5x7_text_width(ptxt2, param_scale);
      int tx1= ml + (pw - tw1) / 2;
      int tx2= ml + (pw - tw2) / 2;
      int ty1= hh - 66 * ss;
      int ty2= hh - 38 * ss;
      font5x7_draw_text(img_hi, hw, hh, tx1, ty1, ptxt1, param_scale, 0, 30, 30, 30);
      font5x7_draw_text(img_hi, hw, hh, tx2, ty2, ptxt2, param_scale, 0, 30, 30, 30);
   }

   img= plot_downsample_box(img_hi, hw, hh, ss);
   free(img_hi);

   if (!stbi_write_png(fname, w, h, 3, img, w*3))
      error("Failed to write mixam cycle graph PNG file");
   if (!opt_Q)
      warn("Mixam cycle graph saved to: %s", fname);
   free(img);
}

static int
find_first_iso_voice(Voice *out) {
   Period *pp;
   int ch;
   if (!per) return 0;
   pp= per;
   do {
      for (ch= 0; ch<N_CH; ch++) {
	 if (pp->v0[ch].typ == 8 && pp->v0[ch].amp > 0.0) {
	    *out= pp->v0[ch];
	    return 1;
	 }
      }
      for (ch= 0; ch<N_CH; ch++) {
	 if (pp->v1[ch].typ == 8 && pp->v1[ch].amp > 0.0) {
	    *out= pp->v1[ch];
	    return 1;
	 }
      }
      pp= pp->nxt;
   } while (pp != per);
   return 0;
}

static int
find_first_iso_plot_tone(double *out_carr_hz, double *out_pulse_hz,
			 double *out_amp_pct, int *out_waveform) {
   if (sbx_runtime_ctx) {
      size_t kf_n= sbx_context_keyframe_count(sbx_runtime_ctx);
      size_t voice_n= sbx_context_voice_count(sbx_runtime_ctx);
      size_t aux_n, i, v;
      SbxToneSpec tone;

      for (i= 0; i<kf_n; i++) {
	 for (v= 0; v<voice_n; v++) {
	    SbxProgramKeyframe kf;
	    if (SBX_OK == sbx_context_get_keyframe_voice(sbx_runtime_ctx, i, v, &kf) &&
		kf.tone.mode == SBX_TONE_ISOCHRONIC &&
		kf.tone.amplitude > 0.0 && fabs(kf.tone.beat_hz) > 0.0) {
	       if (out_carr_hz) *out_carr_hz= kf.tone.carrier_hz;
	       if (out_pulse_hz) *out_pulse_hz= fabs(kf.tone.beat_hz);
	       if (out_amp_pct) *out_amp_pct= 100.0 * kf.tone.amplitude;
	       if (out_waveform) *out_waveform= kf.tone.waveform;
	       return 1;
	    }
	 }
      }

      if (SBX_OK == sbx_context_sample_tones(sbx_runtime_ctx, 0.0, 0.0, 1, 0, &tone) &&
	  tone.mode == SBX_TONE_ISOCHRONIC &&
	  tone.amplitude > 0.0 && fabs(tone.beat_hz) > 0.0) {
	 if (out_carr_hz) *out_carr_hz= tone.carrier_hz;
	 if (out_pulse_hz) *out_pulse_hz= fabs(tone.beat_hz);
	 if (out_amp_pct) *out_amp_pct= 100.0 * tone.amplitude;
	 if (out_waveform) *out_waveform= tone.waveform;
	 return 1;
      }

      aux_n= sbx_context_aux_tone_count(sbx_runtime_ctx);
      for (i= 0; i<aux_n; i++) {
	 if (SBX_OK == sbx_context_get_aux_tone(sbx_runtime_ctx, i, &tone) &&
	     tone.mode == SBX_TONE_ISOCHRONIC &&
	     tone.amplitude > 0.0 && fabs(tone.beat_hz) > 0.0) {
	    if (out_carr_hz) *out_carr_hz= tone.carrier_hz;
	    if (out_pulse_hz) *out_pulse_hz= fabs(tone.beat_hz);
	    if (out_amp_pct) *out_amp_pct= 100.0 * tone.amplitude;
	    if (out_waveform) *out_waveform= tone.waveform;
	    return 1;
	 }
      }
   }

   {
      Voice vv;
      if (find_first_iso_voice(&vv)) {
	 if (out_carr_hz) *out_carr_hz= vv.carr;
	 if (out_pulse_hz) *out_pulse_hz= fabs(vv.res);
	 if (out_amp_pct) *out_amp_pct= AMP_AD(vv.amp);
	 if (out_waveform) *out_waveform= vv.waveform;
	 return 1;
      }
   }

   return 0;
}

static void
write_iso_cycle_graph_png(double carr_hz, double pulse_hz,
			  double amp_pct, int waveform) {
   int w= 1600, h= 980, ss= 4;
   int hw= w * ss, hh= h * ss;
   int ml= 130 * ss, mr= 40 * ss, mt= 40 * ss, mb= 160 * ss;
   int gap= 70 * ss;
   int pw= hw - ml - mr;
   int ph= (hh - mt - mb - gap) / 2;
   int top_y0= mt, top_y1= mt + ph - 1;
   int bot_y0= mt + ph + gap, bot_y1= mt + ph + gap + ph - 1;
   int tick_scale= 2 * ss;
   int label_scale= 3 * ss;
   int param_scale= 3 * ss;
   int a, i, prev_ex= -1, prev_ey= -1, prev_wx= -1, prev_wy= -1;
   unsigned char *img_hi;
   unsigned char *img;
   double period_sec;
   char carr_tok[64], pulse_tok[64], amp_tok[64];
   char wave_tok[64], tick_txt[64], fname[512];
   char ptxt1[256], ptxt2[256];
   const char *title= "ISOCHRONIC SINGLE-CYCLE PLOT";

   if (pulse_hz <= 0.0)
      error("-P requires an isochronic pulse frequency > 0 Hz");

   if (waveform < 0 || waveform > 3)
      waveform= 0;

   period_sec= 1.0 / pulse_hz;
   if (period_sec <= 0.0)
      error("-P calculated an invalid cycle period");

   sanitize_filename_token(waveform_name[waveform], wave_tok, sizeof(wave_tok));
   double_to_token(carr_hz, carr_tok, sizeof(carr_tok));
   double_to_token(pulse_hz, pulse_tok, sizeof(pulse_tok));
   double_to_token(amp_pct, amp_tok, sizeof(amp_tok));
   if (opt_I) {
      snprintf(fname, sizeof(fname),
	       "iso_cycle_%s_c%s_p%s_a%s_i%d.png",
	       wave_tok, carr_tok, pulse_tok, amp_tok, opt_I_e);
   } else {
      snprintf(fname, sizeof(fname),
	       "iso_cycle_%s_c%s_p%s_a%s_default.png",
	       wave_tok, carr_tok, pulse_tok, amp_tok);
   }

   if (try_external_iso_cycle_graph_png(fname, carr_hz, pulse_hz, amp_pct, waveform)) {
      if (!opt_Q)
	 warn("Isochronic cycle graph saved to: %s (Python/Cairo)", fname);
      return;
   }

   img_hi= ALLOC_ARR(hw*hh*3, unsigned char);
   plot_fill(img_hi, hw, hh, 255, 255, 255);
   plot_fill_rect(img_hi, hw, hh, ml, top_y0, ml+pw-1, top_y1, 250, 250, 250);
   plot_fill_rect(img_hi, hw, hh, ml, bot_y0, ml+pw-1, bot_y1, 250, 250, 250);

   for (i= 0; i<=10; i++) {
      int gx= ml + (pw-1) * i / 10;
      plot_vline(img_hi, hw, hh, gx, top_y0, top_y1, 236, 236, 236);
      plot_vline(img_hi, hw, hh, gx, bot_y0, bot_y1, 236, 236, 236);
   }
   for (i= 0; i<=10; i++) {
      int gy= top_y0 + (ph-1) * i / 10;
      plot_hline(img_hi, hw, hh, ml, ml+pw-1, gy, 236, 236, 236);
   }
   for (i= 0; i<=10; i++) {
      int gy= bot_y0 + (ph-1) * i / 10;
      plot_hline(img_hi, hw, hh, ml, ml+pw-1, gy, 236, 236, 236);
   }

   plot_hline(img_hi, hw, hh, ml, ml+pw-1, top_y0, 90, 90, 90);
   plot_hline(img_hi, hw, hh, ml, ml+pw-1, top_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml, top_y0, top_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml+pw-1, top_y0, top_y1, 90, 90, 90);

   plot_hline(img_hi, hw, hh, ml, ml+pw-1, bot_y0, 90, 90, 90);
   plot_hline(img_hi, hw, hh, ml, ml+pw-1, bot_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml, bot_y0, bot_y1, 90, 90, 90);
   plot_vline(img_hi, hw, hh, ml+pw-1, bot_y0, bot_y1, 90, 90, 90);

   for (a= 0; a<pw; a++) {
      int ex= ml + a;
      double t= period_sec * a / (double)(pw-1);
      double pulse_phase= pulse_hz * t;
      double carr_phase= carr_hz * t;
      double env= opt_I ?
	 isochronic_mod_factor_phase_custom(pulse_phase, opt_I_s, opt_I_d, opt_I_a, opt_I_r, opt_I_e) :
	 isochronic_mod_factor_phase_legacy(pulse_phase, waveform);
      double wav= wave_sample_phase(waveform, carr_phase) * env * (amp_pct / 100.0);
      int ey= top_y0 + (int)((1.0 - env) * (ph-1) + 0.5);
      int wy= bot_y0 + (int)((1.0 - ((wav + 1.0) * 0.5)) * (ph-1) + 0.5);

      if (prev_ex >= 0) {
	 plot_line_thick(img_hi, hw, hh, prev_ex, prev_ey, ex, ey, ss+1, 220, 40, 40);
	 plot_line_thick(img_hi, hw, hh, prev_wx, prev_wy, ex, wy, ss+1, 34, 94, 224);
      }
      prev_ex= ex; prev_ey= ey;
      prev_wx= ex; prev_wy= wy;
   }

   for (i= 0; i<=10; i++) {
      int gx= ml + (pw-1) * i / 10;
      double xv= period_sec * i / 10.0;
      int tx, tw;
      format_tick_value(xv, tick_txt, sizeof(tick_txt));
      plot_vline(img_hi, hw, hh, gx, bot_y1, bot_y1 + 4*ss, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= gx - tw/2;
      font5x7_draw_text(img_hi, hw, hh, tx, bot_y1 + 10*ss, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   for (i= 0; i<=10; i++) {
      double yv= 1.0 - i * 0.1;
      int gy= top_y0 + (int)((1.0 - yv) * (ph-1) + 0.5);
      int tw, tx, ty;
      format_tick_value(yv, tick_txt, sizeof(tick_txt));
      plot_hline(img_hi, hw, hh, ml - 4*ss, ml, gy, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= ml - 8*ss - tw;
      ty= gy - (7 * tick_scale) / 2;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   for (i= 0; i<=10; i++) {
      double yv= 1.0 - i * 0.2;  /* +1 down to -1 */
      int gy= bot_y0 + (int)((1.0 - ((yv + 1.0) * 0.5)) * (ph-1) + 0.5);
      int tw, tx, ty;
      format_tick_value(yv, tick_txt, sizeof(tick_txt));
      plot_hline(img_hi, hw, hh, ml - 4*ss, ml, gy, 90, 90, 90);
      tw= font5x7_text_width(tick_txt, tick_scale);
      tx= ml - 8*ss - tw;
      ty= gy - (7 * tick_scale) / 2;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, tick_txt, tick_scale, 0, 30, 30, 30);
   }

   {
      int tw= font5x7_text_width(title, label_scale);
      int tx= ml + (pw - tw) / 2;
      font5x7_draw_text(img_hi, hw, hh, tx, 8*ss, title, label_scale, 0, 25, 25, 25);
   }
   {
      const char *x_label= "TIME SEC";
      int tw= font5x7_text_width(x_label, label_scale);
      int tx= ml + (pw - tw) / 2;
      int ty= bot_y1 + 32*ss;
      font5x7_draw_text(img_hi, hw, hh, tx, ty, x_label, label_scale, 0, 30, 30, 30);
   }
   {
      const char *y1_label= "ENVELOPE";
      const char *y2_label= "WAVEFORM";
      int yh1= font5x7_text_height_rot90(y1_label, label_scale);
      int yh2= font5x7_text_height_rot90(y2_label, label_scale);
      int x0= 18 * ss;
      int y01= top_y0 + (ph - yh1) / 2;
      int y02= bot_y0 + (ph - yh2) / 2;
      font5x7_draw_text(img_hi, hw, hh, x0, y01, y1_label, label_scale, 1, 30, 30, 30);
      font5x7_draw_text(img_hi, hw, hh, x0, y02, y2_label, label_scale, 1, 30, 30, 30);
   }

   snprintf(ptxt1, sizeof(ptxt1), "c=%.1fHz p=%.2fHz a=%.1f%% w=%s",
	    carr_hz, pulse_hz, amp_pct, waveform_name[waveform]);
   if (opt_I) {
      snprintf(ptxt2, sizeof(ptxt2), "I:s=%.4f d=%.4f a=%.2f r=%.2f e=%d",
	       opt_I_s, opt_I_d, opt_I_a, opt_I_r, opt_I_e);
   } else {
      snprintf(ptxt2, sizeof(ptxt2), "I=default threshold gate");
   }
   {
      int tw1= font5x7_text_width(ptxt1, param_scale);
      int tw2= font5x7_text_width(ptxt2, param_scale);
      int tx1= ml + (pw - tw1) / 2;
      int tx2= ml + (pw - tw2) / 2;
      int ty1= hh - 66 * ss;
      int ty2= hh - 38 * ss;
      font5x7_draw_text(img_hi, hw, hh, tx1, ty1, ptxt1, param_scale, 0, 30, 30, 30);
      font5x7_draw_text(img_hi, hw, hh, tx2, ty2, ptxt2, param_scale, 0, 30, 30, 30);
   }

   img= plot_downsample_box(img_hi, hw, hh, ss);
   free(img_hi);

   if (!stbi_write_png(fname, w, h, 3, img, w*3))
      error("Failed to write isochronic cycle graph PNG file");
   if (!opt_Q)
      warn("Isochronic cycle graph saved to: %s", fname);
   free(img);
}

void
write_iso_cycle_graph_png_from_sequence(void) {
   double carr_hz, pulse_hz, amp_pct;
   int waveform;
   if (!find_first_iso_plot_tone(&carr_hz, &pulse_hz, &amp_pct, &waveform))
      error("-P requires at least one isochronic (@) tone in the loaded sequence");
   write_iso_cycle_graph_png(carr_hz, pulse_hz, amp_pct, waveform);
}

static void
init_program_dir(char *argv0) {
   char path[PATH_MAX];
   char *src= argv0 && *argv0 ? argv0 : ".";
   char *p;

#ifdef WIN_MISC
   {
      DWORD len= GetModuleFileNameA(0, path, sizeof(path)-1);
      if (len > 0 && len < sizeof(path)) {
	 path[len]= 0;
	 src= path;
      }
   }
#else
   if (src && !strchr(src, '/') && !strchr(src, '\\')) {
      ssize_t len= readlink("/proc/self/exe", path, sizeof(path)-1);
      if (len > 0 && len < sizeof(path)) {
	 path[len]= 0;
	 src= path;
      }
   }
#endif

   pdir= StrDup(src);
   p= strchr(pdir, 0);
   while (p > pdir && p[-1] != '/' && p[-1] != '\\') *--p= 0;

   if (!*pdir) {
      free(pdir);
#ifdef WIN_MISC
      pdir= StrDup(".\\");
#else
      pdir= StrDup("./");
#endif
   }
}

//
//	M A I N
//

int 
main(int argc, char **argv) {
   short test= 0x1100;
   int rv;
   
   init_program_dir(argv[0]);

   argc--; argv++;
   bigendian= ((char*)&test)[0] != 0;
   
   // Process all the options
   rv= scanOptions(&argc, &argv);

   if (argc < 1) usage();
   
   if (rv == 'i') {
      // Immediate mode
      if (opt_G)
	 error("-G is only supported with -p drop/-p sigmoid/-p curve");
      readSeqImm(argc, argv);
   } else if (rv == 'p') {
      // Pre-programmed sequence
      readPreProg(argc, argv);
      if (opt_G || opt_P_sigmoid || opt_P_drop || opt_P_curve || (opt_P && opt_H)) return 0;
   } else {
      // Sequenced mode -- sequence may include options, so options
      // are not settled until below this point
      if (opt_G)
	 error("-G is only supported with -p drop/-p sigmoid/-p curve");
      if (argc < 1) usage();
      readSeq(argc, argv);
   }

   init_sin_table();
   init_mixbeat_hilbert();

   if (opt_P) {
      if (opt_H)
	 write_mixam_cycle_graph_png();
      else
	 write_iso_cycle_graph_png_from_sequence();
      return 0;
   }
   
   if (opt_W && !opt_o && !opt_O)
      error("Use -o or -O with the -W option");
   if (opt_W && opt_L < 0) {
      if (!opt_E) {
         fprintf(stderr, "*** The length has not been specified for the -W option; enabling -E option ***\n");
         fprintf(stderr, "(WAV file will have the same duration as the sequence)\n\n");
         opt_E = 1;  // Automatically enable -E option
      }
   }
   
   mix_in= 0;
   if (opt_M || opt_m) {
      char *p;
      char tmp[8];
      int raw= 1;
      tmp[0]= 0;
      if (opt_M) {
	 mix_in= stdin; 
	 tmp[0]= 0;
      } 
      if (opt_m) {
	 // Pick up #<digits> on end of filename
	 p= strchr(opt_m, 0);
	 mix_cnt= -1;
	 if (p > opt_m && isdigit(p[-1])) {
	    mix_cnt= 0;
	    while (p > opt_m && isdigit(p[-1]))
	       mix_cnt= mix_cnt * 10 + *--p - '0';
	    if (p > opt_m && p[-1] == '#') 
	       *--p= 0;
	    else {
	       p= strchr(opt_m, 0);
	       mix_cnt= -1;
	    } 
	 }
	 // p points to end of filename (NUL)

	 // Open file
	 mix_in= fopen(opt_m, "rb");
	 if (!mix_in && opt_m[0] != '/') {
	    int len= strlen(opt_m) + strlen(pdir) + 1;
	    char *tmp= ALLOC_ARR(len, char);
	    strcpy(tmp, pdir);
	    strcat(tmp, opt_m);
	    mix_in= fopen(tmp, "rb");
	    free(tmp);
	 }
	 if (!mix_in)
	    error("Can't open -m option mix input file: %s", opt_m);

	 // Pick up extension
	 {
	    char *dot= strrchr(opt_m, '.');
	    char *slash= strrchr(opt_m, '/');
	    char *bslash= strrchr(opt_m, '\\');
	    char *sep= slash > bslash ? slash : bslash;
	    if (dot && (!sep || dot > sep) && dot + 1 < p) {
	       int a, len= p - (dot + 1);
	       if (len >= (int)sizeof(tmp))
		  len= sizeof(tmp)-1;
	       for (a= 0; a<len; a++)
		  tmp[a]= tolower((unsigned char)dot[1+a]);
	       tmp[len]= 0;
	    }
	 }
      }
      if (0 == strcmp(tmp, "wav"))	// Skip header on WAV files
	 find_wav_data_start(mix_in);
      if (0 == strcmp(tmp, "ogg")) {
#ifdef OGG_DECODE
	 ogg_init(); raw= 0;
#else
	 error("Sorry: Ogg support wasn't compiled into this executable");
#endif
      }
      if (0 == strcmp(tmp, "mp3")) {
#ifdef MP3_DECODE
	 mp3_init(); raw= 0;
#else
	 error("Sorry: MP3 support wasn't compiled into this executable");
#endif
      }
      if (0 == strcmp(tmp, "flac")) {
#ifdef FLAC_DECODE
	 flac_init(); raw= 0;
#else
	 error("Sorry: FLAC support wasn't compiled into this executable");
#endif
      }
      // If this is a raw/wav data stream, setup a 256*1024-int
      // buffer (3s@44.1kHz)
      if (raw) inbuf_start(raw_mix_in, 256*1024);
   }

   if (opt_A && !mix_in)
      error("-A requires a mix input stream; use -m <file> or -M");
   
   loop();
   
#ifdef ALSA_AUDIO
   cleanup_alsa();
#endif
#ifdef MAC_AUDIO
   cleanup_mac_audio();
#endif

   return 0;
}

//
//	Scan options.  Returns a flag indicating what is expected to
//	interpret the rest of the arguments: 0 normal, 'i' immediate
//	(-i option), 'p' -p option.
//

int 
scanOptions(int *acp, char ***avp) {
   int argc= *acp;
   char **argv= *avp;
   int val;
   double dval;
   char dmy;
   int rv= 0;

   // Scan options
   while (argc > 0 && argv[0][0] == '-' && argv[0][1]) {
      char opt, *p= 1 + *argv++; argc--;
      while ((opt= *p++)) {
	 // Check options that are available on both
	 switch (opt) {
	  case 'Q': opt_Q= 1; break;
	  case 'E': opt_E= 1; break;
	  case 'm':
	     if (argc-- < 1) error("-m option expects filename");
	     // Earliest takes precedence, so command-line overrides sequence file
	     if (!opt_m) opt_m= *argv++;
	     break;
	  case 'A':
	     opt_A= 1;
	     if (*p) {
		if (!is_mix_mod_option_spec(p))
		   error("-A optional spec must be d=<v>:e=<v>:k=<v>:E=<v>");
		parse_mix_mod_option_spec(p);
		p += strlen(p);
	     } else if (argc > 0 && is_mix_mod_option_spec(argv[0])) {
		parse_mix_mod_option_spec(*argv++);
		argc--;
	     }
	     break;
	  case 'I':
	     opt_I= 1;
	     if (*p) {
		if (!is_iso_gate_option_spec(p))
		   error("-I optional spec must be s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>");
		parse_iso_gate_option_spec(p);
		p += strlen(p);
	     } else if (argc > 0 && is_iso_gate_option_spec(argv[0])) {
		parse_iso_gate_option_spec(*argv++);
		argc--;
	     }
	     break;
	  case 'H':
	     opt_H= 1;
	     if (*p) {
		 if (!is_mixam_env_option_spec(p))
		    error("-H optional spec must be m=<pulse|cos>:s=<start-cycle>:d=<duty>:a=<attack>:r=<release>:e=<edge>:f=<floor>");
		parse_mixam_env_option_spec(p);
		p += strlen(p);
	     } else if (argc > 0 && is_mixam_env_option_spec(argv[0])) {
		parse_mixam_env_option_spec(*argv++);
		argc--;
	     }
	     break;
	  case 'S': opt_S= 1;
	     if (!fast_mult) fast_mult= 1; 		// Don't try to sync with real time
	     break;
	  case 'L':
	     if (argc-- < 1 || 0 == (val= readTime(*argv, &opt_L)) || 
		 1 == sscanf(*argv++ + val, " %c", &dmy)) 
		error("-L expects hh:mm or hh:mm:ss time");
	     break;
	  case 'T':
	     if (argc-- < 1 || 0 == (val= readTime(*argv, &opt_T)) || 
		 1 == sscanf(*argv++ + val, " %c", &dmy)) 
		error("-T expects hh:mm or hh:mm:ss time");
	     if (!fast_mult) fast_mult= 1;		// Don't try to sync with real time
	     break;
	  case 'F':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &fade_int, &dmy)) 
		error("-F expects fade-time in ms");
	     break;
#ifdef MAC_AUDIO
	  case 'B':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &opt_B, &dmy)) 
		      error("-B expects buffer size in samples");
       if (opt_B < 1024 || opt_B > BUFFER_SIZE / 2)
          error("Buffer size must be between 1024 and %d samples.", BUFFER_SIZE / 2);
       if ((opt_B & (opt_B - 1)) != 0)
          error("Buffer size must be a power of 2. (e.g. 1024, 2048, 4096, etc.)");
       opt_B *= 2;
	     break;
#endif
	  case 'N': opt_N= 0; break;
	  case 'V':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &opt_V, &dmy)) 
		    error("-V expects volume level in percent (0-100)");
	     if (opt_V < 0 || opt_V > 100)
		    error("Volume level must be between 0 and 100");
	     break;
	  case 'w':
	     if (argc-- < 1) error("-w expects waveform type (sine, square, triangle, sawtooth)");
       if (0 == strcmp(*argv, "sine")) opt_w= 0;
       else if (0 == strcmp(*argv, "square")) opt_w= 1;
       else if (0 == strcmp(*argv, "triangle")) opt_w= 2;
       else if (0 == strcmp(*argv, "sawtooth")) opt_w= 3;
       else error("Unknown waveform: %s (use sine, square, triangle, sawtooth)", *argv);
       argv++;
	     break;
	  case 'c':
	     if (argc-- < 1) error("-c expects argument");
	     setupOptC(*argv++);
	     break;
	  case 'i': rv= 'i'; break;
	  case 'p': rv= 'p'; break;
	  case 'h': help(); break;
	  case 'D': opt_D= 1; break;
	  case 'G': opt_G= 1; break;
	  case 'P': opt_P= 1; break;
	  case 'M': opt_M= 1; break;
	  case 'O': opt_O= 1;
	     if (!fast_mult) fast_mult= 1; 		// Don't try to sync with real time
	     break;
	  case 'W': opt_W= 1;
	     if (!fast_mult) fast_mult= 1; 		// Don't try to sync with real time
	     break;
	  case 'q': 
	     opt_S= 1;
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &fast_mult, &dmy)) 
		error("Expecting an integer after -q");
	     if (fast_mult < 1) fast_mult= 1;
	     break;
	  case 'r':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &out_rate, &dmy))
		error("Expecting an integer after -r");
	     out_rate_def= 0;
	     break;
#ifndef MAC_AUDIO
	  case 'b':
	     if (argc-- < 1 || 
		 1 != sscanf(*argv++, "%d %c", &val, &dmy) ||
		 !(val == 8 || val == 16))
		error("Expecting -b 8 or -b 16");
	     out_mode= (val == 8) ? 0 : 1;
	     break;
#endif
	  case 'o':
	     if (argc-- < 1) error("Expecting filename after -o");
	     opt_o= *argv++;
	     if (!fast_mult) fast_mult= 1;		// Don't try to sync with real time
	     break;
	  case 'K':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &opt_mp3_bitrate, &dmy))
		error("-K expects MP3 bitrate in kbps (8-320)");
	     if (opt_mp3_bitrate < 8 || opt_mp3_bitrate > 320)
		error("MP3 bitrate must be in range 8..320 kbps");
	     opt_mp3_bitrate_set= 1;
	     break;
	  case 'J':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &opt_mp3_quality, &dmy))
		error("-J expects MP3 quality integer (0-9)");
	     if (opt_mp3_quality < 0 || opt_mp3_quality > 9)
		error("MP3 quality must be in range 0..9 (lower is better)");
	     opt_mp3_quality_set= 1;
	     break;
	  case 'X':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%lf %c", &dval, &dmy))
		error("-X expects MP3 VBR quality in range 0..9");
	     if (dval < 0.0 || dval > 9.0)
		error("MP3 VBR quality must be in range 0..9 (lower is better)");
	     opt_mp3_vbr_quality= dval;
	     opt_mp3_vbr_quality_set= 1;
	     break;
	  case 'U':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%lf %c", &dval, &dmy))
		error("-U expects OGG quality in range 0..10");
	     if (dval < 0.0 || dval > 10.0)
		error("OGG quality must be in range 0..10");
	     opt_ogg_quality= dval;
	     opt_ogg_quality_set= 1;
	     break;
	  case 'Z':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%lf %c", &dval, &dmy))
		error("-Z expects FLAC compression level in range 0..12");
	     if (dval < 0.0 || dval > 12.0)
		error("FLAC compression level must be in range 0..12");
	     opt_flac_compression= dval;
	     opt_flac_compression_set= 1;
	     break;
#ifdef ALSA_AUDIO
	  case 'd':
	     if (argc-- < 1) error("Expecting ALSA device name after -d");
	     opt_d= *argv++;
	     break;
#endif
	  case 'R':
	     if (argc-- < 1 || 1 != sscanf(*argv++, "%d %c", &out_prate, &dmy)) 
		error("Expecting integer after -R");
	     break;
	  default:
	     error("Option -%c not known; run 'sbagenx -h' for help", opt);
	 }
      }
   }

   *acp= argc;
   *avp= argv;
   return rv;
}

static int
sbx_read_text_file_alloc_cli(const char *path, char **out_text) {
   FILE *fp;
   long len;
   char *buf;
   size_t got;

   if (!path || !out_text)
      return SBX_EINVAL;
   *out_text= 0;
   fp= fopen(path, "rb");
   if (!fp)
      return SBX_EINVAL;
   if (0 != fseek(fp, 0, SEEK_END)) {
      fclose(fp);
      return SBX_EINVAL;
   }
   len= ftell(fp);
   if (len < 0) {
      fclose(fp);
      return SBX_EINVAL;
   }
   if (0 != fseek(fp, 0, SEEK_SET)) {
      fclose(fp);
      return SBX_EINVAL;
   }
   buf= (char *)malloc((size_t)len + 1);
   if (!buf) {
      fclose(fp);
      return SBX_ENOMEM;
   }
   got= fread(buf, 1, (size_t)len, fp);
   fclose(fp);
   if (got != (size_t)len) {
      free(buf);
      return SBX_EINVAL;
   }
   buf[len]= 0;
   *out_text= buf;
   return SBX_OK;
}

static int
sbx_parse_safe_seqfile_option_line(const char *line,
				   int *out_S, int *out_E,
				   int *out_have_T, int *out_T_ms,
				   int *out_have_q, int *out_q_mult,
				   int *out_have_r, int *out_rate,
				   int *out_have_F, int *out_fade_ms,
				   char **out_mix_path) {
   char *dup, *argv[64], *p;
   int argc= 0, i;

   if (!line || !out_S || !out_E || !out_have_T || !out_T_ms ||
       !out_have_q || !out_q_mult || !out_have_r || !out_rate ||
       !out_have_F || !out_fade_ms || !out_mix_path)
      return 0;
   dup= StrDup((char *)line);
   p= dup;
   while (*p) {
      if (argc >= (int)(sizeof(argv) / sizeof(argv[0]))) {
	 free(dup);
	 return 0;
      }
      while (isspace((unsigned char)*p)) p++;
      if (!*p) break;
      argv[argc++]= p;
      while (*p && !isspace((unsigned char)*p)) p++;
      if (*p) *p++= 0;
   }

   for (i= 0; i<argc; i++) {
      char *tok= argv[i];
      size_t a;
      if (tok[0] != '-' || !tok[1]) {
	 free(dup);
	 return 0;
      }
      for (a= 1; tok[a]; a++) {
	 switch (tok[a]) {
	  case 'S':
	     *out_S= 1;
	     break;
	  case 'E':
	     *out_E= 1;
	     break;
	  case 'T':
	     if (tok[a+1] || i+1 >= argc || 0 == readTime(argv[i+1], out_T_ms)) {
		free(dup);
		return 0;
	     }
	     *out_have_T= 1;
	     i++;
	     a= strlen(tok) - 1;
	     break;
	  case 'm':
	     if (tok[a+1] || i+1 >= argc) {
		free(dup);
		return 0;
	     }
	     if (!*out_mix_path)
		*out_mix_path= StrDup(argv[i+1]);
	     i++;
	     a= strlen(tok) - 1;
	     break;
	  case 'q':
	     if (tok[a+1] || i+1 >= argc || 1 != sscanf(argv[i+1], "%d", out_q_mult)) {
		free(dup);
		return 0;
	     }
	     if (*out_q_mult < 1)
		*out_q_mult= 1;
	     *out_have_q= 1;
	     i++;
	     a= strlen(tok) - 1;
	     break;
	  case 'r':
	     if (tok[a+1] || i+1 >= argc || 1 != sscanf(argv[i+1], "%d", out_rate)) {
		free(dup);
		return 0;
	     }
	     *out_have_r= 1;
	     i++;
	     a= strlen(tok) - 1;
	     break;
	  case 'F':
	     if (tok[a+1] || i+1 >= argc || 1 != sscanf(argv[i+1], "%d", out_fade_ms)) {
		free(dup);
		return 0;
	     }
	     *out_have_F= 1;
	     i++;
	     a= strlen(tok) - 1;
	     break;
	  default:
	     free(dup);
	     return 0;
	 }
      }
   }
   free(dup);
   return 1;
}

static int
sbx_line_contains_seq_mode_option(const char *line) {
   char *dup, *argv[64], *p;
   int argc= 0, i;

   if (!line)
      return 0;
   dup= StrDup((char *)line);
   p= dup;
   while (*p) {
      if (argc >= (int)(sizeof(argv) / sizeof(argv[0])))
	 break;
      while (isspace((unsigned char)*p)) p++;
      if (!*p) break;
      argv[argc++]= p;
      while (*p && !isspace((unsigned char)*p)) p++;
      if (*p) *p++= 0;
   }
   for (i= 0; i<argc; i++) {
      if ((0 == strcmp(argv[i], "-p")) || (0 == strcmp(argv[i], "-i"))) {
	 free(dup);
	 return 1;
      }
   }
   free(dup);
   return 0;
}

static int
sbx_try_run_option_only_seq_wrapper(const char *fnam) {
   char *text, *line;
   int rc, saw_mode= 0;

   if (!fnam || 0 == strcmp(fnam, "-"))
      return 0;
   rc= sbx_read_text_file_alloc_cli(fnam, &text);
   if (rc == SBX_ENOMEM)
      error("Out of memory preparing sequence wrapper file");
   if (rc != SBX_OK)
      return 0;

   /* Pass 1: classify the file without executing options. */
   line= text;
   while (line && *line) {
      char *next= strchr(line, '\n');
      char *trim= line;
      char *end;

      if (next)
	 next++;
      while (*trim && isspace((unsigned char)*trim)) trim++;
      if (!*trim || *trim == '#' || *trim == ';' || (*trim == '/' && trim[1] == '/')) {
	 line= next;
	 continue;
      }
      if (*trim != '-') {
	 free(text);
	 return 0;
      }
      end= line + strlen(line);
      if (next)
	 end= next - 1;
      if (end > line && end[-1] == '\r')
	 end--;
      {
	 char saved= *end;
	 *end= 0;
	 if (sbx_line_contains_seq_mode_option(trim))
	    saw_mode= 1;
	 *end= saved;
      }
      line= next;
   }

   if (!saw_mode) {
      free(text);
      return 0;
   }

   /* Pass 2: execute the wrapper now that it is confirmed to be option-only. */
   line= text;
   while (line && *line) {
      char *next= strchr(line, '\n');
      char *trim= line;
      char *end;

      if (next)
	 next++;
      while (*trim && isspace((unsigned char)*trim)) trim++;
      if (!*trim || *trim == '#' || *trim == ';' || (*trim == '/' && trim[1] == '/')) {
	 line= next;
	 continue;
      }
      end= line + strlen(line);
      if (next)
	 end= next - 1;
      if (end > line && end[-1] == '\r')
	 end--;
      {
	 char saved= *end;
	 *end= 0;
	 handleOptions(trim);
	 *end= saved;
      }
      line= next;
   }

   free(text);
   return 1;
}

static int
sbx_prepare_seqfile_runtime_text(const char *fnam, char **out_text,
				 int *out_S, int *out_E,
				 int *out_have_T, int *out_T_ms,
				 int *out_have_q, int *out_q_mult,
				 int *out_have_r, int *out_rate,
				 int *out_have_F, int *out_fade_ms,
				 char **out_mix_path) {
   char *text, *line;
   int rc;

   if (!out_text || !out_S || !out_E || !out_have_T || !out_T_ms ||
       !out_have_q || !out_q_mult || !out_have_r || !out_rate ||
       !out_have_F || !out_fade_ms || !out_mix_path)
      return SBX_EINVAL;
   *out_text= 0;
   *out_S= 0;
   *out_E= 0;
   *out_have_T= 0;
   *out_T_ms= -1;
   *out_have_q= 0;
   *out_q_mult= 1;
   *out_have_r= 0;
   *out_rate= 44100;
   *out_have_F= 0;
   *out_fade_ms= 60000;
   *out_mix_path= 0;

   rc= sbx_read_text_file_alloc_cli(fnam, &text);
   if (rc != SBX_OK)
      return rc;

   line= text;
   while (line && *line) {
      char *line_nl= strchr(line, '\n');
      char *trim;
      char *line_end;
      char saved= 0;
      char *next= 0;

      if (line_nl) {
	 line_end= line_nl;
	 saved= *line_end;
	 *line_end= 0;
	 next= line_nl + 1;
      } else {
	 line_end= line + strlen(line);
      }
      if (line_end > line && line_end[-1] == '\r')
	 line_end--;
      trim= line;
      while (*trim && isspace((unsigned char)*trim)) trim++;
      if (!*trim) {
	 if (saved) *line_nl= saved;
	 line= next;
	 continue;
      }
      if (*trim == '#' || *trim == ';' || (*trim == '/' && trim[1] == '/')) {
	 if (saved) *line_nl= saved;
	 line= next;
	 continue;
      }
      if (*trim == '-') {
	 if (!sbx_parse_safe_seqfile_option_line(trim,
						 out_S, out_E,
						 out_have_T, out_T_ms,
						 out_have_q, out_q_mult,
						 out_have_r, out_rate,
						 out_have_F, out_fade_ms,
						 out_mix_path)) {
	    if (*out_mix_path) free(*out_mix_path);
	    *out_mix_path= 0;
	    if (saved) *line_nl= saved;
	    free(text);
	    return SBX_EINVAL;
	 }
	 memset(line, ' ', (size_t)(line_end - line));
	 if (saved) *line_nl= saved;
	 line= next;
	 continue;
      }
      if (saved) *line_nl= saved;
      break;
   }

   *out_text= text;
   return SBX_OK;
}

//
//	Handle an option string, breaking it into an (argc/argv) list
//	for scanOptions.
//

void 
handleOptions(char *str0) {
   // Always StrDup() string and don't bother to free(), as normal
   // argv[] strings stick around for the life of the program
   char *str= StrDup(str0);
   int const max_argc= 32;
   char *argv[max_argc+1];
   int argc= 0;

   while (*str) {
      if (argc >= max_argc)
	 error("Too many options at line: %d\n  %s", in_lin, lin_copy);
      argv[argc++]= str;
      while (*str && !isspace(*str)) str++;
      if (!*str) continue;
      *str++= 0;	// NUL-term this word
      while (isspace(*str)) str++;
   }
   argv[argc]= 0;	// Terminate argv list with a NULL

   // Process the options
   {
      char **av= argv;
      int ac= argc;
      int rv;
      
      rv= scanOptions(&ac, &av);

      if (rv == 'i') {
	 // Immediate mode
	 readSeqImm(ac, av);
      } else if (rv == 'p') {
	 // Pre-programmed sequence
	 readPreProg(ac, av);
      } else if (ac)
	 error("Trailing garbage after options at line: %d\n  %s", in_lin, lin_copy);
   }
}

//
//	Setup the ampadj[] array from the given -c spec-string
//

void 
setupOptC(char *spec) {
   char *p= spec, *q;
   int a, b;
   
   while (1) {
      while (isspace(*p) || *p == ',') p++;
      if (!*p) break;

      if (opt_c >= sizeof(ampadj) / sizeof(ampadj[0]))
	 error("Too many -c option frequencies; maxmimum is %d", 
	       sizeof(ampadj) / sizeof(ampadj[0]));

      ampadj[opt_c].freq= strtod(p, &q);
      if (p == q) goto bad;
      if (*q++ != '=') goto bad;
      ampadj[opt_c].adj= strtod(q, &p);
      if (p == q) goto bad;
      opt_c++;
   }

   // Sort the list
   for (a= 0; a<opt_c; a++)
      for (b= a+1; b<opt_c; b++) 
	 if (ampadj[a].freq > ampadj[b].freq) {
	    double tmp;
	    tmp= ampadj[a].freq; ampadj[a].freq= ampadj[b].freq; ampadj[b].freq= tmp;
	    tmp= ampadj[a].adj; ampadj[a].adj= ampadj[b].adj; ampadj[b].adj= tmp;
	 }
   return;
      
 bad:
   error("Bad -c option spec; expecting <freq>=<amp>[,<freq>=<amp>]...:\n  %s", spec);
}


//
//	If this is a WAV file we've been given, skip forward to the
//	'data' section.  Don't bother checking any of the 'fmt '
//	stuff.  If they didn't give us a valid 16-bit stereo file at
//	the right rate, then tough!
//

void 
find_wav_data_start(FILE *in) {
   unsigned char buf[16];

   if (1 != fread(buf, 12, 1, in)) goto bad;
   if (0 != memcmp(buf, "RIFF", 4)) goto bad;
   if (0 != memcmp(buf+8, "WAVE", 4)) goto bad;
   
   while (1) {
      int len;
      if (1 != fread(buf, 8, 1, in)) goto bad;
      if (0 == memcmp(buf, "data", 4)) return;		// We're in the right place!
      len= buf[4] + (buf[5]<<8) + (buf[6]<<16) + (buf[7]<<24);
      if (len & 1) len++;
      if (out_rate_def && 0 == memcmp(buf, "fmt ", 4)) {
	 // Grab the sample rate to use as the default if available
	 if (1 != fread(buf, 8, 1, in)) goto bad;
	 len -= 8;
	 out_rate= buf[4] + (buf[5]<<8) + (buf[6]<<16) + (buf[7]<<24);
	 out_rate_def= 0;
      }
      if (0 != fseek(in, len, SEEK_CUR)) goto bad;
   }

 bad:
   warn("WARNING: Not a valid WAV file, treating as RAW");
   rewind(in);
}

//
//	Input raw audio data from the 'mix_in' stream, and convert to
//	32-bit values (max 'dlen')
//

int 
raw_mix_in(int *dst, int dlen) {
   short *tmp= (short*)(dst + dlen/2);
   int a, rv;
   
   rv= fread(tmp, 2, dlen, mix_in);
   if (rv == 0) {
      if (feof(mix_in))
	 return 0;
      error("Read error on mix input:\n  %s", strerror(errno));
   }

   // Now convert 16-bit little-endian input data into 20-bit native
   // int values
   if (bigendian) {
      char *rd= (char*)tmp;
      for (a= 0; a<rv; a++) {
	 *dst++= ((rd[0]&255) + (rd[1]<<8)) << 4;
	 rd += 2;
      }
   } else {
      for (a= 0; a<rv; a++) 
	 *dst++= *tmp++ << 4;
   }

   return rv;
}

   

//
//	Update a status line
//

void 
status(char *err) {
  int a;
  int nch= N_CH;
  char *p= buf, *p0, *p1;

  if (opt_Q) return;

#ifdef ANSI_TTY
  if (tty_erase) p += sprintf(p, "\033[K");
#endif

  p0= p;		// Start of line
  *p++= ' '; *p++= ' ';
  p += sprintTime(p, now);
  while (nch > 1 && chan[nch-1].v.typ == 0) nch--;
  for (a= 0; a<nch; a++) {
    if (opt_A && mix_in && chan[a].v.typ == 5)
      p += sprintf(p, " mix/%.2f", AMP_AD(chan[a].v.amp) * mix_mod_multiplier(now));
    else
      p += sprintVoice(p, &chan[a].v, 0);
  }
  if (err) p += sprintf(p, " %s", err);
  p1= p;		// End of line

#ifndef ANSI_TTY
  // Truncate line to 79 characters on Windows
  if (p1-p0 > 79) {
    p1 = p0 + 76;
    p1 += sprintf(p1, "...");
  }
#endif

#ifndef ANSI_TTY
  while (tty_erase > p-p0) *p++= ' ';
#endif

  tty_erase= p1-p0;		// Characters that will need erasing
  fprintf(stderr, "%s\r", buf);
  fflush(stderr);
}

void
statusSbx(char *err) {
  int a;
  char *p= buf, *p0, *p1;
  double t_sec;
  int tim_ms;
  SbxToneSpec tone;
  char tok[256];
  size_t aux_n;

  if (opt_Q || !sbx_runtime_active || !sbx_runtime_ctx) return;

#ifdef ANSI_TTY
  if (tty_erase) p += sprintf(p, "\033[K");
#endif

  t_sec= sbx_context_time_sec(sbx_runtime_ctx);
  tim_ms= (int)(t_sec * 1000.0 + 0.5);
  tim_ms %= H24;
  if (tim_ms < 0) tim_ms += H24;

  p0= p;
  *p++= ' '; *p++= ' ';
  p += sprintTime(p, tim_ms);

  if (SBX_OK == sbx_context_sample_tones(sbx_runtime_ctx, t_sec, t_sec, 1, 0, &tone)) {
    if (SBX_OK == sbx_format_tone_spec(&tone, tok, sizeof(tok)))
      p += sprintf(p, " %s", tok);
  }

  aux_n= sbx_context_aux_tone_count(sbx_runtime_ctx);
  for (a= 0; a < (int)aux_n; a++) {
    if (SBX_OK == sbx_context_get_aux_tone(sbx_runtime_ctx, (size_t)a, &tone) &&
	SBX_OK == sbx_format_tone_spec(&tone, tok, sizeof(tok)))
      p += sprintf(p, " %s", tok);
  }

  if (mix_in) {
    double mix_pct= sbx_context_mix_amp_at(sbx_runtime_ctx, t_sec);
    if (opt_A) mix_pct *= mix_mod_multiplier(now);
    p += sprintf(p, " mix/%.2f", mix_pct);
  }

  {
    size_t fx_n= sbx_context_mix_effect_count(sbx_runtime_ctx);
    SbxMixFxSpec fx;
    for (a= 0; a < (int)fx_n; a++) {
      if (SBX_OK == sbx_context_get_mix_effect(sbx_runtime_ctx, (size_t)a, &fx) &&
	  SBX_OK == sbx_format_mix_fx_spec(&fx, tok, sizeof(tok)))
	p += sprintf(p, " %s", tok);
    }
  }

  if (err) p += sprintf(p, " %s", err);
  p1= p;

#ifndef ANSI_TTY
  if (p1-p0 > 79) {
    p1 = p0 + 76;
    p1 += sprintf(p1, "...");
  }
#endif

#ifndef ANSI_TTY
  while (tty_erase > p-p0) *p++= ' ';
#endif

  tty_erase= p1-p0;
  fprintf(stderr, "%s\r", buf);
  fflush(stderr);
}

void 				// Display current period details
dispCurrPer(FILE *fp) {
  int a;
  Voice *v0, *v1;
  char *p0, *p1;
  int len0, len1;
  int nch= N_CH;

  if (opt_Q) return;

  p0= buf;
  p1= buf_copy;
  
  p0 += sprintf(p0, "* ");
  p0 += sprintTime(p0, per->tim);
  p1 += sprintf(p1, "  ");	
  p1 += sprintTime(p1, per->nxt->tim);

  v0= per->v0; v1= per->v1;
  while (nch > 1 && v0[nch-1].typ == 0) nch--;
  for (a= 0; a<nch; a++, v0++, v1++) {
    p0 += len0= sprintVoice(p0, v0, 0);
    p1 += len1= sprintVoice(p1, v1, v0);
    while (len0 < len1) { *p0++= ' '; len0++; }
    while (len1 < len0) { *p1++= ' '; len1++; }
  }
  *p0= 0; *p1= 0;
  fprintf(fp, "%s\n%s\n", buf, buf_copy);
  fflush(fp);
}

int
sprintTime(char *p, int tim) {
  return sprintf(p, "%02d:%02d:%02d",
		 tim % 86400000 / 3600000,
		 tim % 3600000 / 60000,
		 tim % 60000 / 1000);
}

int 
sprintVoice(char *p, Voice *vp, Voice *dup) {
   switch (vp->typ) {
    case 0:
      return sprintf(p, " -");
    case 1:
      if (dup && vp->carr == dup->carr && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      if (vp->res == 0)
	return sprintf(p, " %s:%.2f/%.2f", waveform_name[vp->waveform], vp->carr, AMP_AD(vp->amp));
      return sprintf(p, " %s:%.2f%+.2f/%.2f", waveform_name[vp->waveform], vp->carr, vp->res, AMP_AD(vp->amp));
    case 2:
      if (dup && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " pink/%.2f", AMP_AD(vp->amp));
    case 9:
      if (dup && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " white/%.2f", AMP_AD(vp->amp));
    case 10:
      if (dup && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " brown/%.2f", AMP_AD(vp->amp));
    case 3:
      if (dup && vp->carr == dup->carr && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:bell%.2f/%.2f", waveform_name[vp->waveform], vp->carr, AMP_AD(vp->amp));
    case 4:
      if (dup && vp->carr == dup->carr && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:spin:%.2f%+.2f/%.2f", waveform_name[vp->waveform], vp->carr, vp->res, AMP_AD(vp->amp));
    case 5:
      if (dup && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " mix/%.2f", AMP_AD(vp->amp));
    case 8:  // Isochronic tones
      if (dup && vp->carr == dup->carr && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:%.2f@%.2f/%.2f", waveform_name[vp->waveform], vp->carr, vp->res, AMP_AD(vp->amp));
    case 6:  // Mixspin - spinning mix stream
      if (dup && vp->carr == dup->carr && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:mixspin:%.2f%+.2f/%.2f", waveform_name[vp->waveform], vp->carr, vp->res, AMP_AD(vp->amp));
    case 7:  // Mixpulse - mix stream with pulse effect
      if (dup && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:mixpulse:%.2f/%.2f", waveform_name[vp->waveform], vp->res, AMP_AD(vp->amp));
    case 13:  // Mixbeat - frequency-shifted binauralized mix stream
      if (dup && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:mixbeat:%.2f/%.2f", waveform_name[vp->waveform], vp->res, AMP_AD(vp->amp));
    case 11:  // Bspin - spinning brown noise
      if (dup && vp->carr == dup->carr && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:bspin:%.2f%+.2f/%.2f", waveform_name[vp->waveform], vp->carr, vp->res, AMP_AD(vp->amp));
    case 12:  // Wspin - spinning white noise
      if (dup && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " %s:wspin:%.2f%+.2f/%.2f", waveform_name[vp->waveform], vp->carr, vp->res, AMP_AD(vp->amp));
    default:
      if (vp->typ < -100 || vp->typ > -1)
	return sprintf(p, " ???");
      if (dup && vp->carr == dup->carr && vp->res == dup->res && vp->amp == dup->amp)
	return sprintf(p, "  ::");
      return sprintf(p, " wave%02d:%.2f%+.2f/%.2f", -1-vp->typ, vp->carr, vp->res, AMP_AD(vp->amp));
   }
}

void 
init_sin_table() {
  int a, waveform;
  
  // Initialize all 4 waveform tables
  for (waveform = 0; waveform < 4; waveform++) {
    int *arr = (int*)Alloc(ST_SIZ * sizeof(int));
    
    for (a = 0; a < ST_SIZ; a++) {
      double phase = (a * 2.0 * 3.14159265358979323846) / ST_SIZ;
      double val;
      
      switch(waveform) {
        case 0: // Sine
          val = sin(phase);
          break;
        case 1: // Square
          val = (sin(phase) >= 0) ? 1.0 : -1.0;
          break;
        case 2: // Triangle
          if (phase < 3.14159265358979323846)
            val = (2.0 * phase / 3.14159265358979323846) - 1.0;
          else
            val = 3.0 - (2.0 * phase / 3.14159265358979323846);
          break;
        case 3: // Sawtooth
          val = (2.0 * phase / (2.0 * 3.14159265358979323846)) - 1.0;
          break;
        default:
          val = sin(phase);
      }
      
      arr[a] = (int)(ST_AMP * val);
    }
    
    sin_tables[waveform] = arr;
  }

  // sin_table = sin_tables[opt_w];
}

void
init_mixbeat_hilbert(void) {
  int k;
  int mid = MIXBEAT_HILBERT_TAPS / 2;
  double pi = 3.14159265358979323846;

  memset(mixbeat_hilbert_coeff, 0, sizeof(mixbeat_hilbert_coeff));
  memset(mixbeat_hist, 0, sizeof(mixbeat_hist));
  memset(mixbeat_hist_pos, 0, sizeof(mixbeat_hist_pos));

  for (k = 0; k < MIXBEAT_HILBERT_TAPS; k++) {
    int n = k - mid;
    double h = 0.0;
    double win;

    // Ideal discrete-time Hilbert kernel: 2/(pi*n) on odd n, 0 on even n.
    if (n != 0 && (n & 1))
      h = 2.0 / (pi * (double)n);

    // Smooth finite-tap response to reduce ringing.
    win = 0.54 - 0.46 * cos((2.0 * pi * k) / (MIXBEAT_HILBERT_TAPS - 1));
    mixbeat_hilbert_coeff[k] = h * win;
  }
}

void
reset_mixbeat_channel_state(int ch_index) {
  if (ch_index < 0 || ch_index >= N_CH) return;
  memset(mixbeat_hist[ch_index], 0, sizeof(mixbeat_hist[ch_index]));
  mixbeat_hist_pos[ch_index] = 0;
}

double
mixbeat_hilbert_sample(int ch_index, double x) {
  int k, pos, idx;
  double q = 0.0;

  if (ch_index < 0 || ch_index >= N_CH) return 0.0;

  pos = mixbeat_hist_pos[ch_index];
  mixbeat_hist[ch_index][pos] = x;

  idx = pos;
  for (k = 0; k < MIXBEAT_HILBERT_TAPS; k++) {
    q += mixbeat_hilbert_coeff[k] * mixbeat_hist[ch_index][idx];
    if (--idx < 0) idx = MIXBEAT_HILBERT_TAPS - 1;
  }

  pos++;
  if (pos >= MIXBEAT_HILBERT_TAPS) pos = 0;
  mixbeat_hist_pos[ch_index] = pos;
  return q;
}

void 
error(char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
#ifdef EXIT_KEY
  fprintf(stderr, "Press <RETURN> to continue: ");
  fflush(stderr);
  getchar();
#endif
#ifdef ALSA_AUDIO
  cleanup_alsa();
#endif
#ifdef MAC_AUDIO
  cleanup_mac_audio();
#endif
  exit(1);
}

void 
debug(char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
}

void 
warn(char *fmt, ...) {
  va_list ap; va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
}

void *
Alloc(size_t len) {
  void *p= calloc(1, len);
  if (!p) error("Out of memory");
  return p;
}

char *
StrDup(char *str) {
  char *rv= strdup(str);
  if (!rv) error("Out of memory");
  return rv;
}

#ifdef UNIX_TIME
// Precalculate a reference timestamp to accelerate calcNow().  This
// can be any recent time.  We recalculate it every 10 minutes.  The
// only reason for doing this is to cope with clocks going forwards or
// backwards when entering or leaving summer time so that people wake
// up on time on these two dates; an hour of the sequence will be
// repeated or skipped.  The 'time_ref*' variables will be initialised
// on the first call to calcNow().

static int time_ref_epoch= 0;	// Reference time compared to UNIX epoch
static int time_ref_ms;		// Reference time in sbagen 24-hour milliseconds

void 
setupRefTime() {
  struct tm *tt;
  time_t tim= time(0);
  tt= localtime(&tim);
  time_ref_epoch= tim;
  time_ref_ms= 1000*tt->tm_sec + 60000*tt->tm_min + 3600000*tt->tm_hour;
}  

inline int  
calcNow() {
  struct timeval tv;
  if (0 != gettimeofday(&tv, 0)) error("Can't get current time");
  if (tv.tv_sec - time_ref_epoch > 600) setupRefTime();
  return (time_ref_ms + (tv.tv_sec - time_ref_epoch) * 1000 + tv.tv_usec / 1000) % H24;
}
#endif

#ifdef WIN_TIME
inline int  
calcNow() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  return st.wMilliseconds + 1000*st.wSecond + 60000*st.wMinute + 3600000*st.wHour;
}
#endif

#if DEBUG_CHK_UTIME
inline int 
userTime() {
  struct tms buf;
  times(&buf);
  return buf.tms_utime;
}
#else
// Dummy to avoid complaints on MSVC
int userTime() { return 0; }
#endif

static const char *
output_encoder_name() {
   switch (out_enc_fmt) {
    case OUT_ENC_MP3: return "MP3";
    case OUT_ENC_OGG: return "OGG/Vorbis";
    case OUT_ENC_FLAC: return "FLAC";
    default: return "raw";
   }
}

static void
write_out_fd_raw(const char *buf, int siz) {
   int rv;
   while (-1 != (rv= write(out_fd, buf, siz))) {
      if (rv == 0) break;
      siz -= rv;
      if (!siz) return;
      buf += rv;
   }
   error("Output error");
}

#ifdef WIN_MISC
static DLibHandle
dlib_open_one(const char *name) {
   return LoadLibraryA(name);
}
static void *
dlib_sym(DLibHandle lib, const char *name) {
   return (void*)GetProcAddress(lib, name);
}
static void
dlib_close(DLibHandle lib) {
   if (lib) FreeLibrary(lib);
}
#else
static DLibHandle
dlib_open_one(const char *name) {
   return dlopen(name, RTLD_LAZY);
}
static void *
dlib_sym(DLibHandle lib, const char *name) {
   return dlsym(lib, name);
}
static void
dlib_close(DLibHandle lib) {
   if (lib) dlclose(lib);
}
#endif

#ifdef WIN_MISC
#define DLIB_PATH_SEP '\\'
#else
#define DLIB_PATH_SEP '/'
#endif

static char dlib_exec_dir[1024];
static int dlib_exec_dir_init;

static void
dlib_init_exec_dir() {
   if (dlib_exec_dir_init) return;
   dlib_exec_dir_init= 1;
   dlib_exec_dir[0]= 0;

#ifdef WIN_MISC
   {
      char path[1024];
      DWORD len= GetModuleFileNameA(0, path, sizeof(path)-1);
      if (len > 0 && len < sizeof(path)) {
	 char *p1, *p2, *p;
	 path[len]= 0;
	 p1= strrchr(path, '/');
	 p2= strrchr(path, '\\');
	 p= p1 > p2 ? p1 : p2;
	 if (p) *p= 0;
	 else strcpy(path, ".");
	 strncpy(dlib_exec_dir, path, sizeof(dlib_exec_dir)-1);
	 dlib_exec_dir[sizeof(dlib_exec_dir)-1]= 0;
      }
   }
#else
   {
      char path[PATH_MAX];
      int ok= 0;
#ifdef T_MACOSX
      {
	 uint32_t size= sizeof(path);
	 if (_NSGetExecutablePath(path, &size) == 0)
	    ok= 1;
      }
#endif
      if (!ok) {
	 ssize_t len= readlink("/proc/self/exe", path, sizeof(path)-1);
	 if (len > 0 && len < sizeof(path)) {
	    path[len]= 0;
	    ok= 1;
	 }
      }
      if (ok) {
	 char *p1, *p2, *p;
	 p1= strrchr(path, '/');
	 p2= strrchr(path, '\\');
	 p= p1 > p2 ? p1 : p2;
	 if (p) *p= 0;
	 else strcpy(path, ".");
	 strncpy(dlib_exec_dir, path, sizeof(dlib_exec_dir)-1);
	 dlib_exec_dir[sizeof(dlib_exec_dir)-1]= 0;
      }
   }
#endif

   if (!dlib_exec_dir[0])
      strcpy(dlib_exec_dir, ".");
}

static DLibHandle
dlib_open_best(const char **names) {
   int a;
   char cand[2048];

   dlib_init_exec_dir();
   for (a= 0; names[a]; a++) {
      const char *name= names[a];
      DLibHandle mod;

      if (strchr(name, '/') || strchr(name, '\\')) {
	 mod= dlib_open_one(name);
	 if (mod) return mod;
	 continue;
      }

      snprintf(cand, sizeof(cand), "%s%c%s%c%s", dlib_exec_dir, DLIB_PATH_SEP, "libs", DLIB_PATH_SEP, name);
      mod= dlib_open_one(cand);
      if (mod) return mod;

      snprintf(cand, sizeof(cand), "%s%c%s", dlib_exec_dir, DLIB_PATH_SEP, name);
      mod= dlib_open_one(cand);
      if (mod) return mod;

      snprintf(cand, sizeof(cand), ".%c%s%c%s", DLIB_PATH_SEP, "libs", DLIB_PATH_SEP, name);
      mod= dlib_open_one(cand);
      if (mod) return mod;

      snprintf(cand, sizeof(cand), ".%c%s", DLIB_PATH_SEP, name);
      mod= dlib_open_one(cand);
      if (mod) return mod;

      mod= dlib_open_one(name);
      if (mod) return mod;
   }
   return 0;
}

#undef DLIB_PATH_SEP

static int
is_out_ext(const char *path, const char *ext) {
   const char *p= strrchr(path, '.');
   const char *slash= strrchr(path, '/');
   const char *bslash= strrchr(path, '\\');
   const char *sep= slash > bslash ? slash : bslash;
   int a;

   if (!p || (sep && p < sep) || !p[1]) return 0;
   p++;
   for (a= 0; ext[a] && p[a]; a++)
      if (tolower((unsigned char)p[a]) != tolower((unsigned char)ext[a]))
	 return 0;
   return ext[a] == 0 && p[a] == 0;
}

void
detect_output_encoder() {
   out_enc_fmt= OUT_ENC_NONE;
   if (!opt_o || opt_W) return;

   if (is_out_ext(opt_o, "mp3"))
      out_enc_fmt= OUT_ENC_MP3;
   else if (is_out_ext(opt_o, "ogg"))
      out_enc_fmt= OUT_ENC_OGG;
   else if (is_out_ext(opt_o, "flac"))
      out_enc_fmt= OUT_ENC_FLAC;
}

static void
init_snd_encoder(int format) {
#ifndef STATIC_OUTPUT_ENCODERS
   const char *names[] = {
#ifdef WIN_MISC
      "libsndfile-1.dll",
      "sndfile.dll",
#elif defined(T_MACOSX)
      "libsndfile.1.dylib",
      "libsndfile.dylib",
#else
      "libsndfile.so.1",
      "libsndfile.so",
#endif
      0
   };
#endif
   SF_INFO info;

   memset(&snd_enc, 0, sizeof(snd_enc));
#ifdef STATIC_OUTPUT_ENCODERS
   snd_enc.sf_open_fn= sf_open;
   snd_enc.sf_writef_short_fn= sf_writef_short;
   snd_enc.sf_close_fn= sf_close;
   snd_enc.sf_strerror_fn= sf_strerror;
   snd_enc.sf_command_fn= sf_command;
#else
   snd_enc.lib= dlib_open_best(names);
   if (!snd_enc.lib)
      error("%s output requested, but libsndfile runtime library is not available", output_encoder_name());

   snd_enc.sf_open_fn= (SNDFILE*(*)(const char*, int, SF_INFO*))dlib_sym(snd_enc.lib, "sf_open");
   snd_enc.sf_writef_short_fn= (sf_count_t(*)(SNDFILE*, const short*, sf_count_t))dlib_sym(snd_enc.lib, "sf_writef_short");
   snd_enc.sf_close_fn= (int(*)(SNDFILE*))dlib_sym(snd_enc.lib, "sf_close");
   snd_enc.sf_strerror_fn= (const char*(*)(SNDFILE*))dlib_sym(snd_enc.lib, "sf_strerror");
   snd_enc.sf_command_fn= (int(*)(SNDFILE*, int, void*, int))dlib_sym(snd_enc.lib, "sf_command");
   if (!snd_enc.sf_open_fn || !snd_enc.sf_writef_short_fn || !snd_enc.sf_close_fn ||
       !snd_enc.sf_strerror_fn || !snd_enc.sf_command_fn)
      error("Failed to load required libsndfile symbols for %s output", output_encoder_name());
#endif

   memset(&info, 0, sizeof(info));
   info.channels= 2;
   info.samplerate= out_rate;
   info.format= format;

   snd_enc.snd= snd_enc.sf_open_fn(opt_o, SFM_WRITE, &info);
   if (!snd_enc.snd)
      error("Failed to open output file for %s encoding: %s",
	    output_encoder_name(),
	    snd_enc.sf_strerror_fn(0));

   if (out_enc_fmt == OUT_ENC_OGG) {
      double q= opt_ogg_quality / 10.0;
      if (snd_enc.sf_command_fn(snd_enc.snd, SFC_SET_VBR_ENCODING_QUALITY, &q, sizeof(q)) <= 0)
	 error("Failed to set OGG Vorbis quality to %.2f (range 0..10)", opt_ogg_quality);
      if (!opt_Q)
	 warn("Using OGG Vorbis quality %.2f/10", opt_ogg_quality);
   }

   if (out_enc_fmt == OUT_ENC_FLAC && opt_flac_compression_set) {
      double c= opt_flac_compression / 12.0;
      if (snd_enc.sf_command_fn(snd_enc.snd, SFC_SET_COMPRESSION_LEVEL, &c, sizeof(c)) <= 0)
	 error("Failed to set FLAC compression level to %.2f (range 0..12)", opt_flac_compression);
      if (!opt_Q)
	 warn("Using FLAC compression level %.2f/12", opt_flac_compression);
   }

   out_enc_active= 1;
}

static void
ensure_mp3_buf(int frames) {
   int need= 7200 + (int)(1.25 * frames + 0.5);
   if (need > mp3_enc.buflen) {
      if (!mp3_enc.buf) {
	 mp3_enc.buf= ALLOC_ARR(need, unsigned char);
      } else {
	 mp3_enc.buf= (unsigned char*)realloc(mp3_enc.buf, need);
	 if (!mp3_enc.buf) error("Out of memory");
      }
      mp3_enc.buflen= need;
   }
}

static void
init_mp3_encoder() {
   int mp3_bitrate= opt_mp3_bitrate_set ? opt_mp3_bitrate : 192;
   int mp3_quality= opt_mp3_quality_set ? opt_mp3_quality : 2;
   double mp3_vbr_quality= opt_mp3_vbr_quality_set ? opt_mp3_vbr_quality : 0.0;
#ifndef STATIC_OUTPUT_ENCODERS
   const char *names[] = {
#ifdef WIN_MISC
      "libmp3lame-0.dll",
      "libmp3lame.dll",
#elif defined(T_MACOSX)
      "libmp3lame.0.dylib",
      "libmp3lame.dylib",
#else
      "libmp3lame.so.0",
      "libmp3lame.so",
#endif
      0
   };
#endif

   memset(&mp3_enc, 0, sizeof(mp3_enc));
#ifdef STATIC_OUTPUT_ENCODERS
   mp3_enc.lame_init_fn= lame_init;
   mp3_enc.lame_set_in_samplerate_fn= lame_set_in_samplerate;
   mp3_enc.lame_set_num_channels_fn= lame_set_num_channels;
   mp3_enc.lame_set_quality_fn= lame_set_quality;
   mp3_enc.lame_set_VBR_fn= lame_set_VBR;
   mp3_enc.lame_set_VBR_q_fn= lame_set_VBR_q;
   mp3_enc.lame_set_VBR_quality_fn= lame_set_VBR_quality;
   mp3_enc.lame_set_brate_fn= lame_set_brate;
   mp3_enc.lame_init_params_fn= lame_init_params;
   mp3_enc.lame_encode_buffer_interleaved_fn= lame_encode_buffer_interleaved;
   mp3_enc.lame_encode_flush_fn= lame_encode_flush;
   mp3_enc.lame_close_fn= lame_close;
#else
   mp3_enc.lib= dlib_open_best(names);
   if (!mp3_enc.lib)
      error("MP3 output requested, but libmp3lame runtime library is not available");

   mp3_enc.lame_init_fn= (lame_t(*)(void))dlib_sym(mp3_enc.lib, "lame_init");
   mp3_enc.lame_set_in_samplerate_fn= (int(*)(lame_t,int))dlib_sym(mp3_enc.lib, "lame_set_in_samplerate");
   mp3_enc.lame_set_num_channels_fn= (int(*)(lame_t,int))dlib_sym(mp3_enc.lib, "lame_set_num_channels");
   mp3_enc.lame_set_quality_fn= (int(*)(lame_t,int))dlib_sym(mp3_enc.lib, "lame_set_quality");
   mp3_enc.lame_set_VBR_fn= (int(*)(lame_t,int))dlib_sym(mp3_enc.lib, "lame_set_VBR");
   mp3_enc.lame_set_VBR_q_fn= (int(*)(lame_t,int))dlib_sym(mp3_enc.lib, "lame_set_VBR_q");
   mp3_enc.lame_set_VBR_quality_fn= (int(*)(lame_t,float))dlib_sym(mp3_enc.lib, "lame_set_VBR_quality");
   mp3_enc.lame_set_brate_fn= (int(*)(lame_t,int))dlib_sym(mp3_enc.lib, "lame_set_brate");
   mp3_enc.lame_init_params_fn= (int(*)(lame_t))dlib_sym(mp3_enc.lib, "lame_init_params");
   mp3_enc.lame_encode_buffer_interleaved_fn=
      (int(*)(lame_t, short int*, int, unsigned char*, int))dlib_sym(mp3_enc.lib, "lame_encode_buffer_interleaved");
   mp3_enc.lame_encode_flush_fn=
      (int(*)(lame_t, unsigned char*, int))dlib_sym(mp3_enc.lib, "lame_encode_flush");
   mp3_enc.lame_close_fn= (int(*)(lame_t))dlib_sym(mp3_enc.lib, "lame_close");
#endif
   if (!mp3_enc.lame_init_fn ||
       !mp3_enc.lame_set_in_samplerate_fn ||
       !mp3_enc.lame_set_num_channels_fn ||
       !mp3_enc.lame_set_quality_fn ||
       !(opt_mp3_vbr_quality_set
	 ? (mp3_enc.lame_set_VBR_fn && (mp3_enc.lame_set_VBR_quality_fn || mp3_enc.lame_set_VBR_q_fn))
	 : (mp3_enc.lame_set_brate_fn != 0)) ||
       !mp3_enc.lame_init_params_fn ||
       !mp3_enc.lame_encode_buffer_interleaved_fn ||
       !mp3_enc.lame_encode_flush_fn ||
       !mp3_enc.lame_close_fn)
      error("Failed to load required libmp3lame symbols");

   mp3_enc.gfp= mp3_enc.lame_init_fn();
   if (!mp3_enc.gfp) error("lame_init() failed");

   if (mp3_enc.lame_set_num_channels_fn(mp3_enc.gfp, 2) < 0 ||
       mp3_enc.lame_set_in_samplerate_fn(mp3_enc.gfp, out_rate) < 0 ||
       mp3_enc.lame_set_quality_fn(mp3_enc.gfp, mp3_quality) < 0)
      error("Failed to configure MP3 encoder");

   if (opt_mp3_vbr_quality_set) {
      // 4 = vbr_mtrh in LAME's vbr_mode enum.
      if (mp3_enc.lame_set_VBR_fn(mp3_enc.gfp, 4) < 0)
	 error("Failed to configure MP3 VBR mode");
      if (mp3_enc.lame_set_VBR_quality_fn) {
	 if (mp3_enc.lame_set_VBR_quality_fn(mp3_enc.gfp, (float)mp3_vbr_quality) < 0)
	    error("Failed to configure MP3 VBR quality %.2f (range 0..9)", mp3_vbr_quality);
      } else {
	 int q_i= (int)(mp3_vbr_quality + 0.5);
	 if (q_i < 0) q_i= 0;
	 if (q_i > 9) q_i= 9;
	 if (mp3_enc.lame_set_VBR_q_fn(mp3_enc.gfp, q_i) < 0)
	    error("Failed to configure MP3 VBR quality %d (range 0..9)", q_i);
	 if (!opt_Q && fabs(mp3_vbr_quality - q_i) > 1e-9)
	    warn("MP3 runtime supports integer VBR quality only; -X %.2f rounded to %d", mp3_vbr_quality, q_i);
      }
      if (!opt_Q && opt_mp3_bitrate_set)
	 warn("MP3 bitrate setting (-K) is ignored when MP3 VBR mode (-X) is used");
   } else {
      if (mp3_enc.lame_set_brate_fn(mp3_enc.gfp, mp3_bitrate) < 0)
	 error("Failed to configure MP3 bitrate %d kbps", mp3_bitrate);
   }

   if (mp3_enc.lame_init_params_fn(mp3_enc.gfp) < 0)
      error("Failed to initialize MP3 encoder parameters");

   if (!opt_Q) {
      if (opt_mp3_vbr_quality_set)
	 warn("Using MP3 VBR quality %.2f (0 best .. 9 fastest), LAME quality %d", mp3_vbr_quality, mp3_quality);
      else
	 warn("Using MP3 bitrate %d kbps, quality %d", mp3_bitrate, mp3_quality);
   }

   out_enc_active= 1;
}

void
init_output_encoder() {
   out_enc_active= 0;
   out_enc_finished= 0;
   if (out_enc_fmt == OUT_ENC_NONE) return;

   if (!opt_o)
      error("%s encoding requires -o <file>", output_encoder_name());
   if (out_mode != 1) {
      if (!opt_Q)
	 warn("%s output requires 16-bit PCM input; forcing 16-bit mode", output_encoder_name());
      out_mode= 1;
   }

   if (!opt_Q && out_enc_fmt != OUT_ENC_MP3 &&
       (opt_mp3_bitrate_set || opt_mp3_quality_set || opt_mp3_vbr_quality_set))
      warn("MP3 settings (-K/-J/-X) are ignored unless output filename ends with .mp3");
   if (!opt_Q && out_enc_fmt != OUT_ENC_OGG && opt_ogg_quality_set)
      warn("OGG setting (-U) is ignored unless output filename ends with .ogg");
   if (!opt_Q && out_enc_fmt != OUT_ENC_FLAC && opt_flac_compression_set)
      warn("FLAC setting (-Z) is ignored unless output filename ends with .flac");

   switch (out_enc_fmt) {
    case OUT_ENC_MP3:
      init_mp3_encoder();
      break;
    case OUT_ENC_OGG:
      init_snd_encoder(SF_FORMAT_OGG | SF_FORMAT_VORBIS);
      break;
    case OUT_ENC_FLAC:
      init_snd_encoder(SF_FORMAT_FLAC | SF_FORMAT_PCM_16);
      break;
    default:
      break;
   }

   if (out_enc_active && !out_enc_atexit) {
      atexit(finish_output_encoder);
      out_enc_atexit= 1;
   }

   if (!opt_Q && out_enc_active)
      warn("Outputting %s encoded audio at %d Hz", output_encoder_name(), out_rate);
}

void
output_encoder_write(short *pcm, int frames) {
   if (!out_enc_active || !frames) return;

   if (out_enc_fmt == OUT_ENC_MP3) {
      int rv;
      ensure_mp3_buf(frames);
      rv= mp3_enc.lame_encode_buffer_interleaved_fn(mp3_enc.gfp, pcm, frames, mp3_enc.buf, mp3_enc.buflen);
      if (rv < 0)
	 error("MP3 encoding failed with error code %d", rv);
      if (rv > 0)
	 write_out_fd_raw((char*)mp3_enc.buf, rv);
      return;
   }

   if (out_enc_fmt == OUT_ENC_OGG || out_enc_fmt == OUT_ENC_FLAC) {
      sf_count_t wr= snd_enc.sf_writef_short_fn(snd_enc.snd, pcm, frames);
      if (wr != (sf_count_t)frames)
	 error("%s encoding failed while writing frame data", output_encoder_name());
      return;
   }
}

void
finish_output_encoder() {
   if (!out_enc_active || out_enc_finished) return;
   out_enc_finished= 1;

   if (out_enc_fmt == OUT_ENC_MP3) {
      int rv;
      ensure_mp3_buf(4096);
      rv= mp3_enc.lame_encode_flush_fn(mp3_enc.gfp, mp3_enc.buf, mp3_enc.buflen);
      if (rv < 0)
	 error("MP3 encoder flush failed with error code %d", rv);
      if (rv > 0)
	 write_out_fd_raw((char*)mp3_enc.buf, rv);
      if (mp3_enc.gfp) {
	 mp3_enc.lame_close_fn(mp3_enc.gfp);
	 mp3_enc.gfp= 0;
      }
      if (mp3_enc.buf) { free(mp3_enc.buf); mp3_enc.buf= 0; }
      mp3_enc.buflen= 0;
      dlib_close(mp3_enc.lib);
      mp3_enc.lib= 0;
      return;
   }

   if (out_enc_fmt == OUT_ENC_OGG || out_enc_fmt == OUT_ENC_FLAC) {
      if (snd_enc.snd) {
	 snd_enc.sf_close_fn(snd_enc.snd);
	 snd_enc.snd= 0;
      }
      dlib_close(snd_enc.lib);
      snd_enc.lib= 0;
      return;
   }
}

//
//	Simple random number generator.  Generates a repeating
//	sequence of 65536 odd numbers in the range -65535->65535.
//
//	Based on ZX Spectrum random number generator:
//	  seed= (seed+1) * 75 % 65537 - 1
//

#define RAND_MULT 75

static int seed= 2;

//inline int qrand() {
//  return (seed= seed * 75 % 131074) - 65535;
//}

//
//	Generate next sample for simulated pink noise, with same
//	scaling as the sin_table[].  This version uses an inlined
//	random number generator, and smooths the lower frequency bands
//	as well.
//

#define NS_BANDS 9
typedef struct Noise Noise;
struct Noise {
  int val;		// Current output value
  int inc;		// Increment
};
Noise ntbl[NS_BANDS];
int nt_off;
int noise_buf[256];
uchar noise_off= 0;

static inline int 
noise2() {
  int tot;
  int off= nt_off++;
  int cnt= 1;
  Noise *ns= ntbl;
  Noise *ns1= ntbl + NS_BANDS;

  tot= ((seed= seed * RAND_MULT % 131074) - 65535) * (NS_AMP / 65535 / (NS_BANDS + 1));

  while ((cnt & off) && ns < ns1) {
    int val= ((seed= seed * RAND_MULT % 131074) - 65535) * (NS_AMP / 65535 / (NS_BANDS + 1));
    tot += ns->val += ns->inc= (val - ns->val) / (cnt += cnt);
    ns++;
  }

  while (ns < ns1) {
    tot += (ns->val += ns->inc);
    ns++;
  }

  return noise_buf[noise_off++]= (tot >> NS_ADJ);
}

//
//	Generate next sample for white noise, with same
//	scaling as the sin_table[].
//
static inline int 
white_noise() {
  // White noise is simply a random value without filtering
  return ((seed= seed * RAND_MULT % 131074) - 65535) * (ST_AMP / 65535);
}

//
//	Generate next sample for brown noise, with same
//	scaling as the sin_table[].
//	Brown noise has more energy in lower frequencies,
//	implemented as a random walk (integration of white noise)
//
static int brown_last = 0;
static inline int 
brown_noise() {
  // Generate a random value
  int random = ((seed= seed * RAND_MULT % 131074) - 65535);
  
  // Integrate the random value with a decay factor to avoid overflow
  brown_last = (brown_last + (random / 16)) * 0.9;
  
  // Limit the value to avoid overflow
  if (brown_last > 65535) brown_last = 65535;
  if (brown_last < -65535) brown_last = -65535;
  
  // Scale to the same level as the sin_table
  return brown_last * (ST_AMP / 65535);
}

// Create a spin effect for noise based on the spin_position

void create_noise_spin_effect(
  int typ,
  int amp,
  int spin_position,
  int *left,
  int *right
) {
  // Apply intensity factor to rotation value
	int amplified_val = (int)(spin_position * 1.5);

	// Limit value between -128 and 127
	if (amplified_val > 127) amplified_val = 127;
	if (amplified_val < -128) amplified_val = -128;
	    
  // Use absolute value for calculations
  int pos_val = amplified_val < 0 ? -amplified_val : amplified_val;
  int noise_l, noise_r;

  int base_noise;
  switch (typ) {
    case 11:
      base_noise = brown_noise();
      break;
    case 12:
      base_noise = white_noise();
      break;
    default:
      // Default is pink noise
      base_noise = noise_buf[(uchar)(noise_off+128)];
      break;
  }

  // When val is close to 0, channels are played normally
  // When val approaches +/-128, channels are swapped or muted
  if (amplified_val >= 0) {
    // Rotation to the right: left channel decreases, right channel receives part of the left channel
    noise_l = (base_noise * (128 - pos_val)) >> 7;
    noise_r = base_noise + ((base_noise * pos_val) >> 7);
  } else {
    // Rotation to the left: right channel decreases, left channel receives part of the right channel
    noise_l = base_noise + ((base_noise * pos_val) >> 7);
    noise_r = (base_noise * (128 - pos_val)) >> 7;
  }

  // Apply noise to the left and right channels
  *left = amp * noise_l;
  *right = amp * noise_r;
}

//	//
//	//	Generate next sample for simulated pink noise, scaled the same
//	//	as the sin_table[].  This version uses a library random number
//	//	generator, and no smoothing.
//	//
//	
//	inline double 
//	noise() {
//	  int tot= 0;
//	  int bit= ~0;
//	  int a;
//	  int off;
//	
//	  ns_tbl[ns_off]= (rand() - (RAND_MAX / 2)) / (NS_BIT + 1);
//	  off= ns_off;
//	  for (a= 0; a<=NS_BIT; a++, bit <<= 1) {
//	    off &= bit;
//	    tot += ns_tbl[off];
//	  }
//	  ns_off= (ns_off + 1) & ((1<<NS_BIT) - 1);
//	
//	  return tot * (ST_AMP / (RAND_MAX * 0.5));
//	}

//
//	Play loop
//

void 
loop() {	
  int c, cnt;
  int err;		// Error to add to 'now' until next cnt==0
  int fast= fast_mult != 0;
  int vfast= fast_mult > 20;		// Very fast - update status line often
  int utime= 0;
  int now_lo= 0;			// Low-order 16 bits of 'now' (fractional)
  int err_lo= 0;
  int ms_inc;

  setup_device();
  spin_carr_max= 127.0 / 1E-6 / out_rate;
  cnt= 1 + 1999 / out_buf_ms;	// Update every 2 seconds or so
  now= opt_S ? fast_tim0 : calcNow();
  if (opt_T != -1) now= opt_T;
  err= fast ? out_buf_ms * (fast_mult - 1) : 0;
  if (opt_L)
    byte_count= out_bps * (S64)(opt_L * 0.001 * out_rate);
  if (opt_E) {
    if (sbx_runtime_active && sbx_runtime_total_sec > 0.0) {
       int duration= (int)(sbx_runtime_total_sec * 1000.0 + 0.5);
       byte_count= out_bps * (S64)(duration * 0.001 * out_rate /
				   (fast ? fast_mult : 1));
       if (!opt_Q) {
	  int secs= duration / 1000;
	  warn("*** Sequence duration: %02d:%02d:%02d (hh:mm:ss) ***",
	       secs/3600, (secs/60)%60, secs%60);
       }
    } else {
      // Calculate the correct duration based on the last time in the sequence file
      int duration = t_per0(fast_tim0, fast_tim1);
      byte_count= out_bps * (S64)(duration * 0.001 * out_rate /
                  (fast ? fast_mult : 1));
      if (!opt_Q) {
        printSequenceDuration();
      }
    }
  }

  // Do byte-swapping if bigendian and outputting to a file or stream
  if ((opt_O || opt_o) &&
      out_enc_fmt == OUT_ENC_NONE &&
      out_mode == 1 && bigendian)
     out_mode= 2;

  if (opt_W)
    writeWAV();

  if (!opt_Q) fprintf(stderr, "\n");
  if (!sbx_runtime_active) {
     corrVal(0);		// Get into correct period
     dispCurrPer(stderr);	// Display
     status(0);
  } else if (!opt_Q) {
     warn("SBAGENXLIB runtime active");
     statusSbx(0);
  }
  
  while (1) {
    for (c= 0; c < cnt; c++) {
      if (!sbx_runtime_active)
	 corrVal(1);
      outChunk();
      ms_inc= out_buf_ms + err;
      now_lo += out_buf_lo + err_lo;
      if (now_lo >= 0x10000) { ms_inc += now_lo >> 16; now_lo &= 0xFFFF; }
      now += ms_inc;
      if (now > H24) now -= H24;
      if (vfast && (c&1)) {
	 if (sbx_runtime_active) statusSbx(0);
	 else status(0);
      }
    }

    if (fast) {
      if (!vfast) {
	 if (sbx_runtime_active) statusSbx(0);
	 else status(0);
      }
    }
    else {
      // Synchronize with real clock, gently over the next second or so
      char buf[32];
      int diff= calcNow() - now;
      if (abs(diff) > H12) diff= 0;
      sprintf(buf, "(%d)", diff); 

      err_lo= diff * 0x10000 / cnt;
      err= err_lo >> 16;
      err_lo &= 0xFFFF;

      if (DEBUG_CHK_UTIME) {
	int prev= utime;
	utime= userTime();
	sprintf(buf, "%d ticks", utime-prev);		// Replaces standard message
      }
      if (sbx_runtime_active)
	 statusSbx(buf);
      else
	 status(buf);
    }
  }
}


//
//	Output a chunk of sound (a buffer-ful), then return
//
//	Note: Optimised for 16-bit output.  Eight-bit output is
//	slower, but then it probably won't have to run at as high a
//	sample rate.
//

int rand0, rand1;

void 
outChunk() {
   if (sbx_runtime_active) {
      outChunkSbx();
      return;
   }

   int off= 0;
   double mix_mod_mul= mix_mod_multiplier(now);

   if (mix_in) {
      int rv= inbuf_read(tmp_buf, out_blen);
      if (rv == 0) {
	 if (!opt_Q) warn("\nEnd of mix input audio stream");
	 exit(0);
      }
      while (rv < out_blen) tmp_buf[rv++]= 0;
   }
   
   while (off < out_blen) {
      int ns= noise2();		// Use same pink noise source for everything
      int tot1, tot2;		// Left and right channels
      int mix1, mix2;		// Incoming mix signals
      int val, a;
      Channel *ch;
      int *tab;

      mix1= tmp_buf[off];
      mix2= tmp_buf[off+1];

      // Do default mixing at 100% if no mix/* stuff is present
      if (!mix_flag) {
	 tot1= (int)((mix1 << 12) * mix_mod_mul);
	 tot2= (int)((mix2 << 12) * mix_mod_mul);
      } else {
	 tot1= tot2= 0;
      }
      
      ch= &chan[0];
      for (a= 0; a<N_CH; a++, ch++) switch (ch->typ) {
       case 0:
	  break;
       case 1:	// Binaural tones
	  ch->off1 += ch->inc1;
	  ch->off1 &= (ST_SIZ << 16) - 1;
	  // tot1 += ch->amp * sin_table[ch->off1 >> 16];
	  tot1 += ch->amp * sin_tables[ch->v.waveform][ch->off1 >> 16];
	  ch->off2 += ch->inc2;
	  ch->off2 &= (ST_SIZ << 16) - 1;
	  // tot2 += ch->amp2 * sin_table[ch->off2 >> 16];
	  tot2 += ch->amp2 * sin_tables[ch->v.waveform][ch->off2 >> 16];
	  break;
       case 2:	// Pink noise
	  val= ns * ch->amp;
	  tot1 += val;
	  tot2 += val;
	  break;
       case 9:	// White noise
	  val= white_noise() * ch->amp;
	  tot1 += val;
	  tot2 += val;
	  break;
       case 10:	// Brown noise
	  val= brown_noise() * ch->amp;
	  tot1 += val;
	  tot2 += val;
	  break;
       case 3:	// Bell
	  if (ch->off2) {
	     ch->off1 += ch->inc1;
	     ch->off1 &= (ST_SIZ << 16) - 1;
	     // val= ch->off2 * sin_table[ch->off1 >> 16];
	     val= ch->off2 * sin_tables[ch->v.waveform][ch->off1 >> 16];
	     tot1 += val; tot2 += val;
	     if (--ch->inc2 < 0) {
		ch->inc2= out_rate/20;
		ch->off2 -= 1 + ch->off2 / 12;	// Knock off 10% each 50 ms
	     }
	  }
	  break;
       case 4:	// Spinning pink noise
	  ch->off1 += ch->inc1;
	  ch->off1 &= (ST_SIZ << 16) - 1;
	  // val= (ch->inc2 * sin_table[ch->off1 >> 16]) >> 24;
	  val= (ch->inc2 * sin_tables[ch->v.waveform][ch->off1 >> 16]) >> 24;
	  {
      int spin_left, spin_right;
      create_noise_spin_effect(4, ch->amp, val, &spin_left, &spin_right);
      tot1 += spin_left;
      tot2 += spin_right;
    }
	  break;
       case 5:	// Mix level
	  tot1 += (int)(mix1 * (ch->amp * mix_mod_mul));
	  tot2 += (int)(mix2 * (ch->amp * mix_mod_mul));
	  break;
       case 6:	// Mixspin - spinning mix stream
	  ch->off1 += ch->inc1;
	  ch->off1 &= (ST_SIZ << 16) - 1;
	  // val= (ch->inc2 * sin_table[ch->off1 >> 16]) >> 24;
	  val= (ch->inc2 * sin_tables[ch->v.waveform][ch->off1 >> 16]) >> 24;
	  
	  // Mixspin intensity control
	  {
	    // Calculate intensity factor based on amplitude
 	    // Amplitude varies from 0 to 4096 (0-100%)
 	    // Normalize to a factor between 0.5 and 4.0
 	    double intensity_factor = 0.5 + (ch->amp / 4096.0) * 3.5;

	    // Apply intensity factor to rotation value
	    int amplified_val = (int)(val * intensity_factor);
	    
	    // Limit value between -128 and 127
	    if (amplified_val > 127) amplified_val = 127;
	    if (amplified_val < -128) amplified_val = -128;
	    
	    // Use absolute value for calculations
	    int pos_val = amplified_val < 0 ? -amplified_val : amplified_val;
	    int mix_l, mix_r;
	    
	    // When val is close to 0, channels are played normally
	    // When val approaches +/-128, channels are swapped or muted
	    if (amplified_val >= 0) {
	      // Rotation to the right: left channel decreases, right channel receives part of the left channel
	      mix_l = (mix1 * (128 - pos_val)) >> 7;
	      mix_r = mix2 + ((mix1 * pos_val) >> 7);
	    } else {
	      // Rotation to the left: right channel decreases, left channel receives part of the right channel
	      mix_l = mix1 + ((mix2 * pos_val) >> 7);
	      mix_r = (mix2 * (128 - pos_val)) >> 7;
	    }

	    // Apply base volume (using 70% of amplitude for volume)
	    double base_amp = mix_amp_current * 0.7 * mix_mod_mul;
	    tot1 += (int)(base_amp * mix_l);
	    tot2 += (int)(base_amp * mix_r);
	  }
	  break;
       case 7:	// Mixpulse - mix stream with pulse effect
          ch->off2 += ch->inc2;  // Modulator (pulse frequency)
          ch->off2 &= (ST_SIZ << 16) - 1;
          
          // Create the isochronic pulse effect in the audio stream
          {
            int mod_val = sin_tables[ch->v.waveform][ch->off2 >> 16];
            // Apply a threshold to create distinct pulses with space between them
            double mod_factor = 0.0;
            
            // Use only the positive part of the sine wave and apply a threshold
            // to create a space between pulses
            if (mod_val > ST_AMP * 0.3) {  // Threshold of 30% of the maximum value
              // Normalize from ST_AMP*0.3 to ST_AMP to 0 to 1
              mod_factor = (mod_val - (ST_AMP * 0.3)) / (double)(ST_AMP * 0.7);
              // Smooth the edges of the pulse to avoid clicks
              mod_factor = mod_factor * mod_factor * (3 - 2 * mod_factor);  // Cubic smoothing
            }
            
            // Apply the modulation factor to the audio stream
            // Use base_amp as in mixspin (70% of amplitude for volume)
            double base_amp = mix_amp_current * 0.7 * mix_mod_mul;
            
            // Calculate the intensity of the effect based on amplitude (ch->amp)
            // When ch->amp is 0, there is no effect (only the original audio)
            // When ch->amp is 4096 (100%), the effect is maximum
            double effect_intensity = (ch->amp / 4096.0) * 1.5;
            
            // Apply the modulation to the audio stream with variable intensity
            // When effect_intensity is 0, only the original audio is reproduced
            // When effect_intensity is 1, the audio is fully modulated by the pulse
            double gain = (1.0 - effect_intensity) + (effect_intensity * mod_factor);
            tot1 += (int)(base_amp * mix1 * gain);
            tot2 += (int)(base_amp * mix2 * gain);
          }
          break;
       case 13:	// Mixbeat - Hilbert frequency-shifted mix stream
          ch->off2 += ch->inc2;
          ch->off2 &= (ST_SIZ << 16) - 1;

          {
            int phase = (ch->off2 >> 16) & (ST_SIZ - 1);
            int phase_q = (phase + (ST_SIZ >> 2)) & (ST_SIZ - 1);
            double s = sin_tables[ch->v.waveform][phase] / (double)ST_AMP;
            double c = sin_tables[ch->v.waveform][phase_q] / (double)ST_AMP;
            double mono = 0.5 * (mix1 + mix2);
            double q = mixbeat_hilbert_sample(a, mono);
            double up = mono * c - q * s;   // +shift (right)
            double down = mono * c + q * s; // -shift (left)
            double effect_intensity = ch->amp / 4096.0;
            double wet_l, wet_r, out_l, out_r;
            double base_amp;

            if (effect_intensity < 0.0) effect_intensity = 0.0;
            if (effect_intensity > 1.0) effect_intensity = 1.0;

            wet_l = down;
            wet_r = up;
            out_l = mix1 * (1.0 - effect_intensity) + wet_l * effect_intensity;
            out_r = mix2 * (1.0 - effect_intensity) + wet_r * effect_intensity;

            // Keep same base volume rule as other mix effects.
            base_amp = mix_amp_current * 0.7 * mix_mod_mul;
            tot1 += (int)(base_amp * out_l);
            tot2 += (int)(base_amp * out_r);
          }
          break;
	       case 8:  // Isochronic tones
	          ch->off1 += ch->inc1;  // Carrier (tone frequency)
	          ch->off1 &= (ST_SIZ << 16) - 1;
	          ch->off2 += ch->inc2;  // Modulator (pulse frequency)
	          ch->off2 &= (ST_SIZ << 16) - 1;
	          
	          {
	            double mod_factor = isochronic_mod_factor(ch);
	            // Apply the modulation envelope to the carrier waveform.
	            val = ch->amp * sin_tables[ch->v.waveform][ch->off1 >> 16] * mod_factor;
	            tot1 += val;
	            tot2 += val;
	          }
	          break;
       case 11:  // Bspin - spinning brown noise
          ch->off1 += ch->inc1;
          ch->off1 &= (ST_SIZ << 16) - 1;
          // val= (ch->inc2 * sin_table[ch->off1 >> 16]) >> 24;
          val= (ch->inc2 * sin_tables[ch->v.waveform][ch->off1 >> 16]) >> 24;
          
          {
            int spin_left, spin_right;
            create_noise_spin_effect(11, ch->amp, val, &spin_left, &spin_right);
            tot1 += spin_left;
            tot2 += spin_right;
          }
          break;
       case 12:  // Wspin - spinning white noise
          ch->off1 += ch->inc1;
          ch->off1 &= (ST_SIZ << 16) - 1;
          // val= (ch->inc2 * sin_table[ch->off1 >> 16]) >> 24;
          val= (ch->inc2 * sin_tables[ch->v.waveform][ch->off1 >> 16]) >> 24;
          
          {
            int spin_left, spin_right;
            create_noise_spin_effect(12, ch->amp, val, &spin_left, &spin_right);
            tot1 += spin_left;
            tot2 += spin_right;
          }
          break;
       default:	// Custom-envelope binaural tones (wave00..wave99)
	  tab= waves[-1 - ch->typ];
	  ch->off1 += ch->inc1;
	  ch->off1 &= (ST_SIZ << 16) - 1;
	  ch->off2 += ch->inc2;
	  ch->off2 &= (ST_SIZ << 16) - 1;
	  ch->off3 += ch->inc3;
	  ch->off3 &= (ST_SIZ << 16) - 1;
	  {
	     int env= tab[ch->off3 >> 16];
	     int env_amp= (int)(((S64)ch->amp * env + ST_AMP/2) / ST_AMP);
	     tot1 += env_amp * sin_tables[ch->v.waveform][ch->off1 >> 16];
	     tot2 += env_amp * sin_tables[ch->v.waveform][ch->off2 >> 16];
	  }
	  break;
      }

      // Apply volume level
      if (opt_V != 100) {
        tot1 = ((long long)tot1 * opt_V + 50) / 100;
        tot2 = ((long long)tot2 * opt_V + 50) / 100;
      }

      // White noise dither; you could also try (rand0-rand1) for a
      // dither with more high frequencies
      rand0= rand1; 
      rand1= (rand0 * 0x660D + 0xF35F) & 0xFFFF;
      if (tot1 <= 0x7FFF0000) tot1 += rand0;
      if (tot2 <= 0x7FFF0000) tot2 += rand0;

      out_buf[off++]= tot1 >> 16;
      out_buf[off++]= tot2 >> 16;
  }

  // Generate debugging amplitude output
  if (DEBUG_DUMP_AMP) {
    short *sp= out_buf;
    short *end= out_buf + out_blen;
    int max= 0;
    while (sp < end) {
       int val= (int)sp[0] + (int)sp[1]; sp += 2;
       if (val < 0) val= -val;
       if (val > max) max= val;
    }
    max /= 328;
    while (max-- > 0) putc('#', stdout);
    printf("\n"); fflush(stdout);
  }

  // Rewrite buffer for 8-bit mode
  if (out_mode == 0) {
    short *sp= out_buf;
    short *end= out_buf + out_blen;
    char *cp= (char*)out_buf;
    while (sp < end) *cp++= (*sp++ >> 8) + 128;
  }

  // Rewrite buffer for 16-bit byte-swapping
  if (out_mode == 2) {
    char *cp= (char*)out_buf;
    char *end= (char*)(out_buf + out_blen);
    while (cp < end) { char tmp= *cp++; cp[-1]= cp[0]; *cp++= tmp; }
  }

  // Check and update the byte count if necessary
  if (byte_count > 0) {
    if (byte_count <= out_bsiz) {
      writeOut((char*)out_buf, byte_count);
#ifdef ALSA_AUDIO
      cleanup_alsa();
#endif
      exit(0);		// All done
    }
    else {
      writeOut((char*)out_buf, out_bsiz);
      byte_count -= out_bsiz;
    }
  }
  else
    writeOut((char*)out_buf, out_bsiz);
} 

void
outChunkSbx() {
   int frames= out_blen / 2;
   int off= 0;
   int rc;
   double t0;
   double sr= (double)out_rate;
   double mix_mod_mul= mix_mod_multiplier(now);

   if (!sbx_runtime_ctx)
      error("Internal error: sbagenxlib runtime context is not initialized");

   if ((size_t)out_blen > sbx_runtime_fcap) {
      sbx_runtime_fbuf= (float*)realloc(sbx_runtime_fbuf, out_blen * sizeof(float));
      if (!sbx_runtime_fbuf) error("Out of memory");
      sbx_runtime_fcap= out_blen;
   }

   if (mix_in) {
      int rv= inbuf_read(tmp_buf, out_blen);
      if (rv == 0) {
	 if (!opt_Q) warn("\nEnd of mix input audio stream");
	 exit(0);
      }
      while (rv < out_blen) tmp_buf[rv++]= 0;
   }

   t0= sbx_context_time_sec(sbx_runtime_ctx);
   rc= sbx_context_render_f32(sbx_runtime_ctx, sbx_runtime_fbuf, frames);
   if (rc != SBX_OK)
      error("sbagenxlib render failed: %s", sbx_context_last_error(sbx_runtime_ctx));

   while (off < out_blen) {
      int mix1= mix_in ? tmp_buf[off] : 0;
      int mix2= mix_in ? tmp_buf[off+1] : 0;
      int idx= off;
      int frm= off / 2;
      double gen_l= sbx_runtime_fbuf[idx];
      double gen_r= sbx_runtime_fbuf[idx+1];
      double out_l= gen_l * 32767.0;
      double out_r= gen_r * 32767.0;

      if (mix_in) {
	 double t_sec= t0 + frm / sr;
	 double mix_add_l= 0.0;
	 double mix_add_r= 0.0;
	 rc= sbx_context_mix_stream_sample(sbx_runtime_ctx, t_sec, mix1, mix2, mix_mod_mul,
					   &mix_add_l, &mix_add_r);
	 if (rc != SBX_OK)
	    error("sbagenxlib mix stream processing failed: %s", sbx_context_last_error(sbx_runtime_ctx));
	 out_l += mix_add_l;
	 out_r += mix_add_r;
      }

      if (opt_V != 100) {
	 out_l= (out_l * opt_V) / 100.0;
	 out_r= (out_r * opt_V) / 100.0;
      }

      if (out_l > 32767.0) out_l= 32767.0;
      if (out_l < -32768.0) out_l= -32768.0;
      if (out_r > 32767.0) out_r= 32767.0;
      if (out_r < -32768.0) out_r= -32768.0;

      out_buf[off++]= (short)lrint(out_l);
      out_buf[off++]= (short)lrint(out_r);
   }

   // Rewrite buffer for 8-bit mode
   if (out_mode == 0) {
      short *sp= out_buf;
      short *end= out_buf + out_blen;
      char *cp= (char*)out_buf;
      while (sp < end) *cp++= (*sp++ >> 8) + 128;
   }

   // Rewrite buffer for 16-bit byte-swapping
   if (out_mode == 2) {
      char *cp= (char*)out_buf;
      char *end= (char*)(out_buf + out_blen);
      while (cp < end) { char tmp= *cp++; cp[-1]= cp[0]; *cp++= tmp; }
   }

   // Check and update the byte count if necessary
   if (byte_count > 0) {
      if (byte_count <= out_bsiz) {
	 writeOut((char*)out_buf, byte_count);
#ifdef ALSA_AUDIO
	 cleanup_alsa();
#endif
	 exit(0);		// All done
      } else {
	 writeOut((char*)out_buf, out_bsiz);
	 byte_count -= out_bsiz;
      }
   } else {
      writeOut((char*)out_buf, out_bsiz);
   }
}

void 
writeOut(char *buf, int siz) {
  int rv;

  if (out_enc_active) {
     if (siz & 3)
	error("Internal error: encoded output chunk is not frame-aligned (%d bytes)", siz);
     output_encoder_write((short*)buf, siz/4);
     return;
  }

#ifdef WIN_AUDIO
  if (out_fd == -9999) {
     // Win32 output: write it to a header and send it off
     MMRESULT rv;

     //debug_win32_buffer_status();

     //while (aud_cnt == BUFFER_COUNT) {
     //while (aud_head[aud_current]->dwFlags & WHDR_INQUEUE) {
     while (!(aud_head[aud_current]->dwFlags & WHDR_DONE)) {
	//debug("SLEEP %d", out_buf_ms / 2 + 1);
	Sleep(out_buf_ms / 2 + 1);
	//debug_win32_buffer_status();
     }

     memcpy(aud_head[aud_current]->lpData, buf, siz);
     aud_head[aud_current]->dwBufferLength= (DWORD)siz;

     //debug("Output buffer %d", aud_current);
     rv= waveOutWrite(aud_handle, aud_head[aud_current], sizeof(WAVEHDR));

     if (rv != MMSYSERR_NOERROR) {
        char buf[255];
        waveOutGetErrorText(rv, buf, sizeof(buf)-1);
        error("Error writing a fragment to the audio device:\n  %s", buf);
     }
   
     aud_cnt++; 
     aud_current++;
     aud_current %= BUFFER_COUNT;

     return;
  }
#endif

#ifdef MAC_AUDIO
  if (out_fd == -9999) {
    int new_wr= (aud_wr + 1) % BUFFER_COUNT;

    // Wait until there is space
    while (new_wr == aud_rd) delay(20);

    memcpy(aud_buf[aud_wr], buf, siz);
    aud_wr= new_wr;

    return;
  }
#endif

#ifdef ALSA_AUDIO
  if (out_fd == -9998) {
    int err;
    int frames = siz / (out_mode ? 4 : 2); // Number of frames (stereo samples)
    
    // Write data to the ALSA device
    if ((err = snd_pcm_writei(alsa_handle, buf, frames)) < 0) {
      if (err == -EPIPE) {
        // Underflow occurred, try to recover
        if ((err = snd_pcm_prepare(alsa_handle)) < 0) {
          error("Unable to recover from underrun: %s", snd_strerror(err));
        }
        // Try to write again
        if ((err = snd_pcm_writei(alsa_handle, buf, frames)) < 0) {
          error("Failed to write to ALSA device after recovery: %s", snd_strerror(err));
        }
      } else {
        error("Failed to write to ALSA device: %s", snd_strerror(err));
      }
    }
    
    return;
  }
#endif

  while (-1 != (rv= write(out_fd, buf, siz))) {
    if (0 == (siz -= rv)) return;
    buf += rv;
  }
  error("Output error");
}

//
//	Calculate amplitude adjustment factor for frequency 'freq'
//

double 
ampAdjust(double freq) {
   int a;
   struct AmpAdj *p0, *p1;

   if (!opt_c) return 1.0;
   if (freq <= ampadj[0].freq) return ampadj[0].adj;
   if (freq >= ampadj[opt_c-1].freq) return ampadj[opt_c-1].adj;

   for (a= 1; a<opt_c; a++) 
      if (freq < ampadj[a].freq) 
	 break;
   
   p0= &ampadj[a-1];
   p1= &ampadj[a];
      
   return p0->adj + (p1->adj - p0->adj) * (freq - p0->freq) / (p1->freq - p0->freq);
}
   

//
//	Correct channel values and types according to current period,
//	and current time
//

void 
corrVal(int running) {
   int a;
   int t0= per->tim;
   int t1= per->nxt->tim;
   Channel *ch;
   Voice *v0, *v1, *vv;
   double rat0, rat1;
   int trigger= 0;
   
   // Move to the correct period
   while ((now >= t0) ^ (now >= t1) ^ (t1 > t0)) {
      per= per->nxt;
      t0= per->tim;
      t1= per->nxt->tim;
      if (running) {
	 if (tty_erase) {
#ifdef ANSI_TTY	
	    fprintf(stderr, "\033[K");
#else
	    fprintf(stderr, "%*s\r", tty_erase, ""); 
	    tty_erase= 0;
#endif
	 }
	 dispCurrPer(stderr); status(0);
      }
      trigger= 1;		// Trigger bells or whatever
   }
   
   // Run through to calculate voice settings for current time
   rat1= t_per0(t0, now) / (double)t_per24(t0, t1);
   rat0= 1 - rat1;
   for (a= 0; a<N_CH; a++) {
      ch= &chan[a];
      v0= &per->v0[a];
      v1= &per->v1[a];
      vv= &ch->v;

      // Pointer to the amplitude of the mix to use with mix effects.
      if(vv->typ == 5 && mix_amp == NULL)
        mix_amp= &vv->amp;
      
      if (vv->typ != v0->typ) {
	 switch (vv->typ= ch->typ= v0->typ) {
	  case 1:
	     ch->off1= ch->off2= 0; break;
	  case 2:
	     break;
	  case 3:
	     ch->off1= ch->off2= 0; break;
	  case 4:
	     ch->off1= ch->off2= 0; break;
	  case 5:
	     break;
	  case 8:  // Isochronic tones
	     ch->off1= ch->off2= 0; break;
	  case 6:  // Mixspin
	     ch->off1= ch->off2= 0; break;
	  case 7:  // Mixpulse
	     ch->off1= ch->off2= 0; break;
	  case 13:  // Mixbeat
	     ch->off1= ch->off2= 0;
	     reset_mixbeat_channel_state(a);
	     break;
	  case 11:  // Bspin - spinning brown noise
	     ch->off1= ch->off2= 0; break;
	  case 12:  // Wspin - spinning white noise
	     ch->off1= ch->off2= 0; break;
	  default:
	     ch->off1= ch->off2= ch->off3= 0; break;
	 }
      }
      
      // Setup vv->*
      switch (vv->typ) {
       case 1:
	  vv->amp= rat0 * v0->amp + rat1 * v1->amp;
	  vv->carr= rat0 * v0->carr + rat1 * v1->carr;
	  vv->res= rat0 * v0->res + rat1 * v1->res;
	  vv->waveform= v0->waveform;
	  break;
       case 2:
	  vv->amp= rat0 * v0->amp + rat1 * v1->amp;
	  vv->waveform= v0->waveform;
	  break;
       case 3:
	  vv->amp= v0->amp;		// No need to slide, as bell only rings briefly
	  vv->carr= v0->carr;
	  vv->waveform= v0->waveform;
	  break;
       case 4:
	  vv->amp= rat0 * v0->amp + rat1 * v1->amp;
	  vv->carr= rat0 * v0->carr + rat1 * v1->carr;
	  vv->res= rat0 * v0->res + rat1 * v1->res;
	  if (vv->carr > spin_carr_max) vv->carr= spin_carr_max; // Clipping sweep width
	  if (vv->carr < -spin_carr_max) vv->carr= -spin_carr_max;
	  vv->waveform= v0->waveform;
	  break;
       case 5:
	  vv->amp= rat0 * v0->amp + rat1 * v1->amp;
	  vv->waveform= v0->waveform;
	  break;
       case 8:  // Isochronic tones
          vv->amp= rat0 * v0->amp + rat1 * v1->amp;
          vv->carr= rat0 * v0->carr + rat1 * v1->carr;
          vv->res= rat0 * v0->res + rat1 * v1->res;
          vv->waveform= v0->waveform;
          break;
       case 6:  // Mixspin
          vv->amp= rat0 * v0->amp + rat1 * v1->amp;
          vv->carr= rat0 * v0->carr + rat1 * v1->carr;
          vv->res= rat0 * v0->res + rat1 * v1->res;
          if (vv->carr > spin_carr_max) vv->carr= spin_carr_max; // Clipping sweep width
          if (vv->carr < -spin_carr_max) vv->carr= -spin_carr_max;
          vv->waveform= v0->waveform;
          break;
       case 7:  // Mixpulse
          vv->amp= rat0 * v0->amp + rat1 * v1->amp;
          vv->res= rat0 * v0->res + rat1 * v1->res;
          vv->waveform= v0->waveform;
          break;
       case 13:  // Mixbeat
          vv->amp= rat0 * v0->amp + rat1 * v1->amp;
          vv->res= rat0 * v0->res + rat1 * v1->res;
          vv->waveform= v0->waveform;
          break;
       case 11:  // Bspin - spinning brown noise
          vv->amp= rat0 * v0->amp + rat1 * v1->amp;
          vv->carr= rat0 * v0->carr + rat1 * v1->carr;
          vv->res= rat0 * v0->res + rat1 * v1->res;
          if (vv->carr > spin_carr_max) vv->carr= spin_carr_max; // Clipping sweep width
          if (vv->carr < -spin_carr_max) vv->carr= -spin_carr_max;
          vv->waveform= v0->waveform;
          break;
       case 12:  // Wspin - spinning white noise
          vv->amp= rat0 * v0->amp + rat1 * v1->amp;
          vv->carr= rat0 * v0->carr + rat1 * v1->carr;
          vv->res= rat0 * v0->res + rat1 * v1->res;
          if (vv->carr > spin_carr_max) vv->carr= spin_carr_max; // Clipping sweep width
          if (vv->carr < -spin_carr_max) vv->carr= -spin_carr_max;
          vv->waveform= v0->waveform;
          break;
       default:		// Waveform based binaural
          vv->amp= rat0 * v0->amp + rat1 * v1->amp;
          vv->carr= rat0 * v0->carr + rat1 * v1->carr;
          vv->res= rat0 * v0->res + rat1 * v1->res;
          vv->waveform= v0->waveform;
          break;
      }

      // For selected built-in modes, evaluate carrier/beat directly
      // from a function instead of segment-to-segment interpolation.
      apply_func_curve(now, a, vv);
   }
   
   // Check and limit amplitudes if -c option in use
   if (opt_c) {
      double tot_beat= 0, tot_other= 0;
      for (a= 0; a<N_CH; a++) {
	 vv= &chan[a].v;
	 if (vv->typ == 1) {
	    double adj1= ampAdjust(vv->carr + vv->res/2);
	    double adj2= ampAdjust(vv->carr - vv->res/2);
	    if (adj2 > adj1) adj1= adj2;
	    tot_beat += vv->amp * adj1;
	 } else if (vv->typ) {
	    tot_other += vv->amp;
	 }
      }
      if (tot_beat + tot_other > 4096) {
	 double adj_beat= (tot_beat > 4096) ? 4096 / tot_beat : 1.0;
	 double adj_other= (4096 - tot_beat * adj_beat) / tot_other;
	 for (a= 0; a<N_CH; a++) {
	    vv= &chan[a].v;
	    if (vv->typ == 1)
	       vv->amp *= adj_beat;
	    else if (vv->typ) 	
	       vv->amp *= adj_other;
	 }
      }
   }
   
   // Setup Channel data from Voice data
   for (a= 0; a<N_CH; a++) {
      ch= &chan[a];
      vv= &ch->v;
      
      // Setup ch->* from vv->*
      switch (vv->typ) {
	 double freq1, freq2;
       case 1:
	  freq1= vv->carr + vv->res/2;
	  freq2= vv->carr - vv->res/2;
	  if (opt_c) {
	     ch->amp= vv->amp * ampAdjust(freq1);
	     ch->amp2= vv->amp * ampAdjust(freq2);
	  } else 
	     ch->amp= ch->amp2= (int)vv->amp;
	  ch->inc1= (int)(freq1 / out_rate * ST_SIZ * 65536);
	  ch->inc2= (int)(freq2 / out_rate * ST_SIZ * 65536);
	  break;
       case 2:
	  ch->amp= (int)vv->amp;
	  break;
       case 3:
	  ch->amp= (int)vv->amp;
	  ch->inc1= (int)(vv->carr / out_rate * ST_SIZ * 65536);
	  if (trigger) {		// Trigger the bell only on entering the period
	     ch->off2= ch->amp;
	     ch->inc2= out_rate/20;
	  }
	  break;
       case 4:
	  ch->amp= (int)vv->amp;
	  ch->inc1= (int)(vv->res / out_rate * ST_SIZ * 65536);
	  ch->inc2= (int)(vv->carr * 1E-6 * out_rate * (1<<24) / ST_AMP);
	  break;
       case 5:
	  ch->amp= (int)vv->amp;
	  break;
       case 8:  // Isochronic tones
          ch->amp= (int)vv->amp;
          // Carrier (tone frequency)
          ch->inc1= (int)(vv->carr / out_rate * ST_SIZ * 65536);
          // Modulator (pulse frequency)
          ch->inc2= (int)(vv->res / out_rate * ST_SIZ * 65536);
          break;
       case 6:  // Mixspin
          ch->amp= (int)vv->amp;
          ch->inc1= (int)(vv->res / out_rate * ST_SIZ * 65536);
          ch->inc2= (int)(vv->carr * 1E-6 * out_rate * (1<<24) / ST_AMP);
          break;
       case 7:  // Mixpulse
          ch->amp= (int)vv->amp;
          // Modulator (pulse frequency)
          ch->inc2= (int)(vv->res / out_rate * ST_SIZ * 65536);
          break;
       case 13:  // Mixbeat
          ch->amp= (int)vv->amp;
          // Oscillator at beat/2: L and R receive opposite frequency shifts.
          ch->inc2= (int)((vv->res * 0.5) / out_rate * ST_SIZ * 65536);
          break;
       case 11:  // Bspin - spinning brown noise
          ch->amp= (int)vv->amp;
          ch->inc1= (int)(vv->res / out_rate * ST_SIZ * 65536);
          ch->inc2= (int)(vv->carr * 1E-6 * out_rate * (1<<24) / ST_AMP);
          break;
       case 12:  // Wspin - spinning white noise
          ch->amp= (int)vv->amp;
          ch->inc1= (int)(vv->res / out_rate * ST_SIZ * 65536);
          ch->inc2= (int)(vv->carr * 1E-6 * out_rate * (1<<24) / ST_AMP);
          break;
       default:		// Custom-envelope binaural (waveNN)
	  ch->amp= (int)vv->amp;
	  ch->inc1= (int)((vv->carr + vv->res/2) / out_rate * ST_SIZ * 65536);
	  ch->inc2= (int)((vv->carr - vv->res/2) / out_rate * ST_SIZ * 65536);
	  ch->inc3= (int)(fabs(vv->res) / out_rate * ST_SIZ * 65536);
	  break;
      }
   }

   // Track current mix/<amp> value for mix-effect base volume logic.
   mix_amp_current= 4096.0;
   for (a= 0; a<N_CH; a++) {
      if (chan[a].typ == 5) {
	 mix_amp_current= chan[a].amp;
	 break;
      }
   }
}       
      
//
//	Setup audio device
//

void 
setup_device(void) {

  detect_output_encoder();
  if (opt_O && out_enc_fmt != OUT_ENC_NONE)
     error("%s output requires a filename with -o (stdout is not supported for this format yet)",
	   output_encoder_name());
  if (!opt_Q && out_enc_fmt == OUT_ENC_NONE &&
      (opt_mp3_bitrate_set || opt_mp3_quality_set || opt_mp3_vbr_quality_set || opt_ogg_quality_set || opt_flac_compression_set))
     warn("Encoder settings (-K/-J/-X/-U/-Z) are ignored unless -o uses .mp3/.ogg/.flac");

  if (!opt_Q && opt_V != 100) {
    warn("Global volume level set to %d%%", opt_V);
  }

  if (!opt_Q && opt_w != 0) {
   warn("Using global %s waveform for brainwave tones", waveform_name[opt_w]);
  }

  if (!opt_Q && opt_I) {
   warn("Using custom isochronic envelope: s=%g d=%g a=%g r=%g e=%d",
	opt_I_s, opt_I_d, opt_I_a, opt_I_r, opt_I_e);
  }

  // Handle output to files and pipes
  if (opt_O || opt_o) {
    if (opt_O)
      out_fd= 1;		// stdout
    else {
      if (out_enc_fmt == OUT_ENC_OGG || out_enc_fmt == OUT_ENC_FLAC) {
	 out_fd= -1;		// libsndfile opens and manages the file internally
      } else {
	 FILE *out;		// Need to create a stream to set binary mode for DOS
	 if (!(out= fopen(opt_o, "wb")))
	    error("Can't open \"%s\", errno %d", opt_o, errno);
	 out_fd= fileno(out);
      }
    }

    init_output_encoder();

    out_blen= out_rate * 2 / out_prate;		// 10 fragments a second by default
    while (out_blen & (out_blen-1)) out_blen &= out_blen-1;		// Make power of two
    out_bsiz= out_blen * (out_mode ? 2 : 1);
    out_bps= out_mode ? 4 : 2;
    out_buf= (short*)Alloc(out_blen * sizeof(short));
    out_buf_lo= (int)(0x10000 * 1000.0 * 0.5 * out_blen / out_rate);
    out_buf_ms= out_buf_lo >> 16;
    out_buf_lo &= 0xFFFF;
    tmp_buf= (int*)Alloc(out_blen * sizeof(int));

    if (!opt_Q && !opt_W && !out_enc_active)		// Informational message for opt_W/encoded is written later
       warn("Outputting %d-bit raw audio data at %d Hz with %d-sample blocks, %d ms per block",
	    out_mode ? 16 : 8, out_rate, out_blen/2, out_buf_ms);
    return;
  }

#ifdef ALSA_AUDIO
  // ALSA audio output
  {
    int err;
    unsigned int rate = out_rate;
    snd_pcm_format_t format;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    
    // Open the PCM device for playback
    if ((err = snd_pcm_open(&alsa_handle, opt_d, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
      error("Unable to open ALSA device %s: %s", opt_d, snd_strerror(err));
    }
    
    // Allocate hardware parameters
    snd_pcm_hw_params_alloca(&alsa_params);
    
    // Fill with default values
    if ((err = snd_pcm_hw_params_any(alsa_handle, alsa_params)) < 0) {
      error("Unable to configure default parameters: %s", snd_strerror(err));
    }
    
    // Configure access
    if ((err = snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
      error("Unable to configure access: %s", snd_strerror(err));
    }
    
    // Configure format (16 bits or 8 bits)
    format = out_mode ? SND_PCM_FORMAT_S16 : SND_PCM_FORMAT_U8;
    if ((err = snd_pcm_hw_params_set_format(alsa_handle, alsa_params, format)) < 0) {
      error("Unable to configure format %s: %s", 
            out_mode ? "16-bit" : "8-bit", snd_strerror(err));
    }
    
    // Configure channels (stereo)
    if ((err = snd_pcm_hw_params_set_channels(alsa_handle, alsa_params, 2)) < 0) {
      error("Unable to configure stereo channels: %s", snd_strerror(err));
    }
    
    // Configure sampling rate
    if ((err = snd_pcm_hw_params_set_rate_near(alsa_handle, alsa_params, &rate, 0)) < 0) {
      error("Unable to configure sampling rate %d: %s", 
            out_rate, snd_strerror(err));
    }
    
    // Configure buffer and period size
    period_size = 1024; // Initial value
    if ((err = snd_pcm_hw_params_set_period_size_near(alsa_handle, alsa_params, &period_size, 0)) < 0) {
      error("Unable to configure period size: %s", snd_strerror(err));
    }
    
    buffer_size = period_size * 4; // Buffer of 4 periods
    if ((err = snd_pcm_hw_params_set_buffer_size_near(alsa_handle, alsa_params, &buffer_size)) < 0) {
      error("Unable to configure buffer size: %s", snd_strerror(err));
    }
    
    // Apply hardware parameters
    if ((err = snd_pcm_hw_params(alsa_handle, alsa_params)) < 0) {
      error("Unable to apply hardware parameters: %s", snd_strerror(err));
    }
    
    // Get the actual period size
    snd_pcm_hw_params_get_period_size(alsa_params, &period_size, 0);
    
    // Configure output buffer size
    out_blen = period_size * 2; // Stereo
    out_bsiz = out_blen * (out_mode ? 2 : 1); // 16 bits or 8 bits
    out_bps = out_mode ? 4 : 2;
    out_buf = (short*)Alloc(out_blen * sizeof(short));
    out_buf_lo = (int)(0x10000 * 1000.0 * 0.5 * out_blen / out_rate);
    out_buf_ms = out_buf_lo >> 16;
    out_buf_lo &= 0xFFFF;
    tmp_buf = (int*)Alloc(out_blen * sizeof(int));
    
    // Mark that we are using ALSA
    out_fd = -9998; // Special value for ALSA
    
    if (!opt_Q) {
      if(rate != out_rate && out_rate_def)
        warn("*** WARNING: Device output rate is %d Hz, but SBaGenX is configured for %d Hz ***", rate, out_rate);
      warn("ALSA audio output %d-bit at %d Hz with period of %lu samples, %d ms per period",
           out_mode ? 16 : 8, out_rate, period_size, out_buf_ms);
    }
    return;
  }
#endif

#ifdef WIN_AUDIO
  // Output using Win32 calls
  {
     MMRESULT rv;
     WAVEFORMATEX fmt;
     int a;

     fmt.wFormatTag= WAVE_FORMAT_PCM;           
     fmt.nChannels= 2;
     fmt.nSamplesPerSec= out_rate;
     fmt.wBitsPerSample= out_mode ? 16 : 8;
     fmt.nBlockAlign= fmt.nChannels * (fmt.wBitsPerSample/8);
     fmt.nAvgBytesPerSec= fmt.nSamplesPerSec * fmt.nBlockAlign;
     fmt.cbSize= 0;
     aud_handle= NULL;

     // if (MMSYSERR_NOERROR != 
     //    waveOutOpen(&aud_handle, WAVE_MAPPER, &fmt, 0, 
     //                0L, WAVE_FORMAT_QUERY))
     //    error("Windows is rejecting our audio request (%d-bit stereo, %dHz)",
     //          out_mode ? 16 : 8, out_rate);

     if (MMSYSERR_NOERROR != 
	 (rv= waveOutOpen(&aud_handle, WAVE_MAPPER, 
			  (WAVEFORMATEX*)&fmt, (DWORD_PTR)win32_audio_callback, 
			  (DWORD)0, CALLBACK_FUNCTION))) {
	char buf[255];
	waveOutGetErrorText(rv, buf, sizeof(buf)-1);
	error("Can't open audio device (%d-bit stereo, %dHz):\n  %s",
               out_mode ? 16 : 8, out_rate, buf);
     }

     if (fmt.nChannels != 2)
	error("Can't open audio device in stereo");
     if (fmt.wBitsPerSample != (out_mode ? 16 : 8))
	error("Can't open audio device in %d-bit mode", out_mode ? 16 : 8);
     
     aud_current= 0;
     aud_cnt= 0;

     for (a= 0; a<BUFFER_COUNT; a++) {
	char *p= (char *)Alloc(sizeof(WAVEHDR) + BUFFER_SIZE);
	WAVEHDR *w= aud_head[a]= (WAVEHDR*)p;

	w->lpData= (LPSTR)p + sizeof(WAVEHDR);
	w->dwBufferLength= (DWORD)BUFFER_SIZE;
	w->dwBytesRecorded= 0L;
	w->dwUser= 0;
	w->dwFlags= 0;
	w->dwLoops= 0;
	w->lpNext= 0;
	w->reserved= 0;

	rv= waveOutPrepareHeader(aud_handle, w, sizeof(WAVEHDR));
	if (rv != MMSYSERR_NOERROR) {
	   char buf[255];
	   waveOutGetErrorText(rv, buf, sizeof(buf)-1);
	   error("Can't setup a wave header %d:\n  %s", a, buf);
	}
	w->dwFlags |= WHDR_DONE;
     }     
     
     out_rate= fmt.nSamplesPerSec;
     out_bsiz= BUFFER_SIZE;
     out_blen= out_mode ? out_bsiz/2 : out_bsiz;
     out_bps= out_mode ? 4 : 2;
     out_buf= (short*)Alloc(out_blen * sizeof(short));
     out_buf_lo= (int)(0x10000 * 1000.0 * 0.5 * out_blen / out_rate);
     out_buf_ms= out_buf_lo >> 16;
     out_buf_lo &= 0xFFFF;
     out_fd= -9999;
     tmp_buf= (int*)Alloc(out_blen * sizeof(int));

     if (!opt_Q)
	warn("Outputting %d-bit audio at %d Hz with %d %d-sample fragments, "
	     "%d ms per fragment", out_mode ? 16 : 8, 
	     out_rate, BUFFER_COUNT, out_blen/2, out_buf_ms);
  }
#endif
#ifdef MAC_AUDIO
  // Mac CoreAudio for OS X
  {
    char deviceName[256];
    OSStatus err;
    UInt32 propertySize, bufferByteCount;
    struct AudioStreamBasicDescription streamDesc;
    
    int device_out_rate;
    int buffer_size= opt_B > 0 ? opt_B : 4096; // Default is 2048 samples (L+R)

    // Initialize the audio buffers for Mac here
    init_mac_audio();

    out_bsiz= buffer_size;
    out_blen= out_mode ? out_bsiz/2 : out_bsiz;
    out_bps= out_mode ? 4 : 2;
    out_buf= (short*)Alloc(out_blen * sizeof(short));
    out_buf_lo= (int)(0x10000 * 1000.0 * 0.5 * out_blen / out_rate);
    out_buf_ms= out_buf_lo >> 16;
    out_buf_lo &= 0xFFFF;
    tmp_buf= (int*)Alloc(out_blen * sizeof(int));

    // N.B.  Both -r and -b flags are totally ignored for CoreAudio --
    // we just use whatever the default device is set to, and feed it
    // floats.
    out_mode= 1;
    out_fd= -9999;

    // Find default device
    propertySize= sizeof(aud_dev);
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };
    if ((err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &propertySize, &aud_dev)))
    {
        error("Get default output device failed, status = %d", (int)err);
    }
    
    if (aud_dev == kAudioDeviceUnknown)
      error("No default audio device found");
    
    // Get device name
    propertySize = sizeof(deviceName);
    propertyAddress.mSelector = kAudioDevicePropertyDeviceName;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;
    if ((err = AudioObjectGetPropertyData(aud_dev, &propertyAddress, 0, NULL, &propertySize, deviceName)))
    {
        error("Get audio device name failed, status = %d", (int)err);
    }
    
    // Get device properties
    propertySize = sizeof(streamDesc);
    propertyAddress.mSelector = kAudioDevicePropertyStreamFormat;
    propertyAddress.mScope = kAudioObjectPropertyScopeOutput; // Adjusted for output
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;
    if ((err = AudioObjectGetPropertyData(aud_dev, &propertyAddress, 0, NULL, &propertySize, &streamDesc)))
    {
        error("Get audio device properties failed, status = %d", (int)err);
    }

    device_out_rate = (int)streamDesc.mSampleRate;

    if (streamDesc.mChannelsPerFrame != 2) 
      error("SBaGenX requires a stereo output device -- \n"
	    "default output has %d channels",
	    streamDesc.mChannelsPerFrame);

    if (streamDesc.mFormatID != kAudioFormatLinearPCM ||
	!(streamDesc.mFormatFlags & kLinearPCMFormatFlagIsFloat)) 
      error("Expecting a 32-bit float linear PCM output stream -- \n"
	    "default output uses another format");

    // Set buffer size
    bufferByteCount = (float) buffer_size / 2 * sizeof(float);
    propertySize = sizeof(bufferByteCount);
    propertyAddress.mSelector = kAudioDevicePropertyBufferSize;
    propertyAddress.mScope = kAudioObjectPropertyScopeGlobal;
    propertyAddress.mElement = kAudioObjectPropertyElementMaster;
    if ((err = AudioObjectSetPropertyData(aud_dev, &propertyAddress, 0, NULL, propertySize, &bufferByteCount)))
    {
        error("Set audio output buffer size failed, status = %d", (int)err);
    }

    // Setup callback and start it
    err = AudioDeviceCreateIOProcID(aud_dev, mac_callback, (void *)1, &proc_id);
    if (err != noErr) {
        error("Failed to create audio callback, status = %d", (int)err);
    }
    
    err = AudioDeviceStart(aud_dev, proc_id);
    if (err != noErr) {
        AudioDeviceDestroyIOProcID(aud_dev, proc_id);
        error("Failed to start audio device, status = %d", (int)err);
    }

    // Report settings      
    if (!opt_Q) {
       if (device_out_rate != out_rate && out_rate_def) 
	      warn("*** WARNING: Device output rate is %d Hz, but SBaGenX is configured for %d Hz ***", device_out_rate, out_rate);
       warn("Outputting %d-bit audio at %d Hz to \"%s\",\n"
	    "  using %d %d-sample fragments, %d ms per fragment",
	    (int)streamDesc.mBitsPerChannel, out_rate, deviceName,
	    BUFFER_COUNT, out_blen/2, out_buf_ms);
    }
  }
#endif
#ifdef NO_AUDIO
  error("Direct output to soundcard not supported on this platform.\n"
	"Use -o or -O to write raw data, or -Wo or -WO to write a WAV file.");
#endif
}

//
//	Audio callback for Win32
//

#ifdef WIN_AUDIO
void CALLBACK
win32_audio_callback(HWAVEOUT hand, UINT uMsg,     
		     DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
   switch (uMsg) {
    case WOM_CLOSE:
       break;
    case WOM_OPEN:
       break;
    case WOM_DONE:
       aud_cnt--;
       //debug("Buffer done (cnt==%d)", aud_cnt);
       //debug_win32_buffer_status();
       break;
   }
}

void debug_win32_buffer_status() {
   char tmp[80];
   char *p= tmp;
   int a;
   for (a= 0; a<BUFFER_COUNT; a++) {
      *p++= (aud_head[a]->dwFlags & WHDR_INQUEUE) ? 'I' : '-';
      *p++= (aud_head[a]->dwFlags & WHDR_DONE) ? 'D' : '-';
      *p++= ' ';
   }
   p[-1]= 0;
   debug(tmp);
}
#endif

//
//	Audio callback for Mac OS X
//

#ifdef MAC_AUDIO
OSStatus mac_callback(AudioDeviceID inDevice,
		      const AudioTimeStamp *inNow,
		      const AudioBufferList *inInputData,
		      const AudioTimeStamp *inInputTime,
		      AudioBufferList *outOutputData,
		      const AudioTimeStamp *inOutputTime,
		      void *inClientData) 
{
  float *fp= outOutputData->mBuffers[0].mData;
  int cnt= opt_B > 0 ? opt_B / 2 : BUFFER_SIZE / 2;
  short *sp;

  if (aud_rd == aud_wr) {
    // Nothing in buffer list, so fill with silence
    while (cnt-- > 0) *fp++= 0.0;
  } else {
    // Consume a buffer
    sp= (short*)aud_buf[aud_rd];
    while (cnt-- > 0) *fp++= *sp++ * (1/32768.0);
    aud_rd= (aud_rd + 1) % BUFFER_COUNT;
  }
  
  return kAudioHardwareNoError;
}
#endif

//
//	Write a WAV header, and setup out_mode if byte-swapping is
//	required.  'byte_count' should have been set up by this point.
//

#define addU4(xx) { int a= xx; *p++= a; *p++= (a >>= 8); *p++= (a >>= 8); *p++= (a >>= 8); }
#define addStr(xx) { char *q= xx; *p++= *q++; *p++= *q++; *p++= *q++; *p++= *q++; }

void 
writeWAV() {
  char buf[44], *p= buf;

  if (byte_count + 36 != (int)(byte_count + 36)) {
     int tmp;
     byte_count= 0xFFFFFFF8-36;
     tmp= byte_count/out_bps/out_rate;
     warn("WARNING: Selected length is too long for the WAV format; truncating to %dh%02dm%02ds",
	  tmp/3600, tmp/60%60, tmp%60);
  }

  addStr("RIFF");
  addU4(byte_count + 36);
  addStr("WAVE");
  addStr("fmt ");
  addU4(16);
  addU4(0x00020001);
  addU4(out_rate);
  addU4(out_rate * out_bps);
  addU4(0x0004 + 0x10000*(out_bps*4));	// 2,4 -> 8,16 - always assume stereo
  addStr("data");
  addU4(byte_count);
  writeOut(buf, 44);

  if (!opt_Q)
     warn("Outputting %d-bit WAV data at %d Hz, file size %d bytes",
	  out_mode ? 16 : 8, out_rate, byte_count + 44);
}

//
//	Read a line, discarding blank lines and comments.  Rets:
//	Another line?  Comments starting with '##' are displayed on
//	stderr.
//   

int 
readLine() {
   char *p;
   lin= buf;
   
   while (1) {
      if (!fgets(lin, sizeof(buf), in)) {
	 if (feof(in)) return 0;
	 error("Read error on sequence file");
      }
      
      in_lin++;
      
      while (isspace(*lin)) lin++;
      p= strchr(lin, '#');
      if (p && p[1] == '#') fprintf(stderr, "%s", p);
      p= p ? p : strchr(lin, 0);
      while (p > lin && isspace(p[-1])) p--;
      if (p != lin) break;
   }
   *p= 0;
   lin_copy= buf_copy;
   strcpy(lin_copy, lin);
   return 1;
}

//
//	Get next word at '*lin', moving lin onwards, or return 0
//

char *
getWord() {
  char *rv, *end;
  while (isspace(*lin)) lin++;
  if (!*lin) return 0;

  rv= lin;
  while (*lin && !isspace(*lin)) lin++;
  end= lin;
  if (*lin) lin++;
  *end= 0;

  return rv;
}

//
//	Bad sequence file
//

void 
badSeq() {
  error("Bad sequence file content at line: %d\n  %s", in_lin, lin_copy);
}

// Convenience for situations where buffer is being filled by
// something other than readLine()
void 
readNameDef2() {
   lin= buf; lin_copy= buf_copy;
   strcpy(lin_copy, lin);
   readNameDef();
}
void 
readTimeLine2() {
   lin= buf; lin_copy= buf_copy;
   strcpy(lin_copy, lin);
   readTimeLine();
}

// Convenience for creating sequences on the fly
void 
formatNameDef(char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   readNameDef2();
}
void 
formatTimeLine(int tim, char *fmt, ...) {
   va_list ap;
   char *p= buf + sprintf(buf, "%02d:%02d:%02d ", tim/3600, tim/60%60, tim%60);
   va_start(ap, fmt);
   vsnprintf(p, buf + sizeof(buf) - p, fmt, ap);
   readTimeLine2();
}

//
//	Generate a list of Period structures, based on the tone-specs
//	passed in (ac,av)
//

void 
readSeqImm(int ac, char **av) {
   char *p= buf;

   if (sbx_try_readSeqImm_runtime(ac, av))
      return;

   in_lin= 0;
   p += sprintf(p, "immediate:");
   while (ac-- > 0) p += sprintf(p, " %s", *av++);
   readNameDef2();
   
   strcpy(buf, "00:00 immediate");
   readTimeLine2();

   correctPeriods();
}

int
sbx_try_readSeq_runtime(int ac, char **av) {
   const char *fnam;
   SbxEngineConfig cfg;
   SbxContext *ctx;
   int rc;
   char *seq_text= 0;
   char *safe_mix_path= 0;
   int safe_S= 0, safe_E= 0, safe_have_T= 0, safe_T_ms= -1;
   int safe_have_q= 0, safe_q_mult= 1;
   int safe_have_r= 0, safe_rate= 44100;
   int safe_have_F= 0, safe_fade_ms= 60000;

   if (ac != 1 || !av)
      return 0;
   fnam= av[0];
   if (!fnam || 0 == strcmp(fnam, "-"))
      return 0;

   if (sbx_try_run_option_only_seq_wrapper(fnam))
      return 1;

   rc= sbx_prepare_seqfile_runtime_text(fnam, &seq_text,
					&safe_S, &safe_E,
					&safe_have_T, &safe_T_ms,
					&safe_have_q, &safe_q_mult,
					&safe_have_r, &safe_rate,
					&safe_have_F, &safe_fade_ms,
					&safe_mix_path);
   if (rc == SBX_ENOMEM)
      error("Out of memory preparing sequence file for sbagenxlib runtime");
   if (rc != SBX_OK)
      return 0;

   if (safe_have_r) {
      out_rate= safe_rate;
      out_rate_def= 0;
   }
   if (safe_have_F)
      fade_int= safe_fade_ms;
   if (safe_have_q) {
      opt_S= 1;
      fast_mult= safe_q_mult;
   }

   sbx_default_engine_config(&cfg);
   cfg.sample_rate= (double)out_rate;
   cfg.channels= 2;
   ctx= sbx_context_create(&cfg);
   if (!ctx)
      goto fail;
   if (sbx_context_set_default_waveform(ctx, opt_w) != SBX_OK) {
      sbx_context_destroy(ctx);
      goto fail;
   }

   rc= sbx_context_load_sbg_timing_text(ctx, seq_text, 0);
   if (rc != SBX_OK)
      rc= sbx_context_load_sequence_text(ctx, seq_text, 0);
   if (rc != SBX_OK) {
      sbx_context_destroy(ctx);
      goto fail;
   }

   if (safe_S) {
      opt_S= 1;
      if (!fast_mult) fast_mult= 1;
   }
   if (safe_E)
      opt_E= 1;
   if (safe_have_T && opt_T == -1) {
      opt_T= safe_T_ms;
      if (!fast_mult) fast_mult= 1;
   }
   if (safe_mix_path && !opt_m)
      opt_m= StrDup(safe_mix_path);

   if (opt_D) {
      emit_periods_from_sbx_context(ctx, 0);
      sbx_context_destroy(ctx);
      free(seq_text);
      if (safe_mix_path) free(safe_mix_path);
      return 1;
   }

   if (sbx_context_has_mix_amp_control(ctx) && !mix_in && !opt_m && !opt_M)
      warn("mix/<amp> was specified in sequence file but no mix input stream is active");
   if (sbx_context_has_mix_effects(ctx) && !mix_in && !opt_m && !opt_M)
      error("sequence file mix effects require a mix input stream (-m / -M)");
   if (opt_A && !opt_m && !opt_M)
      error("-A requires a mix input stream; use -m <file> or -M");

   // Runtime playback via sbagenxlib context.
   sbx_runtime_clear();
   sbx_runtime_ctx= ctx;
   sbx_runtime_active= 1;
   sbx_runtime_total_sec= sbx_context_duration_sec(ctx);
   if (!opt_Q)
      warn("Using sbagenxlib runtime for sequence file subset: %s", fnam);
   free(seq_text);
   if (safe_mix_path) free(safe_mix_path);
   return 1;

fail:
   if (seq_text) free(seq_text);
   if (safe_mix_path) free(safe_mix_path);
   return 0;
}

static int
sbx_seq_backend_mode(void) {
   const char *v= getenv("SBAGENX_SEQ_BACKEND");
   if (!v || !*v) return 0; /* auto */
   if (0 == strcmp(v, "auto")) return 0;
   if (0 == strcmp(v, "legacy")) return 1;
   if (0 == strcmp(v, "sbagenxlib") || 0 == strcmp(v, "sbagenlib") ||
       0 == strcmp(v, "library")) return 2;
   warn("Unknown SBAGENX_SEQ_BACKEND=%s (expected auto|legacy|sbagenxlib); using auto", v);
   return 0;
}

//
//	Read a list of sequence files, and generate a list of Period
//	structures
//

void 
readSeq(int ac, char **av) {
   int seq_backend= sbx_seq_backend_mode();
   if (seq_backend != 1 && !(seq_backend == 0 && opt_D)) {
      if (sbx_try_readSeq_runtime(ac, av))
	 return;
      if (seq_backend == 2)
	 error("SBAGENX_SEQ_BACKEND=sbagenxlib requested, but sequence input is not compatible with sbagenxlib subset loader");
   }

   // Setup a 'now' value to use for NOW in the sequence file
   now= calcNow();	

   while (ac-- > 0) {
      char *fnam= *av++;
      int start= 1;
      
      in= (0 == strcmp("-", fnam)) ? stdin : fopen(fnam, "r");
      if (!in) error("Can't open sequence file: %s", fnam);
      
      in_lin= 0;
      
      while (readLine()) {
	 char *p= lin;

	 // Blank lines
	 if (!*p) continue;
	 
	 // Look for options
	 if (*p == '-') {
	    if (!start) 
	       error("Options are only permitted at start of sequence file:\n  %s", p);
	    handleOptions(p);
	    continue;
	 }

	 // Check to see if it fits the form of <name>:<white-space>
	 start= 0;
	 if (!isalpha(*p)) 
	    p= 0;
	 else {
	    while (isalnum(*p) || *p == '_' || *p == '-') p++;
	    if (*p++ != ':' || !isspace(*p)) 
	       p= 0;
	 }
	 
	 if (p)
	    readNameDef();
	 else 
	    readTimeLine();
      }
      
      if (in != stdin) fclose(in);
   }
   
   correctPeriods();
}


//
//	Fill in all the correct information for the Periods, assuming
//	they have just been loaded using readTimeLine()
//


void 
correctPeriods() {
  if (opt_A && !opt_m && !opt_M)
    error("-A requires a mix input stream; use -m <file> or -M");

  // Get times all correct
  {
    Period *pp= per;
    do {
      if (pp->fi == -2) {
	pp->tim= pp->nxt->tim;
	pp->fi= -1;
      }

      pp= pp->nxt;
    } while (pp != per);
  }

  // Make sure that the transitional periods each have enough time
  {
    Period *pp= per;
    do {
      if (pp->fi == -1) {
	int per= t_per0(pp->tim, pp->nxt->tim);
	if (per < fade_int) {
	  int adj= (fade_int - per) / 2, adj0, adj1;
	  adj0= t_per0(pp->prv->tim, pp->tim);
	  adj0= (adj < adj0) ? adj : adj0;
	  adj1= t_per0(pp->nxt->tim, pp->nxt->nxt->tim);
	  adj1= (adj < adj1) ? adj : adj1;
	  pp->tim= (pp->tim - adj0 + H24) % H24;
	  pp->nxt->tim= (pp->nxt->tim + adj1) % H24;
	}
      }

      pp= pp->nxt;
    } while (pp != per);
  }

  // Fill in all the voice arrays, and sort out details of
  // transitional periods
  {
    Period *pp= per;
    do {
      if (pp->fi < 0) {
	int fo, fi;
	int a;
	int midpt= 0;

	Period *qq= (Period*)Alloc(sizeof(*qq));
	qq->prv= pp; qq->nxt= pp->nxt;
	qq->prv->nxt= qq->nxt->prv= qq;

	qq->tim= t_mid(pp->tim, qq->nxt->tim);

	memcpy(pp->v0, pp->prv->v1, sizeof(pp->v0));
	memcpy(qq->v1, qq->nxt->v0, sizeof(qq->v1));

	// Special handling for bells
	for (a= 0; a<N_CH; a++) {
	  if (pp->v0[a].typ == 3 && pp->fi != -3)
	    pp->v0[a].typ= 0;

	  if (qq->v1[a].typ == 3 && pp->fi == -3)
	    qq->v1[a].typ= 0;
	}
	      
	fo= pp->prv->fo;
	fi= qq->nxt->fi;

	// Special handling for -> slides:
	//   always slide, and stretch slide if possible
	if (pp->fi == -3) {
	  fo= fi= 2;		// Force slides for ->
	  for (a= 0; a<N_CH; a++) {
	    Voice *vp= &pp->v0[a];
	    Voice *vq= &qq->v1[a];
	    if (vp->typ == 0 && vq->typ != 0 && vq->typ != 3) {
	      memcpy(vp, vq, sizeof(*vp)); vp->amp= 0;
	    }
	    else if (vp->typ != 0 && vq->typ == 0) {
	      memcpy(vq, vp, sizeof(*vq)); vq->amp= 0;
	    }
	  }
	}

	memcpy(pp->v1, pp->v0, sizeof(pp->v1));
	memcpy(qq->v0, qq->v1, sizeof(qq->v0));

	for (a= 0; a<N_CH; a++) {
	  Voice *vp= &pp->v1[a];
	  Voice *vq= &qq->v0[a];
	  if ((fo == 0 || fi == 0) ||		// Fade in/out to silence
	      (vp->typ != vq->typ) ||		// Different types
        (vp->waveform != vq->waveform) || // Different waveforms
	      ((fo == 1 || fi == 1) &&		// Fade thru, but different pitches
	       (vp->typ == 1 || vp->typ < 0) && 
	       (vp->carr != vq->carr || vp->res != vq->res))
	      ) {
	    vp->amp= vq->amp= 0;		// To silence
	    midpt= 1;				// Definitely need the mid-point

	    if (vq->typ == 3) {	 		// Special handling for bells
	      vq->amp= qq->v1[a].amp; 
	      qq->nxt->v0[a].typ= qq->nxt->v1[a].typ= 0;
	    }
	  }
	  else if (vp->typ == 3) {		// Else smooth transition - for bells not so smooth
	    qq->v0[a].typ= qq->v1[a].typ= 0;
	  }
	  else {				// Else smooth transition
	    vp->amp= vq->amp= (vp->amp + vq->amp) / 2;
	    if (vp->typ == 1 || vp->typ == 4 || vp->typ < 0) {
	      vp->carr= vq->carr= (vp->carr + vq->carr) / 2;
	      vp->res= vq->res= (vp->res + vq->res) / 2;
	    }
	  }
	}

	// If we don't really need the mid-point, then get rid of it
	if (!midpt) {
	  memcpy(pp->v1, qq->v1, sizeof(pp->v1));
	  qq->prv->nxt= qq->nxt;
	  qq->nxt->prv= qq->prv;
	  free(qq);
	}
	else pp= qq;
      }

      pp= pp->nxt;
    } while (pp != per);
  }

  // Clear out zero length sections, and duplicate sections
  {
    Period *pp;
    while (per != per->nxt) {
      pp= per;
      do {
	if (voicesEq(pp->v0, pp->v1) &&
	    voicesEq(pp->v0, pp->nxt->v0) &&
	    voicesEq(pp->v0, pp->nxt->v1))
	  pp->nxt->tim= pp->tim;

	if (pp->tim == pp->nxt->tim) {
	  if (per == pp) per= per->prv;
	  pp->prv->nxt= pp->nxt;
	  pp->nxt->prv= pp->prv;
	  free(pp);
	  pp= 0;
	  break;
	}
	pp= pp->nxt;
      } while (pp != per);
      if (pp) break;
    }
  }

  // Make sure that the total is 24 hours only (not more !)
  if (per->nxt != per) {
    int tot= 0;
    Period *pp= per;
    
    do {
      tot += t_per0(pp->tim, pp->nxt->tim);
      pp= pp->nxt;
    } while (pp != per);

    if (tot > H24) {
      warn("Total time is greater than 24 hours.  Probably two times are\n"
	   "out of order.  Suspicious intervals are:\n");
      pp= per;
      do {
	if (t_per0(pp->tim, pp->nxt->tim) >= H12) 
	   warn("  %02d:%02d:%02d -> %02d:%02d:%02d",
		pp->tim % 86400000 / 3600000,
		pp->tim % 3600000 / 60000,
		pp->tim % 60000 / 1000,
		pp->nxt->tim % 86400000 / 3600000,
		pp->nxt->tim % 3600000 / 60000,
		pp->nxt->tim % 60000 / 1000);
	pp= pp->nxt;
      } while (pp != per);
      error("\nCheck the sequence around these times and try again");
    }
  }

  // Print the whole lot out
  if (opt_D) {
    Period *pp;
    if (per->nxt != per)
      while (per->prv->tim < per->tim) per= per->nxt;

    pp= per;

    if(!opt_Q) {
      printSequenceDuration();
    }

    do {
      dispCurrPer(stdout);
      per= per->nxt;
    } while (per != pp);
    printf("\n");

    exit(0);		// All done
  }  
}

int 
voicesEq(Voice *v0, Voice *v1) {
  int a= N_CH;

  while (a-- > 0) {
    if (v0->typ != v1->typ) return 0;
    switch (v0->typ) {
     case 1:
     case 4:
     default:
       if (v0->amp != v1->amp ||
	   v0->carr != v1->carr ||
	   v0->res != v1->res)
	 return 0;
       break;
     case 2:
     case 5:
       if (v0->amp != v1->amp)
	 return 0;
       break;
     case 3:
       if (v0->amp != v1->amp ||
	   v0->carr != v1->carr)
	 return 0;
       break;
    }
    v0++; v1++;
  }
  return 1;
}

static int
legacy_voice_from_sbx_tone(const SbxToneSpec *tone, Voice *out) {
  if (!tone || !out) return 0;

  out->waveform= tone->waveform;
  out->carr= tone->carrier_hz;
  out->res= tone->beat_hz;
  out->amp= AMP_DA(tone->amplitude * 100.0);

  switch (tone->mode) {
   case SBX_TONE_BINAURAL:
      out->typ= 1;
      return 1;
   case SBX_TONE_ISOCHRONIC:
      out->typ= 8;
      out->res= fabs(out->res);
      return 1;
   case SBX_TONE_PINK_NOISE:
      out->typ= 2;
      out->carr= 0.0;
      out->res= 0.0;
      return 1;
   case SBX_TONE_WHITE_NOISE:
      out->typ= 9;
      out->carr= 0.0;
      out->res= 0.0;
      return 1;
   case SBX_TONE_BROWN_NOISE:
      out->typ= 10;
      out->carr= 0.0;
      out->res= 0.0;
      return 1;
   case SBX_TONE_BELL:
      out->typ= 3;
      out->res= 0.0;
      return 1;
   case SBX_TONE_SPIN_PINK:
      out->typ= 4;
      return 1;
   case SBX_TONE_SPIN_BROWN:
      out->typ= 11;
      return 1;
   case SBX_TONE_SPIN_WHITE:
      out->typ= 12;
      return 1;
   default:
      break;
  }

  return 0;
}

static void
apply_mixam_envelope_override(SbxMixFxSpec *fx) {
   if (!fx || !opt_H || fx->type != SBX_MIXFX_AM)
      return;
   fx->mixam_mode= opt_H_m;
   fx->mixam_start= opt_H_s;
   fx->mixam_duty= opt_H_d;
   fx->mixam_attack= opt_H_a;
   fx->mixam_release= opt_H_r;
   fx->mixam_edge_mode= opt_H_e;
   fx->mixam_floor= opt_H_f;
}

static int
parse_legacy_voice_token(const char *tok, Voice *out, int *out_sets_mix_flag) {
   char dmy;
   double amp, carr, res;
   double mix_pct= 100.0;
   int wave;
   int extra_type= SBX_EXTRA_INVALID;
   SbxMixFxSpec fx;
   SbxToneSpec tone;

   if (!tok || !out) return 0;
   if (out_sets_mix_flag) *out_sets_mix_flag= 0;

   if (4 == sscanf(tok, "wave%d:%lf%lf/%lf %c", &wave, &carr, &res, &amp, &dmy)) {
      if (wave < 0 || wave >= 100)
	 error("Only wave00 to wave99 is permitted at line: %d\n  %s", in_lin, lin_copy);
      if (!waves[wave])
	 error("Waveform %02d has not been defined, line: %d\n  %s", wave, in_lin, lin_copy);
      out->typ= -1-wave;
      out->waveform= opt_w;
      out->carr= carr;
      out->res= res;
      out->amp= AMP_DA(amp);
      return 1;
   }

   if (SBX_OK == sbx_parse_extra_token(tok, opt_w, &extra_type, &tone, &fx, &mix_pct)) {
      if (extra_type == SBX_EXTRA_MIXFX)
	 apply_mixam_envelope_override(&fx);
      if (extra_type == SBX_EXTRA_MIXAMP) {
	 out->typ= 5;
	 out->amp= AMP_DA(mix_pct);
	 if (out_sets_mix_flag) *out_sets_mix_flag= 1;
	 return 1;
      } else if (extra_type == SBX_EXTRA_MIXFX) {
	 checkMixInSequence();
	 out->waveform= fx.waveform;
	 out->res= fx.res;
	 out->amp= AMP_DA(fx.amp * 100.0);
	 switch (fx.type) {
	  case SBX_MIXFX_SPIN:
	     out->typ= 6;
	     out->carr= fx.carr;
	     return 1;
	  case SBX_MIXFX_PULSE:
	     out->typ= 7;
	     out->carr= 0.0;
	     return 1;
	  case SBX_MIXFX_BEAT:
	     out->typ= 13;
	     out->carr= 0.0;
	     return 1;
	  default:
	     return 0;
	 }
      } else if (extra_type == SBX_EXTRA_TONE && legacy_voice_from_sbx_tone(&tone, out)) {
	 return 1;
      }
   }

   return 0;
}

//
//	Read a name definition
//

void 
readNameDef() {
  char *p, *q;
  NameDef *nd;
  int ch;

  if (!(p= getWord())) badSeq();

  q= strchr(p, 0) - 1;
  if (*q != ':') badSeq();
  *q= 0;
  for (q= p; *q; q++) if (!isalnum(*q) && *q != '-' && *q != '_') 
    error("Bad name \"%s\" in definition, line %d:\n  %s", p, in_lin, lin_copy);

  // Waveform definition ?
  if (0 == memcmp(p, "wave", 4) && 
      isdigit(p[4]) &&
      isdigit(p[5]) &&
      !p[6]) {
     int ii= (p[4] - '0') * 10 + (p[5] - '0');
     int siz= ST_SIZ * sizeof(int);
     int *arr= (int*)Alloc(siz);
     double *dp0= (double*)arr;
     double *dp1= (double*)(siz + (char*)arr);
     double *dp= dp0;
     double dmax= 0, dmin= 1;
     int np;

     if (waves[ii])
	error("Waveform %02d already defined, line %d:\n  %s",
	      ii, in_lin, lin_copy);
     waves[ii]= arr;
     
     while ((p= getWord())) {
	double dd;
	char dmy;
	if (1 != sscanf(p, "%lf %c", &dd, &dmy)) 
	   error("Expecting floating-point numbers on this waveform "
		 "definition line, line %d:\n  %s",
		 in_lin, lin_copy);
	if (dp >= dp1)
	   error("Too many samples on line (maximum %d), line %d:\n  %s",
		 dp1-dp0, in_lin, lin_copy);
	*dp++= dd;
	if (dmax < dmin) dmin= dmax= dd;
	else {
	   if (dd > dmax) dmax= dd;
	   if (dd < dmin) dmin= dd;
	}
     }
     dp1= dp;
     np= dp1 - dp0;
     if (np < 2) 
	error("Expecting at least two samples in the waveform, line %d:\n  %s",
	      in_lin, lin_copy);
     if (dmax == dmin)
	error("Waveform samples must not all be identical, line %d:\n  %s",
	      in_lin, lin_copy);

     // Adjust to range 0-1
     for (dp= dp0; dp < dp1; dp++)
	*dp= (*dp - dmin) / (dmax - dmin);

     sinc_interpolate(dp0, np, arr);
     
     if (DEBUG_DUMP_WAVES) {
	int a;
	printf("Dumping wave%02d:\n", ii);
	for (a= 0; a<ST_SIZ; a++)
	   printf("%d %g\n", a, arr[a] * 1.0 / ST_AMP);
     }
     return;
  }

  // Must be block or tone-set, then, so put into a NameDef
  nd= (NameDef*)Alloc(sizeof(NameDef));
  nd->name= StrDup(p);

  // Block definition ?
  if (*lin == '{') {
    BlockDef *bd, **prvp;
    if (!(p= getWord()) || 
	0 != strcmp(p, "{") || 
	0 != (p= getWord()))
      badSeq();

    prvp= &nd->blk;
    
    while (readLine()) {
      if (*lin == '}') {
	if (!(p= getWord()) || 
	    0 != strcmp(p, "}") || 
	    0 != (p= getWord()))
	  badSeq();
	if (!nd->blk) error("Empty blocks not permitted, line %d:\n  %s", in_lin, lin_copy);
	nd->nxt= nlist; nlist= nd;
	return;
      }
      
      if (*lin != '+') 
	error("All lines in the block must have relative time, line %d:\n  %s",
	      in_lin, lin_copy);
      
      bd= (BlockDef*) Alloc(sizeof(*bd));
      *prvp= bd; prvp= &bd->nxt;
      bd->lin= StrDup(lin);
    }
    
    // Hit EOF before }
    error("End-of-file within block definition (missing '}')");
  }

  // Normal line-definition
  for (ch= 0; ch < N_CH && (p= getWord()); ch++) {
    int sets_mix_flag= 0;

    // Interpret word into Voice nd->vv[ch]
    if (0 == strcmp(p, "-")) continue;
    if (parse_legacy_voice_token(p, &nd->vv[ch], &sets_mix_flag)) {
       if (sets_mix_flag) mix_flag= 1;
       continue;
    }
    badSeq();
  }
  
  // Normalize total amplitude before adding to the list
  normalizeAmplitude(nd->vv, N_CH, lin_copy, in_lin);
  
  nd->nxt= nlist; nlist= nd;
}  

//
//	Bad time
//

void 
badTime(char *tim) {
  error("Badly constructed time \"%s\", line %d:\n  %s", tim, in_lin, lin_copy);
}

//
//	Read a time-line of either type
//

void 
readTimeLine() {
  char *p, *tim_p;
  int nn;
  int fo, fi;
  Period *pp;
  NameDef *nd;
  static int last_abs_time= -1;
  int tim, rtim = 0;

  if (!(p= getWord())) badSeq();
  tim_p= p;
  
  // Read the time represented
  tim= -1;
  if (0 == memcmp(p, "NOW", 3)) {
    last_abs_time= tim= now;
    p += 3;
  }

  while (*p) {
    if (*p == '+') {
      if (tim < 0) {
	if (last_abs_time < 0) 
	  error("Relative time without previous absolute time, line %d:\n  %s", in_lin, lin_copy);
	tim= last_abs_time;
      }
      p++;
    }
    else if (tim != -1) badTime(tim_p);

    if (0 == (nn= readTime(p, &rtim))) badTime(tim_p);
    p += nn;

    if (tim == -1) 
      last_abs_time= tim= rtim;
    else 
      tim= (tim + rtim) % H24;
  }

  if (fast_tim0 < 0) fast_tim0= tim;		// First time
  fast_tim1= tim;				// Last time
      
  if (!(p= getWord())) badSeq();
      
  fi= fo= 1;
  if (!isalpha(*p)) {
    switch (p[0]) {
     case '<': fi= 0; break;
     case '-': fi= 1; break;
     case '=': fi= 2; break;
     default: badSeq();
    }
    switch (p[1]) {
     case '>': fo= 0; break;
     case '-': fo= 1; break;
     case '=': fo= 2; break;
     default: badSeq();
    }
    if (p[2]) badSeq();

    if (!(p= getWord())) badSeq();
  }
      
  for (nd= nlist; nd && 0 != strcmp(p, nd->name); nd= nd->nxt) ;
  if (!nd) error("Name \"%s\" not defined, line %d:\n  %s", p, in_lin, lin_copy);

  // Check for block name-def
  if (nd->blk) {
    char *prep= StrDup(tim_p);		// Put this at the start of each line
    BlockDef *bd= nd->blk;

    while (bd) {
      lin= buf; lin_copy= buf_copy;
      sprintf(lin, "%s%s", prep, bd->lin);
      strcpy(lin_copy, lin);
      readTimeLine();		// This may recurse, and that's why we're StrDuping the string
      bd= bd->nxt;
    }
    free(prep);
    return;
  }
      
  // Normal name-def
  pp= (Period*)Alloc(sizeof(*pp));
  pp->tim= tim;
  pp->fi= fi;
  pp->fo= fo;
      
  memcpy(pp->v0, nd->vv, N_CH * sizeof(Voice));
  memcpy(pp->v1, nd->vv, N_CH * sizeof(Voice));

  if (!per)
    per= pp->nxt= pp->prv= pp;
  else {
    pp->nxt= per; pp->prv= per->prv;
    pp->prv->nxt= pp->nxt->prv= pp;
  }

  // Automatically add a transitional period
  pp= (Period*)Alloc(sizeof(*pp));
  pp->fi= -2;		// Unspecified transition
  pp->nxt= per; pp->prv= per->prv;
  pp->prv->nxt= pp->nxt->prv= pp;

  if (0 != (p= getWord())) {
    if (0 != strcmp(p, "->")) badSeq();
    pp->fi= -3;		// Special '->' transition
    pp->tim= tim;
  }
}

int
readTime(char *p, int *timp) {		// Rets chars consumed, or 0 error
  size_t nn= 0;
  double sec= 0.0;
  if (SBX_OK != sbx_parse_sbg_clock_token(p, &nn, &sec))
    return 0;
  *timp= (int)(sec * 1000.0 + 0.5);
  return (int)nn;
}

//
//	Takes a set of points over one cycle and interpolates them
//	using a periodic sinc kernel (see
//	http://www-ccrma.stanford.edu/~jos/resample/) and writes
//	the resulting unipolar table (0..ST_AMP) to arr[].
//

void sinc_interpolate(double *dp, int np, int *arr) {
   double *sinc;	// Temporary sinc-table
   double *out;		// Temporary output table
   int a, b;
   double dmax, dmin;
   double adj;

   // Generate a modified periodic sin(x)/x function to be used for
   // each of the points.  Really this should be sin(x)/x modified
   // by the sum of an endless series.  However, this doesn't
   // converge very quickly, so to save time I'm approximating this
   // series by 1-4*t*t where t ranges from 0 to 0.5 over the first
   // half of the periodic cycle.  If you do the maths, this is at
   // most 5% out.  This will have to do - it's smooth, and I don't
   // know enough maths to make this series converge quicker.
   sinc= (double *)Alloc(ST_SIZ * sizeof(double));
   sinc[0]= 1.0;
   for (a= ST_SIZ/2; a>0; a--) {
      double tt= a * 1.0 / ST_SIZ;
      double t2= tt*tt;
      double adj= 1 - 4 * t2;
      double xx= 2 * np * 3.14159265358979323846 * tt;
      double vv= adj * sin(xx) / xx;
      sinc[a]= vv;
      sinc[ST_SIZ-a]= vv;
   }
   
   // Build one-cycle waveform into buffer
   out= (double *)Alloc(ST_SIZ * sizeof(double));
   for (b= 0; b<np; b++) {
      int off= b * ST_SIZ / np;
      double val= dp[b];
      for (a= 0; a<ST_SIZ; a++) {
	 out[(a + off)&(ST_SIZ-1)] += sinc[a] * val;
      }
   }

   // Look for maximum for normalization
   dmax= dmin= 0;
   for (a= 0; a<ST_SIZ; a++) {
      if (out[a] > dmax) dmax= out[a];
      if (out[a] < dmin) dmin= out[a];
   }

   // Write out to output buffer as unipolar amplitude (0..ST_AMP)
   adj= ST_AMP / (dmax - dmin);
   for (a= 0; a<ST_SIZ; a++) {
      double vv= (out[a] - dmin) * adj;
      if (vv < 0) vv= 0;
      if (vv > ST_AMP) vv= ST_AMP;
      arr[a]= (int)(vv + 0.5);
   }

   free(sinc);
   free(out);
}

//
//	Handling pre-programmed sequences
//

void 
readPreProg(int ac, char **av) {
   clear_func_curve();
   clear_mix_mod_curve();
   sbx_runtime_clear();
   opt_P_sigmoid= 0;
   opt_P_drop= 0;
   opt_P_curve= 0;

   if (ac < 1) 
      error("Expecting a pre-programmed sequence description.  Examples:" 
	    NL "  drop 25ds+ pink/30"
	    NL "  drop 25gs+/2 mix/60"
	    NL "  sigmoid 00ds+:l=0.125:h=0"
	    NL "  curve examples/basics/curve-sigmoid-like.sbgf 00ds+:l=0.2:h=0"
	    NL "  libseq examples/sbagenxlib/minimal-keyframes.sbxseq"
	    NL "  libsbg examples/sbagenxlib/minimal-sbg-timing.sbg"
	    NL "  libsbg examples/sbagenxlib/minimal-sbg-named.sbg"
	    NL "  libsbg examples/sbagenxlib/minimal-sbg-block.sbg"
	    NL "  libsbg examples/sbagenxlib/minimal-sbg-nested-block.sbg"
	    );

   if (opt_A && !opt_m && !opt_M)
      error("-A requires a mix input stream; use -m <file> or -M");

   if (opt_P && !opt_G && opt_H) {
      write_mixam_cycle_graph_png();
      return;
   }

   if (opt_G &&
       strcmp(av[0], "drop") != 0 &&
       strcmp(av[0], "sigmoid") != 0 &&
       strcmp(av[0], "curve") != 0) {
      error("-G is only supported with -p drop/-p sigmoid/-p curve");
   }
   
   // Handle 'drop'
   if (0 == strcmp(av[0], "drop")) {
      if (opt_G || (opt_P && !opt_H))
	 opt_P_drop= 1;
      ac--; av++;
      create_drop(ac, av);
      return;
   }

   // Handle 'curve'
   if (0 == strcmp(av[0], "curve")) {
      if (opt_G || (opt_P && !opt_H))
	 opt_P_curve= 1;
      ac--; av++;
      create_curve(ac, av);
      return;
   }

   // Handle 'libseq' (sbagenxlib keyframe file: <time> <tone> [interp])
   if (0 == strcmp(av[0], "libseq")) {
      if (opt_G)
	 error("-G is not supported with -p libseq");
      ac--; av++;
      create_libseq(ac, av, 0);
      return;
   }

   // Handle 'libsbg' (sbagenxlib HH:MM[:SS] timing subset)
   if (0 == strcmp(av[0], "libsbg")) {
      if (opt_G)
	 error("-G is not supported with -p libsbg");
      ac--; av++;
      create_libseq(ac, av, 1);
      return;
   }

   // Handle 'slide'
   if (0 == strcmp(av[0], "slide")) {
      if (opt_G)
	 error("-G is not supported with -p slide");
      if (opt_A && !opt_Q)
	 warn("-A mix modulation currently applies only to -p drop/-p sigmoid; ignoring for -p slide");
      ac--; av++;
      create_slide(ac, av);
      return;
   }

   // Handle 'sigmoid'
   if (0 == strcmp(av[0], "sigmoid")) {
      if (opt_G || (opt_P && !opt_H))
	 opt_P_sigmoid= 1;
      ac--; av++;
      create_sigmoid(ac, av);
      return;
   }

   error("Unknown pre-programmed sequence type: %s", av[0]);
}

static int
is_loop_flag_token(const char *s) {
   if (!s) return 0;
   return 0 == strcmp(s, "loop");
}

static void
emit_periods_from_sbx_context_with_extra(SbxContext *ctx, int loop_requested, const char *extra) {
   size_t i, n, voice_n, mix_n, mixfx_n, mixfx_slots;
   const char *extra_tail= (extra && *extra) ? extra : "";

   n= sbx_context_keyframe_count(ctx);
   if (n == 0)
      error("sbagenxlib context does not contain keyframes");
   voice_n= sbx_context_voice_count(ctx);
   if (voice_n == 0)
      voice_n= 1;
   mix_n= sbx_context_mix_amp_keyframe_count(ctx);
   mixfx_n= sbx_context_timed_mix_effect_keyframe_count(ctx);
   mixfx_slots= sbx_context_timed_mix_effect_slot_count(ctx);

   printf("# sbagenxlib keyframes (%d)\n", (int)n);
   if (voice_n > 1)
      printf("# voice-lanes: %d\n", (int)voice_n);
   if (mix_n > 0)
      printf("# mix-keyframes: %d\n", (int)mix_n);
   if (mixfx_n > 0)
      printf("# timed-mixfx-keyframes: %d slots:%d\n", (int)mixfx_n, (int)mixfx_slots);
   if (*extra_tail)
      printf("# extras:%s\n", extra_tail);
   if (loop_requested)
      printf("# loop: enabled\n");
   for (i= 0; i<n; i++) {
      SbxProgramKeyframe kf;
      char spec[256];
      const char *interp;
      size_t vi;

      if (sbx_context_get_keyframe(ctx, i, &kf) != SBX_OK)
	 error("sbagenxlib keyframe retrieval failed at index %d", (int)i);
      if (sbx_format_tone_spec(&kf.tone, spec, sizeof(spec)) != SBX_OK)
	 error("sbagenxlib keyframe %d has unsupported tone mode %d", (int)i, (int)kf.tone.mode);
      interp= (kf.interp == SBX_INTERP_STEP) ? "step" : "linear";
      printf("%.6f %s%s %s\n", kf.time_sec, spec, extra_tail, interp);
      for (vi= 1; vi<voice_n; vi++) {
	 SbxProgramKeyframe vkf;
	 if (sbx_context_get_keyframe_voice(ctx, i, vi, &vkf) != SBX_OK)
	    error("sbagenxlib keyframe voice retrieval failed at index %d lane %d", (int)i, (int)vi);
	 if (sbx_format_tone_spec(&vkf.tone, spec, sizeof(spec)) != SBX_OK)
	    error("sbagenxlib keyframe %d lane %d has unsupported tone mode %d",
		  (int)i, (int)vi, (int)vkf.tone.mode);
	 printf("#   lane[%d] %.6f %s %s\n", (int)vi, vkf.time_sec, spec, interp);
      }
   }
   for (i= 0; i<mix_n; i++) {
      SbxMixAmpKeyframe mkf;
      const char *interp;
      if (sbx_context_get_mix_amp_keyframe(ctx, i, &mkf) != SBX_OK)
	 error("sbagenxlib mix keyframe retrieval failed at index %d", (int)i);
      interp= (mkf.interp == SBX_INTERP_STEP) ? "step" : "linear";
      printf("#   mix[%d] %.6f mix/%.6f %s\n", (int)i, mkf.time_sec, mkf.amp_pct, interp);
   }
   for (i= 0; i<mixfx_n; i++) {
      SbxTimedMixFxKeyframeInfo info;
      const char *interp;
      size_t slot;
      if (sbx_context_get_timed_mix_effect_keyframe_info(ctx, i, &info) != SBX_OK)
	 error("sbagenxlib timed mix-effect keyframe retrieval failed at index %d", (int)i);
      interp= (info.interp == SBX_INTERP_STEP) ? "step" : "linear";
      for (slot= 0; slot<mixfx_slots; slot++) {
	 SbxMixFxSpec fx;
	 int present= 0;
	 if (sbx_context_get_timed_mix_effect_slot(ctx, i, slot, &fx, &present) != SBX_OK)
	    error("sbagenxlib timed mix-effect keyframe slot retrieval failed at index %d slot %d",
		  (int)i, (int)slot);
	 if (present) {
	    char tok[256];
	    if (sbx_format_mix_fx_spec(&fx, tok, sizeof(tok)) != SBX_OK)
	       error("sbagenxlib timed mix-effect keyframe %d slot %d has unsupported mix effect",
		     (int)i, (int)slot);
	    printf("#   mixfx[%d][%d] %.6f %s %s\n", (int)i, (int)slot, info.time_sec, tok, interp);
	 }
      }
   }
   fflush(stdout);
   exit(0);
}

static void
emit_periods_from_sbx_context(SbxContext *ctx, int loop_requested) {
   emit_periods_from_sbx_context_with_extra(ctx, loop_requested, 0);
}

void
create_libseq(int ac, char **av, int sbg_timing) {
   const char *path;
   int loop_flag= 0;
   int rc;
   SbxEngineConfig cfg;
   SbxContext *ctx;

   if (ac < 1)
      error("Bad arguments: expecting -p %s <file> [loop]",
	    sbg_timing ? "libsbg" : "libseq");

   path= av[0];
   ac--; av++;
   if (ac > 1)
      error("Bad arguments: expecting -p %s <file> [loop]",
	    sbg_timing ? "libsbg" : "libseq");
   if (ac == 1) {
      if (!is_loop_flag_token(av[0]))
	 error("Unknown trailing token for -p %s: %s (expected 'loop')",
	       sbg_timing ? "libsbg" : "libseq", av[0]);
      loop_flag= 1;
   }

   sbx_default_engine_config(&cfg);
   cfg.sample_rate= (double)out_rate;
   cfg.channels= 2;
   ctx= sbx_context_create(&cfg);
   if (!ctx)
      error("Failed to create sbagenxlib context");
   if (sbx_context_set_default_waveform(ctx, opt_w) != SBX_OK)
      error("Failed to set sbagenxlib default waveform");

   if (sbg_timing)
      rc= sbx_context_load_sbg_timing_file(ctx, path, loop_flag);
   else
      rc= sbx_context_load_sequence_file(ctx, path, loop_flag);
   if (rc != SBX_OK)
      error("sbagenxlib failed loading %s: %s", path, sbx_context_last_error(ctx));

   if (opt_D) {
      emit_periods_from_sbx_context(ctx, loop_flag);
      sbx_context_destroy(ctx);
      return;
   }

   if (sbx_context_has_mix_amp_control(ctx) && !mix_in && !opt_m && !opt_M)
      warn("mix/<amp> was specified in %s but no mix input stream is active",
           sbg_timing ? "libsbg input" : "libseq input");
   if (sbx_context_has_mix_effects(ctx) && !mix_in && !opt_m && !opt_M)
      error("%s mix effects require a mix input stream (-m / -M)",
            sbg_timing ? "libsbg input" : "libseq input");

   // Runtime playback via sbagenxlib context.
   sbx_runtime_clear();
   sbx_runtime_ctx= ctx;
   sbx_runtime_active= 1;
   sbx_runtime_total_sec= sbx_context_duration_sec(ctx);
}

static void
   sbx_runtime_clear(void) {
   sbx_runtime_active= 0;
   sbx_runtime_total_sec= 0.0;
   if (sbx_runtime_ctx) {
      sbx_context_destroy(sbx_runtime_ctx);
      sbx_runtime_ctx= 0;
   }
   if (sbx_runtime_fbuf) {
      free(sbx_runtime_fbuf);
      sbx_runtime_fbuf= 0;
      sbx_runtime_fcap= 0;
   }
}

static void
sbx_runtime_activate_immediate_tones(const SbxToneSpec *tones, size_t n, double mix_amp_pct,
				     const SbxMixFxSpec *mix_fx, size_t mix_fx_count) {
   SbxEngineConfig cfg;
   int rc;

   if (!tones || n == 0)
      error("internal error: empty immediate tone list for sbagenxlib runtime");

   sbx_runtime_clear();

   sbx_default_engine_config(&cfg);
   cfg.sample_rate= (double)out_rate;
   cfg.channels= 2;
   sbx_runtime_ctx= sbx_context_create(&cfg);
   if (!sbx_runtime_ctx)
      error("Failed to create sbagenxlib runtime context");

   rc= sbx_context_set_tone(sbx_runtime_ctx, &tones[0]);
   if (rc != SBX_OK)
      error("Failed to load sbagenxlib immediate tone: %s",
	    sbx_context_last_error(sbx_runtime_ctx));

   sbx_runtime_active= 1;
   sbx_runtime_total_sec= 0.0;
   rc= sbx_context_configure_runtime(sbx_runtime_ctx,
				     0, 0, mix_amp_pct,
				     mix_fx, mix_fx_count,
				     (n > 1) ? (tones + 1) : 0,
				     (n > 1) ? (n - 1) : 0);
   if (rc != SBX_OK)
      error("Failed to configure sbagenxlib runtime extras: %s",
	    sbx_context_last_error(sbx_runtime_ctx));
}

int
sbx_try_readSeqImm_runtime(int ac, char **av) {
   SbxToneSpec tones[SBX_MAX_AUX_TONES + 1];
   SbxMixFxSpec mix_fx[SBX_MAX_AUX_TONES];
   size_t tone_count= 0;
   size_t mix_fx_count= 0;
   int have_mix= 0;
   double mix_amp_pct= 100.0;
   int i;

   if (ac <= 0 || !av)
      return 0;

   for (i= 0; i<ac; i++) {
      const char *tok= av[i];
      int extra_type= SBX_EXTRA_INVALID;
      SbxToneSpec tone_tmp;
      SbxMixFxSpec mix_fx_tmp;
      double mix_pct= mix_amp_pct;
      if (SBX_OK != sbx_parse_extra_token(tok, opt_w, &extra_type, &tone_tmp, &mix_fx_tmp, &mix_pct))
	 return 0;
      if (extra_type == SBX_EXTRA_MIXAMP) {
	 have_mix= 1;
	 mix_amp_pct= mix_pct;
	 continue;
      }
      if (extra_type == SBX_EXTRA_MIXFX) {
	 apply_mixam_envelope_override(&mix_fx_tmp);
	 if (mix_fx_count >= SBX_MAX_AUX_TONES)
	    return 0;
	 mix_fx[mix_fx_count++]= mix_fx_tmp;
	 continue;
      }
      if (extra_type == SBX_EXTRA_TONE) {
	 if (tone_count >= SBX_MAX_AUX_TONES + 1)
	    return 0;
	 tones[tone_count++]= tone_tmp;
	 continue;
      }
      return 0;
   }

   if (tone_count == 0)
      return 0;

   if (have_mix && !mix_in && !opt_m && !opt_M)
      warn("mix/<amp> was specified in -i tones but no mix input stream is active");
   if (mix_fx_count > 0 && !mix_in && !opt_m && !opt_M)
      error("-i mix effects require a mix input stream (-m / -M)");
   if (mix_fx_count > 0 && !have_mix)
      error("-i mix effects require mix/<amp> in tones");

   if (opt_D) {
      printf("# sbagenxlib immediate tones (%d)\n", (int)tone_count);
      if (have_mix)
	 printf("# mix/%g\n", mix_amp_pct);
      for (i= 0; i<(int)tone_count; i++) {
	 char spec[256];
	 if (sbx_format_tone_spec(&tones[i], spec, sizeof(spec)) != SBX_OK)
	    error("Failed formatting sbagenxlib immediate tone %d", i);
	 printf("%s\n", spec);
      }
      for (i= 0; i<(int)mix_fx_count; i++) {
	 char spec[256];
	 if (sbx_format_mix_fx_spec(&mix_fx[i], spec, sizeof(spec)) == SBX_OK)
	    printf("%s\n", spec);
      }
      fflush(stdout);
      exit(0);
   }

   sbx_runtime_activate_immediate_tones(tones, tone_count, have_mix ? mix_amp_pct : 100.0,
					mix_fx, mix_fx_count);
   return 1;
}

static void
sbx_runtime_activate_from_keyframes(const SbxProgramKeyframe *kfs, size_t n, int loop_flag,
				    double mix_amp_pct,
				    const SbxToneSpec *aux_tones, size_t aux_count,
				    const SbxMixFxSpec *mix_fx, size_t mix_fx_count,
				    const SbxMixAmpKeyframe *mkf, size_t mkf_n) {
   SbxEngineConfig cfg;
   int rc;
   if (!kfs || n == 0)
      error("internal error: empty sbagenxlib runtime keyframe list");

   sbx_runtime_clear();

   sbx_default_engine_config(&cfg);
   cfg.sample_rate= (double)out_rate;
   cfg.channels= 2;
   sbx_runtime_ctx= sbx_context_create(&cfg);
   if (!sbx_runtime_ctx)
      error("Failed to create sbagenxlib runtime context");

   rc= sbx_context_load_keyframes(sbx_runtime_ctx, kfs, n, loop_flag);
   if (rc != SBX_OK)
      error("Failed to load sbagenxlib runtime keyframes: %s",
	    sbx_context_last_error(sbx_runtime_ctx));

   sbx_runtime_active= 1;
   sbx_runtime_total_sec= kfs[n-1].time_sec;
   rc= sbx_context_configure_runtime(sbx_runtime_ctx,
				     mkf, mkf_n, mix_amp_pct,
				     mix_fx, mix_fx_count,
				     aux_tones, aux_count);
   if (rc != SBX_OK)
      error("Failed to configure sbagenxlib runtime extras: %s",
	    sbx_context_last_error(sbx_runtime_ctx));
}

static void
sbx_emit_periods_from_keyframes_with_extra(const SbxProgramKeyframe *kfs, size_t n, int loop_flag,
					   const char *extra) {
   SbxEngineConfig cfg;
   SbxContext *ctx;
   int rc;
   sbx_default_engine_config(&cfg);
   cfg.sample_rate= (double)out_rate;
   cfg.channels= 2;
   ctx= sbx_context_create(&cfg);
   if (!ctx)
      error("Failed to create sbagenxlib context");
   rc= sbx_context_load_keyframes(ctx, kfs, n, loop_flag);
   if (rc != SBX_OK)
      error("Failed to load sbagenxlib keyframes: %s", sbx_context_last_error(ctx));
   emit_periods_from_sbx_context_with_extra(ctx, loop_flag, extra);
   sbx_context_destroy(ctx);
}

static int
sbx_parse_runtime_extra_tokens(const char *extra, SbxRuntimeExtraSpec *spec) {
   const char *p= extra;
   if (!spec) return 0;
   memset(spec, 0, sizeof(*spec));
   spec->mix_amp_pct= 100.0;
   while (p && *p) {
      const char *s= p;
      const char *e;
      int n;
      char tok[256];
      while (*s && isspace((unsigned char)*s)) s++;
      if (!*s) break;
      e= s;
      while (*e && !isspace((unsigned char)*e)) e++;
      n= (int)(e-s);
      if (n >= (int)sizeof(tok)) n= sizeof(tok)-1;
      memcpy(tok, s, n);
      tok[n]= 0;
      {
	 int extra_type= SBX_EXTRA_INVALID;
	 SbxMixFxSpec mix_fx;
	 SbxToneSpec tone;
	 double mix_pct= spec->mix_amp_pct;
	 if (SBX_OK == sbx_parse_extra_token(tok, opt_w, &extra_type, &tone, &mix_fx, &mix_pct)) {
	    if (extra_type == SBX_EXTRA_MIXAMP) {
	       spec->have_mix= 1;
	       spec->mix_amp_pct= mix_pct;
	    } else if (extra_type == SBX_EXTRA_MIXFX) {
	       apply_mixam_envelope_override(&mix_fx);
	       if (spec->mix_fx_count >= SBX_MAX_AUX_TONES)
		  error("Too many extra sbagenxlib mix effect specs (max %d)", SBX_MAX_AUX_TONES);
	       spec->mix_fx[spec->mix_fx_count++]= mix_fx;
	    } else if (extra_type == SBX_EXTRA_TONE) {
	       if (spec->aux_count >= SBX_MAX_AUX_TONES)
		  error("Too many extra sbagenxlib tone-specs (max %d)", SBX_MAX_AUX_TONES);
	       spec->aux_tones[spec->aux_count++]= tone;
	    }
	 } else {
	    spec->unsupported= 1;
	    if (!spec->bad_token[0]) {
	       strncpy(spec->bad_token, tok, sizeof(spec->bad_token)-1);
	       spec->bad_token[sizeof(spec->bad_token)-1]= 0;
	    }
	 }
      }
      p= e;
   }
   return 1;
}

static void
sbx_validate_runtime_extra_mix_fx(const char *prog_name, const SbxRuntimeExtraSpec *spec) {
   if (!spec || spec->mix_fx_count == 0)
      return;
   if (!mix_in && !opt_m && !opt_M)
      error("%s mix effects require a mix input stream (-m / -M)", prog_name ? prog_name : "sbagenx");
   if (!spec->have_mix)
      error("%s mix effects require mix/<amp> in extra tone-specs", prog_name ? prog_name : "sbagenx");
}

static int
sbx_runtime_extra_has_mixam(const SbxRuntimeExtraSpec *spec) {
   size_t i;
   if (!spec) return 0;
   for (i= 0; i<spec->mix_fx_count; i++) {
      if (spec->mix_fx[i].type == SBX_MIXFX_AM)
	 return 1;
   }
   return 0;
}

static void
sbx_handle_runtime_unsupported_extra(const char *prog_name, const SbxRuntimeExtraSpec *spec) {
   if (!spec || !spec->unsupported)
      return;
   if (!opt_D)
      error("Unsupported extra tone-spec '%s' for %s sbagenxlib runtime"
	    NL "Supported extras: mix/<amp>, mixspin/mixpulse/mixbeat/mixam, sbagenxlib tones (+/-/@/M, single tones), bell, spin/bspin/wspin, and white/pink/brown noise",
	    spec->bad_token, prog_name ? prog_name : "sbagenx");
   warn("Unsupported extra tone-spec '%s' is preserved textually in -D output only (not renderable by sbagenxlib runtime)",
	spec->bad_token);
}

typedef struct {
   SbxProgramKeyframe *v;
   size_t n;
   size_t cap;
} SbxKfBuilder;

typedef struct {
   SbxMixAmpKeyframe *v;
   size_t n;
   size_t cap;
} SbxMixKfBuilder;

static void
sbx_kfb_add(SbxKfBuilder *b, double t_sec, const SbxToneSpec *tone, int interp) {
   if (b->n == b->cap) {
      size_t ncap= b->cap ? b->cap * 2 : 64;
      SbxProgramKeyframe *tmp= (SbxProgramKeyframe*)realloc(b->v, ncap * sizeof(*tmp));
      if (!tmp) error("Out of memory");
      b->v= tmp;
      b->cap= ncap;
   }
   b->v[b->n].time_sec= t_sec;
   b->v[b->n].tone= *tone;
   b->v[b->n].interp= interp;
   b->n++;
}

static void
sbx_mixkfb_add(SbxMixKfBuilder *b, double t_sec, double amp_pct, int interp) {
   if (b->n == b->cap) {
      size_t ncap= b->cap ? b->cap * 2 : 64;
      SbxMixAmpKeyframe *tmp= (SbxMixAmpKeyframe*)realloc(b->v, ncap * sizeof(*tmp));
      if (!tmp) error("Out of memory");
      b->v= tmp;
      b->cap= ncap;
   }
   b->v[b->n].time_sec= t_sec;
   b->v[b->n].amp_pct= amp_pct;
   b->v[b->n].interp= interp;
   b->n++;
}

static void
sbx_fill_tone_spec(SbxToneSpec *tone, int isisochronic, int ismono,
		   double carr_hz, double beat_hz, double amp_pct) {
   sbx_default_tone_spec(tone);
   tone->mode= isisochronic ? SBX_TONE_ISOCHRONIC : (ismono ? SBX_TONE_MONAURAL : SBX_TONE_BINAURAL);
   tone->carrier_hz= carr_hz;
   tone->beat_hz= beat_hz;
   tone->amplitude= amp_pct / 100.0;
   tone->waveform= opt_w;
   tone->duty_cycle= opt_I ? opt_I_d : 0.4;
}

//
//	Error for bad p-drop args
//

void 
bad_drop() {
   error("Bad arguments: expecting -p drop [<time-spec>] <drop-spec> [<tone-specs...>]"
	 NL "<drop-spec> is <signed-level|N><a-l>[s|k][+][^][@|M][/<amp>][:S=<val>]"
	 NL "<signed-level> is <digit><digit>[.<digit>...] or"
	 NL "  -<digit><digit>[.<digit>...] (e.g. 00, 34.5, -01)"
	 NL "  N mutes generated program tones (useful with mixam:beat)"
	 NL "The optional <time-spec> is t<drop-time>,<hold-time>,<wake-time>, all times"
	 NL "  in minutes (the default is equivalent to 't30,30,3')."
	 NL "The optional :S=<val> sets start beat/pulse frequency in Hz (default 10)."
	 NL "'@' selects isochronic pulse mode.  'M' selects monaural mode."
	 NL "Use -A[spec] to enable mix modulation; spec is d=<v>:e=<v>:k=<v>:E=<v>."
	 NL "The optional <tone-specs...> let you mix other stuff with the drop"
	 NL "  sequence like pink noise or a mix soundtrack, e.g 'pink/20' or 'mix/60'");
}

//
//	Generate a p-drop sequence
//
//	Credits: Jonathan Bisson created the first version of this C
//	code.  This is a rewrite to make it fit with the rest of the
//	code better.
//

void 
create_drop(int ac, char **av) {
   char *fmt;
   char *p, *q;
   int a;
   int mute_prog_tone= 0;
   int slide, n_step, islong, wakeup, isisochronic, ismono;
   int have_step_mode;
   double carr, amp, c0, c2;
   double beat_target, beat_start= 10.0;
   double beat[40];
   static double beat_targets[]= { 
      4.4, 3.7, 3.1, 2.5, 2.0, 1.5, 1.2, 0.9, 0.7, 0.5, 0.4, 0.3
   };
   char extra[256];
   SbxRuntimeExtraSpec extra_spec;
   int len, len0= 1800, len1= 1800, len2= 180;
   int steplen;
   
#define BAD bad_drop()

   // Pick up optional time-spec
   if (ac < 1) BAD;
   if (av[0][0] == 't') {
      double v0, v1, v2;
      char dmy;
      if (3 != sscanf(av[0]+1, "%lf,%lf,%lf %c", &v0, &v1, &v2, &dmy)) BAD;
      len0= 60 * (int)v0;	// Whole minutes only
      len1= 60 * (int)v1;
      len2= 60 * (int)v2;
      ac--; av++;
   }

   // Handle argument list
   if (ac < 1) BAD;
   fmt= *av++; ac--;
   p= extra; *p= 0;
   while (ac > 0) {
      if (p + strlen(av[0]) + 2 > extra + sizeof(extra))
	 error("Too many extra tone-specs after -p drop");
      p += sprintf(p, " %s", av[0]);
      ac--; av++;
   }

   // Scan the format
   if (fmt[0] == 'N' || fmt[0] == 'n') {
      mute_prog_tone= 1;
      carr= 200.0;
      p= fmt + 1;
   } else {
      carr= 200 - 2 * strtod(fmt, &p);
      if (p == fmt || carr < 0) BAD;
   }

   a= tolower(*p) - 'a'; p++;
   if (a < 0 || a >= sizeof(beat_targets) / sizeof(beat_targets[0])) BAD;
   beat_target= beat_targets[a];

   slide= 0;
   steplen= 180;
   islong= 0;
   wakeup= 0;
   isisochronic= 0;
   ismono= 0;
   have_step_mode= 0;
   amp= 1.0;

   // Parse optional flags in any order
   while (1) {
      if (*p == 's' || *p == 'k') {
	 if (have_step_mode) BAD;
	 have_step_mode= 1;
	 if (*p == 's') { slide= 1; steplen= 60; }
	 else steplen= 60;
	 p++;
	 continue;
      }
      if (*p == '+') { islong= 1; p++; continue; }
      if (*p == '^') { wakeup= 1; p++; continue; }
      if (*p == '@') { isisochronic= 1; p++; continue; }
      if (*p == 'M') { ismono= 1; p++; continue; }
      if (*p == '/') {
	 p++; q= p;
	 amp= strtod(p, &p);
	 if (p == q) BAD;
	 continue;
      }
      if (*p == ':') {
	 p++;
	 if (p[0] == 'S' && p[1] == '=') {
	    p += 2; q= p;
	    beat_start= strtod(p, &p);
	    if (p == q) BAD;
	    continue;
	 }
	 BAD;
      }
      break;
   }

   while (isspace(*p)) p++;
   if (*p) error("Trailing rubbish after -p drop spec: \"%s\"", p);

   if (ismono && isisochronic)
      error("M monaural mode cannot be combined with '@' isochronic drop specs");
   if (!(beat_start > 0.0) || !isfinite(beat_start))
      error("Invalid drop start beat/pulse frequency :S=%g (must be > 0)", beat_start);
   if (mute_prog_tone)
      amp= 0.0;

#undef BAD
      
   n_step= 1 + (len0-1) / steplen;	// Round up
   if (n_step < 2) n_step= 2;
   len0= n_step * steplen;
   if (!slide) len1= (1 + (len1-1) / steplen) * steplen;

   // Sort out carriers
   len= islong ? len0 + len1 : len0;
   c0= carr + 5.0;
   // Keep the same linear carrier drop rate in hold as in drop.
   // Drop phase always drops 5Hz over len0 seconds.
   c2= islong ? (carr - (5.0 * len1 / len0)) : carr;

   if (opt_A)
      setup_mix_mod_curve(opt_A_d, opt_A_e, opt_A_k, opt_A_E, len/60.0, len2/60.0, wakeup);

   if (!sbx_parse_runtime_extra_tokens(extra, &extra_spec))
      error("Internal error parsing -p drop extra tone-specs");

   if (opt_P && !opt_G && !opt_H && sbx_runtime_extra_has_mixam(&extra_spec)) {
      write_mixam_cycle_graph_png();
      return;
   }

   if (opt_G || opt_P_drop) {
      opt_P_drop= 1;
      write_drop_graph_png(fmt, (200.0 - carr) / 2.0, (char)('a' + a),
			   len0, len1, len2,
			   beat_start, beat_target,
			   slide, n_step, steplen,
			   isisochronic, ismono);
      return;
   }

   // Calculate beats
   for (a= 0; a<n_step; a++)
      beat[a]= beat_start * exp(log(beat_target/beat_start) * a / (n_step-1));

   {
      SbxKfBuilder kfb= {0};
      SbxToneSpec tone;
      int end_sec;

      if (extra_spec.have_mix && !mix_in && !opt_m && !opt_M)
	 warn("mix/<amp> was specified in -p drop extras but no mix input stream is active");
      sbx_validate_runtime_extra_mix_fx("-p drop", &extra_spec);

      // Display summary
      warn("DROP summary:");
      if (slide) {
	 warn(" Carrier slides from %gHz to %gHz over %d minutes",
	      c0, c2, len/60);
	 if (ismono)
	    warn(" Monaural beat frequency slides from %gHz to %gHz over %d minutes",
		 beat[0], beat[n_step-1], len0/60);
	 else
	    warn(" %s frequency slides from %gHz to %gHz over %d minutes",
		 isisochronic ? "Pulse" : "Beat", beat[0], beat[n_step-1], len0/60);
      } else {
	 warn(" Carrier steps from %gHz to %gHz over %d minutes",
	      c0, c2, len/60);
	 if (ismono)
	    warn(" Monaural beat frequency steps from %gHz to %gHz over %d minutes:",
		 beat[0], beat[n_step-1], len0/60);
	 else
	    warn(" %s frequency steps from %gHz to %gHz over %d minutes:",
		 isisochronic ? "Pulse" : "Beat", beat[0], beat[n_step-1], len0/60);
	 fprintf(stderr, "   ");
	 for (a= 0; a<n_step; a++) fprintf(stderr, " %.2f", beat[a]);
	 fprintf(stderr, "\n");
      }
      if (ismono)
	 warn(" Monaural tone pair per step: f1=carr-beat/2, f2=carr+beat/2");
      if (wakeup) {
	 warn(" Final wake-up of %d minutes, to return to initial frequencies", len2/60);
      }
      if (mute_prog_tone)
	 warn(" N mode active: generated program tones are muted; beat/pulse trajectory is still generated");
      if (opt_A) {
	 warn(" Mix modulation enabled: -A d=%g:e=%g:k=%g:E=%g (T=%g min%s)",
	      opt_A_d, opt_A_e, opt_A_k, opt_A_E, len/60.0, wakeup ? ", wake ramp active" : "");
	 warn(" Note: status line shows runtime-effective mix/<amp> when -A is active.");
      }

      if (slide) {
	 for (a= 0; a<n_step; a++) {
	    double tim= a * len0 / (double)(n_step-1);
	    double carr_t= c0 + (c2-c0) * tim / len;
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_t, beat[a], amp);
	    sbx_kfb_add(&kfb, tim, &tone, SBX_INTERP_LINEAR);
	 }
	 if (islong) {
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, c2, beat[n_step-1], amp);
	    sbx_kfb_add(&kfb, (double)len, &tone, SBX_INTERP_LINEAR);
	 }
	 end_sec= len;
      } else {
	 int lim= len / steplen;
	 for (a= 0; a<lim; a++) {
	    int tim0= a * steplen;
	    int tim1= (a+1) * steplen;
	    double carr_t= c0 + (c2-c0) * tim1/len;
	    double beat_t= beat[(a>=n_step) ? n_step-1 : a];
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_t, beat_t, amp);
	    sbx_kfb_add(&kfb, (double)tim0, &tone, SBX_INTERP_STEP);
	 }
	 sbx_fill_tone_spec(&tone, isisochronic, ismono, c2, beat[n_step-1], amp);
	 sbx_kfb_add(&kfb, (double)len, &tone, SBX_INTERP_STEP);
	 end_sec= len;
      }

      if (wakeup) {
	 sbx_fill_tone_spec(&tone, isisochronic, ismono, c0, beat[0], amp);
	 sbx_kfb_add(&kfb, (double)(end_sec + len2), &tone, SBX_INTERP_LINEAR);
	 end_sec += len2;
      }

      sbx_fill_tone_spec(&tone, isisochronic, ismono, wakeup ? c0 : c2, wakeup ? beat[0] : beat[n_step-1], 0.0);
      sbx_kfb_add(&kfb, (double)(end_sec + 10), &tone, SBX_INTERP_LINEAR);

      sbx_handle_runtime_unsupported_extra("-p drop", &extra_spec);

      if (opt_D) {
	 sbx_emit_periods_from_keyframes_with_extra(kfb.v, kfb.n, 0, extra);
      } else {
	 sbx_runtime_activate_from_keyframes(kfb.v, kfb.n, 0,
					     extra_spec.have_mix ? extra_spec.mix_amp_pct : 100.0,
					     extra_spec.aux_tones, extra_spec.aux_count,
					     extra_spec.mix_fx, extra_spec.mix_fx_count,
					     0, 0);
      }

      if (kfb.v) free(kfb.v);
   }
}

//
//	Error for bad p-sigmoid args
//

void
bad_sigmoid() {
   error("Bad arguments: expecting -p sigmoid [<time-spec>] <sigmoid-spec> [<tone-specs...>]"
	 NL "<sigmoid-spec> is <signed-level|N><a-l>[s|k][+][^][@|M][/<amp>][:l=<val>][:h=<val>][:S=<val>]"
	 NL "<signed-level> is <digit><digit>[.<digit>...] or"
	 NL "  -<digit><digit>[.<digit>...] (e.g. 00, 34.5, -01)"
	 NL "  N mutes generated program tones (useful with mixam:beat)"
	 NL "The optional <time-spec> is t<drop-time>,<hold-time>,<wake-time>, all times"
	 NL "  in minutes (the default is equivalent to 't30,30,3')."
	 NL "The optional shape parameters are l and h (defaults: l=0.125, h=0),"
	 NL "  and :S=<val> sets start beat/pulse frequency in Hz (default 10)."
	 NL "'@' selects isochronic pulse mode.  'M' selects monaural mode."
	 NL "Use -A[spec] to enable mix modulation; spec is d=<v>:e=<v>:k=<v>:E=<v>."
	 NL "The optional <tone-specs...> let you mix other stuff with the sequence"
	 NL "  like pink noise or a mix soundtrack, e.g 'pink/20' or 'mix/60'");
}

//
//	Generate a p-sigmoid sequence
//

void
create_sigmoid(int ac, char **av) {
   char *fmt;
   char *p, *q;
   char depth_ch;
   int a;
   int mute_prog_tone= 0;
   int slide, n_step, islong, wakeup, isisochronic, ismono;
   int have_step_mode;
   double level;
   double carr, amp, c0, c2;
   double beat_target, beat_start= 10.0;
   double beat[40];
   static double beat_targets[]= {
      4.4, 3.7, 3.1, 2.5, 2.0, 1.5, 1.2, 0.9, 0.7, 0.5, 0.4, 0.3
   };
   char extra[256];
   SbxRuntimeExtraSpec extra_spec;
   int len, len0= 1800, len1= 1800, len2= 180;
   int steplen;
   double sig_l= 0.125, sig_h= 0.0;
   double d_min, u0, u1, den, sig_a, sig_b;

#define BAD bad_sigmoid()

   // Pick up optional time-spec
   if (ac < 1) BAD;
   if (av[0][0] == 't') {
      double v0, v1, v2;
      char dmy;
      if (3 != sscanf(av[0]+1, "%lf,%lf,%lf %c", &v0, &v1, &v2, &dmy)) BAD;
      len0= 60 * (int)v0;	// Whole minutes only
      len1= 60 * (int)v1;
      len2= 60 * (int)v2;
      ac--; av++;
   }

   // Handle argument list
   if (ac < 1) BAD;
   fmt= *av++; ac--;
   p= extra; *p= 0;
   while (ac > 0) {
      if (p + strlen(av[0]) + 2 > extra + sizeof(extra))
	 error("Too many extra tone-specs after -p sigmoid");
      p += sprintf(p, " %s", av[0]);
      ac--; av++;
   }

   // Scan the format
   if (fmt[0] == 'N' || fmt[0] == 'n') {
      mute_prog_tone= 1;
      level= 0.0;
      carr= 200.0;
      p= fmt + 1;
   } else {
      level= strtod(fmt, &p);
      carr= 200 - 2 * level;
      if (p == fmt || carr < 0) BAD;
   }

   depth_ch= tolower((unsigned char)*p);
   a= depth_ch - 'a'; p++;
   if (a < 0 || a >= sizeof(beat_targets) / sizeof(beat_targets[0])) BAD;
   beat_target= beat_targets[a];

   slide= 0;
   steplen= 180;
   islong= 0;
   wakeup= 0;
   isisochronic= 0;
   ismono= 0;
   have_step_mode= 0;
   amp= 1.0;

   // Parse optional flags in any order, including :l=, :h= and :S=
   while (1) {
      if (*p == 's' || *p == 'k') {
	 if (have_step_mode) BAD;
	 have_step_mode= 1;
	 if (*p == 's') { slide= 1; steplen= 60; }
	 else steplen= 60;
	 p++;
	 continue;
      }
      if (*p == '+') { islong= 1; p++; continue; }
      if (*p == '^') { wakeup= 1; p++; continue; }
      if (*p == '@') { isisochronic= 1; p++; continue; }
      if (*p == 'M') { ismono= 1; p++; continue; }
      if (*p == '/') {
	 p++; q= p;
	 amp= strtod(p, &p);
	 if (p == q) BAD;
	 continue;
      }
      if (*p == ':') {
	 p++;
	 if (p[0] == 'l' && p[1] == '=') {
	    p += 2; q= p;
	    sig_l= strtod(p, &p);
	    if (p == q) BAD;
	    continue;
	 }
	 if (p[0] == 'h' && p[1] == '=') {
	    p += 2; q= p;
	    sig_h= strtod(p, &p);
	    if (p == q) BAD;
	    continue;
	 }
	 if (p[0] == 'S' && p[1] == '=') {
	    p += 2; q= p;
	    beat_start= strtod(p, &p);
	    if (p == q) BAD;
	    continue;
	 }
	 BAD;
      }
      break;
   }

   while (isspace(*p)) p++;
   if (*p) error("Trailing rubbish after -p sigmoid spec: \"%s\"", p);

   if (ismono && isisochronic)
      error("M monaural mode cannot be combined with '@' isochronic sigmoid specs");
   if (!(beat_start > 0.0) || !isfinite(beat_start))
      error("Invalid sigmoid start beat/pulse frequency :S=%g (must be > 0)", beat_start);
   if (mute_prog_tone)
      amp= 0.0;

#undef BAD

   // Tidy timings
   if (len0 < 60)
      error("Sigmoid drop-time must be at least 1 minute");
   n_step= 1 + (len0-1) / steplen;	// Round up
   if (n_step < 2) n_step= 2;
   len0= n_step * steplen;
   if (!slide) len1= (1 + (len1-1) / steplen) * steplen;

   // Sort out carriers and sigmoid coefficients
   len= islong ? len0 + len1 : len0;
   c0= carr + 5.0;
   // Keep the same linear carrier drop rate in hold as in drop.
   // Drop phase always drops 5Hz over len0 seconds.
   c2= islong ? (carr - (5.0 * len1 / len0)) : carr;
   d_min= len0 / 60.0;
   u0= tanh(sig_l * (0.0 - d_min/2 - sig_h));
   u1= tanh(sig_l * (d_min - d_min/2 - sig_h));
   den= u1 - u0;
   if (fabs(den) < 1e-9)
      error("Sigmoid parameters produce an invalid curve (try different l/h values)");
   sig_a= (beat_target - beat_start) / den;
   sig_b= beat_start - sig_a * u0;

   if (opt_A)
      setup_mix_mod_curve(opt_A_d, opt_A_e, opt_A_k, opt_A_E, len/60.0, len2/60.0, wakeup);

   if (!sbx_parse_runtime_extra_tokens(extra, &extra_spec))
      error("Internal error parsing -p sigmoid extra tone-specs");

   if (opt_P && !opt_G && !opt_H && sbx_runtime_extra_has_mixam(&extra_spec)) {
      write_mixam_cycle_graph_png();
      return;
   }

   if (opt_G || opt_P_sigmoid) {
      write_sigmoid_graph_png(fmt, level, depth_ch, len0, len1, len2,
			      beat_start, beat_target, sig_l, sig_h, sig_a, sig_b);
      return;
   }

   // Calculate display/checkpoint beat values
   for (a= 0; a<n_step; a++) {
      double tim= a * len0 / (double)(n_step-1);
      double t_min= tim / 60.0;
      if (t_min >= d_min)
	 beat[a]= beat_target;
      else
	 beat[a]= sig_a * tanh(sig_l * (t_min - d_min/2 - sig_h)) + sig_b;
   }

   {
      SbxKfBuilder kfb= {0};
      SbxToneSpec tone;
      int end_sec;

      if (extra_spec.have_mix && !mix_in && !opt_m && !opt_M)
	 warn("mix/<amp> was specified in -p sigmoid extras but no mix input stream is active");
      sbx_validate_runtime_extra_mix_fx("-p sigmoid", &extra_spec);

      // Display summary
      warn("SIGMOID summary:");
      if (slide) {
	 warn(" Carrier slides from %gHz to %gHz over %d minutes",
	      c0, c2, len/60);
	 if (ismono)
	    warn(" Monaural beat frequency follows sigmoid from %gHz to %gHz over %d minutes",
		 beat_start, beat_target, len0/60);
	 else
	    warn(" %s frequency follows sigmoid from %gHz to %gHz over %d minutes",
		 isisochronic ? "Pulse" : "Beat", beat_start, beat_target, len0/60);
      } else {
	 warn(" Carrier steps from %gHz to %gHz over %d minutes",
	      c0, c2, len/60);
	 if (ismono)
	    warn(" Monaural beat frequency steps over sigmoid from %gHz to %gHz over %d minutes:",
		 beat_start, beat_target, len0/60);
	 else
	    warn(" %s frequency steps over sigmoid from %gHz to %gHz over %d minutes:",
		 isisochronic ? "Pulse" : "Beat", beat_start, beat_target, len0/60);
	 fprintf(stderr, "   ");
	 for (a= 0; a<n_step; a++) fprintf(stderr, " %.2f", beat[a]);
	 fprintf(stderr, "\n");
      }
      if (ismono)
	 warn(" Monaural tone pair per step: f1=carr-beat/2, f2=carr+beat/2");
      warn(" Sigmoid shape parameters: l=%g h=%g", sig_l, sig_h);
      if (wakeup) {
	 warn(" Final wake-up of %d minutes, to return to initial frequencies", len2/60);
      }
      if (mute_prog_tone)
	 warn(" N mode active: generated program tones are muted; beat/pulse trajectory is still generated");
      if (opt_A) {
	 warn(" Mix modulation enabled: -A d=%g:e=%g:k=%g:E=%g (T=%g min%s)",
	      opt_A_d, opt_A_e, opt_A_k, opt_A_E, len/60.0, wakeup ? ", wake ramp active" : "");
	 warn(" Note: status line shows runtime-effective mix/<amp> when -A is active.");
      }

      if (slide) {
	 for (a= 0; a<n_step; a++) {
	    double tim= a * len0 / (double)(n_step-1);
	    double carr_t= c0 + (c2-c0) * tim / len;
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_t, beat[a], amp);
	    sbx_kfb_add(&kfb, tim, &tone, SBX_INTERP_LINEAR);
	 }
	 if (islong) {
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, c2, beat[n_step-1], amp);
	    sbx_kfb_add(&kfb, (double)len, &tone, SBX_INTERP_LINEAR);
	 }
	 end_sec= len;
      } else {
	 int lim= len / steplen;
	 for (a= 0; a<lim; a++) {
	    int tim0= a * steplen;
	    int tim1= (a+1) * steplen;
	    double carr_t= c0 + (c2-c0) * tim1/len;
	    double beat_t= beat[(a>=n_step) ? n_step-1 : a];
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_t, beat_t, amp);
	    sbx_kfb_add(&kfb, (double)tim0, &tone, SBX_INTERP_STEP);
	 }
	 sbx_fill_tone_spec(&tone, isisochronic, ismono, c2, beat[n_step-1], amp);
	 sbx_kfb_add(&kfb, (double)len, &tone, SBX_INTERP_STEP);
	 end_sec= len;
      }

      if (wakeup) {
	 sbx_fill_tone_spec(&tone, isisochronic, ismono, c0, beat[0], amp);
	 sbx_kfb_add(&kfb, (double)(end_sec + len2), &tone, SBX_INTERP_LINEAR);
	 end_sec += len2;
      }

      sbx_fill_tone_spec(&tone, isisochronic, ismono, wakeup ? c0 : c2, wakeup ? beat[0] : beat[n_step-1], 0.0);
      sbx_kfb_add(&kfb, (double)(end_sec + 10), &tone, SBX_INTERP_LINEAR);

      sbx_handle_runtime_unsupported_extra("-p sigmoid", &extra_spec);

      if (opt_D) {
	 sbx_emit_periods_from_keyframes_with_extra(kfb.v, kfb.n, 0, extra);
      } else {
	 sbx_runtime_activate_from_keyframes(kfb.v, kfb.n, 0,
					     extra_spec.have_mix ? extra_spec.mix_amp_pct : 100.0,
					     extra_spec.aux_tones, extra_spec.aux_count,
					     extra_spec.mix_fx, extra_spec.mix_fx_count,
					     0, 0);
      }

      if (kfb.v) free(kfb.v);
   }
}

//
//	Error for bad p-curve args
//

void
bad_curve() {
   error("Bad arguments: expecting -p curve <curve-file.sbgf> [<time-spec>] <curve-spec> [<tone-specs...>]"
	 NL "<curve-file.sbgf> must define beat either as 'beat = <expression>'"
	 NL "  or piecewise lines such as 'beat<p1 = <expression>'"
	 NL "Optional lines in .sbgf: carrier = <expression>, amp = <expression>,"
	 NL "  mixamp = <expression>, and piecewise variants of carrier/amp/mixamp,"
	 NL "  param <name> = <value>[m|s],"
	 NL "  solve <u1>,<u2>,... : <lhs1>=<rhs1> ; <lhs2>=<rhs2> ; ..."
	 NL "<curve-spec> is <signed-level|N><a-l>[s|k][+][^][@|M][/<amp>][:S=<val>][:<name>=<value>...]"
	 NL "<signed-level> is <digit><digit>[.<digit>...] or"
	 NL "  -<digit><digit>[.<digit>...] (e.g. 00, 34.5, -01)"
	 NL "  N mutes generated program tones (useful with mixam:beat)"
	 NL "The optional <time-spec> is t<drop-time>,<hold-time>,<wake-time>, all times"
	 NL "  in minutes (default is equivalent to 't30,30,3')."
	 NL "'@' selects isochronic pulse mode.  'M' selects monaural mode."
	 NL "Use :S=<val> to set start beat/pulse frequency in Hz (default 10)."
	 NL "Use :<name>=<value> in <curve-spec> to override .sbgf parameters."
	 NL "Use -A[spec] to enable mix modulation; spec is d=<v>:e=<v>:k=<v>:E=<v>.");
}

//
//	Generate a p-curve sequence using expressions from a .sbgf file
//

void
create_curve(int ac, char **av) {
   char *curve_file;
   char *fmt;
   char *p, *q;
   char depth_ch;
   int a;
   int mute_prog_tone= 0;
   int slide, n_step, islong, wakeup, isisochronic, ismono;
   int have_step_mode;
   int len, len0= 1800, len1= 1800, len2= 180;
   int steplen;
   int ov_count= 0;
   char ov_name[CURVE_MAX_PARAMS][CURVE_NAME_MAX];
   double ov_val[CURVE_MAX_PARAMS];
   double level;
   double carr, amp, c0, c2;
   double base_mix_amp= 100.0;
   double beat_target, beat_start= 10.0;
   double beat[40], carr_sam[40];
   double amp_sam[40], mix_sam[40];
   double beat_end, carr_end;
   double amp_end= 0.0, mix_end= 0.0;
   double beat_start_eval, carr_start_eval;
   double amp_start_eval= 0.0, mix_start_eval= 0.0;
   int have_mix_in_extra= 0;
   int have_amp_curve= 0, have_mixamp_curve= 0;
   char extra[256];
   SbxRuntimeExtraSpec extra_spec;
   static double beat_targets[]= {
      4.4, 3.7, 3.1, 2.5, 2.0, 1.5, 1.2, 0.9, 0.7, 0.5, 0.4, 0.3
   };

#define BAD bad_curve()

   if (ac < 2) BAD;
   curve_file= *av++;
   ac--;
   if (!curve_has_sbgf_ext(curve_file))
      error("Curve file must use .sbgf extension: %s", curve_file);

   // Pick up optional time-spec
   if (ac < 1) BAD;
   if (av[0][0] == 't') {
      double v0, v1, v2;
      char dmy;
      if (3 != sscanf(av[0]+1, "%lf,%lf,%lf %c", &v0, &v1, &v2, &dmy)) BAD;
      len0= 60 * (int)v0;	// Whole minutes only
      len1= 60 * (int)v1;
      len2= 60 * (int)v2;
      ac--; av++;
   }

   // Handle argument list
   if (ac < 1) BAD;
   fmt= *av++; ac--;
   p= extra; *p= 0;
   while (ac > 0) {
      if (p + strlen(av[0]) + 2 > extra + sizeof(extra))
	 error("Too many extra tone-specs after -p curve");
      p += sprintf(p, " %s", av[0]);
      ac--; av++;
   }
   have_mix_in_extra= curve_find_mix_amp_in_extra(extra, &base_mix_amp);

   // Scan the format
   if (fmt[0] == 'N' || fmt[0] == 'n') {
      mute_prog_tone= 1;
      level= 0.0;
      carr= 200.0;
      p= fmt + 1;
   } else {
      level= strtod(fmt, &p);
      carr= 200 - 2 * level;
      if (p == fmt || carr < 0) BAD;
   }

   depth_ch= tolower((unsigned char)*p);
   a= depth_ch - 'a'; p++;
   if (a < 0 || a >= sizeof(beat_targets) / sizeof(beat_targets[0])) BAD;
   beat_target= beat_targets[a];

   slide= 0;
   steplen= 180;
   islong= 0;
   wakeup= 0;
   isisochronic= 0;
   ismono= 0;
   have_step_mode= 0;
   amp= 1.0;

   while (1) {
      if (*p == 's' || *p == 'k') {
	 if (have_step_mode) BAD;
	 have_step_mode= 1;
	 if (*p == 's') { slide= 1; steplen= 60; }
	 else steplen= 60;
	 p++;
	 continue;
      }
      if (*p == '+') { islong= 1; p++; continue; }
      if (*p == '^') { wakeup= 1; p++; continue; }
      if (*p == '@') { isisochronic= 1; p++; continue; }
      if (*p == 'M') { ismono= 1; p++; continue; }
      if (*p == '/') {
	 p++; q= p;
	 amp= strtod(p, &p);
	 if (p == q) BAD;
	 continue;
      }
      if (*p == ':') {
	 int ni= 0;
	 char name[CURVE_NAME_MAX];
	 p++;
	 if (p[0] == 'S' && p[1] == '=') {
	    p += 2; q= p;
	    beat_start= strtod(p, &p);
	    if (p == q) BAD;
	    continue;
	 }
	 if (!curve_name_char((unsigned char)*p, 1)) BAD;
	 while (*p && curve_name_char((unsigned char)*p, ni == 0)) {
	    if (ni >= CURVE_NAME_MAX-1) BAD;
	    name[ni++]= *p++;
	 }
	 name[ni]= 0;
	 if (*p != '=') BAD;
	 p++;
	 q= p;
	 if (ov_count >= CURVE_MAX_PARAMS)
	    error("Too many curve parameter overrides in -p curve spec");
	 ov_val[ov_count]= strtod(p, &p);
	 if (p == q) BAD;
	 memcpy(ov_name[ov_count], name, (size_t)ni + 1);
	 ov_count++;
	 continue;
      }
      break;
   }

   while (isspace((unsigned char)*p)) p++;
   if (*p) error("Trailing rubbish after -p curve spec: \"%s\"", p);

   if (ismono && isisochronic)
      error("M monaural mode cannot be combined with '@' isochronic curve specs");
   if (!(beat_start > 0.0) || !isfinite(beat_start))
      error("Invalid curve start beat/pulse frequency :S=%g (must be > 0)", beat_start);
   if (mute_prog_tone)
      amp= 0.0;

#undef BAD

   n_step= 1 + (len0-1) / steplen;	// Round up
   if (n_step < 2) n_step= 2;
   len0= n_step * steplen;
   if (!slide) len1= (1 + (len1-1) / steplen) * steplen;

   len= islong ? len0 + len1 : len0;
   c0= carr + 5.0;
   // Keep carrier endpoints aligned with drop/sigmoid conventions.
   c2= islong ? (carr - (5.0 * len1 / len0)) : carr;

   if (opt_A)
      setup_mix_mod_curve(opt_A_d, opt_A_e, opt_A_k, opt_A_E, len/60.0, len2/60.0, wakeup);

   load_curve_file(curve_file);
   for (a= 0; a<ov_count; a++)
      curve_set_param_value(ov_name[a], ov_val[a], 1);

   if (!setup_custom_func_curve(0, isisochronic ? 8 : 1, 0,
				c0, c2, len,
				beat_start, beat_target, len0,
				islong ? len1/60.0 : 0.0,
				len/60.0,
				wakeup ? len2/60.0 : 0.0,
				amp, base_mix_amp))
      error("Unable to setup custom curve from %s", curve_file);
   if (ismono)
      setup_func_curve_monaural_pair(0, 1);

   have_amp_curve= func_curve.have_amp_expr || func_curve.amp_piece_count > 0;
   have_mixamp_curve= func_curve.have_mixamp_expr || func_curve.mixamp_piece_count > 0;

   for (a= 0; a<n_step; a++) {
      double tim= a * len0 / (double)(n_step-1);
      if (!eval_custom_curve_point(tim, &beat[a], &carr_sam[a], &amp_sam[a], &mix_sam[a]))
	 error("Custom curve evaluation failed at t=%g seconds", tim);
   }
   if (!eval_custom_curve_point(len, &beat_end, &carr_end, &amp_end, &mix_end))
      error("Custom curve evaluation failed at end of main session");
   if (!eval_custom_curve_point(0.0, &beat_start_eval, &carr_start_eval, &amp_start_eval, &mix_start_eval))
      error("Custom curve evaluation failed at session start");

   if (!sbx_parse_runtime_extra_tokens(extra, &extra_spec))
      error("Internal error parsing -p curve extra tone-specs");

   if (opt_P && !opt_G && !opt_H && sbx_runtime_extra_has_mixam(&extra_spec)) {
      clear_func_curve();
      write_mixam_cycle_graph_png();
      return;
   }

   if (opt_G || opt_P_curve) {
      opt_P_curve= 1;
      write_curve_graph_png(fmt, level, depth_ch,
			    len0, len1, len2,
			    slide, n_step, steplen,
			    isisochronic, ismono,
			    beat_start_eval, beat_end);
      clear_func_curve();
      return;
   }

   {
      int end_sec;
      SbxKfBuilder kfb= {0};
      SbxMixKfBuilder mkfb= {0};
      SbxToneSpec tone;

      have_mix_in_extra= extra_spec.have_mix;
      if (extra_spec.have_mix && !mix_in && !opt_m && !opt_M)
	 warn("mix/<amp> was specified in -p curve extras but no mix input stream is active");
      sbx_validate_runtime_extra_mix_fx("-p curve", &extra_spec);

      // Display summary
      warn("CURVE summary:");
      warn(" Source file: %s", func_curve.src_file);
      if (slide) {
	 warn(" Carrier follows custom curve from %gHz to %gHz over %d minutes",
	      carr_sam[0], carr_end, len/60);
	 if (ismono)
	    warn(" Monaural beat frequency follows custom curve from %gHz to %gHz over %d minutes",
		 beat[0], beat_end, len0/60);
	 else
	    warn(" %s frequency follows custom curve from %gHz to %gHz over %d minutes",
		 isisochronic ? "Pulse" : "Beat", beat[0], beat_end, len0/60);
	 warn(" Using function-driven curve from .sbgf for sliding mode");
      } else {
	 warn(" Carrier/beat are sampled from custom curve in stepped mode over %d minutes", len/60);
	 if (ismono)
	    warn(" Monaural beat checkpoints from %gHz to %gHz", beat[0], beat_end);
	 else
	    warn(" %s checkpoints from %gHz to %gHz",
		 isisochronic ? "Pulse" : "Beat", beat[0], beat_end);
	 fprintf(stderr, "   ");
	 for (a= 0; a<n_step; a++) fprintf(stderr, " %.2f", beat[a]);
	 fprintf(stderr, "\n");
      }
      if (func_curve.have_carr_expr)
	 warn(" Carrier expression enabled from .sbgf");
      if (have_amp_curve)
	 warn(" Beat amplitude expression enabled from .sbgf");
      if (mute_prog_tone && have_amp_curve)
	 warn(" N mode active: amp expression from .sbgf is ignored because generated program tones are muted");
      if (have_mixamp_curve) {
	 if (have_mix_in_extra)
	    warn(" Mix amplitude expression enabled from .sbgf");
	 else
	    warn(" mixamp expression is defined, but no mix/<amp> tone-spec was provided");
      }
      if (func_curve.param_count > 0) {
	 fprintf(stderr, " Parameters:");
	 for (a= 0; a<func_curve.param_count; a++)
	    fprintf(stderr, " %s=%g", func_curve.param_names[a], func_curve.param_values[a]);
	 fprintf(stderr, "\n");
      }
      if (ismono)
	 warn(" Monaural tone pair per step: f1=carr-beat/2, f2=carr+beat/2");
      if (mute_prog_tone)
	 warn(" N mode active: generated program tones are muted; beat/pulse trajectory is still generated");
      if (wakeup)
	 warn(" Final wake-up of %d minutes, to return to initial frequencies", len2/60);
      if (opt_A) {
	 warn(" Mix modulation enabled: -A d=%g:e=%g:k=%g:E=%g (T=%g min%s)",
	      opt_A_d, opt_A_e, opt_A_k, opt_A_E, len/60.0, wakeup ? ", wake ramp active" : "");
	 warn(" Note: status line shows runtime-effective mix/<amp> when -A is active.");
      }

      if (slide) {
	 for (a= 0; a<n_step; a++) {
	    double amp_t= have_amp_curve ? amp_sam[a] : amp;
	    if (mute_prog_tone) amp_t= 0.0;
	    double tim= a * len0 / (double)(n_step-1);
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_sam[a], beat[a], amp_t);
	    sbx_kfb_add(&kfb, tim, &tone, SBX_INTERP_LINEAR);
	    if (have_mixamp_curve && have_mix_in_extra)
	       sbx_mixkfb_add(&mkfb, tim, mix_sam[a], SBX_INTERP_LINEAR);
	 }
	 if (islong) {
	    double amp_t= have_amp_curve ? amp_end : amp;
	    if (mute_prog_tone) amp_t= 0.0;
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_end, beat_end, amp_t);
	    sbx_kfb_add(&kfb, (double)len, &tone, SBX_INTERP_LINEAR);
	    if (have_mixamp_curve && have_mix_in_extra)
	       sbx_mixkfb_add(&mkfb, (double)len, mix_end, SBX_INTERP_LINEAR);
	 }
	 end_sec= len;
      } else {
	 int lim= len / steplen;
	 for (a= 0; a<lim; a++) {
	    int tim0= a * steplen;
	    int tim1= (a+1) * steplen;
	    double beat_t, carr_t, amp_t, mix_t;
	    if (!eval_custom_curve_point(tim1, &beat_t, &carr_t, &amp_t, &mix_t))
	       error("Custom curve evaluation failed at t=%d seconds", tim1);
	    if (!have_amp_curve) amp_t= amp;
	    if (mute_prog_tone) amp_t= 0.0;
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_t, beat_t, amp_t);
	    sbx_kfb_add(&kfb, (double)tim0, &tone, SBX_INTERP_STEP);
	    if (have_mixamp_curve && have_mix_in_extra)
	       sbx_mixkfb_add(&mkfb, (double)tim0, mix_t, SBX_INTERP_STEP);
	 }
	 {
	    double amp_t= have_amp_curve ? amp_end : amp;
	    if (mute_prog_tone) amp_t= 0.0;
	    sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_end, beat_end, amp_t);
	 }
	 sbx_kfb_add(&kfb, (double)len, &tone, SBX_INTERP_STEP);
	 if (have_mixamp_curve && have_mix_in_extra)
	    sbx_mixkfb_add(&mkfb, (double)len, mix_end, SBX_INTERP_STEP);
	 end_sec= len;
      }

      if (wakeup) {
	 double amp_t= have_amp_curve ? amp_start_eval : amp;
	 if (mute_prog_tone) amp_t= 0.0;
	 sbx_fill_tone_spec(&tone, isisochronic, ismono, carr_start_eval, beat_start_eval, amp_t);
	 sbx_kfb_add(&kfb, (double)(end_sec + len2), &tone, SBX_INTERP_LINEAR);
	 if (have_mixamp_curve && have_mix_in_extra)
	    sbx_mixkfb_add(&mkfb, (double)(end_sec + len2), mix_start_eval, SBX_INTERP_LINEAR);
	 end_sec += len2;
      }

      sbx_fill_tone_spec(&tone, isisochronic, ismono,
			 wakeup ? carr_start_eval : carr_end,
			 wakeup ? beat_start_eval : beat_end,
			 0.0);
      sbx_kfb_add(&kfb, (double)(end_sec + 10), &tone, SBX_INTERP_LINEAR);
      if (have_mixamp_curve && have_mix_in_extra)
	 sbx_mixkfb_add(&mkfb, (double)(end_sec + 10), wakeup ? mix_start_eval : mix_end, SBX_INTERP_LINEAR);

      sbx_handle_runtime_unsupported_extra("-p curve", &extra_spec);
      if (opt_D && have_mixamp_curve && have_mix_in_extra)
	 warn("Curve mixamp expression is ignored in -D keyframe dump output");

      if (opt_D) {
	 sbx_emit_periods_from_keyframes_with_extra(kfb.v, kfb.n, 0, extra);
      } else {
	 sbx_runtime_activate_from_keyframes(kfb.v, kfb.n, 0,
					     extra_spec.have_mix ? extra_spec.mix_amp_pct : 100.0,
					     extra_spec.aux_tones, extra_spec.aux_count,
					     extra_spec.mix_fx, extra_spec.mix_fx_count,
					     (have_mixamp_curve && have_mix_in_extra) ? mkfb.v : 0,
					     (have_mixamp_curve && have_mix_in_extra) ? mkfb.n : 0);
      }

      if (kfb.v) free(kfb.v);
      if (mkfb.v) free(mkfb.v);
   }

   clear_func_curve();
}

//
//	Generate a -p slide sequence
//
//	The idea of this is to hold the beat frequency constant, but
//	to slide down through the carrier frequencies from about 200Hz.
//
//	-p slide [t<duration-minutes>] <carr>+<beat>/<amp> [extra tone-sets]

void 
bad_slide() {
   error("Bad arguments: expecting -p slide [<time-spec>] <slide-spec> [<tone-specs...>]"
	 NL "<slide-spec> is just like a tone-spec: <carrier><sign><beat>/<amp>"
	 NL "<sign> may be +, -, @ (isochronic), or M (monaural)"
	 NL "The optional <time-spec> is t<slide-time>, giving length of session in"
	 NL "  minutes (the default is equivalent to 't30')."
	 NL "The optional <tone-specs...> let you mix other stuff with the drop"
	 NL "  sequence like white/pink/brown noise or a mix soundtrack, e.g 'pink/20', 'white/20', 'brown/20', or 'mix/60'");
}

void 
create_slide(int ac, char **av) {
   int len= 1800;
   char *p, dmy, signal;
   int ismono;
   double val, c0, c1, beat, amp;
   double beat_abs;
   char extra[256];

#define BAD bad_slide()

   // Handle arguments
   if (ac < 1) BAD;
   if (av[0][0] == 't') {
      val= strtod(av[0]+1, &p);
      if (p == av[0] + 1 || *p) BAD;
      len= 60.0 * val;
      ac--; av++;
   }

   if (ac < 1) BAD;
   if (4 != sscanf(av[0], "%lf%c%lf/%lf %c", &c0, &signal, &beat, &amp, &dmy)) BAD;

   if (signal != '+' && signal != '-' && signal != '@' && signal != 'M') BAD;

   ismono= (signal == 'M');
   beat_abs= fabs(beat);
   c1= beat/2;
   ac--; av++;

#undef BAD

   // Gather 'extra'
   p= extra; *p= 0;
   while (ac > 0) {
      if (p + strlen(av[0]) + 2 > extra + sizeof(extra))
	 error("Too many extra tone-specs after -p slide");
      p += sprintf(p, " %s", av[0]);
      ac--; av++;
   }

   {
      SbxRuntimeExtraSpec extra_spec;
      SbxKfBuilder kfb= {0};
      SbxToneSpec tone;

      if (!sbx_parse_runtime_extra_tokens(extra, &extra_spec))
	 error("Internal error parsing -p slide extra tone-specs");
      if (extra_spec.have_mix && !mix_in && !opt_m && !opt_M)
	 warn("mix/<amp> was specified in -p slide extras but no mix input stream is active");
      sbx_validate_runtime_extra_mix_fx("-p slide", &extra_spec);

      // Summary
      warn("SLIDE summary:");
      warn(" Sliding carrier from %gHz to %gHz over %g minutes",
	   c0, c1, len/60.0);
      if (ismono) {
	 warn(" Holding monaural beat constant at %gHz", beat_abs);
	 warn(" Monaural tone pair: f1=carr-beat/2, f2=carr+beat/2");
      } else {
	 warn(" Holding %s constant at %gHz", signal == '@' ? "pulse" : "beat", beat);
      }

      sbx_fill_tone_spec(&tone, signal == '@', ismono, c0, signal == '-' ? -beat_abs : beat_abs, amp);
      sbx_kfb_add(&kfb, 0.0, &tone, SBX_INTERP_LINEAR);
      sbx_fill_tone_spec(&tone, signal == '@', ismono, c1, signal == '-' ? -beat_abs : beat_abs, amp);
      sbx_kfb_add(&kfb, (double)len, &tone, SBX_INTERP_LINEAR);
      sbx_fill_tone_spec(&tone, signal == '@', ismono, c1, signal == '-' ? -beat_abs : beat_abs, 0.0);
      sbx_kfb_add(&kfb, (double)len + 10.0, &tone, SBX_INTERP_LINEAR);

      sbx_handle_runtime_unsupported_extra("-p slide", &extra_spec);

      if (opt_D) {
	 sbx_emit_periods_from_keyframes_with_extra(kfb.v, kfb.n, 0, extra);
      } else {
	 sbx_runtime_activate_from_keyframes(kfb.v, kfb.n, 0,
					     extra_spec.have_mix ? extra_spec.mix_amp_pct : 100.0,
					     extra_spec.aux_tones, extra_spec.aux_count,
					     extra_spec.mix_fx, extra_spec.mix_fx_count,
					     0, 0);
      }

      if (kfb.v) free(kfb.v);
   }
}   

// Function to normalize the total amplitude of voices
void normalizeAmplitude(Voice *voices, int numChannels, const char *line, int lineNum) {
  double totalAmplitude = 0.0;
  
  // First, check mixspin/mixpulse/mixbeat separately (these have different logic)
  for (int ch = 0; ch < numChannels; ch++) {
    if (voices[ch].typ == 6 || voices[ch].typ == 7 || voices[ch].typ == 13) {
      double ampPercentage = voices[ch].amp / 40.96;
      if (ampPercentage > 100.0) {
        error("Total intensity of mixspin/mixpulse/mixbeat exceeds 100%% (%.2f%%) at line %d:\n  %s",
              ampPercentage, lineNum, line);
      }
    }
  }
  
  // Calculate the total amplitude of all voices (excluding mixspin/mixpulse/mixbeat)
  for (int ch = 0; ch < numChannels; ch++) {
    if (voices[ch].typ != 0 && voices[ch].typ != 6 && voices[ch].typ != 7 && voices[ch].typ != 13) {
      double ampPercentage = voices[ch].amp / 40.96;
      totalAmplitude += ampPercentage;
    }
  }
  
  // If total amplitude exceeds 100%, normalize all active voices (except mix effects)
  if (opt_N && totalAmplitude > 100.0) {
    double normalizationFactor = 100.0 / totalAmplitude;
    
    if (!opt_Q) {
      warn("Total amplitude %.2f%% exceeds 100%% at line %d, auto-normalizing by factor %.3f", 
           totalAmplitude, lineNum, normalizationFactor);
    }
    
    // Apply normalization to all active voices (except mix effects)
    for (int ch = 0; ch < numChannels; ch++) {
      if (voices[ch].typ != 0 && voices[ch].typ != 6 && voices[ch].typ != 7 && voices[ch].typ != 13) {
        voices[ch].amp *= normalizationFactor;
      }
    }
  }

  // If total amplitude exceeds 100%, warn the user
  if (!opt_N && !opt_Q && totalAmplitude > 100.0) {
    warn("*** WARNING: Total amplitude %.2f%% exceeds 100%% at line %d, distortion may occur ***", 
         totalAmplitude, lineNum);
  }
}

void printSequenceDuration() {
  int duration = t_per0(fast_tim0, fast_tim1);
  fprintf(stdout, "\n*** Sequence duration: %02d:%02d:%02d (hh:mm:ss) ***\n\n", 
          duration/3600000, (duration/60000)%60, (duration/1000)%60);
}

void checkMixInSequence() {
  char *ln= lin_copy;
  int mix_exists= 0;

  while((ln= strstr(ln, "mix/")) != NULL) {
    if(isdigit(ln[4])) {
      mix_exists= 1;
      break;
    }
    ln++;
  }

  if(!mix_exists)
    error("mixspin/mixpulse/mixbeat without mix/<amp> specified, line %d:\n  %s", in_lin, lin_copy);
}

// END //
