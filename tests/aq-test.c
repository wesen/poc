
#ifdef AQ1_TEST
#include <stdio.h>

int main(int argc, char *argv[]) {
  mp3_file_t  file;
  mp3_frame_t frame;
  aq_t        qin;

  char *f;
  if (!(f = *++argv)) {
    fprintf(stderr, "Usage: aq1 mp3file\n");
    return 1;
  }

  aq_init(&qin);

  if (!mp3_open_read(&file, f)) {
    fprintf(stderr, "Could not open mp3 file: %s\n", f);
    return 1;
  }

  while (mp3_next_frame(&file, &frame) > 0) {
    if (!aq_add_frame(&qin, &frame)) {
      printf("could not generate an ADU\n");
    } else {
      printf("could generate an ADU\n");
    }
    fgetc(stdin);
  }

  file_close(&file);

  aq_destroy(&qin);

  return 0;
}

#endif /* AQ1_TEST */

#ifdef AQ2_TEST
#include <stdio.h>
unsigned long cksum(unsigned char *buf, int cnt) {
  unsigned long res = 0;

  int i;
  for (i = 0; i < cnt; i++)
    res += buf[i];

  return res;
}

int main(int argc, char *argv[]) {
  char *f[2];

  if (!(f[0] = *++argv) || !(f[1] = *++argv)) {
    fprintf(stderr, "Usage: mp3-write mp3in mp3out\n");
    return 1;
  }

  file_t in;
  if (!file_open_read(&in, f[0])) {
    fprintf(stderr, "Could not open mp3 file for read: %s\n", f[0]);
    return 1;
  }

  file_t out;
  if (!file_open_write(&out, f[1])) {
    fprintf(stderr, "Could not open mp3 file for write: %s\n", f[1]);
    file_close(&in);
    return 1;
  }

  aq_t qin, qout;
  aq_init(&qin);
  aq_init(&qout);

  mp3_frame_t frame;
  while (mp3_next_frame(&in, &frame) > 0) {
    static int cin = 0;
    printf("%d in frame_size %ld, backptr %d, adu_size %ld, cksum %ld\n",
           cin++,
           frame.frame_data_size, frame.si.main_data_end, frame.adu_size,
           cksum(frame.raw, frame.frame_size));

    if (aq_add_frame(&qin, &frame)) {
      adu_t *adu = aq_get_adu(&qin);
      assert(adu != NULL);

      static int count = 0;
      if (count > 2000)
        break;

      if ((count++ % 25) <= 10) {
        free(adu);
        continue;
      }

      if (aq_add_adu(&qout, adu)) {
        mp3_frame_t *frame_out = aq_get_frame(&qout);
        assert(frame_out != NULL);

        static int cout = 0;

        memset(frame_out->raw, 0, 4 + frame_out->si_size);
        if (!mp3_fill_hdr(frame_out) ||
            !mp3_fill_si(frame_out) ||
            (mp3_write_frame(&out, frame_out) <= 0)) {
          fprintf(stderr, "Could not write frame\n");
          file_close(&in);
          file_close(&out);
          return 1;
        }

        printf("%d out frame_size %ld, backptr %d, adu_size %ld, cksum %ld\n",
               cout++,
               frame_out->frame_data_size,
               frame_out->si.main_data_end,
               frame_out->adu_size,
               cksum(frame_out->raw, frame_out->frame_size));

        free(frame_out);
      }

      free(adu);
    }

    //    fgetc(stdin);
  }

  file_close(&in);
  file_close(&out);

  aq_destroy(&qin);
  aq_destroy(&qout);

  return 0;
}

#endif /* AQ2_TEST */

