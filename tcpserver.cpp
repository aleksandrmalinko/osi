#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h> // Директива линковщику: использовать библиотеку сокетов 
#pragma comment(lib, "ws2_32.lib") 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
char array[500][50000];

int init() {
	WSADATA wsa_data; 
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
}
void deinit() {
	WSACleanup(); 
}
int sock_err(const char* function, int s) {
	int err;
	err = WSAGetLastError();
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}
void s_close(int s) { 
	closesocket(s); 
}

int s_send(int s, const void *data, int size)
{
	int sent = 0;
	int flags = 0;

	while (sent < size)
	{
		int res = send(s, (char *)data + sent, size - sent, flags);
		if (res < 0) return sock_err("send", s);

		sent += res;
	}

	return sent;
}

int send_notice(int cs)
{
	char buffer[2] = { 'o', 'k' };
	s_send(cs, buffer, 2);

	return 0;
}

int recv_string(int cs) {
	char buffer[50000];
	char number[5];
	char data[5];
	char time1[5];
	char time2[5];
	char length[5];
	int curlen = 0; 

	char *full_mes_buffer; // Полный сформированный буффер из потока.
	int full_mes_buffer_alloc; // Текущая выделенная память под full_mes_buffer.
	int rcv, buf_offset, total_rcv;

	total_rcv = 0;
	buf_offset = 0;
	full_mes_buffer_alloc = 0;
	full_mes_buffer = NULL;

	struct timeval tv;
	fd_set rfd;
	do
	{
		rcv = recv(cs, buffer, sizeof(buffer), 0);
		if (rcv <= 0) break;
		else total_rcv += rcv;

		full_mes_buffer_alloc += rcv;
		full_mes_buffer = (char *)realloc(full_mes_buffer, full_mes_buffer_alloc);
		if (full_mes_buffer == NULL) { printf("Memory allocation error! Exit.\n"); exit(-1); }
		else memcpy(full_mes_buffer + (total_rcv - rcv), buffer, rcv);

		tv = { 0, 100 * 1000 };
		FD_ZERO(&rfd);
		FD_SET(cs, &rfd);
	} while (select(cs, &rfd, NULL, 0, &tv) > 0); // Пока идет поток данных от клиента, получаем их.


	number[0] = buffer[3];
	number[1] = buffer[2];
	number[2] = buffer[1];
	number[3] = buffer[0];
	int numberi;
	memcpy(&numberi, &number, sizeof(number)-1);
	printf("number : %i\n",numberi);

	data[0] = buffer[7];
	data[1] = buffer[6];
	data[2] = buffer[5];
	data[3] = buffer[4];
	int datai;
	memcpy(&datai, &data, sizeof(data)-1);
	printf("data : %i\n", datai);

	time1[0] = buffer[11];
	time1[1] = buffer[10];
	time1[2] = buffer[9];
	time1[3] = buffer[8];
	int time1i;
	memcpy(&time1i, &time1, sizeof(time1) - 1);
	printf("time1 : %i\n", time1i);

	time2[0] = buffer[15];
	time2[1] = buffer[14];
	time2[2] = buffer[13];
	time2[3] = buffer[12];
	int time2i;
	memcpy(&time2i, &time2, sizeof(time2) - 1);
	printf("time2 : %i\n", time2i);

	length[0] = buffer[19];
	length[1] = buffer[18];
	length[2] = buffer[17];
	length[3] = buffer[16];
	int lengthi;
	memcpy(&lengthi, &length, sizeof(length) - 1);
	printf("length : %i\n", lengthi);
	char* mess = (char*)malloc(lengthi);
	printf("Message : ");
	for (int i = 0; i < lengthi; i++) mess[i] = buffer[20+i];
	mess[lengthi] = '\0';
	printf("%s", mess);
	send_notice(cs);
	char stop[5] = { 's','t','o','p','\0' };
	if (strcmp(mess, stop) == 0) {
		return 0;
	}
	return 1;
}

int main() {
	int s; struct sockaddr_in addr;
	// Инициалиазация сетевой библиотеки 
	init();
	// Создание TCP-сокета
	
	do { // Принятие очередного подключившегося клиента 
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
			return sock_err("socket", s);
		// Заполнение адреса прослушивания
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(9000); // Сервер прослушивает порт 9000 
		addr.sin_addr.s_addr = htonl(INADDR_ANY); // Все адреса
												  // Связывание сокета и адреса прослушивания 
		if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0)
			return sock_err("bind", s);
		// Начало прослушивания
		if (listen(s, 1) < 0)
			return sock_err("listen", s);

		int addrlen = sizeof(addr); 
		int cs = accept(s, (struct sockaddr*) &addr, &addrlen);
		unsigned int ip;
		int len;
		if (cs < 0) {
			sock_err("accept", s);
			break; 
		}
		// Вывод адреса клиента
		ip = ntohl(addr.sin_addr.s_addr);
		printf(" Client connected: %u.%u.%u.%u ", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, (ip) & 0xFF);
		// Прием от клиента строки 
		len = recv_string(cs);
		// Отправка клиенту сообщения
		if (len == 0) break;
		//for (int i = 0; i< 30; i++)
		//	printf("%c", array[0][i]);
		s_close(s);
	} while (1); // Повторение этого алгоритма в беск. цикле  
	deinit();
	
	return 0;
}