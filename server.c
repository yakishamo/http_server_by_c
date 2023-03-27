#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define SERVER_ADDR "150.69.229.253"
#define SERVER_PORT 8080
#define SIZE (5*1024)

int httpServer(int);
int recvRequestMessage(int, char*, unsigned int);
int parseRequestMessage(char*, char*, char*);
int getProcessing(char*, char*);
int createResponseMessage(char*, int, char*, char*, unsigned int);
int sendResponseMessage(int, char*, unsigned int);
unsigned int getFileSize(const char*);

//ファイルサイズを取得する
unsigned int getFileSize(const char *path) {
	int size, read_size;
	char read_buf[SIZE];
	FILE *f;

	f = fopen(path, "rb");
	if (f == NULL) {
		return 0;
	}

	size = 0;
	do {
		read_size = fread(read_buf, 1, SIZE, f);
		size += read_size;
	} while(read_size != 0);

	fclose(f);
	return size;
}

/* リクエストメッセージを送信する。
 * sock : 接続済みのソケット
 * request_message : リクエストメッセージを格納するバッファへのアドレス
 * buf_size : そのバッファのサイズ
 * 戻り値 : 受信したデータサイズ（バイト）
 */
int recvRequestMessage(int sock, char *request_message, unsigned int buf_size) {
	int recv_size;
	recv_size = recv(sock, request_message, buf_size, 0);
	return recv_size;
}

/* リクエストメッセージを解析する（リクエスト行のみ）
 * method : メソッドを格納するバッファへのアドレス
 * target : リクエストターゲットを格納するバッファへのアドレス
 * request_message : 解析するリクエストメッセージが格納されたバッファへのアドレス
 * 戻り値 : 成功は0、失敗は-1
 */
int parseRequestMessage(char *method, char *target, char *request_message) {
	char *line;
	char *tmp_method;
	char *tmp_target;

	//リクエストメッセージの１行目のみ取得
	line = strtok(request_message, "\n");

	//" "までの文字列を取得しmethodにコピー
	tmp_method = strtok(line, " ");
	if(tmp_method == NULL) {
		printf("get method error\n");
		return -1;
	}
	strcpy(method, tmp_method);

	//次の" "までの文字列を取得しtargetにコピー
	tmp_target = strtok(NULL, " ");
	if(tmp_target == NULL) {
		printf("get target error\n");
		return -1;
	}
	strcpy(target, tmp_target);

	return 0;
}

/* リクエストに対する処理を行う（GETのみ）
 * body : ボディを格納するバッファへのアドレス
 * file_path : リクエストターゲットに対応するファイルへのパス
 * 戻り値 : ステータスコード
 */
int getProcessing(char *body, char *file_path) {
	FILE *f;
	int file_size;

	//ファイルサイズを取得
	file_size = getFileSize(file_path);
	if(file_size == 0) {
		printf("getFileSize error\n");
		return 404;
	}

	//ファイルを読み込んでボディとする
	f = fopen(file_path, "r");
	fread(body, 1, file_size, f);
	fclose(f);

	return 200;
}

/* レスポンスメッセージを作成する
 * response_message : レスポンスメッセージを格納するバッファへのアドレス
 * status : ステータスコード
 * header : ヘッダフィールドを格納したバッファへのアドレス
 * body : ボディを格納したバッファへのアドレス
 * body_size : ボディのサイズ
 * 戻り値 : レスポンスメッセージのサイズ
 */
int createResponseMessage(char *response_message, int status, char *header, char *body, unsigned int body_size) {
	unsigned int no_body_len;
	unsigned int body_len;

	response_message[0] = '\0';

	if(status == 200) {
		sprintf(response_message, "HTTP/1.1 200 OK\r\n%s\r\n", header);
		no_body_len = strlen(response_message);
		body_len = body_size;
		memcpy(&response_message[no_body_len], body, body_len);
	} else if(status == 404) {
		sprintf(response_message, "HTTP/1.1 404 Not Found\r\n%s\r\n", header);
		no_body_len = strlen(response_message);
		body_len = 0;
	} else {
		printf("Not support status(%d)\n", status);
		return -1;
	}
	return no_body_len + body_len;
}

/* レスポンスメッセージを送信する
 * sock : 接続済みのソケット
 * response_message : 送信するレスポンスメッセージ
 * message_size : 送信するメッセージのサイズ
 * 戻り値 : 送信したサイズ
 */
int sendResponseMessage(int sock, char *response_message, unsigned int message_size) {
	int send_size;
	send_size = send(sock, response_message, message_size, 0);
	return send_size;
}

void showMessage(char *message, unsigned int size) {
//*
	unsigned int i;
	printf("Show Message\n\n");
	for(i = 0; i < size; i++) {
		putchar(message[i]);
	}
	printf("\n\n");
// */
}

/* HTTPサーバの処理を行う関数
 * sock : 接続済のソケット
 * 戻り値 : 0
 */
int httpServer(int sock) {
	int request_size, response_size;
	char request_message[SIZE];
	char response_message[SIZE];
	char method[SIZE];
	char target[SIZE];
	char header_field[SIZE];
	char body[SIZE];
	int status;
	unsigned int file_size;

	while(1) {
		request_size = recvRequestMessage(sock, request_message, SIZE);
		if(request_size == -1) {
			printf("recvRequestMessage error\n");
			break;
		}

		if(request_size == 0) {
			printf("connection ended\n");
			break;
		}

		showMessage(request_message, request_size);
		
		if(parseRequestMessage(method, target, request_message) == -1) {
			printf("parseRequestMessage error\n");
			break;
		}

		if(strcmp(method, "GET") == 0) {
			if(strcmp(target, "/") == 0) {
				strcpy(target, "/index.html");
			}
			status = getProcessing(body, &target[1]);
		} else {
			status = 404;
		}

		file_size = getFileSize(&target[1]);
		sprintf(header_field, "Content-Length: %u\r\n", file_size);

		response_size = createResponseMessage(response_message, status, header_field, body, file_size);
		if(response_size == -1) {
			printf("createResponseMessage error\n");
			break;
		}

		//showMessage(response_message, response_size);

		sendResponseMessage(sock, response_message, response_size);
	}
	return 0;
}

int main(void) {
	int w_addr, c_sock;
	struct sockaddr_in a_addr;

	w_addr = socket(AF_INET, SOCK_STREAM, 0);
	if(w_addr == -1) {
		printf("socket error\n");
		perror(strerror(errno));
		return -1;
	}

	memset(&a_addr, 0, sizeof(struct sockaddr_in));

	a_addr.sin_family = AF_INET;
	a_addr.sin_port = htons((unsigned short)SERVER_PORT);
	a_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);

	int yes = 1;
	if(setsockopt(w_addr, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0) {
		perror("setsockopt error");
		close(w_addr);
		return -1;
	}

	if(bind(w_addr, (const struct sockaddr *)&a_addr, sizeof(a_addr)) == -1) {
		printf("bind error\n");
		perror("bind");
		close(w_addr);
		return -1;
	}

	if(listen(w_addr, 3) == -1) {
		printf("listen error\n");
		perror("listen");
		close(w_addr);
		return -1;
	}

	while(1) {
		printf("Waiting connect...\n");
		c_sock = accept(w_addr, NULL, NULL);
		if(c_sock == -1) {
			perror("accept");
			close(w_addr);
			return -1;
		}
		printf("Connected!!\n");

		httpServer(c_sock);

		close(c_sock);
	}
	
	close(w_addr);
	return 0;
}
