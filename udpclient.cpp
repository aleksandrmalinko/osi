#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <winsock2.h>
	// Директива линковщику: использовать библиотеку сокетов
	#pragma comment(lib, "ws2_32.lib")
#else // LINUX
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <sys/select.h>
	#include <netdb.h>
	#include <errno.h>
	#include <unistd.h>
	#include <arpa/inet.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// #############################################################################

struct datagrams // Список сформированных дейтаграмм из сообщений в txt файле.
{
	unsigned int message_id; // Служебный ID сообщения.

	char *datagram; // Дейтаграмма для отправки, сформированная из сообщения.
	int datagram_length; // Размер дейтаграммы в байтах.

	struct datagrams *next; // Формирование связного списка.
} *root_datagram;

struct datagrams * add_datagram(unsigned int id, char *datagram, int datagram_length)
{
	struct datagrams *tmp = root_datagram;

	struct datagrams *new_datagram = (struct datagrams *) malloc(sizeof(struct datagrams));
	memset(new_datagram, 0, sizeof(new_datagram));

	new_datagram->message_id = id;
	new_datagram->datagram = datagram;
	new_datagram->datagram_length = datagram_length;
	new_datagram->next = NULL;

	if (root_datagram == NULL) root_datagram = new_datagram;
	else
	{
		while (tmp->next != NULL)
			tmp = tmp->next;
		tmp->next = new_datagram;
	}

	return new_datagram;
}

void remove_datagram(struct datagrams *datagram)
{
	if (root_datagram == NULL || datagram == NULL) return;

	struct datagrams *tmp = root_datagram;
	struct datagrams *prev_datagram = NULL;

	if (root_datagram == datagram) root_datagram = datagram->next;
	else
	{
		while (tmp->next != NULL)
		{
			prev_datagram = tmp;
			tmp = tmp->next;
			if (tmp == datagram) break;
		}

		if (prev_datagram != NULL)
			prev_datagram->next = datagram->next;
	}

	if (datagram->datagram_length != 0 && datagram->datagram != NULL)
		free(datagram->datagram);
	free(datagram);
}

struct datagrams * find_datagram(int id)
{
	struct datagrams *tmp = root_datagram;

	while (tmp)
	{
		if (tmp->message_id == id) return tmp;
		tmp = tmp->next;
	}

	return NULL;
}

// #############################################################################

int init()
{
#ifdef _WIN32
    // Для Windows следует вызвать WSAStartup перед началом использования сокетов
    WSADATA wsa_data;
    return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
    return 1; // Для других ОС действий не требуется
#endif
}

void deinit()
{
#ifdef _WIN32
    // Для Windows следует вызвать WSACleanup в конце работы
    WSACleanup();
#else
    // Для других ОС действий не требуется
#endif
}

int sock_err(const char * function, int s)
{
    int err;

#ifdef _WIN32
    err = WSAGetLastError();
#else
    err = errno;
#endif

    fprintf(stderr, "%s: socket error: %d\n", function, err);
    return -1;
}

void s_close(int s)
{
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}


// Функция определяет IP-адрес узла по его имени.
// Адрес возвращается в сетевом порядке байтов.
unsigned int get_host_ipn(const char * name)
{
    struct addrinfo * addr = 0;
    unsigned int ip4addr   = 0;

    // Функция возвращает все адреса указанного хоста
    // в виде динамического однонаправленного списка
    if (0 == getaddrinfo(name, 0, 0, &addr))
    {
        struct addrinfo * cur = addr;
        while (cur)
        {
            // Интересует только IPv4 адрес, если их несколько - то первый
            if (cur->ai_family == AF_INET)
            {
                ip4addr = ((struct sockaddr_in *) cur->ai_addr)->sin_addr.s_addr;
                break;
            }
            cur = cur->ai_next;
        }
        freeaddrinfo(addr);
    }

    return ip4addr;
}

void send_request(int s, struct sockaddr_in* addr, struct datagrams *datagram)
{
#ifdef _WIN32
    int flags = 0;
#else
    int flags = MSG_NOSIGNAL;
#endif

    int res = sendto(s, datagram->datagram, datagram->datagram_length, flags, (struct sockaddr*) addr, sizeof(struct sockaddr_in));
    if (res <= 0) sock_err("sendto", s);
}

/* Добавляет значение в массив.
   Вернет true, в случае успеха, или false, если значение уже было или нет места.
   Значение в массиве хранятся как value + 1. */
bool add_to_array(unsigned int *array, const int size, unsigned int value)
{
	for (int i = 0; i < size; i++)
		if (array[i] == (value + 1)) return false;
		else if (array[i] == 0) { array[i] = value + 1; return true; };

	return false;
}

/* Обрабатывает полученную с сервера дейтаграмму.
   Вернет кол-во гарантированно свежедоставленных (которых еще нет в массиве received_ids) дейтаграмм. */
unsigned int parse_datagram(char *datagram, int datagram_size, unsigned int *received_ids)
{
	unsigned int message_id, count = 0;

	char *offset = datagram;
	while (offset < (datagram + datagram_size))
	{
		memcpy(&message_id, offset, 4);
		message_id = ntohl(message_id);

		if (message_id < 0) break;
		else if (add_to_array(received_ids, 20, ntohl(message_id))) count++;

		offset += 4;
	}

	return count;
}

// Функция принимает дейтаграмму от удаленной стороны.
// Возвращает 0, если в течение 100 миллисекунд не было получено ни одной дейтаграммы
unsigned int recv_response(int s, unsigned int *received_ids)
{
    char datagram[1024];

    struct timeval tv = { 0, 100 * 1000 }; // 100 msec;
	int res, ret_value = 0;

	struct sockaddr_in addr;
	int addrlen = sizeof(addr);

    fd_set fds;
	FD_ZERO(&fds); FD_SET(s, &fds);

    // Проверка - если в сокете входящие дейтаграммы
    // (ожидание в течение tv)
    while (0 < (res = select(s + 1, &fds, 0, 0, &tv))) // Если данные в потоке есть, считываем их.
	{
		tv = { 0, 100 * 1000 };
		FD_ZERO(&fds); FD_SET(s, &fds);

		memset(datagram, 0xFF, sizeof(datagram));
        int received = recvfrom(s, datagram, sizeof(datagram), 0, (struct sockaddr*) &addr, (socklen_t *) &addrlen);
        if (received <= 0)
		{
            // Ошибка считывания полученной дейтаграммы
            sock_err("recvfrom", s);
            break;
        }

        ret_value += parse_datagram(datagram, sizeof(datagram), received_ids);
    }

	return ret_value;
}

/* Формирует список дейтаграмм из сообщений в txt файле. */
void form_datagrams(char *txt)
{
	unsigned int datagram_length, size;

	// "Сырые" значения.
	unsigned int message_id = 0;
    char *date, *hhmmss_1, *hhmmss_2;

	 // Отправляемые значения.
    unsigned int s_message_id, s_date, s_hhmmss_1, s_hhmmss_2, s_msg_length;
	char *message;

	char *next_note = strtok(txt, "\n"), *cur_note;
	while (next_note != NULL) // Разбираем запись на части и отправляем ее.
	{
		cur_note = next_note;
        next_note = strtok(next_note + strlen(next_note) + 1, "\n");

        date = strtok(cur_note, " ");
        hhmmss_1 = strtok(NULL, " ");
        hhmmss_2 = strtok(NULL, " ");
        message = hhmmss_2 + 8 + 1;

		if (date == NULL || hhmmss_1 == NULL || hhmmss_2 == NULL || message == NULL ||
			strlen(date) != 10 || strlen(hhmmss_1) != 8 || strlen(hhmmss_2) != 8 ||
			date[2] != '.' || date[5] != '.' || hhmmss_1[2] != ':' || hhmmss_1[5] != ':' || hhmmss_2[2] != ':' || hhmmss_2[5] != ':')
		{
			printf("Invalid message format, skip (message id %d).\n", message_id);
			continue;
		}

		// Приводим данные к виду, указанному в спецификации протокола.
		s_message_id	= htonl(message_id);
		s_date			= htonl(atoi(strtok(date, ".")) + atoi(strtok(NULL, ".")) * 100 + atoi(strtok(NULL, ".")) * 10000);
		s_hhmmss_1		= htonl(atoi(strtok(hhmmss_1, ":")) * 10000 + atoi(strtok(NULL, ":")) * 100 + atoi(strtok(NULL, ":")));
		s_hhmmss_2		= htonl(atoi(strtok(hhmmss_2, ":")) * 10000 + atoi(strtok(NULL, ":")) * 100 + atoi(strtok(NULL, ":")));
		s_msg_length	= htonl((unsigned int) strlen(message));

		datagram_length = sizeof(char) * (20 + strlen(message));
		char *datagram = (char *) malloc(datagram_length);
		char *datagram_w = datagram;

		size = 4;
		memcpy(datagram_w, &s_message_id, size);	datagram_w += size;
		memcpy(datagram_w, &s_date, size);			datagram_w += size;
		memcpy(datagram_w, &s_hhmmss_1, size);		datagram_w += size;
		memcpy(datagram_w, &s_hhmmss_2, size);		datagram_w += size;
		memcpy(datagram_w, &s_msg_length, size);	datagram_w += size;
		size = strlen(message);
		memcpy(datagram_w, message, size);			datagram_w += size;

		add_datagram(message_id, datagram, datagram_length);

		message_id++;
	}
}

/* Отпределяет, была ли доставлена ли дейтаграмма (содержится ли ее ID в arr).
   Вернет true, если доставлена. */
bool is_delivered(struct datagrams *datagram, unsigned int *arr)
{
	const int arr_size = 20;

	for (int i = 0; i < arr_size; i++)
		if (arr[i] == (datagram->message_id + 1)) return true;

	return false;
}

/* Формирует дейтаграммы из сообщений и отправляет их поочередно. */
int start_sending(int s, struct sockaddr_in* addr)
{
	unsigned int received_ids[20]; // Список доставленных сообщений (дейтаграмм), хранит ID + 1.
	memset(received_ids, 0, sizeof(received_ids));

	unsigned send_attempt_num = 0; // Номер неудачной попытки отправки (увеличивается при неудачном отправлении дейтаграммы и сбрасывается, если все-же доставка удалась).
	unsigned datagrams_sent = 0; // Общее кол-во отправленных дейтаграмм.
	unsigned datagrams_sent_now = 0; // Кол-во отправленных дейтаграмм за данный вызов recv_response.
	unsigned datagrams_count = 0; // Кол-во дейтаграмм, доступных для отправки.

	for (struct datagrams *tmp = root_datagram; tmp != NULL; tmp = tmp->next)
		datagrams_count++;

	struct datagrams *datagram = root_datagram;
	while (datagram != NULL && datagrams_sent < datagrams_count && datagrams_sent <= 20)// && send_attempt_num < 5) // Пока не отправим все дейтаграммы (не более 20).
	{
		if (is_delivered(datagram, received_ids)) goto next; // Если дейтаграмма уже была доставлена, пропустим ее.

		send_request(s, addr, datagram); // Отправка запроса на удаленный сервер.
		datagrams_sent_now = recv_response(s, received_ids); // Ответ сервера. Принимаем кол-во свежедоставленных дейтаграмм (которые ранее не доставлялись).
		datagrams_sent += datagrams_sent_now;

		if (datagrams_sent_now == 0) // Нет ответа сервера, увеличиваем счетчик неудачных попыток подключения.
		{
			send_attempt_num++;
			continue;
		} else // Подключение удалось. Обнуляем счетчик неудачных попыток подключения.
			send_attempt_num = 0;

next:	datagram = datagram->next;
		if (datagram == NULL) datagram = root_datagram;
	}

	if (send_attempt_num)
		printf("No server response %d times in a row. Termination.\n", send_attempt_num); // Нет ответа сервера несколько раз подряд. Отмена попыток отправки.

	printf("%d msg(s) sent, %d msg(s) left in queue.\n", datagrams_sent, datagrams_count - datagrams_sent);
}

/* Считывает текстовый файл и возвращает указатель на буффер. */
char * read_txt(char *filepath)
{
    FILE *file = fopen(filepath, "rb");
    if (file == NULL || fileno(file) == -1)
    {
        printf("Can't open file '%s'\n", filepath);
        exit(-1);
    }

    char *buf = NULL;
    int buf_size = 0;

    int c, count = 0;
    while ((c = fgetc(file)) != EOF)
    {
        if (buf_size == count)
        {
            buf_size += 5000;
            buf = (char *) realloc(buf, (buf_size + 1) * sizeof(char));
        }

        *(buf + count) = (char) c;
        count++;
    }

    buf[count] = '\0';
    return buf;
}

int main(int argc, char *argv[])
{
	char *arg_ip_port, *arg_filename;

	if (argc == 3)
	{
		arg_ip_port = argv[1];
		arg_filename = argv[2];
	} else
	{
		printf("Wrong number of args. Use this format: udpclient ip:port filename\n");
		exit(-1);
	}

	char *txt = read_txt(arg_filename);

	char *str_ip = strtok(arg_ip_port, ":");
	char *str_port = strtok(NULL, ":");
	if (!str_port) { printf("Wrong args. Use this format: udpclient ip:port filename\n"); exit(-1); }

    int s, i;
    struct sockaddr_in addr;

	// Инициалиазация сетевой библиотеки
    init();

	// Создание UDP-сокета
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return sock_err("socket", s);

	// Заполнение структуры с адресом удаленного узла
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(str_port));
    addr.sin_addr.s_addr = get_host_ipn(str_ip);

	form_datagrams(txt);
	start_sending(s, &addr);

	// Завершение.
	for (struct datagrams *tmp = root_datagram; tmp != NULL;)
	{
		struct datagrams *tmp_next = tmp->next;
		remove_datagram(tmp);
		tmp = tmp_next;
	}

    // Закрытие сокета
    s_close(s);
    deinit();

    return 0;
}
