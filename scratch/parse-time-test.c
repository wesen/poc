#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int parse_time(const char *str, unsigned long *time) {
  /* strtok madness */
  char strbuf[256];
  strncpy(strbuf, str, sizeof(strbuf));
  strbuf[sizeof(strbuf) - 1] = '\0';
  
  *time = 0;
  
  char buf[16];

  char *first = strtok(strbuf, ":");
  char *endptr = NULL;
  if (first == NULL)
    return -1;
  strncpy(buf, first, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  unsigned long minutes = strtoul(buf, &endptr, 10);
  if ((*endptr != '\0') || (endptr == buf))
    return -1;
  *time += minutes;
  *time *= 60;

  char *second = strtok(NULL, ":");
  if (second == NULL)
    return -1;
  strncpy(buf, second, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  unsigned long seconds = strtoul(buf, &endptr, 10);
  if ((seconds >= 60) || (*endptr != '\0') || (endptr == buf))
    return -1;
  *time += seconds;
  *time *= 1000;

  char *third = strtok(NULL, ":");
  if (third == NULL)
    return 0;
  strncpy(buf, third, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  unsigned long ms = strtoul(buf, &endptr, 10);
  if ((ms >= 1000) || (*endptr != '\0') || (endptr == buf))
    return -1;
  *time += ms;

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
  test_parse_time("12:23:42", ((12 * 60) + 23) * 1000 + 42);
  test_parse_time("12:23", ((12 * 60) + 23) * 1000 + 0);
  test_parse_time("123:23", ((123 * 60) + 23) * 1000 + 0);
  test_parse_time("123:23:999", ((123 * 60) + 23) * 1000 + 999);
  test_parse_time("123:61:999", ((123 * 60) + 61) * 1000 + 999);
  test_parse_time("123", ((123 * 60) + 0) * 1000 + 0);
  test_parse_time("123::59:999", ((123 * 60) + 59) * 1000 + 999);
  test_parse_time("123:61:999", ((123 * 60) + 61) * 1000 + 999);
  return 0;
}
