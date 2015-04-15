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
	cout << "[ERROR] " << message << " failed" << "\r\n";
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
	cout << "Used IP: " << inet_ntoa(address.sin_addr) << "\r\n" << "\r\n";

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
	char *joinMessage = new char[MSGSIZE];
	
	cout << "Enter your name: ";
	cin >> me.name;
	fflush(stdin);
	strcpy(me.ip, inet_ntoa(localAddress.sin_addr));

	sprintf(joinMessage, "%d%s_%s", JOIN, me.ip, me.name);												// [TYPE][MY_IP]_[MY_NAME]
	if (sendto(sockSend, joinMessage, MSGSIZE, 0, (sockaddr*)&multiAddress, sizeof(multiAddress)) == 0)
		Error("sending");
	
	delete joinMessage;
}
void SendJoinResponse(user target)
{
	char *addMessage = new char[MSGSIZE];

	sprintf(addMessage, "%d%s_%s<%s>%s", ADD, me.ip, me.name, target.ip, target.name);					// [TYPE][MY_IP]_[MY_NAME]<[TARGET_IP]>[TARGET_NAME]	
	if (sendto(sockSend, addMessage, MSGSIZE, 0, (sockaddr*)&multiAddress, sizeof(multiAddress)) == 0)
		Error("sending");

	delete addMessage;
}

void ParseMessage(string message)
{
	int pos, pos2, pos3;
	bool myADD = false;
	messageType type;
	user us;

	type = (messageType)(message[0] - '0');

	if (type == MESSAGE)
	{
		pos = message.find('_');
		pos2 = message.find('>');
		strcpy(us.ip,	message.substr(1, pos - 1).c_str());
		strcpy(us.name, message.substr(pos + 1, pos2 - pos - 1).c_str());		
	}

	if (type == JOIN || type == LEFT)
	{
		pos = message.find('_');
		strcpy(us.ip,	message.substr(1, pos - 1).c_str());
		strcpy(us.name, message.substr(pos + 1, message.length() - pos - 1).c_str());
	}

	if (type == ADD)
	{
		pos = message.find('_');
		pos2 = message.find('<');
		pos3 = message.find('>');
		strcpy(us.ip,	message.substr(1, pos - 1).c_str());
		strcpy(us.name, message.substr(pos + 1, pos2 - pos).c_str());
		if (message.substr(pos2 + 1, pos3 - pos2 - 1) == me.ip &&
			message.substr(pos3 + 1, message.length() - pos3) == me.name)
			myADD = true;
	}
	
	if (strcmp(us.name, me.name) == 0)
	{
		switch(type)
		{
	 		case JOIN: 
				users.push_back(us);	
				cout << "You have joined to chat\r\n";
				break;
			case MESSAGE:
				break;
			default  :	break;
		}
	}
	else 
		switch(type)
		{
			case MESSAGE:
				break;
			case JOIN: 
				users.push_back(us);
				cout << us.name << " have joined to chat" << "\r\n";
				SendJoinResponse(us);
				break;
			case ADD :
				if (myADD)
					users.push_back(us);
				break;
			case LEFT:
				for(int i=0; i<users.capacity(); i++)
					if (users[i].Compare(us) == true)
						users.erase(users.begin() + i);
				cout << us.name << " have left the chat" << "\r\n";
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
		cout << "\r\n\r\n";
		return 0;
	}

	if (strcmp(message + mesHeaderSize, "/exit") == 0)
	{
		CLOSE_CHAT = true;
		char *leftMessage = new char[34];
		sprintf(leftMessage, "%d%s_%s", LEFT, me.ip, me.name);
		if (sendto(sockSend, leftMessage, MSGSIZE, 0, (sockaddr*)&multiAddress, sizeof(multiAddress)) == 0)
			Error("sending");
		delete leftMessage;
		return 2;
	}

	return 1;
}

DWORD WINAPI SendThread(LPVOID NaN)
{
	char *message = new char[MSGSIZE];

	CreateSendSocket();
	JoinToChat();
	sprintf(message, "%d%s_%s>", MESSAGE, me.ip, me.name);		// [TYPE][MY_IP]_[MY_NAME]>MESSAGE
	mesHeaderSize = strlen(message);

	while(true)
	{
		cout << "\r\n";
		gets(message + mesHeaderSize);
		cout << strlen(message) << "\r\n";	// TEST
		
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
		cout << strlen(message) << "\r\n";
		ParseMessage(string(message));
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
