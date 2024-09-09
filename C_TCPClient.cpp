#include "StdAfx.h"
#include "C_TCPClient.h"

//DECLARE_LOGER(Loger);

C_TCPClient::C_TCPClient(void)
{
	CString strLogInfo; strLogInfo.Format(_T("进入TCPClient"));
	//WRITE_LOG(Loger, 2, FALSE, _T("%s"), strLogInfo);

	//加载Winsock
	if (WSAStartup(MAKEWORD(2 , 2) , &m_wsData) != 0) 
	{
		TRACE(("===WSAStartup failed==="));
	}

	m_svrIP = 0;
	m_svrPort = 8888;
	m_tcpClientSock = INVALID_TCPSOCKET;
	m_pTCPClientCallback = NULL;
	m_act = 0;
}

C_TCPClient::~C_TCPClient(void)
{
	CString strLogInfo;strLogInfo.Format(_T("退出TCPClient"));
	//WRITE_LOG(Loger, 2, FALSE, _T("%s"), strLogInfo);

	if(m_tcpClientSock != INVALID_TCPSOCKET)
	{
		TCPCloseSocket(m_tcpClientSock);
		m_tcpClientSock = INVALID_TCPSOCKET;
	}
	m_pTCPClientCallback = NULL;
	m_act = 0;

	//清理Winsock
	WSACleanup(); 
}

//////////////////////////////////////////////////////////////////////////
BOOL C_TCPClient::InitClient(CString strIP, CString strPort, void* act, IClientTCPCallback* pTCPCallback)
{
	if(m_tcpClientSock != INVALID_TCPSOCKET) 
		return FALSE;
	m_svrIP = atol(strIP.GetBuffer());
	m_svrPort = atoi(strPort.GetBuffer());
	m_act = act;
	m_pTCPClientCallback = pTCPCallback;

	//
	m_tcpClientSock = TCPCreateSocket();
	if(m_tcpClientSock == INVALID_TCPSOCKET)
	{
		TCPCloseSocket(m_tcpClientSock);
		m_tcpClientSock = INVALID_TCPSOCKET;

		CString strLogInfo;strLogInfo.Format(_T("[InitClient]: %s"), _T("TCPCreateSocket 返回错误..."));
		//WRITE_LOG(Loger, 0, FALSE, _T("%s"), strLogInfo);
		return FALSE;
	}

	if(TCPConnect(m_tcpClientSock, this, m_act, m_svrIP, m_svrPort, INADDR_ANY, 0, this) == -1)
	{
		TCPCloseSocket(m_tcpClientSock);
		m_tcpClientSock = INVALID_TCPSOCKET;

		CString strLogInfo;strLogInfo.Format(_T("[InitClient]: %s "), _T("TCPConnect 返回错误..."));
		//WRITE_LOG(Loger, 0, FALSE, _T("%s"), strLogInfo);
		return FALSE;
	}

	CString strLogInfo;strLogInfo.Format(_T("[InitClient]: 初始化TCP客户端成功: Port:%d"), m_svrPort);
	//WRITE_LOG(Loger, 2, FALSE, _T("%s"), strLogInfo);
	return TRUE;
}

void C_TCPClient::UnInitClient()
{
	if(m_tcpClientSock != INVALID_TCPSOCKET)
	{
		HandleClose(m_tcpClientSock,NULL);
		m_tcpClientSock = INVALID_TCPSOCKET;
	}
}

BOOL C_TCPClient::SendNetMessage(int nMsgType, CString strMessage)
{
	BOOL bResult = FALSE;
	if (m_tcpClientSock != INVALID_TCPSOCKET)
	{
		int iBufLen = strMessage.GetLength() * sizeof(TCHAR);
		char* pBuffer = new char[iBufLen+1];
		memset(pBuffer, 0, iBufLen*sizeof(char));
		TCHAR* tBuf = (TCHAR*)strMessage.GetBuffer(0);
		memcpy(pBuffer, tBuf, iBufLen);
		//ParseUTF16BE(pBuffer, iBufLen);

		PNPHDR hdr;
		hdr.type = nMsgType;
		hdr.len = sizeof(hdr) + iBufLen;
		hdr.symbol = PNP_SYMBOL;
		hdr.msg = 0;//default
		hdr.off = sizeof(hdr);
		hdr.checksum = 0;
		hdr.checksum = Checksum((u_short*)&hdr, hdr.off);

		iovec vec[2];
		u_long ssize = sizeof(hdr)+iBufLen;
		vec[0].buf = (char*)&hdr;
		vec[0].len = sizeof(hdr);
		vec[1].buf = const_cast<char*>(pBuffer);
		vec[1].len = iBufLen;

		if(TCPBlockSend2(m_tcpClientSock, vec, 2, &ssize) == -1)
		{
			TCPCloseSocket(m_tcpClientSock);
			m_tcpClientSock = INVALID_TCPSOCKET;
			if (pBuffer)
			{
				delete[] pBuffer;
				pBuffer = NULL;
			}
			bResult = TRUE;
		}

		if (pBuffer)
		{
			delete[] pBuffer;
			pBuffer = NULL;
		}
		return TRUE;
	}

	return FALSE;
}

//////////////////////////////////////////////////////////////////////////
BOOL C_TCPClient::IsValidPacket_i(const char* buf, int len)
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

//////////////////////////////////////////////////////////////////////////
int  C_TCPClient::GetMsgBlock(const char* buf, int len)
{
	if(len < sizeof(PNPHDR)) return len;

	LPPNPHDR pHdr = (LPPNPHDR)buf;
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
void C_TCPClient::HandleAccept(TCPSOCKET s, void* act)
{
	//不实现
}
void C_TCPClient::HandleConnect(TCPSOCKET s, void* act, BOOL bConnected)
{
	if(m_pTCPClientCallback)
		m_pTCPClientCallback->HandleConnect(s, act, bConnected);
}
void C_TCPClient::HandleInput(TCPSOCKET s, void* act, char* buf, int len)
{
	if(m_pTCPClientCallback)
		m_pTCPClientCallback->HandleInput(s, act, buf, len);
		
}
void C_TCPClient::HandleOutput(TCPSOCKET s, void* act)
{
	//不实现
}
void C_TCPClient::HandleClose(TCPSOCKET s, void* act)
{
	if(m_pTCPClientCallback)
		m_pTCPClientCallback->HandleClose(s, act);
}
