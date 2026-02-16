//
//	FLAC decoding using dr_flac (single-file decoder), with optional
//	SBAGEN_LOOPER-based looping.
//
//	(c) 2026 SBaGenX contributors.
//

#define DR_FLAC_IMPLEMENTATION
#include "libs/dr_flac.h"

extern FILE *mix_in;
extern int mix_cnt;
extern void *Alloc(size_t);
extern void error(char *fmt, ...);
extern int out_rate, out_rate_def;

void flac_init();
int flac_read(int *dst, int dlen);

static drflac *flac_file;
static short *flac_buf;
static int flac_buf_frames;
static int flac_buf_pos;
static int flac_buf_len;
static int flac_channels;
static int flac_mult;

// Decoded PCM used by SBAGEN_LOOPER mode. Interleaved stereo samples.
static short *flac_pcm;
static int flac_datcnt;
static int flac_datcnt0;
static int flac_datbase;
static int flac_datrate;

static int flac_fade_cnt;
static int flac_seg0, flac_seg1;
static int flac_ch2;
static int flac_ch2_swap;
static uint flac_del_amp;
static int flac_intro_cnt;
static int flac_intro_pos;
static int flac_intro_first_seg;

typedef struct {
   int off;
   int src;
   int chan;
   int mode;
   int cnt;
   int cnt_all;
   uint amp;
   uint del;
} FlacStream;

static FlacStream flac_str[3];

static unsigned short flac_zxrand_seed;

static int flac_zxrand_0_65536();
static int flac_zxrand_0(int mult);
static int flac_zxrand(int r0, int r1);
static int flac_zxrandM(int def, char *fmt, ...);
static void flac_looper_init(char *looper);
static int flac_looper_read(int *dst, int dlen);
static void flac_looper_sched();
static void flac_looper_sched2();


typedef struct {
   char *looper;
   int looper_len;
   int mult;
} FlacMeta;

typedef struct {
   FILE *in;
   FlacMeta *meta;
} FlacOpenCtx;

static FlacOpenCtx flac_open_ctx;


static size_t
flac_cb_read(void *pUserData, void *pBufferOut, size_t bytesToRead) {
   FlacOpenCtx *ctx= (FlacOpenCtx*)pUserData;
   FILE *in= ctx->in;
   return fread(pBufferOut, 1, bytesToRead, in);
}

static drflac_bool32
flac_cb_seek(void *pUserData, int offset, drflac_seek_origin origin) {
   FlacOpenCtx *ctx= (FlacOpenCtx*)pUserData;
   FILE *in= ctx->in;
   int whence;

   switch (origin) {
    case DRFLAC_SEEK_SET:
      whence= SEEK_SET;
      break;
    case DRFLAC_SEEK_CUR:
      whence= SEEK_CUR;
      break;
    default:
      return DRFLAC_FALSE;
   }

   return 0 == fseek(in, offset, whence);
}

static drflac_bool32
flac_cb_tell(void *pUserData, drflac_int64 *pCursor) {
   FlacOpenCtx *ctx= (FlacOpenCtx*)pUserData;
   FILE *in= ctx->in;
   long pos= ftell(in);
   if (pos < 0) return DRFLAC_FALSE;
   *pCursor= (drflac_int64)pos;
   return DRFLAC_TRUE;
}

static double
flac_parse_dbl(const char *p, int len, int *ok) {
   char buf[128];
   char *end;

   if (len >= (int)sizeof(buf)) len= sizeof(buf)-1;
   memcpy(buf, p, len);
   buf[len]= 0;

   *ok= 0;
   end= 0;
   {
      double val= strtod(buf, &end);
      if (end && end != buf) {
         *ok= 1;
         return val;
      }
   }
   return 0;
}

static char *
flac_dup_range(const char *p, int len) {
   char *out= ALLOC_ARR(len + 1, char);
   memcpy(out, p, len);
   out[len]= 0;
   return out;
}

static void
flac_meta_proc(void *pUserData, drflac_metadata *pMetadata) {
   FlacOpenCtx *ctx= (FlacOpenCtx*)pUserData;
   FlacMeta *meta= ctx->meta;

   if (pMetadata->type != DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT)
      return;

   {
      drflac_vorbis_comment_iterator it;
      const char *comment;
      drflac_uint32 comment_len;

      drflac_init_vorbis_comment_iterator(&it,
	  pMetadata->data.vorbis_comment.commentCount,
	  pMetadata->data.vorbis_comment.pComments);

      while ((comment= drflac_next_vorbis_comment(&it, &comment_len))) {
	 if (comment_len >= 14 &&
	     0 == memcmp(comment, "SBAGEN_LOOPER=", 14)) {
	    if (meta->looper) free(meta->looper);
	    meta->looper_len= comment_len - 14;
	    meta->looper= flac_dup_range(comment + 14, meta->looper_len);
	 }
	 if (comment_len >= 22 &&
	     0 == memcmp(comment, "REPLAYGAIN_TRACK_GAIN=", 22)) {
	    int ok;
	    double val= flac_parse_dbl(comment + 22, comment_len - 22, &ok);
	    if (!ok)
	       warn("Ignoring bad REPLAYGAIN_TRACK_GAIN in FLAC metadata");
	    else {
	       val -= 3; // Adjust vorbisgain's 89dB reference to 86dB
	       meta->mult= (int)(floor(0.5 + 16 * pow(10, val/20)));
	    }
	 }
      }
   }
}

static void
flac_decode_all_to_pcm() {
   drflac_int16 *tmp;
   drflac_uint64 got;
   drflac_uint64 total;
   drflac_uint64 cap;
   drflac_uint64 used;
   drflac_uint64 a;

   total= flac_file->totalPCMFrameCount;
   cap= total ? total : (drflac_uint64)flac_file->sampleRate * 60;
   if (cap < 4096) cap= 4096;

   if (cap > 0x3FFFFFFF)
      error("FLAC input too large to fit in memory");

   flac_pcm= ALLOC_ARR(cap * 2, short);
   tmp= ALLOC_ARR(4096 * flac_channels, drflac_int16);
   used= 0;

   while (1) {
      got= drflac_read_pcm_frames_s16(flac_file, 4096, tmp);
      if (!got) break;

      if (used + got > cap) {
	 drflac_uint64 new_cap= cap;
	 while (new_cap < used + got) {
	    new_cap *= 2;
	    if (new_cap < cap)
	       error("FLAC size overflow while decoding");
	 }
	 if (new_cap > 0x3FFFFFFF)
	    error("FLAC input too large to fit in memory");
	 flac_pcm= (short*)realloc(flac_pcm, (size_t)(new_cap * 2 * sizeof(short)));
	 if (!flac_pcm)
	    error("Out of memory expanding FLAC decode buffer");
	 cap= new_cap;
      }

      if (flac_channels == 1) {
	 for (a= 0; a<got; a++) {
	    short v= tmp[a];
	    flac_pcm[(used+a)*2 + 0]= v;
	    flac_pcm[(used+a)*2 + 1]= v;
	 }
      } else {
	 for (a= 0; a<got; a++) {
	    short *fr= tmp + a * flac_channels;
	    flac_pcm[(used+a)*2 + 0]= fr[0];
	    flac_pcm[(used+a)*2 + 1]= fr[1];
	 }
      }

      used += got;
   }

   free(tmp);

   if (!used)
      error("FLAC stream appears to contain no PCM data");

   if (used > 0x7FFFFFFF)
      error("FLAC stream too long for loop scheduler");

   if (used < cap) {
      flac_pcm= (short*)realloc(flac_pcm, (size_t)(used * 2 * sizeof(short)));
      if (!flac_pcm)
	 error("Out of memory shrinking FLAC decode buffer");
   }

   flac_datcnt0= (int)used;
}

void
flac_init() {
   FlacMeta meta;

   memset(&meta, 0, sizeof(meta));
   meta.mult= 16;
   flac_open_ctx.in= mix_in;
   flac_open_ctx.meta= &meta;

   if (0 != fseek(mix_in, 0, SEEK_SET))
      error("Can't seek to start of FLAC file: %s", strerror(errno));

   flac_file= drflac_open_with_metadata(flac_cb_read,
			       flac_cb_seek,
			       flac_cb_tell,
			       flac_meta_proc,
			       &flac_open_ctx,
			       0);
   if (!flac_file)
      error("Input does not appear to be a FLAC bitstream");

   flac_channels= flac_file->channels;
   if (flac_channels < 1)
      error("Unsupported FLAC stream with %d channels", flac_channels);

   flac_datrate= flac_file->sampleRate;
   if (out_rate_def) out_rate= flac_datrate;
   out_rate_def= 0;

   flac_mult= meta.mult;
   if (flac_mult != 16)
      warn("ReplayGain setting detected, FLAC scaling by %.2f", flac_mult/16.0);

   if (meta.looper && *meta.looper) {
      flac_decode_all_to_pcm();
      drflac_close(flac_file);
      flac_file= 0;
      flac_looper_init(meta.looper);
      free(meta.looper);
      return;
   }

   if (meta.looper) free(meta.looper);

   flac_buf_frames= 2048;
   flac_buf= ALLOC_ARR(flac_buf_frames * flac_channels, short);
   flac_buf_pos= 0;
   flac_buf_len= 0;

   inbuf_start(flac_read, 256*1024);
}

int
flac_read(int *dst, int dlen) {
   int *dst0= dst;

   while (dlen > 0) {
      int avail;

      if (flac_buf_pos == flac_buf_len) {
	 drflac_uint64 got= drflac_read_pcm_frames_s16(flac_file,
					       flac_buf_frames,
					       flac_buf);
	 if (!got) return dst-dst0;
	 if (got > 0x7FFFFFFF)
	    error("UNEXPECTED: FLAC decoder returned an excessively large frame count");
	 flac_buf_len= (int)got;
	 flac_buf_pos= 0;
      }

      avail= flac_buf_len - flac_buf_pos;
      while (avail > 0 && dlen > 0) {
	 int l, r;
	 short *fr= flac_buf + flac_buf_pos * flac_channels;
	 if (flac_channels == 1) {
	    l= fr[0];
	    r= fr[0];
	 } else {
	    l= fr[0];
	    r= fr[1];
	 }
	 *dst++= l * flac_mult;
	 dlen--;
	 if (dlen > 0) {
	    *dst++= r * flac_mult;
	    dlen--;
	 }
	 flac_buf_pos++;
	 avail--;
      }
   }

   return dst-dst0;
}


// Simple/crude 16-bit pseudo-random number generator, from ZX Spectrum
static int
flac_zxrand_0_65536() {
   flac_zxrand_seed= (1 + (int)flac_zxrand_seed) * 75 % 65537 - 1;
   return flac_zxrand_seed;
}

static int
flac_zxrand_0(int mult) {
   long long tmp= mult;
   tmp *= flac_zxrand_0_65536();
   tmp >>= 16;
   return (int)tmp;
}

// Returns crude pseudo-random value from r0 to r1-1, or r0 if the range is invalid
static int
flac_zxrand(int r0, int r1) {
   if (r1 <= r0) return r0;
   return r0 + flac_zxrand_0(r1-r0);
}

// Return random value out of multiple ranges.
static int
flac_zxrandM(int def, char *fmt, ...) {
   va_list ap;
   int cnt= 0;
   int val;
   char *p;
   int olo, ohi;

   va_start(ap, fmt);
   olo= 0x80000000;
   ohi= 0x7FFFFFFF;
   for (p= fmt; *p; p++) {
      int lo= va_arg(ap, int);
      int hi= va_arg(ap, int);
      if (*p == 'o') {
	 olo= lo;
	 ohi= hi;
      } else if (*p == 'r') {
	 if (lo < olo) lo= olo;
	 if (hi > ohi) hi= ohi;
	 if (hi-lo > 0) cnt += hi-lo;
      } else {
	 error("Bad flac_zxrandM format: %s", fmt);
      }
   }
   va_end(ap);

   if (!cnt) return def;
   val= flac_zxrand_0(cnt);

   va_start(ap, fmt);
   olo= 0x80000000;
   ohi= 0x7FFFFFFF;
   for (p= fmt; *p; p++) {
      int lo= va_arg(ap, int);
      int hi= va_arg(ap, int);
      if (*p == 'o') {
	 olo= lo;
	 ohi= hi;
      } else if (*p == 'r') {
	 if (lo < olo) lo= olo;
	 if (hi > ohi) hi= ohi;
	 cnt= hi-lo;
	 if (cnt > 0) {
	    if (val < cnt) return lo + val;
	    val -= cnt;
	 }
      }
   }
   va_end(ap);

   return def;
}

static void
flac_looper_init(char *looper) {
   int a;
   int intro= 0;
   int prev_flag= 0;
   int on;

   flac_zxrand_seed= 0xFFFF & time(NULL);

   flac_datcnt= flac_datcnt0;
   flac_datbase= 0;
   flac_seg0= flac_seg1= flac_datcnt;
   flac_fade_cnt= flac_datrate;
   flac_ch2= 0;
   flac_ch2_swap= 1;
   on= 1;
   if (mix_cnt < 0) mix_cnt= 0;

   if (*looper == 'i') {
      if (isspace((unsigned char)looper[1]))
	 intro= 1;
      else
	 warn("Ignoring SBAGEN_LOOPER intro flag: 'i' must be followed by whitespace");
      looper++;
   }

   while (*looper) {
      char flag, *p;
      double val;

      flag= *looper++;
      if (isspace((unsigned char)flag)) continue;
      if (!strchr("s-fcwd#", flag)) {
	 warn("Bad SBAGEN_LOOPER flag: %c", flag);
	 continue;
      }
      if (flag == '-') switch (prev_flag) {
       case 's': flag= 'S'; break;
       case 'd': flag= 'D'; break;
       default:
	 warn("SBAGEN_LOOPER '-' found not in form s<val>-<val>");
	 continue;
      }
      prev_flag= flag;

      val= strtod(looper, &p);
      if (p == looper) {
	 warn("Bad SBAGEN_LOOPER value for flag '%c': %s", flag, p);
	 continue;
      }
      looper= p;

      if (flag == '#')
	 on= (val == mix_cnt);
      else if (on) switch (flag) {
       case 's': flac_seg0= flac_seg1= val * flac_datrate; break;
       case 'S': flac_seg1= val * flac_datrate; break;
       case 'd': flac_datbase= val * flac_datrate; flac_datcnt= flac_datcnt0 - flac_datbase; break;
       case 'D': flac_datcnt= val * flac_datrate - flac_datbase; break;
       case 'f': flac_fade_cnt= val * flac_datrate; break;
       case 'c': flac_ch2= val > 1.5; break;
       case 'w': flac_ch2_swap= val > 0.5; break;
      }
   }

   if (flac_fade_cnt < flac_datrate/50) flac_fade_cnt= flac_datrate/50;
   if (flac_datcnt + flac_datbase > flac_datcnt0) flac_datcnt= flac_datcnt0 - flac_datbase;
   if (flac_datcnt < 0)
      error("Source data range invalid in SBAGEN_LOOPER settings");
   if (flac_datcnt <= 3 * flac_fade_cnt)
      error("Length of source data 'd' too short for fade-length of %gs\n"
	    " in SBAGEN_LOOPER settings", flac_fade_cnt * 1.0 / flac_datrate);
   if (flac_seg0 > flac_datcnt) flac_seg0= flac_datcnt;
   if (flac_seg1 > flac_datcnt) flac_seg1= flac_datcnt;
   if (flac_seg0 > flac_seg1) flac_seg0= flac_seg1;
   if (flac_seg0 < 3 * flac_fade_cnt) {
      flac_seg0= 3 * flac_fade_cnt;
      warn("SBAGEN_LOOPER segment size too short for fade-length of %gs; adjusted.",
	   flac_fade_cnt * 1.0 / flac_datrate);
   }
   if (flac_seg1 < flac_seg0) flac_seg1= flac_seg0;

   flac_intro_cnt= (intro && flac_datbase > 0) ? flac_datbase : 0;
   flac_intro_pos= 0;
   flac_intro_first_seg= flac_intro_cnt ? 1 : 0;
   if (intro && !flac_intro_cnt)
      warn("SBAGEN_LOOPER intro requested, but d-start is not positive; ignoring intro");

   flac_del_amp= 0xFFFFFFFFU/flac_fade_cnt;
   if (flac_del_amp * (uint)flac_fade_cnt < 0xF0000000)
      error("Internal rounding error in calculating amplitude delta");
   if (flac_ch2) flac_del_amp >>= 1;

   for (a= 0; a<3; a++)
      memset(&flac_str[a], 0, sizeof(flac_str[a]));

   if (!flac_intro_cnt)
      flac_looper_sched();

   inbuf_start(flac_looper_read, 256*1024);
}

#define FLAC_CNT_TO_END(ss) \
((ss->mode == 1) ? (ss->cnt_all + ss->cnt) : \
 (ss->mode == 2) ? (ss->cnt_all - flac_fade_cnt + ss->cnt) : \
 (ss->mode == 3) ? (flac_fade_cnt + ss->cnt) : \
 (ss->mode == 4) ? ss->cnt : 0)

static int
flac_looper_read(int *dst, int dlen) {
   int *dst0= dst;
   int *dst1= dst + dlen;

   memset(dst0, 0, (char*)dst1 - (char*)dst0);

   while (dst0 < dst1) {
      int a;
      int len= (dst1-dst0)/2;
      int resched= 0;

      if (flac_intro_cnt > 0) {
	 int intro_len= len;
	 if (intro_len > flac_intro_cnt)
	    intro_len= flac_intro_cnt;
	 for (a= 0; a<intro_len; a++) {
	    int off= (flac_intro_pos + a) * 2;
	    dst0[a*2 + 0] += flac_pcm[off + 0] * flac_mult;
	    dst0[a*2 + 1] += flac_pcm[off + 1] * flac_mult;
	 }
	 dst0 += intro_len * 2;
	 flac_intro_pos += intro_len;
	 flac_intro_cnt -= intro_len;
	 if (!flac_intro_cnt)
	    flac_looper_sched();
	 continue;
      }

      for (a= 0; a<3; a++) {
	 FlacStream *ss= &flac_str[a];
	 if (ss->mode && ss->cnt < len)
	    len= ss->cnt;
      }

      for (a= 0; a<3; a++) {
	 FlacStream *ss= &flac_str[a];
	 int cnt= len;

	 if (!ss->mode) continue;

	 dst= dst0;
	 while (cnt > 0) {
	    int l, r;

	    if (ss->mode == 1) {
	       ss->cnt -= cnt;
	       cnt= 0;
	       continue;
	    }

	    if (ss->src >= 0 && ss->src < flac_datcnt0) {
	       int off= ss->src * 2;
	       l= flac_pcm[off + 0];
	       r= flac_pcm[off + 1];
	    } else {
	       l= 0;
	       r= 0;
	    }

	    {
	       uint amp= (~ss->amp) >> 16;
	       amp= (~(amp*amp)) >> 21;
	       amp *= flac_mult;
	       if (ss->chan && flac_ch2_swap) {
		  *dst++ += ((int)(r * amp)) >> 11;
		  *dst++ += ((int)(l * amp)) >> 11;
	       } else {
		  *dst++ += ((int)(l * amp)) >> 11;
		  *dst++ += ((int)(r * amp)) >> 11;
	       }
	    }

	    ss->amp += ss->del;
	    ss->cnt--;
	    ss->src++;
	    cnt--;
	 }

	 if (!ss->cnt) switch (ss->mode) {
	  case 1:
	     ss->mode= 2;
	     ss->cnt= flac_fade_cnt;
	     ss->del= flac_del_amp;
	     break;
	  case 2:
	     ss->mode= 3;
	     ss->cnt= ss->cnt_all - 2 * flac_fade_cnt;
	     ss->del= 0;
	     break;
	  case 3:
	     ss->mode= 4;
	     ss->cnt= flac_fade_cnt;
	     ss->del= -flac_del_amp;
	     break;
	  case 4:
	     ss->mode= 0;
	     resched= 1;
	     break;
	 }
      }

      dst0 += len * 2;
      if (resched) flac_looper_sched();
   }

   return dlen;
}

static void
flac_looper_sched() {
   if (flac_ch2) {
      flac_looper_sched2();
      return;
   }

   while (1) {
      FlacStream *aa= &flac_str[0];
      FlacStream *bb= &flac_str[1];
      int cnt_all;

      if (aa->mode && bb->mode) break;

      if (bb->mode && !aa->mode) {
	 FlacStream *tmp= aa;
	 aa= bb;
	 bb= tmp;
      }

      if (!aa->mode) aa= 0;

      bb->off= -1;
      bb->mode= 1;
      bb->cnt= 0;
      bb->amp= 0;
      bb->src= flac_datbase;

      if (aa) {
	 bb->cnt= FLAC_CNT_TO_END(aa) - flac_fade_cnt;
	 if (bb->cnt < 0) bb->cnt= 0;
      }

      bb->cnt_all= cnt_all= flac_zxrand(flac_seg0, flac_seg1+1);

      if (!aa && flac_intro_first_seg) {
	 bb->off= 0;
	 bb->mode= 3;
	 bb->cnt= cnt_all - flac_fade_cnt;
	 bb->amp= 0xFFFFFFFFU;
	 bb->del= 0;
	 flac_intro_first_seg= 0;
      }

      if (aa) {
	 bb->off= flac_zxrandM(-1, "rr",
			      0, aa->off - cnt_all,
			      aa->off + aa->cnt_all, flac_datcnt - cnt_all);
      }

      if (bb->off < 0)
	 bb->off= flac_zxrand(0, flac_datcnt - cnt_all);

      bb->src= flac_datbase + bb->off;
   }
}

static void
flac_looper_sched2() {
   while (1) {
      FlacStream *aa= &flac_str[0];
      FlacStream *bb= &flac_str[1];
      FlacStream *cc= &flac_str[2];
      FlacStream *tmp;
      int cnt_all;

      if (aa->mode && bb->mode && cc->mode) break;

      if (!aa->mode && bb->mode) {
	 tmp= aa; aa= bb; bb= tmp;
      }
      if (!aa->mode && cc->mode) {
	 tmp= aa; aa= cc; cc= tmp;
      }
      if (!bb->mode && cc->mode) {
	 tmp= bb; bb= cc; cc= tmp;
      }

      if (!bb->mode) {
	 bb->chan= aa ? !aa->chan : 0;
	 bb->off= -1;
	 bb->mode= 1;
	 bb->amp= 0;
	 bb->cnt= 0;
	 bb->src= flac_datbase;

	 if (!aa) {
	    cnt_all= flac_zxrand(flac_seg0, flac_seg1+1);
	 } else {
	    int end= FLAC_CNT_TO_END(aa);
	    cnt_all= flac_zxrandM(-1, "orr",
				  flac_seg0, flac_seg1+1,
				  flac_seg0, end-flac_fade_cnt,
				  end+flac_fade_cnt, flac_seg1+1);
	    if (cnt_all < 0) {
	       bb->cnt= end+flac_fade_cnt - flac_seg1;
	       cnt_all= flac_seg1;
	    }
	 }
	 bb->cnt_all= cnt_all;
	 if (bb->cnt < 0) bb->cnt= 0;

	 if (!aa && flac_intro_first_seg) {
	    bb->off= 0;
	    bb->mode= 3;
	    bb->cnt= cnt_all - flac_fade_cnt;
	    bb->amp= 0xFFFFFFFFU;
	    bb->del= 0;
	    flac_intro_first_seg= 0;
	 }

	 if (aa) {
	    bb->off= flac_zxrandM(-1, "rr",
				   0, aa->off - cnt_all,
				   aa->off + aa->cnt_all, flac_datcnt - cnt_all);
	 }
	 if (bb->off < 0)
	    bb->off= flac_zxrand(0, flac_datcnt - cnt_all);

	 bb->src= flac_datbase + bb->off;
	 continue;
      }

      if (aa->chan == bb->chan)
	 error("UNEXPECTED: internal error, flac_looper_sched2(), aa/bb on same chan");

      if (FLAC_CNT_TO_END(aa) > FLAC_CNT_TO_END(bb)) {
	 tmp= aa;
	 aa= bb;
	 bb= tmp;
      }

      cc->chan= aa->chan;
      cc->cnt= FLAC_CNT_TO_END(aa) - flac_fade_cnt;
      cc->off= -1;
      cc->mode= 1;
      cc->amp= 0;
      cc->src= flac_datbase;

      {
	 int end= FLAC_CNT_TO_END(bb);
	 end -= cc->cnt;
	 cnt_all= flac_zxrandM(-1, "orr",
			      flac_seg0, flac_seg1+1,
			      flac_seg0, end-flac_fade_cnt,
			      end+flac_fade_cnt, flac_seg1+1);
	 if (cnt_all < 0) {
	    if (end-flac_fade_cnt > flac_fade_cnt * 2)
	       cnt_all= end-flac_fade_cnt;
	    else
	       cnt_all= end+flac_fade_cnt;
	 }
      }
      cc->cnt_all= cnt_all;

      {
	 int r0= aa->off;
	 int r1= aa->off + aa->cnt_all;
	 int r2= bb->off;
	 int r3= bb->off + bb->cnt_all;
	 if (r0 > r2) {
	    int t;
	    t= r0; r0= r2; r2= t;
	    t= r1; r1= r3; r3= t;
	 }
	 cc->off= flac_zxrandM(-1, "rrr",
			      0, r0 - cnt_all,
			      r1, r2 - cnt_all,
			      r3, flac_datcnt - cnt_all);
      }

      if (cc->off < 0)
	 cc->off= flac_zxrand(0, flac_datcnt - cnt_all);

      cc->src= flac_datbase + cc->off;
   }
}

// END //
