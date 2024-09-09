#pragma once
#include "pnp.h"
#include "XWSock.h"
using namespace XWSock;

#include "C_TCPClient.h"
struct ClientNode 
{
	C_TCPClient		sock;
	u_long			dstip;
	u_long			srcip;
	u_short			port;
	CString			RecvMsg;
	CString			SendMsg;

	ClientNode()
	{
		dstip = 0;
		srcip = 0;
		port = 0;
		RecvMsg = _T("");
		SendMsg = _T("");
	}
};

//////////////////////////////////////////////////////////////////////////
class IServerTCPCallback
{
public:
	virtual void HandleConnect(TCPSOCKET s, void* act, BOOL bConnected) = 0;
	virtual void HandleInput(TCPSOCKET s, void* act, char* buf, int len) = 0;
	virtual void HandleClose(TCPSOCKET s, void* act) = 0;
};

class C_TCPServer:
	public ITCPHandler, 
	public IMsgBlockStrategy
{
public:
	C_TCPServer(void);
	virtual ~C_TCPServer(void);

	BOOL InitServer(CString strIP, CString strPort,IServerTCPCallback* pTCPCallback);
	BOOL UnInitServer();
	BOOL SendMsg2Client(void* sock, int nMsgType, CString strMessage);
	

protected:
	BOOL IsValidPacket_i(const char* buf, int len);
	CString GetTextIP(u_long uip);

protected:
	virtual int  GetMsgBlock(const char* buf, int len);
	virtual void HandleAccept(TCPSOCKET s, void* act);
	virtual void HandleConnect(TCPSOCKET s, void* act, BOOL bConnected);
	virtual void HandleInput(TCPSOCKET s, void* act, char* buf, int len);
	virtual void HandleOutput(TCPSOCKET s, void* act);
	virtual void HandleClose(TCPSOCKET s, void* act);

public:
	WSADATA						m_wsData;
	u_long						m_svrIP;
	u_short						m_svrPort;
	TCPSOCKET					m_tcpServerSock;
	IServerTCPCallback*			m_pTCPServerCallback;

	CRITICAL_SECTION			m_lockClientArray;
	std::vector<ClientNode*>	m_VecClientArray;
};
