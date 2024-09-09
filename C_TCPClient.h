#pragma once
#include "pnp.h"
#include "XWSock.h"
using namespace XWSock;

//////////////////////////////////////////////////////////////////////////
class IClientTCPCallback
{
public:
	virtual void HandleConnect(TCPSOCKET s, void* act, BOOL bConnected) = 0;
	virtual void HandleInput(TCPSOCKET s, void* act, char* buf, int len) = 0;
	virtual void HandleClose(TCPSOCKET s, void* act) = 0;
};

class C_TCPClient 
	: public ITCPHandler
	, public IMsgBlockStrategy
{
public:
	C_TCPClient(void);
	~C_TCPClient(void);

public:
	BOOL InitClient(CString strIP, CString strPort, void* act, IClientTCPCallback* pTCPCallback);
	void UnInitClient();
	BOOL SendNetMessage(int nMsgType, CString strMessage);

protected:
	BOOL IsValidPacket_i(const char* buf, int len);

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
	TCPSOCKET					m_tcpClientSock;
	IClientTCPCallback*			m_pTCPClientCallback;

	void*						m_act;
};

