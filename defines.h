#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <vector>
#if defined(_WIN32) || defined(_WIN64)
#include <ws2tcpip.h>
#include <iostream>
#include <conio.h>
#include <string>
#endif
#if defined(linux) || defined(__linux)
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
typedef int SOCKET;
typedef unsigned long DWORD;
#endif

#pragma comment(lib, "ws2_32.lib")
using namespace std;

#define PORT		5000
#define GROUP		"239.0.0.1"
#define MSGSIZE		256
#define STDSIZE		17

// Message types
#define JOIN		'0'
#define RESET		'1'
#define LEFT		'2'
#define GETIP		'3'
#define MESSAGE		'4'
#define WHISPER		'5'

bool CLOSE_CHAT = false;
char resetMessage[STDSIZE];

struct user 
{
	char name[16];
	DWORD timer;

public:	
	bool Compare(user other)
	{
		if (strcmp(other.name, name) == 0)
			return true;
		else return false;
	}
} me;


vector<user> users;

int optval = 1;
int mesHeaderSize;

SOCKET sockSend;
SOCKET sockRecv;

sockaddr_in localAddress;
sockaddr_in multiAddress;
ip_mreq mreq;

HANDLE hThreads[2];