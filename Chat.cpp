#include "defines.h"

void Delay(int seconds)
{
#if defined(linux) || defined(__linux)
	sleep(seconds);
#else
	Sleep(seconds*1000);
#endif
}
void Error(string message) {
	cout << "[ERROR] " << message << " failed\r\n";
	Delay(1);
}
void Start() {
#if defined(_WIN32) || defined(_WIN64)
	WSADATA WSAData;
	if (WSAStartup(0x202, &WSAData)) 
		Error("starting");
#endif
}
void End() {
#if defined(_WIN32) || defined(_WIN64)
	WSACleanup();
#endif
}

in_addr FindNetInterface()
{
	int nAdapter; 
	sockaddr_in address;
	hostent *sh = gethostbyname(0);  
    
	for (nAdapter = 0; sh->h_addr_list[nAdapter] != 0; ++nAdapter);
    if (nAdapter <= 0) 
		Error("search for network interfaces");
    memcpy(&address.sin_addr, sh->h_addr_list[0], sh->h_length);
	cout << "Used Interface: " << inet_ntoa(address.sin_addr) << ":" << address.sin_port << "\r\n\r\n";

	return address.sin_addr;
}

void CloseSocket(SOCKET socket)
{
	shutdown(socket, 2);
#if defined(linux) || defined(__linux)
	close(socket);
#else
	closesocket(socket);
#endif
}
void Setsockopt(SOCKET socket, int level, int optname, int* optval, int optlen)
{
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(socket, level, optname, (char*)optval, optlen);
#else
    setsockopt(socket, level, optname, optval, (socklen_t)optlen);
#endif
}
void CreateSendSocket()
{
	if ((sockSend = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)
        Error("sending socket creation");
    Setsockopt(sockSend, IPPROTO_IP, IP_MULTICAST_TTL,  &optval, sizeof(optval));
    Setsockopt(sockSend, IPPROTO_IP, IP_MULTICAST_LOOP, &optval, sizeof(optval));	
}
void CreateRecvSocket()
{
	if ((sockRecv = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0)   
		Error("receiving socket creation");
	Setsockopt(sockRecv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	bind(sockRecv, (sockaddr*)&localAddress, sizeof(localAddress));

	#if defined(_WIN32) || defined(_WIN64)
	setsockopt(sockRecv, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq));
	#else
	setsockopt(sockRecv, IPPROTO_IP, IP_ADD_MEMBERSHIP, mreq, sizeof(mreq));
	#endif	
}
void JoinToChat()
{
	char *joinMessage = new char[STDSIZE];
	
	cout << "Enter nickname: ";
	cin  >> me.name;
	fflush(stdin);

	sprintf(resetMessage, "%c%s", RESET, me.name);
	sprintf(joinMessage,  "%c%s", JOIN,  me.name);															// [JOIN][NAME]
	
	if (sendto(sockSend, joinMessage, STDSIZE, 0, (sockaddr*)&multiAddress, sizeof(multiAddress)) == 0)
		Error("sending");
	
	delete joinMessage;
}

void ParseMessage(string message)
{
	int pos;
	char type = message[0];
	user us;

	if (type == JOIN || type == LEFT || type == RESET)
		strcpy(us.name, message.substr(1, message.length()-1).c_str());
	else
	{
		type = MESSAGE;		
		pos = message.find(':');
		strcpy(us.name, message.substr(0, pos - 1).c_str());		
	}

	if (strcmp(us.name, me.name) == 0)
	{
		switch(type)
		{
	 		case JOIN: 
				us.timer = GetTickCount();
				users.push_back(us);	
				cout << "You have joined to chat\r\n";
				break;
			case MESSAGE:
				cout << message << "\r\n";
				for(int i=0; i<users.capacity(); i++)
					if (users[i].Compare(us) == true)
						users[i].timer = GetTickCount();
				break;
			case RESET:
				for(int i=0; i<users.capacity(); i++)
					if (users[i].Compare(us) == true)
						users[i].timer = GetTickCount();
				break;
			default  :	break;
		}
	}
	else 
		switch(type)
		{
			case JOIN: 
				us.timer = GetTickCount();
				users.push_back(us);
				cout << us.name << " have joined to chat" << "\r\n";
				break;
			case MESSAGE:
				for(int i=0; i<users.capacity(); i++)
					if (users[i].Compare(us) == true)
						users[i].timer = GetTickCount();
				cout << message << "\r\n";
				break;
			case RESET:
				for(int i=0; i<users.capacity(); i++)
					if (users[i].Compare(us) == true)
						users[i].timer = GetTickCount();
				break;
			case LEFT:
				for(int i=0; i<users.capacity(); i++)
					if (users[i].Compare(us) == true)
						users.erase(users.begin() + i);
				cout << us.name << " left the chat\r\n";
				break;
			default  :	break;
		}
}
int  ParseRequest(char *message)		// 0 - not send		// 1 - send		// 2 - exit
{
	if (strcmp(message + mesHeaderSize, "/online") == 0)
	{
		cout << "\r\n\r\nUsers Online";
		for(int i=0; i<users.capacity(); i++)
			cout << "\r\n" << users[i].name;
		cout << "\r\n\r\n";
		return 0;
	}

	if (strcmp(message + mesHeaderSize, "/exit") == 0)
	{
		CLOSE_CHAT = true;
		char *leftMessage = new char[STDSIZE];
		sprintf(leftMessage, "%c%s", LEFT, me.name);
		if (sendto(sockSend, leftMessage, STDSIZE, 0, (sockaddr*)&multiAddress, sizeof(multiAddress)) == 0)
			Error("sending");
		delete leftMessage;
		return 2;
	}

	return 1;
}
void CheckTimers()
{
	user us;
	DWORD currentTime = GetTickCount(); 

	for(int i = 1; i < users.capacity(); i++)
	{
		if (currentTime - users[i].timer > 600000)			// 10 minutes (minimum four RESET or one LEFT lost)
		{
			users.erase(users.begin() + i);
			cout << us.name << " left the chat\r\n";
		}
	}

	if (currentTime - users[0].timer > 120000)				// 2 minutes (RESET sents to all)
		if (sendto(sockSend, resetMessage, STDSIZE, 0, (sockaddr*)&multiAddress, sizeof(multiAddress)) == 0)
			Error("sending");
}

DWORD WINAPI SendThread(LPVOID NaN)
{
	char *message = new char[MSGSIZE];

	CreateSendSocket();
	JoinToChat();
	sprintf(message, "%s: ", me.name);		
	mesHeaderSize = strlen(message);	// 18

	while(true)
	{
		cout << "\r\n";
		gets(message + mesHeaderSize);
		
		if (ParseRequest(message) == 0)	continue;
		if (CLOSE_CHAT == true)	break;
		if (sendto(sockSend, message, MSGSIZE, 0, (sockaddr*)&multiAddress, sizeof(multiAddress)) == 0)
			Error("sending");
	}

	delete message;
	return 0;
}
DWORD WINAPI RecvThread(LPVOID NaN)
{
	char *message = new char[MSGSIZE];

	CreateRecvSocket();

	while(true)
	{
		if (recvfrom(sockRecv, message, MSGSIZE, 0, 0, 0) <= 0) 
			Error("receiving");
		if (CLOSE_CHAT)	break;
		ParseMessage(string(message));
		CheckTimers();
	}

	delete message;
	return 0;
}


int main() 
{
	Start();

	memset(&localAddress, 0, sizeof(localAddress));
	memset(&multiAddress, 0, sizeof(multiAddress));

	localAddress.sin_addr   = FindNetInterface();
	localAddress.sin_family = AF_INET;
	localAddress.sin_port   = htons(PORT);			

	multiAddress.sin_addr.s_addr = inet_addr(GROUP);
	multiAddress.sin_family = AF_INET;
	multiAddress.sin_port   = htons(PORT);

	mreq.imr_multiaddr.s_addr = inet_addr(GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	hThreads[0] = CreateThread(NULL, 0, SendThread, 0, 0, NULL);
	hThreads[1] = CreateThread(NULL, 0, RecvThread, 0, 0, NULL);

	WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);

	CloseHandle(hThreads[0]);
	CloseHandle(hThreads[1]);

	End();
    
    return 0;
}