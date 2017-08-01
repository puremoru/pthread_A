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

#define WORKER_THREAD_NUM (3)
#define MAX_QUEUE_NUM (1024)/*最大キュー*/

/*ファイルキュー（生産者・消費者キュー）*/
typedef struct _queue{
  char* fname[MAX_QUEUE_NUM];
  char* dname[MAX_QUEUE_NUM];
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
  char* keyword;
  int filenum;
}thread_arg_t;

/*プロトタイプ宣言*/
void initqueue(queue_t* q);
void enqueue(queue_t* q, char* path, char* dpath,int size);
void dequeue(queue_t* q, char** fname, char** dname,int* flen);
void master_func(thread_arg_t* arg);
int traverse_dir_thread(queue_t* q, char* dirname);
void worker_func(thread_arg_t* arg);
int traverse_file(char* filename, char* keyword);
int traverse_buffer(char* a, char* b);

void worker_func2(thread_arg_t* arg);



#define END_MARK_FNAME  "///" /*キューの終端データ*/
#define END_MARK_FLENGTH 6    /*終端データの文字数*/


/*キューの初期化*/
void initqueue(queue_t* q){
  int i;
  q->rp = q->wp = q->remain = 0;

  for(i = 0; i<MAX_QUEUE_NUM; ++i){
    q->fname[i]=NULL;
    q->dname[i]=NULL;
  }
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty,NULL);
    return;
}

/*ファイルキューにファイルを挿入(エンキュー)*/
void enqueue(queue_t* q, char* path, char* dpath,int size){
  pthread_mutex_lock(&q->mutex);
  while(q->remain == MAX_QUEUE_NUM){
    pthread_cond_wait(&q->not_full,&q->mutex);
  }
  char** fname=(char**)&q->fname[q->wp];
  if (*fname != NULL) free(*fname);
  *fname = (char*)malloc(size);
  strcpy(*fname,path);

  char** dname=(char**)&q->dname[q->wp];
  if (*dname != NULL) free(*dname);
  *dname = (char*)malloc(size);
  strcpy(*dname,dpath);


  q->flength[q->wp] = size;
  q->wp++;
  q->remain++;
  if (q->wp == MAX_QUEUE_NUM) q->wp =0;
  pthread_cond_signal(&q->not_empty);
  pthread_mutex_unlock(&q->mutex);
  return;
}

/*ファイルキューからファイルを取得する*/
void dequeue(queue_t* q, char** fname, char** dname,int* flen){
  pthread_mutex_lock(&q->mutex);
  while(q->remain == 0)
    pthread_cond_wait(&q->not_empty, &q->mutex);

  *flen = q->flength[q->rp];
  if (*fname != NULL) free(*fname);
  *fname = (char*)malloc(*flen);
  strcpy(*fname, q->fname[q->rp]);

  if (*dname != NULL) free(*dname);
  *dname = (char*)malloc(*flen);
  strcpy(*dname, q->dname[q->rp]);

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

  if (argc != 4){
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
    targ[i].keyword = argv[3];
    targ[i].filenum = 0;
    targ[i].cpuid = i%cpu_num;
  }
  result.fname = NULL;



  pthread_mutex_init(&result.mutex, NULL);

  start_timer(&t);

  /*スレッドの生成*/
  pthread_create(&master, NULL, (void*)master_func, (void*)&targ[0]);

  pthread_create(&worker[1], NULL, (void*)worker_func2,(void*)&targ[1]);

  for(i=2;i<thread_num; ++i)
    pthread_create(&worker[i], NULL, (void*)worker_func,(void*)&targ[i]);

  /*スレッドの終了待ち*/
  for(i=1;i<thread_num; ++i){
    pthread_join(worker[i],NULL);
    pthread_join(worker[1],NULL);
  }

  travdirtime = stop_timer(&t);
  print_timer(travdirtime);

  for(i=1; i<thread_num; ++i){
      if((targ[i].q)->fname[i] != NULL)
          free((targ[i].q)->fname[i]);
      }
      if(result.fname != NULL)
          free(result.fname);

      return 0;
  }


void master_func(thread_arg_t* arg){
  queue_t* q = arg->q;
  int i;
  arg->filenum = traverse_dir_thread(q, arg->dirname);

  /*エンキュー END_MARK*/
  for(i=0; i<WORKER_THREAD_NUM; ++i)
      enqueue(q,END_MARK_FNAME,END_MARK_FNAME,END_MARK_FLENGTH);
  return;
}


void worker_func2(thread_arg_t* arg){
  queue_t* q = arg->q;
  int i;
  arg->filenum = traverse_dir_thread(q, arg->dirname);

  /*エンキュー END_MARK*/
  for(i=0; i<WORKER_THREAD_NUM; ++i)
      enqueue(q,END_MARK_FNAME,END_MARK_FNAME,END_MARK_FLENGTH);
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
      enqueue(q,path,dent->d_name,size);
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
  char* dname = NULL;
  char* keyword = arg->keyword;
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


  while(1){
    int f;
  /*キューからファイル情報を取り出す*/
    dequeue(q,&fname,&dname,&flen);
  /*終了判定*/
    if(strcmp(fname,END_MARK_FNAME)==0){
      break;
    }
  //printf("%s,%s,%s,%s\n",fname,dname,keyword,srchstr);
  if(strcmp(dname,srchstr)==0){
        f =traverse_file(fname,keyword);
        if(f!=0){
         printf("find:%s\n",fname);
       }
  }


  }
  return;
}


int traverse_file(char* filename, char* keyword){
  char buf[1024];
  char* s;
  int sumn = 0;
  int n=0;

  unsigned int t;

  /*ファイルハンドルの取得*/
  FILE* fd;
  if ((fd = fopen(filename, "r"))==NULL){
    printf("file %s not found.\n",filename);
    return 0;
  }

  /*文字列の読み込み*/
while (fgets(buf, 1024,fd) !=NULL){
    n = traverse_buffer(buf,keyword);
    if(n==1){
    break;
  }
  }
  fclose(fd);
  return n;
}

int traverse_buffer(char* a, char* b){
  int i, j;
  int lena, lenb;
  int n = 0;

  /*文字列の取得*/
  lena = strlen(a);
  lenb = strlen(b);

  /*含まれる検索文字列をカウント*/
  if(strstr(a,b)!=NULL){
  return 1;
}else{
  return 0;
}
  }
