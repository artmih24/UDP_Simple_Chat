#include <conio.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <regex>
#include <iostream>
#include <fstream>
#include <stdlib.h> 
#include <windows.h>
using namespace std;

HANDLE g_hEvent = 0;

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType) //  control signal type
{
	if (!g_hEvent)
		return FALSE;

	switch (dwCtrlType)
	{
	case CTRL_C_EVENT:
		printf("Ctrl+C pressed");
		SetEvent(g_hEvent);
		break;
	case CTRL_BREAK_EVENT:
		printf("Ctrl+Break pressed");
		SetEvent(g_hEvent);
		break;
	case CTRL_CLOSE_EVENT:
		printf("Close pressed");
		SetEvent(g_hEvent);
		break;
	case CTRL_LOGOFF_EVENT:
		printf("User logoff");
		SetEvent(g_hEvent);
		break;
	case CTRL_SHUTDOWN_EVENT:
		printf("System shutdown");
		SetEvent(g_hEvent);
		break;
	}

	return TRUE; // as we handle the event
}

// NOTE: link Winsock 2 library to the project

#pragma comment (lib, "ws2_32.lib")

#define APP_PORT 12345
#define RECV_BUF_SIZE 1024
string nickName = "";
//char* nickNameConverted;

unsigned GetCurrHostID()
{
	static int iAppUniqueID = rand();
	return unsigned(iAppUniqueID);
}

char* ConvertToChar(string line) {
	char* str = new char[1 + line.length()];
	return strcpy(str, line.c_str());
}
//
//string GetNick()
//{
//	if (nickName == "") {
//		cout << "Your nickname: ";
//		cin >> nickName;
//	}
//
//	nickNameConverted = ConvertToChar(nickName);
//	//regex nickNamePattern("([A-Za-z]{5, 20}|[A-Za-z0-9]{5, 20})");
//	//smatch sm;
//	//if (regex_match(nickName, sm, nickNamePattern))
//	//	cout << "ok";
//	//else
//	//	cout << "not ok";
//	return nickName;
//}

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
	fout.open("E:\\Artem\\VR_lab2\\lab2_udp_sample\\vs\\file.txt", ios::trunc);
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
	ifstream fin("E:\\Artem\\VR_lab2\\lab2_udp_sample\\vs\\file.txt");
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
enum eMsgType
{
	MSG_PING, MSG_TEXT
};

struct BaseMsg
{
	unsigned uiSize;
	unsigned uiHostID;
	unsigned uiType;
};

struct PingMsg : public BaseMsg
{
	char MsgPing[256];
	PingMsg() { memset(this, 0, sizeof(*this)); uiSize = sizeof(*this); uiType = MSG_PING; uiHostID = GetCurrHostID(); }
};

struct TextMsg : public BaseMsg
{
	char pczMsg[256];
	char pczMsgNickName[256];
	TextMsg() { memset(this, 0, sizeof(*this)); uiSize = sizeof(*this); uiType = MSG_TEXT; uiHostID = GetCurrHostID(); }
};

// Function to simplify UDP data send
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

void UserVerification(SOCKET s, sockaddr_in remoteAddr) {
	int Count = GetUserCount();

	if (Count == 0) {
		cout << "[PING] Hello, " << nickName << "! You're first, who joined to chat!" << endl;
	}
	else {
		cout << "[PING] Hello, " << nickName << "!" << endl;
		PingMsg pingMsg;
		strncpy(pingMsg.MsgPing, ConvertToChar(nickName), 30);
		SendUDPMessage(s, &remoteAddr, pingMsg);
	}
	SetUserCount(Count, 0);
}

void CloseConsole() {
	int Count = GetUserCount();
	SetUserCount(Count, 1);
}

// Define receive buffer
static char g_pczRecvBuffer[RECV_BUF_SIZE];


int main(int argc, char* argv[])
{
	// NOTE: Print tutorial hints
	cout << "****************************************************************" << endl;
	cout << "************************* UDP Simple Chat **********************" << endl;
	cout << "****************************************************************" << endl << endl;
	cout << "!!! Press Esc before closing this window !!!\n\n";

	WSADATA w;


	g_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);

	if (g_hEvent)
	{
		//printf("Waiting for the window to close...\n");
		WaitForSingleObject(g_hEvent, 10);
		CloseHandle(g_hEvent);
	}
	else
		printf("Error creating event\n");

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
	memset(localAddr.sin_zero, 0, 8);

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

	cout << "Current HostID=" << GetCurrHostID() << endl;
	nickName = GetNick();
	cout << "Usage: press 'S' to send sample message, press ESC to quit ..." << std::endl;

	int Count;
	UserVerification(s, remoteAddr);

	while (true)
	{
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
			else if (iKey == 's')
			{
				char message[1024];
				cout << "input message:\n>>> ";
				cin.getline(message, 1024);
				TextMsg msg;
				//strncpy(msg.pczMsg, "Hello! This is a test message.", 256);
				if (string(message) == "")
					cin.getline(message, 1024);
				strncpy(msg.pczMsg, message, 256);
				strncpy(msg.pczMsgNickName, ConvertToChar(nickName), 30);
				SendUDPMessage(s, &remoteAddr, msg);
			}
		}

		// Receive UDP data
		iSize = recvfrom(s, g_pczRecvBuffer, RECV_BUF_SIZE, 0, (struct sockaddr*)&remoteHost, (socklen_t*)&iAdrLen);

		if (iSize == SOCKET_ERROR)
		{
			int iError = WSAGetLastError();
			if (iError != WSAEWOULDBLOCK) // WSAEWOULDBLOCK just means that there's no data on non-blocking socket
				break;
		}
		else // NOTE: Data is here
		{
			BaseMsg* pMessage = reinterpret_cast<BaseMsg*>(g_pczRecvBuffer);

			switch (pMessage->uiType)
			{
				case MSG_PING:
				{
					PingMsg* pPingMsg = static_cast<PingMsg*>(pMessage);
					const char* pRecvNickName = pPingMsg->MsgPing;
					if (string(nickName) != string(pRecvNickName))
						cout << "[PING] User " << string(pRecvNickName) << " joined to chat!\n";
				} break;

				case MSG_TEXT: // NOTE: Ownship position replied
				{
					TextMsg* pMsg = static_cast<TextMsg*>(pMessage);
					const char* pczSenderAddr = inet_ntoa(remoteHost.sin_addr);
					if (string(pMsg->pczMsgNickName) == string(nickName)) 
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN); 
					cout << ((string(pMsg->pczMsgNickName) == string(nickName)) ? "Sent" : "Received") 
						<< " text message from HostID = " << pMsg->uiHostID
						<< " [" << pczSenderAddr << ":" << ntohs(remoteHost.sin_port) << "] "
						<< "NickName=" << pMsg->pczMsgNickName << ": \"" << pMsg->pczMsg << "\"" << endl;
					if (string(pMsg->pczMsgNickName) == string(nickName)) 
						SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE));
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



