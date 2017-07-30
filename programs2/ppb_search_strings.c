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

          if(strcmp(dent->d_name,srchstr)==0){
                printf("find:%s\n",path);
                return;
              }

        }
    }
  }
  closedir(dd);
  return;
}
