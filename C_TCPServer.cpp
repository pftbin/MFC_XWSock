#include "StdAfx.h"
#include "C_TCPServer.h"

//DECLARE_LOGER(Loger);

C_TCPServer::C_TCPServer(void)
{
	CString strLogInfo;strLogInfo.Format(_T("进入TCPServer"));
	//WRITE_LOG(Loger, 2, FALSE, _T("%s"), strLogInfo);

	//加载Winsock
	if (WSAStartup(MAKEWORD(2 , 2) , &m_wsData) != 0) 
	{
		TRACE(("===WSAStartup failed==="));
	}

	m_svrIP = 0;
	m_svrPort = 8888;
	m_tcpServerSock = INVALID_TCPSOCKET;
	m_pTCPServerCallback = NULL;
	InitializeCriticalSection(&m_lockClientArray);
	m_VecClientArray.clear();
}

C_TCPServer::~C_TCPServer(void)
{
	CString strLogInfo;strLogInfo.Format(_T("退出TCPServer"));
	//WRITE_LOG(Loger, 2, FALSE, _T("%s"), strLogInfo);

	if (m_tcpServerSock != INVALID_TCPSOCKET)
	{
		TCPCloseSocket(m_tcpServerSock);
		m_tcpServerSock = INVALID_TCPSOCKET;
	}
	m_pTCPServerCallback = NULL;
	DeleteCriticalSection(&m_lockClientArray);
	m_VecClientArray.clear();

	//清理Winsock
	WSACleanup();  
}

//////////////////////////////////////////////////////////////////////////
BOOL C_TCPServer::InitServer(CString strIP, CString strPort,IServerTCPCallback* pTCPCallback)
{
	if(m_tcpServerSock != INVALID_TCPSOCKET)
		return TRUE;
	m_svrIP = atol(strIP.GetBuffer());
	m_svrPort = atoi(strPort.GetBuffer());
	m_pTCPServerCallback = pTCPCallback;

	//
	m_tcpServerSock = TCPCreateSocket();
	if(m_tcpServerSock == INVALID_TCPSOCKET)
	{
		TCPCloseSocket(m_tcpServerSock);
		m_tcpServerSock = INVALID_TCPSOCKET;

		CString strLogInfo;strLogInfo.Format(_T("[InitServer]: %s"), _T("TCPCreateSocket 返回错误..."));
		//WRITE_LOG(Loger, 0, FALSE, _T("%s"), strLogInfo);
		return FALSE;
	}

	int bNodelay = 1;
	TCPSetSockOpt(m_tcpServerSock, IPPROTO_TCP, TCP_NODELAY, (char*)&bNodelay, sizeof(bNodelay));
	if(TCPListen(m_tcpServerSock, this, NULL, 200, m_svrPort, m_svrIP) != 0)
	{
		TCPCloseSocket(m_tcpServerSock);
		m_tcpServerSock = INVALID_TCPSOCKET;

		CString strLogInfo;strLogInfo.Format(_T("[InitServer]: %s "), _T("TCPListen 返回错误..."));
		//WRITE_LOG(Loger, 0, FALSE, _T("%s"), strLogInfo);
		return FALSE;
	}

	CString strLogInfo;strLogInfo.Format(_T("[InitServer]: 初始化TCP服务端成功: Port:%d"), m_svrPort);
	//WRITE_LOG(Loger, 2, FALSE, _T("%s"), strLogInfo);
	return TRUE;
}

BOOL C_TCPServer::UnInitServer()
{
	if (m_tcpServerSock != INVALID_TCPSOCKET)
	{
		TCPCloseSocket(m_tcpServerSock);
		m_tcpServerSock = INVALID_TCPSOCKET;
	}

	return TRUE;
}



BOOL C_TCPServer::SendMsg2Client(void* sock, int nMsgType, CString strMessage)
{
	BOOL bResult = FALSE;
	if(m_tcpServerSock != INVALID_TCPSOCKET)
	{
		BOOL bFind = FALSE;
		ClientNode* pClient = (ClientNode*)sock;
		EnterCriticalSection(&m_lockClientArray);
		for (std::vector<ClientNode*>::iterator it = m_VecClientArray.begin(); it != m_VecClientArray.end(); ++it)
		{	
			if ((*it)->srcip == pClient->srcip)//Client IP Same
			{
				bFind = TRUE;
				break;
			}
		}
		LeaveCriticalSection(&m_lockClientArray);

		if(bFind && pClient)
		{
			if(pClient->sock.SendNetMessage(nMsgType,strMessage) != -1)
			{
				bResult = TRUE;
			}
			else
			{
				Sleep(100);
				if(pClient->sock.SendNetMessage(nMsgType,strMessage) != -1)
				{
					bResult = TRUE;
				}
			}
		}
	}

	return bResult;
}



//////////////////////////////////////////////////////////////////////////
BOOL C_TCPServer::IsValidPacket_i(const char* buf, int len)
{
	CString strLogInfo;
	BOOL bResult = TRUE;

	LPPNPHDR pHdr = (LPPNPHDR)buf;
	if(pHdr == NULL)				bResult &= FALSE;
	if(len < sizeof(PNPHDR))		bResult &= FALSE;
	if(pHdr->symbol != PNP_SYMBOL)	bResult &= FALSE;
	if(pHdr->off < sizeof(PNPHDR))	bResult &= FALSE;
	if(pHdr->off > pHdr->len)		bResult &= FALSE;
	if(pHdr->len > (u_int)len)		bResult &= FALSE;
	if(Checksum((u_short*)pHdr, pHdr->off) != 0)
	{
		strLogInfo.Format(_T("[IsValidPacket_i]: CheckSum 检查失败退出..."));
		//WRITE_LOG(Loger, 0, FALSE, _T("%s"), strLogInfo);

		bResult &= FALSE
	}
	strLogInfo.Format(_T("[IsValidPacket_i]: 消息检查结果[%d]"), bResult);
	//WRITE_LOG(Loger, 2, FALSE, _T("%s"), strLogInfo);

	return bResult;
}
CString C_TCPServer::GetTextIP(u_long uip)
{
	SOCKADDR_IN addr;
	addr.sin_addr.s_addr = uip;
	LPSTR str = inet_ntoa(addr.sin_addr);
	CString strIP;
	strIP = str;
	return strIP;
}

//////////////////////////////////////////////////////////////////////////
int  C_TCPServer::GetMsgBlock(const char* buf, int len)
{
	if(len < sizeof(PNPHDR))		return len;
	LPPNPHDR pHdr = (LPPNPHDR)buf;
	if(pHdr==NULL) return 0;

	if(pHdr->symbol != PNP_SYMBOL)	return len;
	if(pHdr->off < sizeof(PNPHDR))	return len;
	if(pHdr->off > pHdr->len)		return len;
	if((u_int)len < pHdr->off)		return 0;
	if((u_int)len < pHdr->len)		return 0;

	u_short oldsum = pHdr->checksum;
	pHdr->checksum = 0;
	if(Checksum((u_short*)pHdr, pHdr->off) != oldsum)
	{
		pHdr->checksum = oldsum;
		return len;
	}

	pHdr->checksum = oldsum;
	return pHdr->len;
}
void C_TCPServer::HandleAccept(TCPSOCKET s, void* act)
{
	ClientNode* pClient = new ClientNode;
	TCPSOCKET cs = TCPAccept(s, this, pClient, this);
	if(cs != INVALID_TCPSOCKET)
	{
		u_long ip = INADDR_ANY;
		u_short port = 0;

		TCPGetPeerAddr(cs, ip, port);
		pClient->sock.m_tcpClientSock = cs;
		pClient->sock.m_svrIP = ip;
		pClient->sock.m_svrPort = m_svrPort;
		pClient->srcip = ip;//客户端IP
		CString strClientIP = GetTextIP(pClient->srcip);

		TCPGetHostAddr(cs, ip, port);
		pClient->dstip = ip;
		pClient->port = port;

		EnterCriticalSection(&m_lockClientArray);
		try
		{
			//update
			std::vector<ClientNode*>::iterator it = m_VecClientArray.begin();
			for (it; it != m_VecClientArray.end(); ++it)
			{
				if ((*it)->srcip == pClient->srcip)//客户端IP相同
				{
					(*it)->sock.m_tcpClientSock  = pClient->sock.m_tcpClientSock;
					(*it)->sock.m_svrIP = pClient->sock.m_svrIP;
					(*it)->sock.m_svrPort = pClient->sock.m_svrPort;
					(*it)->srcip =	pClient->srcip;
					(*it)->dstip =	pClient->dstip;
					(*it)->port =	pClient->port;
					LeaveCriticalSection(&m_lockClientArray);
					return;
				}
			}

			//new
			m_VecClientArray.push_back(pClient);
			LeaveCriticalSection(&m_lockClientArray);
		}
		catch(...)
		{
			LeaveCriticalSection(&m_lockClientArray);
		}
	}
	else
	{
		delete pClient;
		pClient = NULL;
	}
}

void C_TCPServer::HandleConnect(TCPSOCKET s, void* act, BOOL bConnected)
{
	//不实现
}

void C_TCPServer::HandleInput(TCPSOCKET s, void* act, char* buf, int len)
{
	if (buf==NULL || len <=0) return;

	int nType = 0;
	if(IsValidPacket_i(buf, len))
	{
		LPPNPHDR pHdr = (LPPNPHDR)buf;
		len = len-pHdr->off;
		buf = buf+pHdr->off;
		nType = pHdr->type;
	}
	//ParseUTF16BE(buf, len);//temp delete

	int iCount = 0;
#ifdef _UNICODE
	iCount = len/2 + 1;
#else
	iCount = len + 2;
#endif	

	if (m_pTCPServerCallback)
	{
		TCHAR* pBuf = new TCHAR[iCount];
		memset(pBuf, 0, iCount * sizeof(TCHAR));
		memcpy(pBuf, buf, len);

		CString strMsg; strMsg = pBuf;
		m_pTCPServerCallback->HandleInput(act, nType, strMsg);

		delete[] pBuf;
	}
}


void C_TCPServer::HandleOutput(TCPSOCKET s, void* act)
{
	//不实现
}

void C_TCPServer::HandleClose(TCPSOCKET s, void* act)
{
	CString strIP;
	u_long uip;u_short port;
	if(TCPGetPeerAddr(s, uip, port) != -1)
	{
		SOCKADDR_IN addr;
		addr.sin_addr.s_addr = uip;
		strIP = inet_ntoa(addr.sin_addr);
	}

	ClientNode* pClient = (ClientNode*)act;
	if(TCPCloseSocket(s) != -1)
	{
		EnterCriticalSection(&m_lockClientArray);
		try
		{
			vector<ClientNode*>::iterator it = m_VecClientArray.begin();
			for (it; it != m_VecClientArray.end(); ++it)
			{
				if ((*it)->srcip == pClient->srcip)//Client IP Same
				{
					m_VecClientArray.erase(it);
					break;
				}
			}
			LeaveCriticalSection(&m_lockClientArray);
		}
		catch(...)
		{
			LeaveCriticalSection(&m_lockClientArray);
		}
	}
}

