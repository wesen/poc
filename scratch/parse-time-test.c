#include <string.h>
#include <stdlib.h>
#include <stdio.h>


static int parse_number(const char *str, unsigned long *result) {
  char buf[16];
  char *endptr = NULL;
  strncpy(buf, str, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  *result = strtoul(buf, &endptr, 10);
  if ((*endptr != '\0') || (endptr == buf))
    return -1;
  return 0;
}

static int parse_time(const char *str, unsigned long *time) {
  /* strtok madness */
  char strbuf[256];
  strncpy(strbuf, str, sizeof(strbuf));
  strbuf[sizeof(strbuf) - 1] = '\0';
  
  *time = 0;
  
  char *token, *token2;
  unsigned long numbers[4];
  unsigned long cnt = 0;

  token = strtok(strbuf, "+:");
  if ((token == NULL) || (parse_number(token, &numbers[cnt++]) < 0))
    return -1;
  token = strtok(NULL, "+:");
  if ((token == NULL) || (parse_number(token, &numbers[cnt++]) < 0))
    return -1;
  token = strtok(NULL, "+:");
  if (token && (parse_number(token, &numbers[cnt++]) < 0))
    return -1;
  token = strtok(NULL, "+:");
  if (token && (parse_number(token, &numbers[cnt++]) < 0))
    return -1;

  int mspresent = (strchr(str, '+') != NULL);
  unsigned long hours = 0, minutes = 0, seconds = 0, ms = 0;
  switch (cnt) {
  case 2:
    if (mspresent)
      return -1;
    minutes = numbers[0];
    seconds = numbers[1];
    break;

  case 3:
    if (mspresent) {
      minutes = numbers[0];
      seconds = numbers[1];
      ms = numbers[2];
    } else {
      hours = numbers[0];
      minutes = numbers[1];
      seconds = numbers[2];
    }
    break;

  case 4:
    hours = numbers[0];
    minutes = numbers[1];
    seconds = numbers[2];
    ms = numbers[3];
    break;

  default:
    return -1;
  }

  if ((minutes >= 60) || (seconds >= 60) || (ms >= 1000))
    return -1;

  *time = (((hours * 60) + minutes) * 60 + seconds) * 1000 + ms;

  return 0;
}
void test_parse_time(const char *str, unsigned long result) {
  unsigned long time;
  printf("\n");
  int ret = parse_time(str, &time);
  if (ret < 0) {
    printf("Error parsing %s\n", str);
  } else {
    printf("Parsed %s, result: %lu == %lu\n", str, time, result);
  }
}

int main(void) {
  test_parse_time("12:23:42", (((12 * 60) + 23) * 60 + 42) * 1000);
  test_parse_time("12:23", ((12 * 60) + 23) * 1000);
  test_parse_time("2:03:23+200", (((2 * 60) + 03) * 60 + 23) * 1000 + 200);
  test_parse_time("03:23+200", (((0 * 60) + 03) * 60 + 23) * 1000 + 200);
  test_parse_time("23:61+999", ((123 * 60) + 61) * 1000 + 999);
  return 0;
}
