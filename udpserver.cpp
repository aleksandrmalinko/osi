#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sys/poll.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char arr_cli[6] = { 0 };
int flags = MSG_NOSIGNAL;


int recv_string(int cs, FILE *f, int port, struct sockaddr_in *addr) {
	char buffer[500];
	char number[5];
	char data[5];
	char time1[5];
	char time2[5];
	char length[5];
	int curlen = 0; 
	int addrlen = sizeof(struct sockaddr_in);
	char datagram[655];

	char *full_mes_buffer; // Полный сформированный буффер из потока.
	int full_mes_buffer_alloc; // Текущая выделенная память под full_mes_buffer.
	int rcv, buf_offset, total_rcv;

	total_rcv = 0;
	buf_offset = 0;
	full_mes_buffer_alloc = 0;
	full_mes_buffer = NULL;

	int received = recvfrom(cs, datagram, 24, 0, (struct sockaddr *) addr, (socklen_t *) &addrlen);


	if (rcv == 0) return 0;
	number[3] = buffer[3];
	number[2] = buffer[2];
	number[1] = buffer[1];
	number[0] = buffer[0];
	int res = sendto(cs, number, 4, flags, (struct sockaddr *) addr, sizeof(struct sockaddr_in));
	int numberi;
	memcpy(&numberi, &number, sizeof(number) - 1);
	//printf("number : %i\n", numberi);

	data[0] = buffer[7];
	data[1] = buffer[6];
	data[2] = buffer[5];
	data[3] = buffer[4];
	int datai;
	memcpy(&datai, &data, sizeof(data) - 1);
	fprintf(f, "127.0.0.1:%d ", port);
	fprintf(f,"%i.%i.%i ", (datai % 100),  (datai-(datai/10000)*10000- (datai % 100))/100, datai / 10000 );

	time1[0] = buffer[11];
	time1[1] = buffer[10];
	time1[2] = buffer[9];
	time1[3] = buffer[8];
	int time1i;
	memcpy(&time1i, &time1, sizeof(time1) - 1);
	fprintf(f,"%i:%i:%i ", time1i / 10000 , (time1i - (time1i / 10000) * 10000 - (time1i % 100)) / 100, (time1i % 100));

	time2[0] = buffer[15];
	time2[1] = buffer[14];
	time2[2] = buffer[13];
	time2[3] = buffer[12];
	int time2i;
	memcpy(&time2i, &time2, sizeof(time2) - 1);
	fprintf(f, "%i:%i:%i ", time2i / 10000, (time2i - (time2i / 10000) * 10000 - (time2i % 100)) / 100, (time2i % 100));

	length[0] = buffer[19];
	length[1] = buffer[18];
	length[2] = buffer[17];
	length[3] = buffer[16];
	int lengthi;
	memcpy(&lengthi, &length, sizeof(length) - 1);
	//printf("length : %i\n", lengthi);
	char* mess = (char*)malloc(lengthi);
	//printf("Message : ");
	for (int i = 0; i < lengthi; i++) mess[i] = buffer[20 + i];
	mess[lengthi] = '\0';
	fprintf(f,"%s\n", mess);
	char stop[5] = { 's','t','o','p','\0' };
	if (strcmp(mess, stop) == 0) {
		return 0;
	}
	return 1;
}

int main(int argc, char* argv[]) {
	FILE *f;
	f = fopen("msg.txt", "w");
	int s;
	int len = 0;
	struct sockaddr_in addr;
	int port = atoi(argv[1]);
	// Инициалиазация сетевой библиотеки
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return -1;
	// Заполнение структуры с адресом прослушивания узла
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(9000); 
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	// Связь адреса и сокета, чтобы он мог принимать входящие дейтаграммы
	if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0)
		return -1;
	do {
		int porte = ntohs(addr.sin_port);
		len = recv_string(s,f,porte,&addr);
		// Отправка клиенту сообщения
		if (len == 0) break;
	} while (1); // Повторение этого алгоритма в беск. цикле  
	return 0;
}