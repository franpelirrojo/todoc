#include <stdio.h>
#include <time.h>

int istoday(time_t time1) {
  time_t today_s;
  struct tm today_tm; 
  struct tm time1_tm; 

  if (time(&today_s) == (time_t)-1) {
    return -1;
  }
  today_tm = *localtime(&today_s);
  time1_tm = *localtime(&time1);
  
  if (today_tm.tm_year != time1_tm.tm_year) {
    return 1;
  }
  if (today_tm.tm_mon != time1_tm.tm_mon) {
    return 1;
  }
  if (today_tm.tm_wday != time1_tm.tm_wday) {
    return 1;
  }

  return 0; 
}

/*
 *año > mes > día
 *0 = (time1) --> (time2)
 *1 = (time1) == (time2)
 *2 = (time2) --> (time1)
 * */
int datecmp(time_t time1, time_t time2) {
  struct tm time1_tm = *localtime(&time1);
  struct tm time2_tm = *localtime(&time2);

  if (time1_tm.tm_year > time2_tm.tm_year) {
    return 2;
  } 
  if (time1_tm.tm_year < time2_tm.tm_year) {
    return 0;
  }

  if (time1_tm.tm_mon > time2_tm.tm_mon) {
    return 2;
  } 
  if (time1_tm.tm_mon < time2_tm.tm_mon) {
    return 0;
  }

  if (time1_tm.tm_mday > time2_tm.tm_mday) {
    return 2;
  } 
  if (time1_tm.tm_mday < time2_tm.tm_mday) {
    return 0;
  }

  return 1;
}
