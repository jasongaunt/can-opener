//////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2004- Tactrix Inc.
//
// You are free to use this file for any purpose, but please keep
// notice of where it came from!
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <tchar.h>
#include <windows.h>
#include <conio.h>
#include <time.h>

#include "..\common\J2534.h"

void usage()
{
	printf(
		"logs (sniffs) K/L/AUX data.\n\n"
		"klogger [logfile] {switches}\n\n"
		"    [logfile]          log file to write\n"
		"    /b [baudrate] baud rate to use\n"
		"    /p {none,odd,even} parity to use (defaults to none)\n"
		"    /c {k,l,aux} channel to use (defaults to K)\n"
		"    /t [timeout] timeout in ms to determine end of message (defaults to 20ms)\n"
		);
	exit(0);
}

J2534 j2534;
unsigned long devID;
unsigned long chanID;
FILE *fpo;

void reportJ2534Error()
{
	char err[512];
	j2534.PassThruGetLastError(err);
	printf("J2534 error [%s].",err);
}

void dump_msg(PASSTHRU_MSG* msg)
{
	if (msg->RxStatus & START_OF_MESSAGE)
		return; // skip

	fprintf(fpo,"[%u] ",msg->Timestamp);
	for (unsigned int i = 0; i < msg->DataSize; i++)
		fprintf(fpo,"%02X ",msg->Data[i]);
	fprintf(fpo,"\n");
}

int _tmain(int argc, _TCHAR* argv[])
{
	char* outfile = NULL;
	unsigned int protocol = ISO9141_K;
	unsigned int baudrate = 10400;
	unsigned int parity = NO_PARITY;
	unsigned int timeout = 20;

	for (int argi = 1; argi < argc; argi++)
	{
		if (argv[argi][0] == '/' || argv[argi][0] == '-')
		{
			// looks like a switch
			char *sw = &argv[argi][1];
			if (strcmp(sw,"p") == 0)
			{
				argi++;
				if (argi >= argc)
					usage();

				if (strcmp(argv[argi],"none") == 0)
					parity = NO_PARITY;
				else if (strcmp(argv[argi],"odd") == 0)
					parity = ODD_PARITY;
				else if (strcmp(argv[argi],"even") == 0)
					parity = EVEN_PARITY;
				else
					usage();
			}
			else if (strcmp(sw,"c") == 0)
			{
				argi++;
				if (argi >= argc)
					usage();

				if (strcmp(argv[argi],"k") == 0)
					protocol = ISO9141_K;
				else if (strcmp(argv[argi],"l") == 0)
					protocol = ISO9141_L;
				else if (strcmp(argv[argi],"aux") == 0)
					protocol = ISO9141_INNO;
				else
					usage();
			}
			else if (strcmp(sw,"b") == 0)
			{
				argi++;
				if (argi >= argc)
					usage();

				if (sscanf(argv[argi],"%d",&baudrate) != 1)
					usage();
			}
			else if (strcmp(sw,"t") == 0)
			{
				argi++;
				if (argi >= argc)
					usage();

				if (sscanf(argv[argi],"%d",&timeout) != 1)
					usage();
			}
			else
				usage();
		}
		else
		{
			if (!outfile)
				outfile = argv[argi];
			else
				usage();
		}
	}
	if (!outfile)
		usage();

	if (NULL == (fpo = fopen(outfile,"wb")))
	{
		printf("can't open output file.\n");
		return 0;
	}

	if (!j2534.init())
	{
		printf("can't connect to J2534 DLL.\n");
		return 0;
	}

	if (j2534.PassThruOpen(NULL,&devID))
	{
		reportJ2534Error();
		return 0;
	}

	// use ISO9141_NO_CHECKSUM to disable checksumming on both tx and rx messages
	if (j2534.PassThruConnect(devID,protocol,ISO9141_NO_CHECKSUM,baudrate,&chanID))
	{
		reportJ2534Error();
		return 0;
	}

	// set timing

	SCONFIG_LIST scl;
	SCONFIG scp[2] = {{P1_MAX,0},{PARITY,0}};
	scl.NumOfParams = 2;
	scp[0].Value = timeout * 2;
	scp[1].Value = parity;
	scl.ConfigPtr = scp;
	if (j2534.PassThruIoctl(chanID,SET_CONFIG,&scl,NULL))
	{
		reportJ2534Error();
		return 0;
	}


	// now setup the filter(s)
	PASSTHRU_MSG rxmsg,txmsg;
	PASSTHRU_MSG msgMask,msgPattern;
	unsigned long msgId;
	unsigned long numRxMsg;

	// simply create a "pass all" filter so that we can see
	// everything unfiltered in the raw stream

	txmsg.ProtocolID = protocol;
	txmsg.RxStatus = 0;
	txmsg.TxFlags = 0;
	txmsg.Timestamp = 0;
	txmsg.DataSize = 1;
	txmsg.ExtraDataIndex = 0;
	msgMask = msgPattern  = txmsg;
	memset(msgMask.Data,0,1); // mask the first 4 byte to 0
	memset(msgPattern.Data,0,1);// match it with 0 (i.e. pass everything)
	if (j2534.PassThruStartMsgFilter(chanID,PASS_FILTER,&msgMask,&msgPattern,NULL,&msgId))
	{
		reportJ2534Error();
		return 0;
	}

	// continuously poll for new messages, waiting up to 1000ms for a new one
	// if someone hits a key, quit.

	unsigned int msgCnt = 0;
	unsigned int byteCnt = 0;
	time_t last_status_update = time(NULL);

	printf("Logging. Press any key to exit...\n");
	while (!_kbhit())
	{
		numRxMsg = 1;
		j2534.PassThruReadMsgs(chanID,&rxmsg,&numRxMsg,1000);
		if (numRxMsg)
		{
			dump_msg(&rxmsg);
			msgCnt++;
			byteCnt += rxmsg.DataSize;
			if (time(NULL) - last_status_update > 0)
			{
				last_status_update = time(NULL);
				printf("messages received: %d total bytes: %d\r",msgCnt,byteCnt);
			}
		}
	}

	fclose(fpo);

	// shut down the channel

	if (j2534.PassThruDisconnect(chanID))
	{
		reportJ2534Error();
		return 0;
	}

	// close the device

	if (j2534.PassThruClose(devID))
	{
		reportJ2534Error();
		return 0;
	}
}
