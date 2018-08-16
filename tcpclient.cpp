#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <unistd.h>
#pragma comment(lib, "Ws2_32.lib");
using namespace std;
int port=0;
char ip[17];
char temp[256000];
char nextstring[1]="";
struct messageform{
		char number[4];
		char data[4];
		char time1[4];
		char time2[4];
		char len[4];
};
typedef struct messageform message;
message pocket[256];
int count=0;
int number;
unsigned int data;
unsigned int time1;
unsigned int time2;
unsigned int len;

int countrec=0;

void parserdata(char temp[],int sd){
	char temp2[5];
    int i=0;
    int i2=0;
    temp2[0]=temp[0+sd];
    temp2[1]=temp[1+sd];
    temp2[2]='\0';
    data = atoi(temp2);
    temp2[0]=temp[3+sd];
    temp2[1]=temp[4+sd];
    temp2[2]='\0';
    data += 100 * atoi(temp2);
    temp2[0]=temp[6+sd];
    temp2[1]=temp[7+sd];
    temp2[2]=temp[8+sd];
    temp2[3]=temp[9+sd];
    temp2[4]='\0';
    data += 10000 * atoi(temp2);
    data = htonl(data);
    pocket[count].data[0]=(data&0x000000FF);
	pocket[count].data[1]=(data&0x0000FF00)>>8;
	pocket[count].data[2]=(data&0x00FF0000)>>16;
	pocket[count].data[3]=(data&0xFF000000)>>24;
	memcpy(pocket[count].data, &data, 4);
}
void parsertime1(char temp[],int sd){
	char temp2[5];
    int i=0;
    int i2=0;
    temp2[0]=temp[0+sd];
    temp2[1]=temp[1+sd];
    temp2[2]='\0';
    time1 = 10000 * atoi(temp2);
    temp2[0]=temp[3+sd];
    temp2[1]=temp[4+sd];
    temp2[2]='\0';
    time1 += 100 * atoi(temp2);
    temp2[0]=temp[6+sd];
    temp2[1]=temp[7+sd];
    temp2[2]='\0';
    time1 += atoi(temp2);
    time1 = htonl(time1);
    pocket[count].time1[0]=(time1&0x000000FF);
	pocket[count].time1[1]=(time1&0x0000FF00)>>8;
	pocket[count].time1[2]=(time1&0x00FF0000)>>16;
	pocket[count].time1[3]=(time1&0xFF000000)>>24;
	memcpy(pocket[count].time1, &time1, 4);
}
void parsertime2(char temp[],int sd){
	char temp2[5];
    int i=0;
    int i2=0;
    temp2[0]=temp[0+sd];
    temp2[1]=temp[1+sd];
    temp2[2]='\0';
    time2 = 10000 * atoi(temp2);
    temp2[0]=temp[3+sd];
    temp2[1]=temp[4+sd];
    temp2[2]='\0';
    time2 += 100 * atoi(temp2);
    temp2[0]=temp[6+sd];
    temp2[1]=temp[7+sd];
    temp2[2]='\0';
    time2 +=atoi(temp2);
    time2 = htonl(time2);
    pocket[count].time2[0]=(time2&0x000000FF);
	pocket[count].time2[1]=(time2&0x0000FF00)>>8;
	pocket[count].time2[2]=(time2&0x00FF0000)>>16;
	pocket[count].time2[3]=(time2&0xFF000000)>>24;
	memcpy(pocket[count].time2, &time2, 4);
}
void parsermes(char temp[], char message[]){
	int i=0;
	while (i <= len){
		message[i]=temp[i+29];
		i++;
	}
	message[i]='\0';
}

int sock_err(const char* function, int s){
	int err;
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}

int send_request(int s,char *buf,int len){
	int flags = MSG_NOSIGNAL;
    int total = 0;
    int n;

    while(total < len)
    {
        n = send(s, buf+total, len-total, 0);
        if(n == -1) { break; }
        total += n;
    }
	return 0;
}

int recv_response(int s){
	char buffer[256];
	int res;
	while ((res = recv(s, buffer, sizeof(buffer), 0)) > 0){
		printf("%s \n", buffer);
		if (res==2) break;
	}
	countrec++;
	if (res < 0)
		return sock_err("recv", s);
	return 0;
}

int main(int argc, char* argv[]){
	int addr_len = strlen(argv[1]);
	char addr1[addr_len];
	strncpy(addr1,argv[1],addr_len);
	addr1[addr_len]='\0';

	int xe=0;
	for (xe=0; xe< addr_len; xe++) {
		if (addr1[xe]==':') break;
		ip[xe]=addr1[xe];
	}
	xe++;
	ip[xe]='\0';
	char portchar[10]={0};
	for (int j=xe; j< addr_len; j++){
		portchar[j-xe]=addr1[j];
	}
	port=atoi(portchar);
    ifstream inp(argv[2]); 
    char put[3]={'p','u','t'};
    int s;
	struct sockaddr_in addr;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip);
	int u=0;
	for (u; u < 10 ;u++){
		if (connect(s, (struct sockaddr*) &addr, sizeof(addr)) != 0)
			sleep(0.1);
		else break;
	}
	if (u==10) return 0;
	send(s , put , 3 , MSG_NOSIGNAL);
    while (!inp.eof()){
    	inp.getline(temp, 256000, '\n');
    	if (strcmp(temp,nextstring)!=0){
    		//cout << "temp-> " << temp << '\n';
	    	len=strlen(temp) - 29;
	    	char message[len];
	    	parserdata(temp,0);
	    	parsertime1(temp,11);
	    	parsertime2(temp,20);
	    	parsermes(temp,message);
	    	number = htonl(count);
	    	pocket[count].number[0]=(number&0x000000FF);
			pocket[count].number[1]=(number&0x0000FF00)>>8;
			pocket[count].number[2]=(number&0x00FF0000)>>16;
			pocket[count].number[3]=(number&0xFF000000)>>24;
	    	pocket[count].len[0]=(htonl(len)&0x000000FF);
			pocket[count].len[1]=(htonl(len)&0x0000FF00)>>8;
			pocket[count].len[2]=(htonl(len)&0x00FF0000)>>16;
			pocket[count].len[3]=(htonl(len)&0xFF000000)>>24;
	    	/*cout << (int)pocket[count].number[3] << " " << (int)pocket[count].number[2] << " " << (int)pocket[count].number[1] << " " << (int)pocket[count].number[0] << '\n';
	    	cout << (int)pocket[count].data[3] << " " << (int)pocket[count].data[2] << " " << (int)pocket[count].data[1] << " " << (int)pocket[count].data[0] << '\n';
	    	cout << (int)pocket[count].time1[3] << " " << (int)pocket[count].time1[2] << " " << (int)pocket[count].time1[1] << " " << (int)pocket[count].time1[0] << '\n';
	    	cout << (int)pocket[count].time2[3] << " " << (int)pocket[count].time2[2] << " " << (int)pocket[count].time2[1] << " " << (int)pocket[count].time2[0] << '\n';
	    	cout << (int)pocket[count].len[3] << " " << (int)pocket[count].len[2] << " " << (int)pocket[count].len[1] << " " << (int)pocket[count].len[0] << '\n';
	    	cout << message << '\n';*/

			send_request(s,pocket[count].number,4);
			send_request(s,pocket[count].data,4);
			send_request(s,pocket[count].time1,4);
			send_request(s,pocket[count].time2,4);
			send_request(s,pocket[count].len,4);
			send(s, &message, sizeof(char)*len ,0);
			//recv_response(s);
	    	data=0;
	    	time1=0;
	    	time2=0;
	    	count++;
    	}
	}
	while (countrec<count) recv_response(s);
	cout << countrec << " " << count << '\n';
    return 0;
}