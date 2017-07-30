#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<dirent.h>
#include<pthread.h>
#include<alloca.h>
#include<sched.h>
#include"timer.h"

#define WORKER_THREAD_NUM (2)
#define MAX_QUEUE_NUM (1024)/*最大キュー*/

/*ファイルキュー（生産者・消費者キュー）*/
typedef struct _queue{
  char* fname[MAX_QUEUE_NUM];
  int flength[MAX_QUEUE_NUM];
  int rp, wp;
  int remain;
  pthread_mutex_t mutex;
  pthread_cond_t not_full;
  pthread_cond_t not_empty;
  bool done;
} queue_t;

/*検索結果を保持する構造体*/
typedef struct _result{
  int num;
  char* fname;
  pthread_mutex_t mutex; /*排他制御の為の制御変数*/
} result_t;

result_t result;

/*スレッド引数のための構造体*/
typedef struct _thread_arg{
  int cpuid;
  queue_t* q;
  char* srchstr;
  char* dirname;
  int filenum;
}thread_arg_t;

/*プロトタイプ宣言*/
void initqueue(queue_t* q);
void enqueue(queue_t* q, char* path, int size);
void master_func(thread_arg_t* arg);
int traverse_dir_thread(queue_t* q, char* dirname);
void worker_func(thread_arg_t* arg);
int traverse_file(char* filename, char* srchstr);
int traverse_buffer(char* a, char* b);
void print_result(thread_arg_t* arg);




#define END_MARK_FNAME  "///" /*キューの終端データ*/
#define END_MARK_FLENGTH 3    /*終端データの文字数*/



/*キューの初期化*/
void initqueue(queue_t* q){
  int i;
  q->rp = q->wp = q->remain = 0;

  for(i = 0; i<MAX_QUEUE_NUM; ++i){
    q->fname[i]=NULL;
  }
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty,NULL);
    return;
}

/*ファイルキューにファイルを挿入(エンキュー)*/
void enqueue(queue_t* q, char* path, int size){
  pthread_mutex_lock(&q->mutex);
  while(q->remain == MAX_QUEUE_NUM){
    pthread_cond_wait(&q->not_full,&q->mutex);
  }
  char** fname=(char**)&q->fname[q->wp];
  if (*fname != NULL) free(*fname);

  *fname = (char*)malloc(size);
  strcpy(*fname,path);
  q->flength[q->wp] = size;
  q->wp++;
  q->remain++;
  if (q->wp == MAX_QUEUE_NUM) q->wp =0;
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->mutex);
  return;
}

/*ファイルキューからファイルを取得する*/
void dequeue(queue_t* q, char** fname, int* flen){
  pthread_mutex_lock(&q->mutex);
  while(q->remain == 0)
    pthread_cond_wait(&q->not_empty, &q->mutex);
  *flen = q->flength[q->rp];
  if (*fname != NULL) free(*fname);
  *fname = (char*)malloc(*flen);
  strcpy(*fname, q->fname[q->rp]);
  q->rp++; q->remain--;
  if (q->rp == MAX_QUEUE_NUM) q->rp = 0;
  pthread_cond_signal(&q->not_full);
  pthread_mutex_unlock(&q->mutex);
  if (strcmp(*fname,"")==0) printf("rp=%d\n",q->rp-1);
  return;
}

/*main関数*/
int main(int argc, char* argv[]){
  int i;
  int thread_num = 1 + WORKER_THREAD_NUM;
  unsigned int t;
  unsigned int travdirtime;
  queue_t q;
  thread_arg_t targ[thread_num];
  pthread_t master;
  pthread_t worker[thread_num];
  int cpu_num;

  if (argc != 3){
    printf("Usage: 引数が間違ってますよ\n");
    return 0;
  }
  cpu_num = sysconf(_SC_NPROCESSORS_CONF);

  /*キューの初期化*/
  initqueue(&q);

  /*スレッド引数と結果構造体の初期化*/
  for(i=0; i<thread_num; ++i){
    targ[i].q = &q;
    targ[i].srchstr = argv[1];
    targ[i].dirname = argv[2];
    targ[i].filenum = 0;
    targ[i].cpuid = i%cpu_num;
  }
  result.fname = NULL;



  pthread_mutex_init(&result.mutex, NULL);

  start_timer(&t);

  /*スレッドの生成*/
  pthread_create(&master, NULL, (void*)master_func, (void*)&targ[0]);
  for(i=1;i<thread_num; ++i)
    pthread_create(&worker[i], NULL, (void*)worker_func,(void*)&targ[i]);

  /*スレッドの終了待ち*/
  for(i=1;i<thread_num; ++i){
    pthread_join(worker[i],NULL);
  }
  travdirtime = stop_timer(&t);
  print_timer(travdirtime);

  print_result(&targ[0]);
  for(i=1; i<thread_num; ++i){
      if((targ[i].q)->fname[i] != NULL)
          free((targ[i].q)->fname[i]);
      }
      if(result.fname != NULL)
          free(result.fname);

      return 0;
  }


void print_result(thread_arg_t* arg){
  printf("cpuid is %d\n",arg->cpuid);
  printf("srchstr is %s\n",arg->srchstr);
  printf("dirname is %s\n",arg->dirname);
  printf("filenum is %d\n",arg->filenum);
}


void master_func(thread_arg_t* arg){
  queue_t* q = arg->q;
  int i;
  arg->filenum = traverse_dir_thread(q, arg->dirname);

  /*エンキュー END_MARK*/
  for(i=0; i<WORKER_THREAD_NUM; ++i)
      enqueue(q,END_MARK_FNAME,END_MARK_FLENGTH);
  return;
}

int traverse_dir_thread(queue_t* q, char* dirname){
  static int cnt = 0;
  struct dirent* dent;
  DIR* dd = opendir(dirname);

  if (dd == NULL){
    printf("Could not open the directory %s\n",dirname);
    return 0;
  }

while ((dent = readdir(dd)) != NULL) {
  if (strncmp(dent->d_name, ".",2)==0) continue;
  if (strncmp(dent->d_name, "..",3)==0) continue;

  int size = strlen(dirname) + strlen(dent->d_name) + 2;
  {
    char* path=(char*)malloc(size);
    sprintf(path, "%s/%s",dirname,dent->d_name);

    struct stat fs;
    if (stat(path, &fs)<0){
        continue;
    }else{
      if (S_ISDIR(fs.st_mode))
          traverse_dir_thread(q,path);
    else if (S_ISREG(fs.st_mode)){
      enqueue(q,path,size);
      cnt++;
    }
    }
  }
}
closedir(dd);
return cnt;
}


void worker_func(thread_arg_t* arg){
  int flen;
  char* fname = NULL;
  queue_t* q = arg->q;
  char* srchstr = arg->srchstr;

  /*スレッドを動作させるプロセッサコアの割り当て*/
#ifdef __CPU_SET
    cpu_set_t mask;
    __CPU_ZERO(&mask);
    __CPU_SET(arg->cpuid, &mask);
    if (sched_setaffinity(0,sizeof(mask),&mask)==-1)
        printf("WARNING: faild to set CPU affinity...\n");
#endif

  char* my_result_fname;
  int my_result_num = 0;
  int my_result_len = 0;
  while(1){
    int n;
  /*キューからファイル情報を取り出す*/
    dequeue(q,&fname,&flen);
  /*終了判定*/
    if(strncmp(fname, END_MARK_FNAME,END_MARK_FLENGTH +1)==0){
      break;
    }
  /*ファイルの中の文字列検索*/
  n = traverse_file(fname,srchstr);

  /*結果情報の更新*/
  if (n>my_result_num){
    my_result_num =n;
    my_result_len=flen;
    my_result_fname=(char*)alloca(flen);
    strcpy(my_result_fname,fname);
  }
  }
  /*結果情報の更新*/
  pthread_mutex_lock(&result.mutex);
  if (my_result_num>result.num){
    result.num = my_result_num;
   if(result.fname != NULL) free(result.fname);
    result.fname = (char*)malloc(my_result_len);
    strcpy(result.fname, my_result_fname);
  }
  pthread_mutex_unlock(&result.mutex);

  return;
}

int traverse_file(char* filename, char* srchstr){
  char buf[1024];
  int n = 0;
  int sumn = 0;
  unsigned int t;

  /*ファイルハンドルの取得*/
  FILE* fd;
  if ((fd = fopen(filename, "r"))==NULL){
    printf("file %s not found.\n",filename);
    return 0;
  }

/*文字列の読み込み*/
while (fgets(buf, 1024,fd) !=NULL){
    /*文字列検索*/
    n = traverse_buffer(buf, srchstr);
    sumn += n;
  }

  fclose(fd);
  return sumn;
}

int traverse_buffer(char* a, char* b){
  int i, j;
  int lena, lenb;
  int n = 0;

  /*文字列の取得*/
  lena = strlen(a);
  lenb = strlen(b);

  /*含まれる検索文字列をカウント*/
  for(i = 0, j = 0; i< lena; ++i){
    if (strncmp(&a[i], &b[j], 1)==0){
      if(j == (lenb-1)){
        n++;
        j=0;
      }else{
        j++;
      }
    }else{
      j=0;
    }
    }
  return n;
  }
