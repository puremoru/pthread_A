#ifndef _H_TIMER
#define _H_TIMER

#include<stdio.h>
#include<sys/time.h>

/*gettimeval: osが管理する時刻を取得*/
inline
unsigned long long gettimeval(void){
  struct timeval tv;
  struct timezone tz;
  gettimeofday(&tv, &tz);
  return((unsigned long long)tv.tv_sec)*1000000+tv.tv_usec;
}

/*start_timer: 時間計測の開始*/
inline
void start_timer(unsigned int *startt){
  *startt = (unsigned int)gettimeval();
  return;
}

/*stop_timer:　時間計測の終了*/
inline
unsigned int stop_timer(unsigned int *startt){
  unsigned int stopt = (unsigned int)gettimeval();
  return (stopt>=*startt)?(stopt-*startt):(stopt);
}

/*print_timer:時間計測結果の表示*/
#define print_timer(te) {printf("time of $s:%f[msec]\n",#te , te*1.0e-3);}

#endif /*_H_TIMER*/
