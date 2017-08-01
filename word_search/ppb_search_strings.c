#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>
#include<dirent.h>
#include"timer.h"

/*プロトタイプ宣言*/
void traverse_dir(char* dirname, char* srchstr, int* filenum);
int traverse_file(char* filename, char* srchstr);
int traverse_buffer(char* a, char* b);


typedef struct _result{
  int num;
  char* fname;
} result_t;

result_t result;

/*計測用の変数*/
static unsigned int travfiletime = 0;
static unsigned int travbufftime = 0;

/*main関数*/
int main(int argc, char const *argv[]) {
  int filenum;
  char* dirname;
  char* srchstr;
  unsigned int t, travdirtime;

  if (argc != 3){
    printf("Usage: 引数が間違ってますよ\n");
    return 0;
  }

  filenum =0;
  /*コマンドライン引数の取得*/
  srchstr = (char*)argv[1];/*検索文字列*/
  dirname = (char*)argv[2];/*検索ディレクトリ*/

  result.num =0;
  result.fname = NULL;

  /*ファイル検索処理*/
  start_timer(&t);
  traverse_dir(dirname, srchstr, &filenum);
  travdirtime = stop_timer(&t);

  print_timer(travdirtime);


  if(result.fname != NULL) free(result.fname);

    return 0;
}

void traverse_dir(char* dirname, char* srchstr, int* filenum){
  struct dirent* dent;

  /*ディレクトリハンドルの取得*/
  DIR* dd = opendir(dirname);
  unsigned int t;

  if (dd == NULL){
    printf("Could not open the directory %s\n",dirname);
    return;
  }

  /*ディレクトリ内のエントリを取得*/
  while ((dent = readdir(dd)) != NULL){

    /*不要なエントリの処理*/
    if (strncmp(dent->d_name, ".",2)==0) continue;
    if (strncmp(dent->d_name, "..",3)==0) continue;

    /*フルパスの生成*/
    int size = strlen(dirname)+strlen(dent->d_name)+2;
    char* path= (char*)malloc(size);
    sprintf(path, "%s/%s", dirname, dent->d_name);

    /*ファイル属性の確認*/
    struct stat fs;
    if (stat(path, &fs) < 0)
        continue;
    else{
      if (S_ISDIR(fs.st_mode))
          /*ファイルエントリはディレクトリ*/
          traverse_dir(path, srchstr, filenum);
        else if (S_ISREG(fs.st_mode)) {
          /*エントリはファイルである*/

          /*ファイル中の文字列を検索*/
          int n = traverse_file(path, srchstr);

          /*結果情報の更新*/
          if (n > result.num){
            result.num = n;
            if (result.fname != NULL) free(result.fname);
            result.fname = (char*)malloc(size);
            strcpy(result.fname,path);
          }
          (*filenum)++;
        }
    }
  }
  closedir(dd);
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
