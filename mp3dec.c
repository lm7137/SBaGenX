//
//\tMP3 decoding using libMAD, with optional indexed
//\tSBAGEN_LOOPER support.
//
//      (c) 1999-2004 Jim Peters <jim@uazu.net>.  All Rights Reserved.
//      For latest version see http://sbagen.sf.net/ or
//      http://uazu.net/sbagen/.  Released under the GNU GPL version 2.
//

#include "libs/mad.h"
#include <stdint.h>

extern FILE *mix_in;
extern void *Alloc(size_t);
extern void error(char *fmt, ...);

int mp3_read(int *dst, int dlen);

#define MP3_FOPEN(path, mode) sbx_mix_fopen_utf8(path, mode)

typedef struct {
   FILE *fp;
   struct mad_stream stream;
   struct mad_frame frame;
   struct mad_synth synth;
   unsigned char *buf;
   int buf_len;
   long buf_file_off;
   int synth_pos;
   int synth_len;
} Mp3Decoder;

typedef struct {
   long file_off;
   int sample_off;
   int sample_count;
} Mp3FrameIndex;

typedef struct {
   Mp3Decoder dec;
   int off;
   int chan;
   int mode;
   int cnt;
   int cnt_all;
   uint amp;
   uint del;
} Mp3LoopStream;

static Mp3Decoder mp3_norm_dec;
static unsigned char *mp3_norm_buf;
static int mp3_norm_buf_len;

static int mp3_looper_active;
static int mp3_datrate;
static int mp3_datcnt;
static int mp3_datcnt0;
static int mp3_datbase;
static int mp3_fade_cnt;
static int mp3_seg0, mp3_seg1;
static int mp3_ch2;
static int mp3_ch2_swap;
static uint mp3_del_amp;
static int mp3_intro_cnt;
static int mp3_intro_first_seg;
static unsigned short mp3_zxrand_seed;
static Mp3FrameIndex *mp3_framev;
static int mp3_framec;
static int mp3_framecap;
static char *mp3_looper_embedded;
static long mp3_audio_start;
static Mp3LoopStream mp3_str[3];

static void mp3_decoder_reset(Mp3Decoder *dec, FILE *fp, unsigned char *buf, int buf_len);
static void mp3_decoder_finish(Mp3Decoder *dec);
static int mp3_decoder_next_frame(Mp3Decoder *dec, long *frame_off_out);
static int mp3_decoder_read_sample(Mp3Decoder *dec, int *l, int *r);
static void mp3_index_reset();
static void mp3_index_append(long file_off, int sample_off, int sample_count);
static void mp3_scan_id3_tag(FILE *fp, char **looper_out, long *audio_start_out);
static void mp3_scan_build_index(FILE *fp);
static int mp3_frame_find(int sample_off);
static FILE *mp3_reopen_stream();
static void mp3_looper_setup(const char *looper);
static void mp3_looper_term();
static int mp3_looper_read_intro(int *dst, int dlen);
static int mp3_looper_read(int *dst, int dlen);
static void mp3_looper_sched();
static void mp3_looper_sched2();
static void mp3_stream_seek(Mp3LoopStream *ss, int sample_off);
static int mp3_zxrand_0_65536();
static int mp3_zxrand_0(int mult);
static int mp3_zxrand(int r0, int r1);
static int mp3_zxrandM(int def, char *fmt, ...);

/* Limiting and truncation to 20 bits. */
#define ROUND(xx) ((((xx)<-MAD_F_ONE) ? -MAD_F_ONE : \
                    ((xx)>=MAD_F_ONE) ? MAD_F_ONE-1 : \
                    (xx)) >> (MAD_F_FRACBITS-15-4))

static int
mp3_synchsafe32(const unsigned char *p) {
   return ((p[0] & 0x7F) << 21) |
          ((p[1] & 0x7F) << 14) |
          ((p[2] & 0x7F) << 7) |
          (p[3] & 0x7F);
}

static int
mp3_be32(const unsigned char *p) {
   return ((int)p[0] << 24) |
          ((int)p[1] << 16) |
          ((int)p[2] << 8) |
          (int)p[3];
}

static char *
mp3_dup_range_trim(const unsigned char *src, int len) {
   char *out;
   while (len > 0 && src[len-1] == 0)
      len--;
   out= ALLOC_ARR(len + 1, char);
   memcpy(out, src, len);
   out[len]= 0;
   return out;
}

static char *
mp3_dup_utf16_asciiish(const unsigned char *src, int len) {
   char *out;
   int a, n= 0, start= 0;

   if (len >= 2 && ((src[0] == 0xFF && src[1] == 0xFE) || (src[0] == 0xFE && src[1] == 0xFF)))
      start= 2;
   out= ALLOC_ARR(len/2 + 1, char);
   for (a= start; a + 1 < len; a += 2) {
      unsigned char b0= src[a + 0];
      unsigned char b1= src[a + 1];
      if (!b0 && !b1)
         break;
      if (b0 && !b1) out[n++]= b0;
      else if (!b0 && b1) out[n++]= b1;
      else out[n++]= '?';
   }
   out[n]= 0;
   return out;
}

static char *
mp3_txxx_value(const unsigned char *data, int len) {
   int enc;
   char *desc= 0;
   char *value= 0;
   int sep;

   if (len <= 1)
      return 0;
   enc= data[0];
   data++;
   len--;

   if (enc == 0 || enc == 3) {
      for (sep= 0; sep < len && data[sep]; sep++)
         ;
      desc= mp3_dup_range_trim(data, sep);
      if (sep < len)
         value= mp3_dup_range_trim(data + sep + 1, len - sep - 1);
      else
         value= mp3_dup_range_trim((const unsigned char*)"", 0);
   } else if (enc == 1 || enc == 2) {
      for (sep= 0; sep + 1 < len; sep += 2) {
         if (!data[sep] && !data[sep+1])
            break;
      }
      desc= mp3_dup_utf16_asciiish(data, sep);
      if (sep + 1 < len)
         value= mp3_dup_utf16_asciiish(data + sep + 2, len - sep - 2);
      else
         value= mp3_dup_range_trim((const unsigned char*)"", 0);
   } else {
      return 0;
   }

   if (!strcmp(desc, "SBAGEN_LOOPER")) {
      free(desc);
      return value;
   }
   if (0 == strncmp(desc, "TXXX:", 5) && !strcmp(desc + 5, "SBAGEN_LOOPER")) {
      free(desc);
      return value;
   }
   if (!desc[0] && 0 == strncmp(value, "SBAGEN_LOOPER=", 14)) {
      char *out= strdup(value + 14);
      free(desc);
      free(value);
      return out;
   }

   free(desc);
   free(value);
   return 0;
}

static void
mp3_scan_id3_tag(FILE *fp, char **looper_out, long *audio_start_out) {
   unsigned char hdr[10];
   int size;
   int footer= 0;
   unsigned char *tag= 0;
   int pos= 0;
   int version;

   *looper_out= 0;
   *audio_start_out= 0;

   if (0 != fseek(fp, 0, SEEK_SET))
      error("Can't seek MP3 input stream: %s", strerror(errno));
   if (1 != fread(hdr, sizeof(hdr), 1, fp)) {
      if (feof(fp)) {
         clearerr(fp);
         if (0 != fseek(fp, 0, SEEK_SET))
            error("Can't rewind MP3 input stream: %s", strerror(errno));
         return;
      }
      error("Read error on MP3 input stream:\n  %s", strerror(errno));
   }
   if (memcmp(hdr, "ID3", 3)) {
      if (0 != fseek(fp, 0, SEEK_SET))
         error("Can't rewind MP3 input stream: %s", strerror(errno));
      return;
   }

   version= hdr[3];
   if (version < 3 || version > 4) {
      warn("Ignoring unsupported ID3v2.%d tag while scanning MP3 loop metadata", version);
      size= mp3_synchsafe32(hdr + 6);
      footer= (version == 4 && (hdr[5] & 0x10)) ? 10 : 0;
      *audio_start_out= 10 + size + footer;
      if (0 != fseek(fp, *audio_start_out, SEEK_SET))
         error("Can't seek past MP3 ID3 tag: %s", strerror(errno));
      return;
   }

   size= mp3_synchsafe32(hdr + 6);
   footer= (version == 4 && (hdr[5] & 0x10)) ? 10 : 0;
   *audio_start_out= 10 + size + footer;
   if (!size) {
      if (0 != fseek(fp, *audio_start_out, SEEK_SET))
         error("Can't seek past MP3 ID3 tag: %s", strerror(errno));
      return;
   }

   tag= ALLOC_ARR(size, unsigned char);
   if (1 != fread(tag, size, 1, fp))
      error("Read error on MP3 ID3 tag:\n  %s", strerror(errno));

   while (pos + 10 <= size) {
      const unsigned char *fr= tag + pos;
      int frame_size;
      char *looper;

      if (!fr[0] && !fr[1] && !fr[2] && !fr[3])
         break;
      if (version == 4)
         frame_size= mp3_synchsafe32(fr + 4);
      else
         frame_size= mp3_be32(fr + 4);
      if (frame_size < 0 || pos + 10 + frame_size > size)
         break;
      if (0 == memcmp(fr, "TXXX", 4)) {
         looper= mp3_txxx_value(fr + 10, frame_size);
         if (looper) {
            if (*looper_out) free(*looper_out);
            *looper_out= looper;
         }
      }
      pos += 10 + frame_size;
   }

   free(tag);
   if (0 != fseek(fp, *audio_start_out, SEEK_SET))
      error("Can't seek past MP3 ID3 tag: %s", strerror(errno));
}

static void
mp3_decoder_reset(Mp3Decoder *dec, FILE *fp, unsigned char *buf, int buf_len) {
   memset(dec, 0, sizeof(*dec));
   dec->fp= fp;
   dec->buf= buf;
   dec->buf_len= buf_len;
   mad_stream_init(&dec->stream);
   mad_frame_init(&dec->frame);
   mad_synth_init(&dec->synth);
   mad_stream_options(&dec->stream, 0);
   dec->stream.error= MAD_ERROR_BUFLEN;
   dec->buf_file_off= ftell(fp);
}

static void
mp3_decoder_finish(Mp3Decoder *dec) {
   mad_synth_finish(&dec->synth);
   mad_frame_finish(&dec->frame);
   mad_stream_finish(&dec->stream);
   memset(dec, 0, sizeof(*dec));
}

static int
mp3_decoder_next_frame(Mp3Decoder *dec, long *frame_off_out) {
   while (1) {
      if (dec->stream.error == MAD_ERROR_BUFLEN) {
         unsigned char *rp= dec->buf;
         int preserve= 0;

         if (dec->stream.next_frame) {
            preserve= (int)(dec->stream.bufend - dec->stream.next_frame);
            dec->buf_file_off += (long)(dec->stream.next_frame - dec->buf);
            memmove(dec->buf, dec->stream.next_frame, preserve);
            rp += preserve;
         } else {
            dec->buf_file_off= ftell(dec->fp);
         }

         {
            int cnt= (int)fread(rp, 1, dec->buf + dec->buf_len - rp, dec->fp);
            if (!cnt) {
               if (feof(dec->fp))
                  return 0;
               error("Read error on MP3 input stream:\n  %s", strerror(errno));
            }
            rp += cnt;
         }

         mad_stream_buffer(&dec->stream, (const unsigned char*)dec->buf, rp - dec->buf);
         dec->stream.error= 0;
      }

      if (mad_frame_decode(&dec->frame, &dec->stream)) {
         if (MAD_RECOVERABLE(dec->stream.error)) {
            warn("MAD recoverable error: %s", mad_stream_errorstr(&dec->stream));
            continue;
         }
         if (dec->stream.error == MAD_ERROR_BUFLEN)
            continue;
         error("Fatal error decoding MP3 stream: %s", mad_stream_errorstr(&dec->stream));
      }

      if (out_rate_def) {
         out_rate= dec->frame.header.samplerate;
         out_rate_def= 0;
      }
      if (frame_off_out)
         *frame_off_out= dec->buf_file_off + (long)(dec->stream.this_frame - dec->buf);
      mad_synth_frame(&dec->synth, &dec->frame);
      dec->synth_pos= 0;
      dec->synth_len= dec->synth.pcm.length;
      return dec->synth_len;
   }
}

static int
mp3_decoder_read_sample(Mp3Decoder *dec, int *l, int *r) {
   while (dec->synth_pos >= dec->synth_len) {
      if (!mp3_decoder_next_frame(dec, 0))
         return 0;
   }

   if (dec->synth.pcm.channels > 1) {
      *l= ROUND(dec->synth.pcm.samples[0][dec->synth_pos]);
      *r= ROUND(dec->synth.pcm.samples[1][dec->synth_pos]);
   } else {
      int v= ROUND(dec->synth.pcm.samples[0][dec->synth_pos]);
      *l= v;
      *r= v;
   }
   dec->synth_pos++;
   return 1;
}

static void
mp3_index_reset() {
   if (mp3_framev) free(mp3_framev);
   mp3_framev= 0;
   mp3_framec= 0;
   mp3_framecap= 0;
   mp3_datcnt= 0;
   mp3_datcnt0= 0;
   mp3_datbase= 0;
}

static void
mp3_index_append(long file_off, int sample_off, int sample_count) {
   if (mp3_framec >= mp3_framecap) {
      int ncap= mp3_framecap ? mp3_framecap * 2 : 1024;
      Mp3FrameIndex *tmp= (Mp3FrameIndex*)realloc(mp3_framev, ncap * sizeof(*tmp));
      if (!tmp)
         error("Out of memory expanding MP3 frame index");
      mp3_framev= tmp;
      mp3_framecap= ncap;
   }
   mp3_framev[mp3_framec].file_off= file_off;
   mp3_framev[mp3_framec].sample_off= sample_off;
   mp3_framev[mp3_framec].sample_count= sample_count;
   mp3_framec++;
}

static void
mp3_scan_build_index(FILE *fp) {
   Mp3Decoder scan;
   unsigned char *buf;
   long frame_off;
   int sample_off= 0;
   int sample_count;
   int rate= 0;

   mp3_index_reset();

   if (0 != fseek(fp, mp3_audio_start, SEEK_SET))
      error("Can't seek MP3 input stream for indexing: %s", strerror(errno));

   buf= (unsigned char*)Alloc(32768);
   mp3_decoder_reset(&scan, fp, buf, 32768);
   while ((sample_count= mp3_decoder_next_frame(&scan, &frame_off)) > 0) {
      if (!rate)
         rate= scan.frame.header.samplerate;
      else if (rate != scan.frame.header.samplerate)
         error("Variable sample-rate MP3 streams are not supported for SBAGEN_LOOPER");
      if (sample_off > INT_MAX - sample_count)
         error("MP3 stream too long for loop scheduler");
      mp3_index_append(frame_off, sample_off, sample_count);
      sample_off += sample_count;
   }
   mp3_decoder_finish(&scan);
   free(buf);

   if (!mp3_framec)
      error("MP3 stream appears to contain no PCM data");

   mp3_datrate= rate ? rate : 44100;
   mp3_datcnt0= sample_off;
   mp3_datcnt= sample_off;
}

static int
mp3_frame_find(int sample_off) {
   int lo= 0;
   int hi= mp3_framec - 1;
   int mid;

   while (lo <= hi) {
      mid= (lo + hi) / 2;
      if (mp3_framev[mid].sample_off <= sample_off) {
         if (mid + 1 >= mp3_framec || mp3_framev[mid + 1].sample_off > sample_off)
            return mid;
         lo= mid + 1;
      } else hi= mid - 1;
   }
   return 0;
}

static FILE *
mp3_reopen_stream() {
   const char *path;
   FILE *fp;

   if (!sbx_mix_active_input || !sbx_mix_active_input->path_hint || !sbx_mix_active_input->path_hint[0])
      error("MP3 SBAGEN_LOOPER requires a file path, not stdin/anonymous input");
   path= sbx_mix_active_input->path_hint;
   fp= MP3_FOPEN(path, "rb");
   if (!fp)
      error("Can't reopen MP3 mix input file: %s", path);
   if (0 != fseek(fp, mp3_audio_start, SEEK_SET))
      error("Can't seek reopened MP3 stream: %s", strerror(errno));
   return fp;
}

static int
mp3_zxrand_0_65536() {
   mp3_zxrand_seed= (1 + (int)mp3_zxrand_seed) * 75 % 65537 - 1;
   return mp3_zxrand_seed;
}

static int
mp3_zxrand_0(int mult) {
   long long tmp= mult;
   tmp *= mp3_zxrand_0_65536();
   tmp >>= 16;
   return (int)tmp;
}

static int
mp3_zxrand(int r0, int r1) {
   if (r1 <= r0) return r0;
   return r0 + mp3_zxrand_0(r1-r0);
}

static int
mp3_zxrandM(int def, char *fmt, ...) {
   va_list ap;
   int cnt= 0;
   int val;
   char *p;
   int olo, ohi;

   va_start(ap, fmt);
   olo= 0x80000000; ohi= 0x7FFFFFFF;
   for (p= fmt; *p; p++) {
      int lo= va_arg(ap, int);
      int hi= va_arg(ap, int);
      if (*p == 'o') { olo= lo; ohi= hi; }
      else if (*p == 'r') {
         if (lo < olo) lo= olo;
         if (hi > ohi) hi= ohi;
         if (hi-lo > 0) cnt += hi-lo;
      } else error("Bad mp3_zxrandM format: %s", fmt);
   }
   va_end(ap);

   if (!cnt) return def;
   val= mp3_zxrand_0(cnt);

   va_start(ap, fmt);
   olo= 0x80000000; ohi= 0x7FFFFFFF;
   for (p= fmt; *p; p++) {
      int lo= va_arg(ap, int);
      int hi= va_arg(ap, int);
      if (*p == 'o') { olo= lo; ohi= hi; }
      else if (*p == 'r') {
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
mp3_stream_seek(Mp3LoopStream *ss, int sample_off) {
   int idx;
   int skip;
   long dummy;
   FILE *fp;
   unsigned char *buf;
   int buf_len;

   if (sample_off < 0) sample_off= 0;
   if (sample_off >= mp3_datcnt0) sample_off= mp3_datcnt0 - 1;

   idx= mp3_frame_find(sample_off);
   if (0 != fseek(ss->dec.fp, mp3_framev[idx].file_off, SEEK_SET))
      error("UNEXPECTED: Can't reseek the looping MP3 stream: %s", strerror(errno));
   fp= ss->dec.fp;
   buf= ss->dec.buf;
   buf_len= ss->dec.buf_len;
   mp3_decoder_finish(&ss->dec);
   mp3_decoder_reset(&ss->dec, fp, buf, buf_len);
   if (!mp3_decoder_next_frame(&ss->dec, &dummy))
      error("UNEXPECTED: Failed to decode MP3 frame after seek");
   skip= sample_off - mp3_framev[idx].sample_off;
   if (skip < 0 || skip > ss->dec.synth_len)
      error("UNEXPECTED: MP3 frame index mismatch after seek");
   ss->dec.synth_pos= skip;
}

#define MP3_CNT_TO_END(ss) \
((ss->mode == 1) ? (ss->cnt_all + ss->cnt) : \
 (ss->mode == 2) ? (ss->cnt_all - mp3_fade_cnt + ss->cnt) : \
 (ss->mode == 3) ? (mp3_fade_cnt + ss->cnt) : \
 (ss->mode == 4) ? ss->cnt : 0)

static void
mp3_looper_setup(const char *looper) {
   int a;
   int intro= 0;
   int prev_flag= 0;
   int on= 1;

   mp3_zxrand_seed= 0xFFFF & time(NULL);

   mp3_datcnt= mp3_datcnt0;
   mp3_datbase= 0;
   mp3_seg0= mp3_seg1= mp3_datcnt;
   mp3_fade_cnt= mp3_datrate;
   mp3_ch2= 0;
   mp3_ch2_swap= 1;
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
       case 's': mp3_seg0= mp3_seg1= val * mp3_datrate; break;
       case 'S': mp3_seg1= val * mp3_datrate; break;
       case 'd': mp3_datbase= val * mp3_datrate; mp3_datcnt= mp3_datcnt0 - mp3_datbase; break;
       case 'D': mp3_datcnt= val * mp3_datrate - mp3_datbase; break;
       case 'f': mp3_fade_cnt= val * mp3_datrate; break;
       case 'c': mp3_ch2= val > 1.5; break;
       case 'w': mp3_ch2_swap= val > 0.5; break;
      }
   }

   if (mp3_fade_cnt < mp3_datrate/50) mp3_fade_cnt= mp3_datrate/50;
   if (mp3_datcnt + mp3_datbase > mp3_datcnt0) mp3_datcnt= mp3_datcnt0 - mp3_datbase;
   if (mp3_datcnt < 0)
      error("Source data range invalid in SBAGEN_LOOPER settings");
   if (mp3_datcnt <= 3 * mp3_fade_cnt)
      error("Length of source data 'd' too short for fade-length of %gs\n in SBAGEN_LOOPER settings",
            mp3_fade_cnt * 1.0 / mp3_datrate);
   if (mp3_seg0 > mp3_datcnt) mp3_seg0= mp3_datcnt;
   if (mp3_seg1 > mp3_datcnt) mp3_seg1= mp3_datcnt;
   if (mp3_seg0 > mp3_seg1) mp3_seg0= mp3_seg1;
   if (mp3_seg0 < 3 * mp3_fade_cnt) {
      mp3_seg0= 3 * mp3_fade_cnt;
      warn("SBAGEN_LOOPER segment size too short for fade-length of %gs; adjusted.",
           mp3_fade_cnt * 1.0 / mp3_datrate);
   }
   if (mp3_seg1 < mp3_seg0) mp3_seg1= mp3_seg0;

   mp3_intro_cnt= (intro && mp3_datbase > 0) ? mp3_datbase : 0;
   mp3_intro_first_seg= mp3_intro_cnt ? 1 : 0;
   if (intro && !mp3_intro_cnt)
      warn("SBAGEN_LOOPER intro requested, but d-start is not positive; ignoring intro");

   mp3_del_amp= 0xFFFFFFFFU/mp3_fade_cnt;
   if (mp3_del_amp * (uint)mp3_fade_cnt < 0xF0000000)
      error("Internal rounding error in calculating amplitude delta");
   if (mp3_ch2) mp3_del_amp >>= 1;

   for (a= 0; a<3; a++) {
      memset(&mp3_str[a], 0, sizeof(mp3_str[a]));
      mp3_str[a].dec.fp= mp3_reopen_stream();
      mp3_str[a].dec.buf= (unsigned char*)Alloc(32768);
      mp3_str[a].dec.buf_len= 32768;
      mp3_decoder_reset(&mp3_str[a].dec, mp3_str[a].dec.fp, mp3_str[a].dec.buf, mp3_str[a].dec.buf_len);
   }

   if (mp3_intro_cnt)
      mp3_stream_seek(&mp3_str[0], 0);
   else if (mp3_ch2)
      mp3_looper_sched2();
   else
      mp3_looper_sched();

   mp3_looper_active= 1;
   inbuf_start(mp3_looper_read, 256*1024);
}

static void
mp3_looper_term() {
   int a;

   for (a= 0; a<3; a++) {
      if (mp3_str[a].dec.fp) {
         FILE *fp= mp3_str[a].dec.fp;
         mp3_decoder_finish(&mp3_str[a].dec);
         fclose(fp);
         mp3_str[a].dec.fp= 0;
      }
      if (mp3_str[a].dec.buf) {
         free(mp3_str[a].dec.buf);
         mp3_str[a].dec.buf= 0;
      }
   }
   if (mp3_looper_embedded) {
      free(mp3_looper_embedded);
      mp3_looper_embedded= 0;
   }
   mp3_index_reset();
   mp3_looper_active= 0;
}

static int
mp3_looper_read_intro(int *dst, int dlen) {
   Mp3LoopStream *aa= &mp3_str[0];
   int out= 0;

   while (out < dlen) {
      int l, r;
      if (!mp3_decoder_read_sample(&aa->dec, &l, &r))
         break;
      *dst++ += l;
      *dst++ += r;
      out++;
   }
   return out;
}

static int
mp3_looper_read(int *dst, int dlen) {
   int *dst0= dst;
   int *dst1= dst + dlen;

   memset(dst0, 0, (char*)dst1 - (char*)dst0);

   while (dst0 < dst1) {
      int a;
      int len= (dst1 - dst0) / 2;
      int resched= 0;

      if (mp3_intro_cnt > 0) {
         int got;
         if (len > mp3_intro_cnt) len= mp3_intro_cnt;
         got= mp3_looper_read_intro(dst0, len);
         dst0 += got * 2;
         mp3_intro_cnt -= got;
         if (got < len) {
            warn("SBAGEN_LOOPER intro hit EOF before d-start; switching to looping");
            mp3_intro_cnt= 0;
         }
         if (!mp3_intro_cnt) {
            if (mp3_ch2)
               mp3_looper_sched2();
            else
               mp3_looper_sched();
         }
         continue;
      }

      for (a= 0; a<3; a++) {
         Mp3LoopStream *aa= &mp3_str[a];
         if (aa->mode && aa->cnt < len)
            len= aa->cnt;
      }

      for (a= 0; a<3; a++) {
         Mp3LoopStream *aa= &mp3_str[a];
         int cnt= len;
         int *dstp= dst0;

         if (!aa->mode) continue;

         if (aa->mode == 1) {
            aa->cnt -= cnt;
            cnt= 0;
         }

         while (cnt > 0) {
            int l, r;
            uint amp;
            if (aa->mode == 1)
               break;
            if (!mp3_decoder_read_sample(&aa->dec, &l, &r)) {
               warn("Hit EOF in looping MP3 stream, filling with zeros");
               l= 0;
               r= 0;
            }
            amp= (~aa->amp) >> 16;
            amp= (~(amp*amp)) >> 21;
            cnt--;
            if (aa->chan && mp3_ch2_swap) {
               *dstp++ += ((int)(r * amp)) >> 11;
               *dstp++ += ((int)(l * amp)) >> 11;
            } else {
               *dstp++ += ((int)(l * amp)) >> 11;
               *dstp++ += ((int)(r * amp)) >> 11;
            }
            aa->amp += aa->del;
            aa->cnt--;
         }

         if (!aa->cnt) switch (aa->mode) {
          case 1:
            aa->mode= 2;
            aa->cnt= mp3_fade_cnt;
            aa->del= mp3_del_amp;
            break;
          case 2:
            aa->mode= 3;
            aa->cnt= aa->cnt_all - 2 * mp3_fade_cnt;
            aa->del= 0;
            break;
          case 3:
            aa->mode= 4;
            aa->cnt= mp3_fade_cnt;
            aa->del= -mp3_del_amp;
            break;
          case 4:
            aa->mode= 0;
            resched= 1;
            break;
         }
      }

      dst0 += len * 2;
      if (resched) {
         if (mp3_ch2)
            mp3_looper_sched2();
         else
            mp3_looper_sched();
      }
   }

   return dlen;
}

static void
mp3_looper_sched() {
   while (1) {
      Mp3LoopStream *aa= &mp3_str[0];
      Mp3LoopStream *bb= &mp3_str[1];
      int cnt_all;

      if (aa->mode && bb->mode) break;
      if (!aa->mode && bb->mode) {
         Mp3LoopStream *tmp= aa; aa= bb; bb= tmp;
      }

      if (!bb->mode) {
         bb->chan= 0;
         bb->off= -1;
         bb->mode= 1;
         bb->amp= 0;
         bb->cnt= 0;

         if (!aa) {
            cnt_all= mp3_zxrand(mp3_seg0, mp3_seg1+1);
         } else {
            int end= MP3_CNT_TO_END(aa);
            cnt_all= mp3_zxrandM(-1, "orr",
                                 mp3_seg0, mp3_seg1+1,
                                 mp3_seg0, end-mp3_fade_cnt,
                                 end+mp3_fade_cnt, mp3_seg1+1);
            if (cnt_all < 0) {
               bb->cnt= end+mp3_fade_cnt - mp3_seg1;
               cnt_all= mp3_seg1;
            }
         }
         bb->cnt_all= cnt_all;
         if (bb->cnt < 0) bb->cnt= 0;

         if (!aa && mp3_intro_first_seg) {
            bb->off= 0;
            bb->mode= 3;
            bb->cnt= cnt_all - mp3_fade_cnt;
            bb->amp= 0xFFFFFFFFU;
            bb->del= 0;
            mp3_intro_first_seg= 0;
         }

         if (aa) {
            bb->off= mp3_zxrandM(-1, "rr",
                                 0, aa->off - cnt_all,
                                 aa->off + aa->cnt_all, mp3_datcnt - cnt_all);
         }
         if (bb->off < 0)
            bb->off= mp3_zxrand(0, mp3_datcnt - cnt_all);

         mp3_stream_seek(bb, mp3_datbase + bb->off);
         continue;
      }
   }
}

static void
mp3_looper_sched2() {
   while (1) {
      Mp3LoopStream *aa= &mp3_str[0];
      Mp3LoopStream *bb= &mp3_str[1];
      Mp3LoopStream *cc= &mp3_str[2];
      Mp3LoopStream *tmp;
      int cnt_all;

      if (aa->mode && bb->mode && cc->mode) break;

      if (!aa->mode && bb->mode) { tmp= aa; aa= bb; bb= tmp; }
      if (!aa->mode && cc->mode) { tmp= aa; aa= cc; cc= tmp; }
      if (!bb->mode && cc->mode) { tmp= bb; bb= cc; cc= tmp; }

      if (!bb->mode) {
         bb->chan= aa ? !aa->chan : 0;
         bb->off= -1;
         bb->mode= 1;
         bb->amp= 0;
         bb->cnt= 0;

         if (!aa) cnt_all= mp3_zxrand(mp3_seg0, mp3_seg1+1);
         else {
            int end= MP3_CNT_TO_END(aa);
            cnt_all= mp3_zxrandM(-1, "orr",
                                 mp3_seg0, mp3_seg1+1,
                                 mp3_seg0, end-mp3_fade_cnt,
                                 end+mp3_fade_cnt, mp3_seg1+1);
            if (cnt_all < 0) {
               bb->cnt= end+mp3_fade_cnt - mp3_seg1;
               cnt_all= mp3_seg1;
            }
         }
         bb->cnt_all= cnt_all;
         if (bb->cnt < 0) bb->cnt= 0;

         if (!aa && mp3_intro_first_seg) {
            bb->off= 0;
            bb->mode= 3;
            bb->cnt= cnt_all - mp3_fade_cnt;
            bb->amp= 0xFFFFFFFFU;
            bb->del= 0;
            mp3_intro_first_seg= 0;
         }

         if (aa) {
            bb->off= mp3_zxrandM(-1, "rr",
                                 0, aa->off - cnt_all,
                                 aa->off + aa->cnt_all, mp3_datcnt - cnt_all);
         }
         if (bb->off < 0)
            bb->off= mp3_zxrand(0, mp3_datcnt - cnt_all);

         mp3_stream_seek(bb, mp3_datbase + bb->off);
         continue;
      }

      if (aa->chan == bb->chan)
         error("UNEXPECTED: internal error, mp3_looper_sched2(), aa/bb on same chan");
      if (MP3_CNT_TO_END(aa) > MP3_CNT_TO_END(bb)) {
         tmp= aa; aa= bb; bb= tmp;
      }

      cc->chan= aa->chan;
      cc->cnt= MP3_CNT_TO_END(aa) - mp3_fade_cnt;
      cc->off= -1;
      cc->mode= 1;
      cc->amp= 0;

      {
         int end= MP3_CNT_TO_END(bb) - cc->cnt;
         cnt_all= mp3_zxrandM(-1, "orr",
                              mp3_seg0, mp3_seg1+1,
                              mp3_seg0, end-mp3_fade_cnt,
                              end+mp3_fade_cnt, mp3_seg1+1);
         if (cnt_all < 0) {
            if (end-mp3_fade_cnt > mp3_fade_cnt * 2)
               cnt_all= end-mp3_fade_cnt;
            else
               cnt_all= end+mp3_fade_cnt;
         }
      }
      cc->cnt_all= cnt_all;

      {
         int r0= aa->off;
         int r1= aa->off + aa->cnt_all;
         int r2= bb->off;
         int r3= bb->off + bb->cnt_all;
         if (r0 > r2) {
            int x;
            x= r0; r0= r2; r2= x;
            x= r1; r1= r3; r3= x;
         }
         cc->off= mp3_zxrandM(-1, "rrr",
                              0, r0 - cnt_all,
                              r1, r2 - cnt_all,
                              r3, mp3_datcnt - cnt_all);
      }
      if (cc->off < 0)
         cc->off= mp3_zxrand(0, mp3_datcnt - cnt_all);
      mp3_stream_seek(cc, mp3_datbase + cc->off);
   }
}

void
mp3_init() {
   const char *looper_override;

   mp3_looper_active= 0;
   mp3_audio_start= 0;
   mp3_looper_embedded= 0;
   mp3_index_reset();

   mp3_scan_id3_tag(mix_in, &mp3_looper_embedded, &mp3_audio_start);
   sbx_mix_input_set_embedded_looper(mp3_looper_embedded);
   looper_override= sbx_mix_input_looper_override();

   if ((looper_override && *looper_override) || (mp3_looper_embedded && *mp3_looper_embedded)) {
      if (!sbx_mix_active_input || !sbx_mix_active_input->path_hint || !sbx_mix_active_input->path_hint[0])
         error("MP3 SBAGEN_LOOPER requires a real file path, not stdin/anonymous input");
      mp3_scan_build_index(mix_in);
      mp3_looper_setup((looper_override && *looper_override) ? looper_override : mp3_looper_embedded);
      return;
   }

   if (0 != fseek(mix_in, mp3_audio_start, SEEK_SET))
      error("Can't seek MP3 input stream: %s", strerror(errno));

   mp3_norm_buf_len= 32768;
   mp3_norm_buf= (unsigned char*)Alloc(mp3_norm_buf_len);
   mp3_decoder_reset(&mp3_norm_dec, mix_in, mp3_norm_buf, mp3_norm_buf_len);
   inbuf_start(mp3_read, 256*1024);
}

void
mp3_term() {
   if (mp3_looper_active) {
      mp3_looper_term();
      return;
   }
   mp3_decoder_finish(&mp3_norm_dec);
   free(mp3_norm_buf);
   mp3_norm_buf= 0;
   if (mp3_looper_embedded) {
      free(mp3_looper_embedded);
      mp3_looper_embedded= 0;
   }
}

int
mp3_read(int *dst, int dlen) {
   int *dst0= dst;
   while (dlen > 0) {
      int l, r;
      if (!mp3_decoder_read_sample(&mp3_norm_dec, &l, &r))
         return dst - dst0;
      *dst++= l;
      *dst++= r;
      dlen -= 2;
   }
   return dst - dst0;
}

// END //
