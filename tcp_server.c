#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*--- 文字列strの長さを返す ---*/
unsigned str_length(const char str[])
{
    unsigned len = 0;
    while (str[len])
        len++;
    return (len);
}

/*--- 文字列を逆順に並べ替える ---*/
char *rev_string(char str[])
{
    int i;
    int len = str_length(str);
    for (i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len-i-1];
        str[len-i-1] = temp;
    }
    return str;
}

int httpserv(int connection_sock,char *inword)
{
  char inbuf[1024]; /* 受信文字列の格納バッファ */
  char outbuf[1024]; /* 返信文字列の格納バッファ */
  char *revword;
  
  /* HTTP server */
  /* receive the request from browser */
  memset(&inbuf,0,sizeof(inbuf));/* inbufの初期化 */
  if(recv(connection_sock,inbuf,sizeof(inbuf),0)<0){
    perror("receive ");
    return -1;
  };/* 受信内容の格納 */
  printf("%s\n",inbuf);/* 受信内容の表示 */
  revword= rev_string(inword);
  /* reply the HTML */
  memset(&outbuf,0,sizeof(outbuf)); /*outbufの初期化 */
  snprintf(outbuf,sizeof(outbuf),
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "\r\n"
    "<font color=red><h1>%s</h1></font>",revword);/*outbufへのmessage格納*/
  if(send(connection_sock,outbuf,(int)strlen(outbuf),0)<0){
    perror("send ");
    return -1;
  }
  return 0;
}


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
  int connection_sock;
  int port_num = 10020 + 21600;/* myid + 21600 */
  char port[10];
  snprintf(port,sizeof(port),"%d",port_num);
  
  /* コマンドライン引数は逆表示したい文字列のみ */
  if (argc != 2) {
    fprintf(stderr,"usage: %s <your favorite word> \n",argv[0]);
    exit(1);
  }

  /*socketを立て、bindし、listenしている*/
  sock = open_socket(port);
  if(sock<0){
    printf("fail to open socket\n");
    exit(1);
  }

  /* acceptする部分 */
  connection_sock=start_server(sock);
  if(connection_sock<0){
    printf("fail to start server\n");
    exit(1);
  }
  
  /* httpサーバ処理部分 */
  if(httpserv(connection_sock,argv[1])<0){
    printf("failed to connect with the browser...\n");
  } else{
    printf("successfully run!!\n");
  }

  close(sock);
  close(connection_sock);
  printf("finish!\n");
}