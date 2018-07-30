#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>


/* httpレスポンス用の送信関数 */
void send_msg(int fd, char *msg) {
  int len;
  len = strlen(msg);
  if ( send(fd, msg, len,0) != len ){
    fprintf(stderr, "error: writing.");
  }
}

int httpserv(int sockfd) {
  char *document_root = "./http";
  char *root_file = "/index.html";

  int len;
  int read_fd;
  char inbuf[1024];/* 受信リクエスト格納バッファ */
  char outbuf[1024];/* 返信リクエスト格納うバッファ */
  char meth_name[16];/* httpメソッド名を格納 */
  char uri_addr[256];/* リクエストされたアドレスを格納 */
  char http_ver[64];/* httpのバージョンを格納 */
  char uri_file[256];/* 返信ファイルのアドレス格納 */

  int return_num = 0;

  printf("start http processing!\n");
  /* リクエストの取得 */
  memset(&inbuf,0,sizeof(inbuf));/* 初期化 */
  printf("initialie inbuf!\n");
  if (recv(sockfd, inbuf, sizeof(inbuf),0) < 0 ) {
    printf("something went wrong...\n");
    perror("receive ");
    fprintf(stderr, "error: reading a request.\n");
    return_num=-1;
  }else {
    printf("%s\n",inbuf);/* 受信内容の表示 */
    sscanf(inbuf, "%s %s %s", meth_name, uri_addr, http_ver);/* 文字列から、部分文字列を取得 */
    /* 現状はgetにのみ対応 */
    if (strcmp(meth_name, "GET") != 0) {
      send_msg(sockfd, "501 Not Implemented. Only 'Get' request is permitted.");
      printf("%s\n",meth_name);
    }else {
      //if(strcmp(uri_addr,"/")==0||strcmp(uri_addr,"favicon.ico")==0||strcmp(uri_addr,"robots.txt")==0){
      if(strcmp(uri_addr,"/")==0){
        snprintf(uri_file,sizeof(uri_file),"%s%s",document_root,root_file);
      }else{
        snprintf(uri_file,sizeof(uri_file),"%s%s",document_root,uri_addr);/* ドキュメントルート以下のファイルから、リクエストされたものを探す */
      }
      printf("%s\n",uri_file);
      if ((read_fd = open(uri_file, O_RDONLY, 0666)) == -1) {
        send_msg(sockfd, "404 Not Found");
      }else {
        send_msg(sockfd, "HTTP/1.0 200 OK\r\n");
        send_msg(sockfd, "text/html\r\n");
        send_msg(sockfd, "\r\n");
        while((len = read(read_fd, inbuf, sizeof(inbuf))) > 0){
          if (send(sockfd, inbuf, len,0) != len) {
            fprintf(stderr, "error: writing a response.\n");
            return_num=-1;
            break;
          }
        }
        close(read_fd);
      }
    }
  }
  return return_num;
}

/* PIv4,IPv6ともに接続するための部分。リストを作成し、順に接続していって成功したらやめる、って感じかな。 */
int open_socket(char *port){
  
  int sock; /* ソケットディスクリプタ */
  struct addrinfo hints; /* getaddrinfoのヒント情報 */
  struct addrinfo *serv0; /* addrinfoリストの先頭の要素*/
  struct addrinfo *serv; /* 処理中のaddrinfoリストの要素 */
  int gair; /* 名前解決の結果 getaddrinfo result */
  int yes = 1;
  int listen_wait = 5;

  /* hintに情報格納 */
  memset(&hints, 0, sizeof(hints));/* ヒント情報の初期化 */
  hints.ai_family = AF_UNSPEC;     /* プロトコルを指定しない */
  hints.ai_flags = AI_PASSIVE;     /* wildcard addressを挿*/
  hints.ai_socktype = SOCK_STREAM;/* ストリーム型(TCP)による通信を指定 */
  
  /* addrinfoリスト(serv0)を取得 */
  gair = getaddrinfo(NULL,port,&hints,&serv0);
  /* errorが発生した場合はエラー文を表示し終了 */
  if (gair) {
    fprintf(stderr, "%s", gai_strerror(gair));
    exit(1);
  }
  
  /* addrinfoリストの要素を先頭から接続できるまで順に試行する */
  for (serv = serv0; serv; serv = serv->ai_next) {
    /* ソケットの生成 */
    sock=socket(serv->ai_family,serv->ai_socktype,serv->ai_protocol);
    if (sock < 0){
      perror("sock ");
      continue; 
    }
    /*同一ポート番号にてIPv4 and IPv6を重ねてbindすることが可能となる*/
    setsockopt(sock,IPPROTO_IPV6,IPV6_V6ONLY,&yes,sizeof(yes));
    /* SO_REUSEADDRを利用し、TIME_WAIT状態でもサーバプログラムを再起動可能とする */
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char *)&yes,sizeof(yes));
    
    /* ポートとプロセスを対応付け */
    if(bind(sock,serv->ai_addr,serv->ai_addrlen)!=0){
      perror("bind ");
      close(sock);
      sock=-1;
      continue; 
    }
    
    /* 待機開始 */
    if(listen(sock,listen_wait)!=0){
      perror("listen");
      close(sock);
      sock=-1;
      continue;
    };
    
    /* ここまで来たら成功 */
    break;
  }
  freeaddrinfo(serv0);
  return sock;
}

/* acceptをして、その結果を格納した変数を返す */
int start_server(int sock){

  struct sockaddr_storage client;
  socklen_t clilen;
  int connection_sock;

  clilen=sizeof(client);
  connection_sock=accept(sock,(struct sockaddr *)&client,&clilen);
  if(connection_sock<0){
    perror("accept ");
  } 
  return connection_sock;
}

int main(int argc,char *argv[])
{
  int sock; /* ソケットディスクリプタ */
  int new_sock;
  int port_num = 10020 + 21600;/* myid + 21600 */
  char port[10];
  snprintf(port,sizeof(port),"%d",port_num);
  
  printf("start http server!\n");

  /*socketを立て、bindし、listenしている*/
  sock = open_socket(port);
  if(sock<0){
    printf("fail to open socket\n");
    exit(1);
  }

  printf("waiting....\n");
  /* 無限ループで対応し続ける */
  while(1){
    /* acceptする部分 */
    new_sock=start_server(sock);
    if(new_sock<0){
      printf("fail to start server\n");
      exit(1);
    }
    printf("successfully connected to the port!\n");
    
    /* httpサーバ処理部分 */
    if(httpserv(new_sock)<0){
      printf("something went wrong when http processing\n");
    }else{
      printf("successfully responsed to the http request\n");  
    }
    
    close(new_sock);
  }
  close(sock);
  printf("finish!\n");
}