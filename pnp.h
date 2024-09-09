#ifndef _PNP_H_
#define _PNP_H_

#define PNP_SYMBOL					0x1111

#define PNP_REGISTER				1
#define PNP_BROADCAST				2
#define PNP_HEARTBEAT				3
#define PNP_MOSMESSAGE				4

typedef struct tagPLAYOUTNODE {
	u_short		channel;
	u_short		studio;
	u_long		type;
}PLAYOUTNODE, *LPPLAYOUTNODE;

/*                          PNP header
*	 -------------------------------------------------------
*	|   version   |     type    |           symbol          |
*	 -------------------------------------------------------
*	|                          len                          |
*	 -------------------------------------------------------
*	|						   src                          |
*	 -------------------------------------------------------
*	|                          dst                          |
*	 -------------------------------------------------------
*	|                          msg                          |
*	 -------------------------------------------------------
*	|          checksum         |            off            |
*	 -------------------------------------------------------
*	|                          data                         |
*	|                          ....                         |
*	 -------------------------------------------------------
*/

typedef struct tagPNPHDR {
	u_char			ver;		// version
	u_char			type;		// type of packet
	u_short			symbol;		// symbol of RPC packet, it must equal 0x1111
	u_int			len;		// total length of packet
	PLAYOUTNODE		src;
	u_long			dst;
	u_long			msg;
	u_short			checksum;	// checksum of header
	u_short			off;		// data offset in the packet
}PNPHDR, *LPPNPHDR;

static void ParseUTF16BE(char* strUtf16be, int iLen)
{
	for (int i = 0; i < iLen-1 ; i+=2)
	{
		char ch = strUtf16be[i+1]; 
		strUtf16be[i+1] = strUtf16be[i];
		strUtf16be[i] = ch;
	}
}
static u_short Checksum(const u_short *buf, int size)
{
	u_long sum = 0;

	if(buf != (u_short *)NULL) 
	{
		while(size > 1) 
		{
			sum += *(buf ++);
			if(sum & 0x80000000)
			{
				sum = (sum & 0xffff) + (sum >> 16);
			}
			size -= sizeof(u_short);
		}    

		if(size)
		{
			sum += (u_short) * (const u_char*) buf;
		}
	}

	while(sum >> 16)
	{
		sum = (sum & 0xffff) + (sum >> 16);
	}
	return (u_short) ~sum;
}


#endif