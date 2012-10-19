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
#include <cstdio>

#include "..\common\J2534.h"

void usage()
{
	printf(
		"logs (sniffs) CAN data.\n\n"
		"canlogger {logfile} {switches}\n\n"
		"    [outfile]          log file to write (or STDOUT if not present)\n"
		"    /b [baudrate]		CAN baud rate to use (500000 is the default)\n"
		"    /p {can,iso15765}  protocol to use (can (default) passes all IDs, iso15765 only looks at 0x07E0 and 0x07E8\n"
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

void dump_msg_to_file(PASSTHRU_MSG* msg)
{
	if (msg->RxStatus & START_OF_MESSAGE)
		return; // skip

	fprintf(fpo,"[%u] ",msg->Timestamp);
	for (unsigned int i = 0; i < msg->DataSize; i++)
		fprintf(fpo,"%02X ",msg->Data[i]);
	fprintf(fpo,"\n");
}

void dump_msg_to_stdio(PASSTHRU_MSG* msg)
{
	if (msg->RxStatus & START_OF_MESSAGE)
		return; // skip

	printf("[%u] ",msg->Timestamp);
	for (unsigned int i = 0; i < msg->DataSize; i++)
		printf("%02X ",msg->Data[i]);
	printf("\n");
}

int _tmain(int argc, _TCHAR* argv[])
{
	char* outfile = NULL;
	unsigned int protocol = CAN;
	unsigned int baudrate = 500000;

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

				if (strcmp(argv[argi],"can") == 0)
					protocol = CAN;
				else if (strcmp(argv[argi],"iso15765") == 0)
					protocol = ISO15765;
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
			else
				usage();
		}
		else
		{
			if (!outfile)
				outfile = argv[argi];
			//else
			//	usage();
		}
	}
	if (outfile)
	{
		if (NULL == (fpo = fopen(outfile,"wb")))
		{
			printf("can't open output file.\n");
			return 0;
		}
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

	// use SNIFF_MODE to listen to packets without any acknowledgement or flow control
	// use CAN_ID_BOTH to pick up both 11 and 29 bit CAN messages
	if (j2534.PassThruConnect(devID,protocol,SNIFF_MODE|CAN_ID_BOTH,baudrate,&chanID))
	{
		reportJ2534Error();
		return 0;
	}

	// now setup the filter(s)
	PASSTHRU_MSG rxmsg,txmsg;
	PASSTHRU_MSG msgMask,msgPattern,msgFlow;
	unsigned long msgId;
	unsigned long numRxMsg;

	if (protocol == CAN)
	{
		// in the CAN case, we simply create a "pass all" filter so that we can see
		// everything unfiltered in the raw stream

		txmsg.ProtocolID = protocol;
		txmsg.RxStatus = 0;
		txmsg.TxFlags = ISO15765_FRAME_PAD;
		txmsg.Timestamp = 0;
		txmsg.DataSize = 4;
		txmsg.ExtraDataIndex = 0;
		msgMask = msgPattern  = txmsg;
		memset(msgMask.Data,0,4); // mask the first 4 byte to 0
		memset(msgPattern.Data,0,4);// match it with 0 (i.e. pass everything)
		if (j2534.PassThruStartMsgFilter(chanID,PASS_FILTER,&msgMask,&msgPattern,NULL,&msgId))
		{
			reportJ2534Error();
			return 0;
		}
	}
	else
	{
		// in the ISO15765 case, we need to create specific flow control filters for the
		// source and destination IDs we would like to listen to (0x07E0 and 0x07E8)

		txmsg.ProtocolID = protocol;
		txmsg.RxStatus = 0;
		txmsg.TxFlags = ISO15765_FRAME_PAD;
		txmsg.Timestamp = 0;
		txmsg.DataSize = 4;
		txmsg.ExtraDataIndex = 0;
		msgMask = msgPattern = msgFlow = txmsg;
		memset(msgMask.Data,0xFF,txmsg.DataSize);

		msgPattern.Data[0] = 0x00;
		msgPattern.Data[1] = 0x00;
		msgPattern.Data[2] = 0x07;
		msgPattern.Data[3] = 0xE0;
		msgFlow.Data[0] = 0x00;
		msgFlow.Data[1] = 0x00;
		msgFlow.Data[2] = 0x07;
		msgFlow.Data[3] = 0xE8;

		if (j2534.PassThruStartMsgFilter(chanID,FLOW_CONTROL_FILTER,&msgMask,&msgPattern,&msgFlow,&msgId))
		{
			reportJ2534Error();
			return 0;
		}

		msgPattern.Data[0] = 0x00;
		msgPattern.Data[1] = 0x00;
		msgPattern.Data[2] = 0x07;
		msgPattern.Data[3] = 0xE8;
		msgFlow.Data[0] = 0x00;
		msgFlow.Data[1] = 0x00;
		msgFlow.Data[2] = 0x07;
		msgFlow.Data[3] = 0xE0;

		if (j2534.PassThruStartMsgFilter(chanID,FLOW_CONTROL_FILTER,&msgMask,&msgPattern,&msgFlow,&msgId))
		{
			reportJ2534Error();
			return 0;
		}
	}

	// continuously poll for new messages, waiting up to 1000ms for a new one
	// if someone hits a key, quit.

	unsigned int msgCnt = 0;
	time_t last_status_update = time(NULL);

	printf("Logging. Press any key to exit...\n");
	while (!_kbhit())
	{
		numRxMsg = 1;
		j2534.PassThruReadMsgs(chanID,&rxmsg,&numRxMsg,1000);
		if (numRxMsg)
		{
			if (outfile)
			{
				dump_msg_to_file(&rxmsg);
			}
			else
			{
				dump_msg_to_stdio(&rxmsg);
			}
			msgCnt++;
			if (time(NULL) - last_status_update > 0)
			{
				last_status_update = time(NULL);
				printf("messages received: %d\r",msgCnt);
			}
		}
	}

	if (outfile)
	{
		fclose(fpo);
	}

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
