#pragma comment (lib, "Ws2_32.lib")
#include <Winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>


using namespace std;

// Структура для хранения данных о погоде
struct Weather 
{
	string date;
	string country;
	string city;
	double latitude = 0.0;
	double longitude = 0.0;
	double temperature = 0.0;
	string sunrise;
	string sunset;
};

// Функция конвертации UNIX timestamp в строку даты и времени
string unixTimeToString(long long unixTime) 
{
	time_t time = unixTime;
	tm tm;
	char buffer[32];

	localtime_s(&tm, &time);
	
	strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", &tm);
	return string(buffer);
}

// Функция парсинга JSON и заполнения Weather
Weather parseWeather(const string& jsonResponse) {
	Weather weather;

	size_t pos;
	size_t endPos = 0; 

	// Извлекаем город
	pos = jsonResponse.find("\"name\":\"");
	if (pos != string::npos) {
		pos += 8; // Пропускаем "\"name\":\""
		weather.city = jsonResponse.substr(pos, jsonResponse.find("\"", pos) - pos);
	}

	// Долгота
	pos = jsonResponse.find("\"lon\":", endPos);
	if (pos != string::npos) {
		pos += 6; // Пропускаем "\"lon\":"
		endPos = jsonResponse.find_first_of(",}", pos);
		weather.longitude = stod(jsonResponse.substr(pos, endPos - pos));
	}

	// Широта
	pos = jsonResponse.find("\"lat\":", endPos);
	if (pos != string::npos) {
		pos += 6; // Пропускаем "\"lat\":"
		endPos = jsonResponse.find(",", pos);
		weather.latitude = stod(jsonResponse.substr(pos, endPos - pos));
	}

	// Страна
	pos = jsonResponse.find("\"country\":\"");
	if (pos != string::npos) {
		pos += 11; // Пропускаем "\"country\":\""
		weather.country = jsonResponse.substr(pos, jsonResponse.find("\"", pos) - pos);
	}

	// Температура
	pos = jsonResponse.find("\"temp\":");
	if (pos != string::npos) {
		pos += 7; // Пропускаем "\"temp\":"
		weather.temperature = stod(jsonResponse.substr(pos, jsonResponse.find(",", pos) - pos));
	}

	// Дата
	pos = jsonResponse.find("\"dt\":", endPos);
	if (pos != string::npos) {
		pos += 5; 
		endPos = jsonResponse.find(",", pos);
		weather.date = unixTimeToString(stoll(jsonResponse.substr(pos, endPos - pos)));
	}

	// Восход
	pos = jsonResponse.find("\"sunrise\":", endPos); 
	if (pos != string::npos) {
		pos += 10; 
		endPos = jsonResponse.find(",", pos);
		weather.sunrise = unixTimeToString(stoll(jsonResponse.substr(pos, endPos - pos)));
	}

	// Закат
	pos = jsonResponse.find("\"sunset\":", endPos); 
	if (pos != string::npos) {
		pos += 9; 
		endPos = jsonResponse.find("}", pos); 
		weather.sunset = unixTimeToString(stoll(jsonResponse.substr(pos, endPos - pos)));
	}

	return weather;
}

int main()
{
	// Получаем дескриптор стандартного вывода
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	setlocale(0, "ru");

	//1. инициализация "Ws2_32.dll" для текущего процесса
	WSADATA wsaData;
	WORD wVersionRequested = MAKEWORD(2, 2);

	int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {

		cout << "WSAStartup failed with error: " << err << endl;
		return 1;
	}

	//инициализация структуры, для указания ip адреса и порта сервера с которым мы хотим соединиться

	char hostname[255] = "api.openweathermap.org";

	addrinfo* result = NULL;

	addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int iResult = getaddrinfo(hostname, "http", &hints, &result);
	if (iResult != 0) {
		cout << "getaddrinfo failed with error: " << iResult << endl;
		WSACleanup();
		return 3;
	}

	SOCKET connectSocket = INVALID_SOCKET;
	addrinfo* ptr = NULL;

	//Пробуем присоединиться к полученному адресу
	for (ptr = result; ptr != NULL; ptr = ptr->ai_next) {

		//2. создание клиентского сокета
		connectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (connectSocket == INVALID_SOCKET) {
			printf("socket failed with error: %ld\n", WSAGetLastError());
			WSACleanup();
			return 1;
		}

		//3. Соединяемся с сервером
		iResult = connect(connectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(connectSocket);
			connectSocket = INVALID_SOCKET;
			continue;
		}
		break;
	}

	//4. HTTP Request

	string uri = "/data/2.5/weather?q=Odessa&appid=75f6e64d49db78658d09cb5ab201e483&mode=JSON&units=metric";

	string request = "GET " + uri + " HTTP/1.1\n";
	request += "Host: " + string(hostname) + "\n";
	request += "Accept: */*\n";
	request += "Accept-Encoding: gzip, deflate, br\n";
	request += "Connection: close\n";
	request += "\n";

	//отправка сообщения
	if (send(connectSocket, request.c_str(), request.length(), 0) == SOCKET_ERROR) {
		cout << "send failed: " << WSAGetLastError() << endl;
		closesocket(connectSocket);
		WSACleanup();
		return 5;
	}
	cout << "send data" << endl;

	//5. HTTP Response

	string response;

	const size_t BUFFERSIZE = 1024;
	char resBuf[BUFFERSIZE];

	int respLength;

	do {
		respLength = recv(connectSocket, resBuf, BUFFERSIZE, 0);
		if (respLength > 0) {
			response += string(resBuf).substr(0, respLength);
		}
		else {
			cout << "recv failed: " << WSAGetLastError() << endl;
			closesocket(connectSocket);
			WSACleanup();
			return 6;
		}

	} while (respLength == BUFFERSIZE);

	cout << response << endl;

	string jsonResponse;
	size_t headerEnd = response.find("\r\n\r\n");

	if (headerEnd != string::npos) 
	{
		jsonResponse = response.substr(headerEnd + 4);// Пропускаем 4 символа (\r\n\r\n), чтобы получить начало JSON
	}
	else 
	{
		cout << "Не удалось найти JSON-тело в ответе" << endl;
		return 6;
	}

	//работа с респонсом и запись в структуру
	Weather weather = parseWeather(jsonResponse);

	// Устанавливаем жёлтый цвет текста
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN);

	cout << "\n\nДата: " << weather.date << ";" << endl;
	cout << "Страна: " << weather.country << ";" << endl;
	cout << "Город: " << weather.city << ";" << endl;

	// Устанавливаем голубой цвет текста
	SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN);

	cout << "Координаты: широта- " << fixed << setprecision(2) << weather.latitude << ", долгота-" << weather.longitude << ";" << endl;
	cout << "Температура: " << weather.temperature << " °C;" << endl;

	// Устанавливаем зелёный цвет текста 
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	cout << "Рассвет: " << weather.sunrise << ";" << endl;
	cout << "Закат: " << weather.sunset << "." << endl;

	// Возвращаем консоль к стандартному цвету текста
	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

	//отключает отправку и получение сообщений сокетом
	iResult = shutdown(connectSocket, SD_BOTH);
	if (iResult == SOCKET_ERROR) 
	{
		cout << "shutdown failed: " << WSAGetLastError() << endl;
		closesocket(connectSocket);
		WSACleanup();
		return 7;
	}

	closesocket(connectSocket);
	WSACleanup();
}

/*
	Дата:
	Страна:
	Город:
	Координаты: (в С)
	Раасвет:
	Закат:
*/
