#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <regex>
#include <map>
#include <cstdlib>
#include <cmath>
#include <ctime>

using namespace std;
using namespace std::chrono;
typedef std::vector<int> Vec;

// запись сообщения
class MessageRecord {
public:
	string nickname;
	string message;
	int64_t timestamp;

	MessageRecord() {}
	MessageRecord(string nick, string msg, int64_t time) : nickname(nick), message(msg), timestamp(time) {}
};

// NOTE: link Winsock 2 library to the project

#pragma comment (lib, "ws2_32.lib")

#define APP_PORT 12345
#define RECV_BUF_SIZE 1024
const int64_t timeoutTime = 3000;
const string saveHistoryArg = "-save";
string nickName = "";
bool first = false;
system_clock::time_point lastSend;
//char* nickNameConverted;

// Define receive buffer
static char g_pczRecvBuffer[RECV_BUF_SIZE];

// флаг, говорящий о том, сохраняем ли мы историю сообщений
static bool saveHistory = false;

/// <summary>вектор истории сообщений</summary>
static vector<MessageRecord> messageHistory;

/// <summary>Генерация уникального ID пира</summary>
/// <returns>ID пира</returns>
unsigned GetCurrHostID()
{
	//static int iAppUniqueID = rand();
	//static int iAppUniqueID = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
	static int iAppUniqueID = duration_cast<seconds>(system_clock::now().time_since_epoch()).count() + rand() - rand();
	return unsigned(iAppUniqueID);
}

int MyID = GetCurrHostID();

/// <summary>Ввод никнейма</summary>
/// <returns>никнейм</returns>
string GetNick() {
	if (nickName == "") {
		cout << "Your nickname: ";
		cin >> nickName;
	}
	return nickName;
}

//count - now parametr User Count, mode: 0 - increment or 1 - decrement
void SetUserCount(int count, int mode) {
	ofstream fout;
	fout.open("C:\\Rasp\\lab2_udp_sample\\vs\\file.txt", ios::trunc);
	if (mode == 0) {
		count++;
	}
	else if (mode == 1)
		count--;

	string str = to_string(count);
	if (fout.is_open())
	{
		fout << str << endl;
	}
	fout.close();
}

int GetUserCount() {
	ifstream fin("C:\\Rasp\\lab2_udp_sample\\vs\\file.txt");
	string str;
	int count = 0;

	if (fin.is_open())
	{
		while (getline(fin, str))
		{
			count = stoi(str);
		}
	}
	fin.close();
	return count;
}

// Define some simple protocol
enum eMsgType : uint32_t
{
	MSG_HELLO,
	MSG_PING,
	MSG_TEXT,
	MSG_HISTOTY_BEGIN,
	MSG_HISTORY_END,
	MSG_HISTORY
};

struct BaseMsg
{
	unsigned uiSize;
	unsigned uiHostID;
	unsigned uiType;
};

struct HelloMsg : public BaseMsg
{
	bool first;
	char MsgNick[256];
	HelloMsg(bool first) { memset(this, 0, sizeof(*this)); uiSize = sizeof(*this); uiType = MSG_HELLO; uiHostID = MyID; this->first = first; }
};

struct PingMsg : public BaseMsg
{
	PingMsg() { memset(this, 0, sizeof(*this)); uiSize = sizeof(*this); uiType = MSG_PING; uiHostID = MyID; }
};

struct TextMsg : public BaseMsg
{
	char message[256];
	char nickname[256];
	int64_t timestamp;
	TextMsg() { memset(this, 0, sizeof(*this)); uiSize = sizeof(*this); uiType = MSG_TEXT; uiHostID = MyID; }
};

struct HistoryMarkMsg : public BaseMsg
{
	HistoryMarkMsg(bool end) { memset(this, 0, sizeof(*this)); uiSize = sizeof(*this); uiType = end ? MSG_HISTORY_END : MSG_HISTOTY_BEGIN; uiHostID = MyID; }
};

struct HistoryMsg : public BaseMsg
{
	char message[256];
	char nickname[256];
	uint64_t timestamp;
	HistoryMsg() { memset(this, 0, sizeof(*this)); uiSize = sizeof(*this); uiType = MSG_HISTORY; uiHostID = MyID; }
};

// Function to simplify UDP data send
/// <summary>Отправка сообщения по UDP</summary>
/// <param name="s">- сокет</param>
/// <param name="*pAddr">- указатель на адрес</param>
/// <param name="rMessage">- адрес сообщения</param>
bool SendUDPMessage(SOCKET s, sockaddr_in* pAddr, BaseMsg& rMessage)
{
	int iSize = sendto(s, reinterpret_cast<const char*>(&rMessage), rMessage.uiSize, 0, (struct sockaddr*)pAddr, sizeof(struct sockaddr_in));

	//int iSize = send(m_socket, reinterpret_cast<const char*>(&rMessage), rMessage.uiSize, 0);
	if (iSize == SOCKET_ERROR)
	{
		// if error:
		// printf( "Send: Failed to send.\n" );
		return false;
	}
	return iSize == rMessage.uiSize;
}

/// <summary>Отправка heartbeat-сообщения</summary>
/// <param name="s">- сокет</param>
/// <param name="*pAddr">- указатель на адрес</param>
void SendHeartbeatMessage(SOCKET s, sockaddr_in* pAddr)
{
	auto now = system_clock::now();
	int64_t diff = duration_cast<milliseconds>(now - lastSend).count();
	if (diff >= 200) {
		PingMsg heartbeat;
		SendUDPMessage(s, pAddr, heartbeat);
		lastSend = now;
	}
}

/// <summary>Отправка сообщения "hello"</summary>
/// <param name="s">- сокет</param>
/// <param name="*pAddr">- указатель на адрес</param>
void SendHelloMessage(SOCKET s, sockaddr_in* pAddr, bool first)
{
	HelloMsg msg(first);
	strncpy(msg.MsgNick, nickName.c_str(), nickName.size());
	SendUDPMessage(s, pAddr, msg);
}

///<summary>Получение времени в мс</summary>
int GetCurrTime()
{
	auto ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() % 60000;
	return ms;
}

///<summary>Идентификация пользователя</summary>
void UserVerification() {
	int Count = GetUserCount();

	if (Count == 0) {
		first = true;
		cout << "[PING] Hello, " << nickName << "! You're first, who joined to chat!" << endl;
	}
	else {
		cout << "[PING] Hello, " << nickName << "!" << endl;
	}
	SetUserCount(Count, 0);
}

void CloseConsole() {
	int Count = GetUserCount();
	SetUserCount(Count, 1);
}

///<summary>Получение сообщения по UDP</summary>
///<param name="s"> - сокет</param>
///<param name="data"> - буффер с данными</param>
///<param name="len"> - длина буффера данных</param>
///<param name="pczRemoteIP"> - возвращаемый IP</param>
///<param name="usRemotePort"> - возвращаемый порт</param>
///<returns>размер сообщения в байтах</returns>
int RecvByUDP(SOCKET s, char* data, int len, const char*& pczRemoteIP, unsigned short& usRemotePort)
{
	int iAdrLen = sizeof(sockaddr_in);
	sockaddr_in remoteHost;

	int iSize = recvfrom(s, data, len, 0, (struct sockaddr*)&remoteHost, (socklen_t*)&iAdrLen);
	if (iSize == SOCKET_ERROR)
	{
		int iError = WSAGetLastError();
		if (iError != WSAEWOULDBLOCK && iError != WSAENOTSOCK && iError != WSAECONNRESET && iError != WSAEMSGSIZE)
			int iError = errno;
		if (iError != EWOULDBLOCK && iError != ENOTSOCK && iError != ECONNRESET && iError != EMSGSIZE)
			return -1;	//Socket error!
		else
			return 0;	//No data
	}
	else if (iSize == 0)
		return -1;

	pczRemoteIP = inet_ntoa(remoteHost.sin_addr);
	usRemotePort = ntohs(remoteHost.sin_port);

	return iSize;
}

///<summary>удаление элемента вектора по заданной позициейу</summary>
///<param name="v"> - вектор</param>
///<param name="pos"> - позиция удаления</param>
void deleteElem(Vec& v, int pos)
{
	v[pos] = v.back();
	v.pop_back();
}

/// <returns>true если всё ещё запускаем</returns>
bool isStartup(system_clock::time_point startupTime, map<int, system_clock::time_point>& historyTime) {
	const int64_t startupDuration = 600;
	const int64_t historyTimeout = 1500;
	int64_t timeSinceStartup = duration_cast<milliseconds>(system_clock::now() - startupTime).count();

	if (timeSinceStartup > startupDuration) {
		bool aliveHistory = false;
		for (auto& it : historyTime)
			aliveHistory |= duration_cast<milliseconds>(system_clock::now() - it.second).count() < historyTimeout;
		return aliveHistory;
	}
	return true;
}

///<summary>Объединение историй присылаемых</summary>
void mergeHistories(map<int, vector<MessageRecord>>& histories) {
	for (auto& it : histories)
		messageHistory.insert(messageHistory.end(), it.second.begin(), it.second.end());
	sort(messageHistory.begin(), messageHistory.end(), [](const auto& a, const auto& b) { return a.timestamp < b.timestamp; });

	int j = 0;
	for (int i = 1; i < messageHistory.size(); i++) {
		if (messageHistory[i].timestamp != messageHistory[j].timestamp ||
			messageHistory[i].nickname != messageHistory[j].nickname ||
			messageHistory[i].message != messageHistory[j].message)
			messageHistory[++j] = messageHistory[i];
	}
	if (messageHistory.size() > 0)
		messageHistory.resize(++j);
}

void printMessage(string nickname, string message, int64_t timestamp) {
	int s = timestamp / 1000;
	int m = s / 60 % 60;
	int h = s / 3600 % 24;
	s = s % 60;
	cout << '[' << setfill('0')
		<< setw(2) << h << ':'
		<< setw(2) << m << ':'
		<< setw(2) << s << ']'
		<< nickname << ": \"" << message << "\"" << endl;
}

///<summary>Ожидание ответа и историй а*уительных</summary>
///<param name="lastMsgTime"> - время последнего полученного сообщения привета</param>
void startup(map<int, system_clock::time_point>& lastMsgTime, SOCKET s, sockaddr_in& remoteAddr) {
	system_clock::time_point start = system_clock::now();
	map<int, system_clock::time_point> historyTime;
	map<int, vector<MessageRecord>> histories;
	const char* remoteIP;
	uint16_t usRemotePort;

	SendHelloMessage(s, &remoteAddr, true);

	while (isStartup(start, historyTime))
	{
		SendHeartbeatMessage(s, &remoteAddr);
		int packSize = RecvByUDP(s, g_pczRecvBuffer, RECV_BUF_SIZE, remoteIP, usRemotePort); // Receive
		if (packSize == SOCKET_ERROR) continue;

		BaseMsg* pMessage = reinterpret_cast<BaseMsg*>(g_pczRecvBuffer);
		if (MyID == pMessage->uiHostID) continue;
		lastMsgTime[pMessage->uiHostID] = system_clock::now();

		switch (pMessage->uiType) {
			case MSG_HELLO: case MSG_PING:
			{
				first = false;
			} break;
			case MSG_HISTOTY_BEGIN:
			{
				cout << "Start history receiving" << endl;
				historyTime[pMessage->uiHostID] = system_clock::now();
				histories[pMessage->uiHostID];
			} break;
			case MSG_HISTORY_END:
			{
				cout << "End history receiving" << endl;
				historyTime.erase(pMessage->uiHostID);
			} break;
			case MSG_HISTORY:
			{
				HistoryMsg& msg = *static_cast<HistoryMsg*>(pMessage);
				histories[msg.uiHostID].push_back(MessageRecord(msg.nickname, msg.message, msg.timestamp));
				historyTime[msg.uiHostID] = system_clock::now();
			} break;
		}
	}

	mergeHistories(histories);

	for (auto& it : messageHistory)
		printMessage(it.nickname, it.message, it.timestamp);
}

///<returns>сала поїш</returns>
bool checkArguments(int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		if (saveHistoryArg == argv[i])
			return true;
	}
	return false;
}

int main(int argc, char* argv[])
{
	// флаг, говорящий о том, сохраняем ли мы историю сообщений
	saveHistory = checkArguments(argc, argv); //	-save

	// NOTE: Print tutorial hints
	cout << "****************************************************************" << endl;
	cout << "************************* UDP Simple Chat **********************" << endl;
	cout << "****************************************************************" << endl << endl;
	cout << "!!! Press Esc before closing this window !!!\n\n";

	WSADATA w;
	map<int, system_clock::time_point> lastMsgTime;
	int lenDel = -1;

	/*printf("\nPress any key to exit!");
	getch();

	BOOL ret = SetConsoleCtrlHandler(HandlerRoutine, TRUE);*/


	//Initialize Windows Sockets API and Locate proper DLL return 0 if good.
	if (WSAStartup(MAKEWORD(2, 2), &w) != 0)
	{
		// if error:
		printf("WSAStartup: Initialization Error.\n");
		return -1;
	}

	//Create the Sender Socket (UDP protocol) and verify that it was created
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
	{
		// if error:
		printf("socket: Socket Creation Error.\n");
		return -1;
	}

	/*
	int iBufSize = 8192;
	int result = setsockopt(m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&iBufSize, sizeof(int));
	result = setsockopt(m_socket, SOL_SOCKET, SO_RCVBUF, (char*)&iBufSize, sizeof(int));
	*/

	// Bind the Socket to an address and register it to the network
	sockaddr_in localAddr;
	localAddr.sin_family = AF_INET;				// Address type in Internet
	localAddr.sin_addr.s_addr = ADDR_ANY;
	//localAddr.sin_addr.s_addr = inet_addr("192.168.57.1");
	localAddr.sin_port = htons(APP_PORT);		// Port number
	memset(localAddr.sin_zero, 0, 8);

	int mode = 1;
	// Make it possible to reuse local address when bind() on another process
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&mode, sizeof(int));
	// Activate socket's broadcast mode
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&mode, sizeof(int));

	if (bind(s, (LPSOCKADDR)&localAddr, sizeof(localAddr)) == SOCKET_ERROR)
	{
		// if error:
		printf("bind: Address Error.\n");
		return -1;
	}

	// NOTE: Save remote address structure
	sockaddr_in remoteAddr;
	remoteAddr.sin_family = AF_INET;                            // Address type is Internet
	remoteAddr.sin_addr.s_addr = inet_addr("255.255.255.255");	// Use broadcast address to send messages. All clients will receive it.
	remoteAddr.sin_port = htons(APP_PORT);                      // Port number
	memset(remoteAddr.sin_zero, 0, 8);

	// Switch the socket to non-blocking mode
	u_long arg = 1;
	if (ioctlsocket(s, FIONBIO, &arg) != 0)
	{
		// if error:
		printf("ioctlsocket: Could not set the socket to non-blocking mode!\n");
		closesocket(s);
		s = INVALID_SOCKET;
		return -1;
	}

	// NOTE: Main application loop

	int iAdrLen = sizeof(sockaddr_in);
	int iSize = 0;
	sockaddr_in remoteHost;

	// Initialize randon number generator with a unique value
	srand(GetTickCount());
	//MyID = GetCurrHostID();
	cout << "Current HostID=" << MyID << endl;
	nickName = GetNick();
	cout << "Usage: press 'S' to send message, press ESC to quit ..." << std::endl;

	int Count;
	UserVerification();
	startup(lastMsgTime, s, remoteAddr);
	//helloStart = system_clock::now();

	while (true)
	{
		SendHeartbeatMessage(s, &remoteAddr);
		// Process input
		if (_kbhit())
		{
			int iKey = _getch();
			if (iKey == 27)	// ESC
			{
				cout << "ESC key pressed. Terminating application ..." << endl;
				Count = GetUserCount();
				SetUserCount(Count, 1);
				break;
			}
			else if (iKey == 's' || iKey == 'S' || iKey == 'ы' || iKey == 'Ы')
			{
				char message[1024];
				cout << "input message:\n>>> ";
				cin.getline(message, 256);
				TextMsg msg;
				if (string(message) == "")
					cin.getline(message, 256);

				strncpy(msg.message, message, 256);
				strncpy(msg.nickname, nickName.c_str(), nickName.size());
				msg.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
				SendUDPMessage(s, &remoteAddr, msg);

				if (saveHistory) {
					MessageRecord rec(nickName, message, msg.timestamp);
					messageHistory.push_back(rec);
				}
			}
		}

		// системное время
		SYSTEMTIME st;
		const char* remoteIP;
		unsigned short usRemotePort;
		int pos;
		bool flagDel = false;
		// Receive UDP data
		iSize = RecvByUDP(s, g_pczRecvBuffer, RECV_BUF_SIZE, remoteIP, usRemotePort);

		if (iSize == SOCKET_ERROR)
		{
			for (auto it = lastMsgTime.cbegin(); it != lastMsgTime.cend(); ) {
				int64_t diff = duration_cast<milliseconds>(system_clock::now() - it->second).count();
				if (diff < timeoutTime)
					++it;
				// если мы обнаружили таймаут, сообщаем, что пользователь удален, 
				// и устанавливаем в 1 флаг, говорящий о том, что его можно удалить из вектора
				else {
					cout << "User #" << it->first << " deleted." << endl;
					lastMsgTime.erase(it++);
					flagDel = true;
				}
			}

			if (flagDel) {
				cout << "Refreshed array after delete:" << endl;
				for (auto it = lastMsgTime.cbegin(); it != lastMsgTime.cend(); it++) {
					int64_t diff = duration_cast<milliseconds>(system_clock::now() - it->second).count();
					cout << "UserID: " << it->first << " ";
					cout << "Time since msg: " << diff / 1000.0 << " sec";
				}
			}

			int iError = WSAGetLastError();
			if (iError != WSAEWOULDBLOCK) // WSAEWOULDBLOCK just means that there's no data on non-blocking socket
				break;
		}
		else // NOTE: Data is here
		{
			BaseMsg* pMessage = reinterpret_cast<BaseMsg*>(g_pczRecvBuffer);
			if (MyID == pMessage->uiHostID)
				continue;


			switch (pMessage->uiType)
			{
				// если приходит пинг
				case MSG_PING:
				{
					// обновляем время последнего сообщения
					lastMsgTime[pMessage->uiHostID] = system_clock::now();
				} break;

				case MSG_TEXT: // NOTE: Ownship position replied
				{
					TextMsg* pMsg = static_cast<TextMsg*>(pMessage);
					//const char* pczSenderAddr = inet_ntoa(remoteHost.sin_addr);
					// если отправитель получает свое же сообщение
					if (pMsg->nickname == nickName)
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);

					// если флаг сохранения установлен в 1, то сохраняем сообщение
					if (saveHistory) {
						MessageRecord rec(pMsg->nickname, pMsg->message, pMsg->timestamp); // TODO: GET REAL TIME
						messageHistory.push_back(rec);
					}

					// вывод полученного сообщения
					printMessage(pMsg->nickname, pMsg->message, pMsg->timestamp);
					//cout << (pMsg->nickname == nickName ? "Sent" : "Received")
					//	<< " text message from HostID = " << pMsg->uiHostID
					//	<< " [" << remoteIP << ":" << usRemotePort << "] "
					//	<< "NickName=" << pMsg->nickname << ": \"" << pMsg->message << "\"" << endl;

					// если отправитель получает свое же сообщение
					if (string(pMsg->nickname) == string(nickName))
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE));
				} break;

				case MSG_HELLO:
				{
					HelloMsg& msg = *static_cast<HelloMsg*>(pMessage);
					lastMsgTime[msg.uiHostID] = system_clock::now();
					cout << "HELLO FROM " << msg.uiHostID << endl;
					if (msg.first) {
						SendHelloMessage(s, &remoteAddr, false);

						if (saveHistory)
						{
							// я хранитель переписки и даю ее крестьянам!
							// Метка начала
							cout << "Start history sharing" << endl;
							HistoryMarkMsg markMsg(false);
							SendUDPMessage(s, &remoteAddr, markMsg);

							// Переписка
							for (const auto& record : messageHistory) {
								HistoryMsg msg;

								// посылаем историю переписки |Текст переписки|Имя|Время| 
								strncpy(msg.message, record.message.c_str(), 256);
								strncpy(msg.nickname, record.nickname.c_str(), 256);
								msg.timestamp = record.timestamp;
								// отправка истории переписки
								SendUDPMessage(s, &remoteAddr, msg);
							}

							// Метка конца
							markMsg = HistoryMarkMsg(true);
							SendUDPMessage(s, &remoteAddr, markMsg);
							cout << "End history sharing" << endl;
						}
					}
				} break;
			}
		}

		// Need to sleep in order to not overload CPU 
		Sleep(50);
	}

	// Close the socket
	closesocket(s);

	// Drop table
	/*sqlite3_open("myDB.db", &db);
	rc = sqlite3_exec(db, "Drop Table CountUsers", NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		cout << "Error insert: " << err << endl;
	}
	sqlite3_close_v2(db);*/

	// Free Winsock 
	if (WSACleanup() == SOCKET_ERROR)
	{
		// if error:
		printf("WSACleanup: Error.\n");
		return -1;
	}

	return 0;
}