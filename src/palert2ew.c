#ifdef _OS2
#define INCL_DOSMEMMGR
#define INCL_DOSSEMAPHORES
#include <os2.h>
#endif
/* Standard C header include */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <trace_buf.h>

/* Local header include */
#include <palert.h>
#include <palert2ew.h>
#include <palert2ew_list.h>
#include <palert2ew_socket.h>
#include <palert2ew_client.h>
#include <palert2ew_server.h>
#include <palert2ew_msg_queue.h>


/* Functions prototype in this source file
 *******************************/
static void palert2ew_config( char * );
static void palert2ew_lookup( void );
static void palert2ew_status( unsigned char, short, char * );
static void palert2ew_end( void );                /* Free all the local memory & close socket */

static thr_ret MessageReceiverF ( void * );  /* Read messages from the socket of forward server */
static thr_ret MessageReceiverP ( void * );  /* Read messages from the socket of Palerts */
static thr_ret UpdatePalertList ( void * );

static TRACE2_HEADER *enrich_tracebuf_header_pmode1( TRACE2_HEADER *, const _STAINFO *, const PALERTMODE1_HEADER * );
static int32_t       *copydata_to_traceubf_pmode1( TracePacket *, const PalertPacket *, const PALERTMODE1_CHANNEL );

/* Ring messages things */
#define WAVE_MSG_LOGO  0
#define RAW_MSG_LOGO   1

static  SHM_INFO  Region[2];      /* shared memory region to use for i/o    */

MSG_LOGO  Putlogo[2];             /* array for requesting module, type, instid */
pid_t     MyPid;                  /* for restarts by startstop               */

/* Thread things */
#define THREAD_STACK 8388608         /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0              /* Thread has not been started      */
#define THREAD_ALIVE  1              /* Thread alive and well            */
#define THREAD_ERR   -1              /* Thread encountered error quit    */
static volatile int8_t MessageReceiverFStatus = THREAD_OFF;
static volatile int8_t MessageReceiverPStatus[2] = { THREAD_OFF, THREAD_OFF };

/* Things to read or derive from configuration file
 **************************************************/
static char     RingName[2][MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint64_t QueueSize;                  /* max messages in output circular buffer */
static uint8_t  ServerSwitch;               /* 0 connect to Palert server; 1 as the server of Palert */
static uint8_t  RawOutputSwitch = 0;
static char     ServerIP[INET6_ADDRSTRLEN];
static char     ServerPort[8];
static uint64_t MaxStationNum;
static DBINFO   DBInfo;
static char     SQLStationTable[MAX_TABLE_LEGTH];
static char     SQLChannelTable[MAX_TABLE_LEGTH];

/* Things to look up in the earthworm.h tables with getutil.c functions
 **********************************************************************/
static int64_t RingKey[2];      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2 = 0;
static uint8_t TypePalertRaw = 0;

/* Error messages used by palert2ew
 *********************************/
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */
//static char Text[150];         /* string for log/error messages          */

static volatile void   *_Root  = NULL;
static volatile _Bool   Finish = 0;
static volatile uint8_t UpdateStatus = 0;

static int64_t TimeShift = 0;            /* Time difference between UTC & local timezone */

/*
 *
 */
int main ( int argc, char **argv )
{
	int  res;

	uint32_t i = 0;
	uint32_t count = 0;
	int64_t  msg_size = 0;

	time_t     timeNow;          /* current time                  */
	time_t     timeLastBeat;     /* time last heartbeat was sent  */
	time_t     timeLastCheck;    /* time last palert connection checking */
	struct tm *timeLocal;

	char   *lockfile;
	int32_t lockfile_fd;

	PACKET        packet  = { 0 };
	PalertPacket *palertp = (PalertPacket *)packet.data;
	_STAINFO     *staptr;
	_CHAINFO     *chaptr;
	TracePacket   tracebuffer;  /* message which is sent to share ring    */

#if defined( _V710 )
	ew_thread_t   tid[3];       /* Thread moving messages from transport to queue */
#else
	unsigned      tid[3];       /* Thread moving messages from transport to queue */
#endif
	const uint8_t number[2] = { 0, 1 };


/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: palert2ew <configfile>\n");
		exit(0);
	}
	Finish = 1;
	UpdateStatus = 0;

/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);
/* Read the configuration file(s) */
	palert2ew_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Read the station list from remote database */
	palert2ew_list_db_fetch( &_Root, SQLStationTable, SQLChannelTable, &DBInfo );
	palert2ew_list_root_switch( &_Root );
	if ( !palert2ew_list_total_station() ) {
		fprintf(stderr, "There is not any station in the list after fetching, exiting!\n");
		exit(-1);
	}

/* Look up important info from earthworm.h tables */
	palert2ew_lookup();
/* Reinitialize logit to desired logging level */
	logit_init(argv[1], 0, 256, LogSwitch);
	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "One instance of %s is already running, exiting!\n", argv[0]);
		exit(-1);
	}
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","palert2ew: Cannot get pid. Exiting!\n");
		exit(-1);
	}

/* Build the message */
	Putlogo[WAVE_MSG_LOGO].instid = InstId;
	Putlogo[WAVE_MSG_LOGO].mod    = MyModId;
	Putlogo[WAVE_MSG_LOGO].type   = TypeTracebuf2;
	Putlogo[RAW_MSG_LOGO].instid  = InstId;
	Putlogo[RAW_MSG_LOGO].mod     = MyModId;
	Putlogo[RAW_MSG_LOGO].type    = TypePalertRaw;

/* Attach to Output shared memory ring */
	for ( i=0; i<2; i++ ) {
		if ( RingKey[i] == -1 ) {
			Region[i].key = RingKey[i];
		}
		else {
			tport_attach( &Region[i], RingKey[i] );
			logit( "", "palert2ew: Attached to public memory region %s: %ld\n",
					&RingName[i][0], RingKey[i] );
		}
	}
/* Initialize the message queue */
	MsgQueueInit((unsigned long)QueueSize, &Region[RAW_MSG_LOGO], &Putlogo[RAW_MSG_LOGO]);

/* Force a heartbeat to be issued in first pass thru main loop */
	timeLastBeat  = time(&timeNow) - HeartBeatInterval - 1;
	timeLastCheck = timeNow;
/* Initialize the timezone shift */
	timeLocal = localtime(&timeNow);
	TimeShift = -timeLocal->tm_gmtoff;
/* Initialize Palert stations list */
	palert2ew_list_fetch( StationTable, &DBInfo );

/*----------------------- setup done; start main loop -------------------------*/
	while(1)
	{
	/* Send palert2ew's heartbeat */
		if  ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			palert2ew_status( TypeHeartBeat, 0, "" );
		}
	/* Start the message receiving thread if it isn't already running. */
		if ( ServerSwitch == 0 )
			check_thread_client();
		else
			check_thread_server();

	/* Process all new messages */
		count = 0;
		do {
		/* See if a termination has been requested */
			if ( tport_getflag( &Region[0] ) == TERMINATE ||
				tport_getflag( &Region[0] ) == MyPid ) {
			/* write a termination msg to log file */
				logit( "t", "palert2ew: Termination requested; exiting!\n" );
				fflush( stdout );
			/* should check the return of these if we really care */
				Finish = 0;
				sleep_ew(500);
			/* detach from shared memory */
				palert2ew_end();
				ew_unlockfile(lockfile_fd);
				ew_unlink_lockfile(lockfile);
				exit( 0 );
			}

			if ( (res = MsgDequeue(&packet, &msg_size)) < 0 )
				break;
			else
				count++;

		/* Process the message */
			if ( palertp->pah.packet_type[0] & 0x01 ) {
			/* Common part */
				staptr = (_STAINFO *)packet.sptr;
				enrich_tracebuf_header_pmode1( &tracebuffer.trh2, staptr, &palertp->pah );
				msg_size = ((tracebuffer.trh2.nsamp) << 2) + sizeof(TRACE2_HEADER);
			/* Each channel part */
				chaptr = (_CHAINFO *)staptr->chaptr;
				for ( i = 0; i < staptr->nchannels; i++, chaptr++ ) {
					strcpy(tracebuffer->trh2.chan, chaptr->chan);
					copydata_to_traceubf_pmode1( &tracebuffer, palertp, chaptr->seq );
					if ( tport_putmsg(&Region[WAVE_MSG_LOGO], &Putlogo[WAVE_MSG_LOGO], msg_size, tracebuffer.msg) != PUT_OK ) {
						logit("e", "palert2ew: Error putting message in region %ld\n", Region[WAVE_MSG_LOGO].key);
					}
				}
			}
		} while ( count < MaxStationNum );  /* end of message-processing-loop */
		sleep_ew( 50 );  /* no more messages; wait for new ones to arrive */
	}
/*-----------------------------end of main loop-------------------------------*/
	Finish = 0;
	sleep_ew(500);
	palert2ew_end();

	return 0;
}

/*
 * palert2ew_config() - processes command file(s) using kom.c functions;
 *                      exits if any errors are encountered.
 */
static void palert2ew_config( char *configfile )
{
	char  init[16];     /* init flags, one byte for each required command */
	char *com;
	char *str;
	int   ncommand;     /* # of required commands you expect to process   */
	int   nmiss;        /* number of required commands that were missed   */
	int   nfiles;
	int   success;
	int   i;

/* Set to zero one init flag for each required command */
	ncommand = 14;
	for( i = 0; i < ncommand; i++ ) {
		if ( i < 9 )
			init[i] = 0;
		else
			init[i] = 1;
	}

/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e","palert2ew: Error opening command file <%s>; exiting!\n", configfile);
		exit(-1);
	}

/* Process all command files */
	while ( nfiles > 0 )   /* While there are command files open */
	{
		while ( k_rd() )        /* Read next line from active file  */
		{
			com = k_str();         /* Get the first token from line */
		/* Ignore blank lines & comments
		 *******************************/
			if ( !com )          continue;
			if ( com[0] == '#' ) continue;
		/* Open a nested configuration file
		 **********************************/
			if ( com[0] == '@' ) {
				success = nfiles + 1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "palert2ew: Error opening command file <%s>; exiting!\n", &com[1]);
					exit(-1);
				}
				continue;
			}

		/* Process anything else as a command
		 ************************************/
		/* 0 */
			if( k_its("LogFile") ) {
				LogSwitch = k_int();
				init[0] = 1;
			}
		/* 1 */
			else if( k_its("MyModuleId") ) {
				str = k_str();
				if(str) strcpy( MyModName, str );
				init[1] = 1;
			}
		/* 2 */
			else if( k_its("OutWaveRing") ) {
				str = k_str();
				if(str) strcpy( &RingName[WAVE_MSG_LOGO][0], str );
				init[2] = 1;
			}
			else if( k_its("OutRawRing") ) {
				str = k_str();
				if(str) strcpy( &RingName[RAW_MSG_LOGO][0], str );
				RawOutputSwitch = 1;
			}
		/* 3 */
			else if( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[3] = 1;
			}
		/* 4 */
			else if( k_its("QueueSize") ) {
				QueueSize = k_long();
				init[4] = 1;
			}
		/* 5 */
			else if( k_its("MaxStationNum") ) {
				MaxStationNum = k_long();
				init[5] = 1;
			}
		/* 6 */
			else if( k_its("ServerSwitch") ) {
				if ( (ServerSwitch = k_int()) >= 1 ) {
					ServerSwitch = 1;
					for ( i = 7; i < 9; i++ )
						init[i] = 1;
				}
				else ServerSwitch = 0;
				init[6] = 1;
			}
		/* 7 */
			else if( k_its("ServerIP") ) {
				str = k_str();
				if ( str ) strcpy( ServerIP, str );
				init[7] = 1;
			}
		/* 8 */
			else if( k_its("ServerPort") ) {
				str = k_str();
				if ( str ) strcpy( ServerPort, str );
				init[8] = 1;
			}
			else if( k_its("SQLHost") ) {
				str = k_str();
				if ( str ) strcpy(DBInfo.host, str);
				for ( i = 9; i < 14; i++ )
					init[i] = 0;
			}
		/* 9 */
			else if( k_its("SQLPort") ) {
				DBInfo.port = k_long();
				init[9] = 1;
			}
		/* 10 */
			else if( k_its("SQLUser") ) {
				str = k_str();
				if ( str ) strcpy(DBInfo.user, str);
				init[10] = 1;
			}
		/* 11 */
			else if( k_its("SQLPassword") ) {
				str = k_str();
				if ( str ) strcpy(DBInfo.password, str);
				init[11] = 1;
			}
		/* 12 */
			else if( k_its("SQLDatabase") ) {
				str = k_str();
				if ( str ) strcpy(DBInfo.database, str);
				init[12] = 1;
			}
		/* 13 */
			else if ( k_its("SQLStationTable") ) {
				str = k_str();
				if ( str ) strcpy(SQLStationTable, str);
				init[13] = 1;
			}
			else if ( k_its("SQLChannelTable") ) {
				str = k_str();
				if ( str ) strcpy(SQLChannelTable, str);
			}
			else if ( k_its("Palert") ) {
				int serial = k_int();
			/* */
				char sta[TRACE2_STA_LEN];
				str = k_str();
				if ( str ) strcpy(sta, str);
			/* */
				char net[TRACE2_NET_LEN];
				str = k_str();
				if ( str ) strcpy(net, str);
			/* */
				char loc[TRACE2_LOC_LEN];
				str = k_str();
				if ( str ) strcpy(loc, str);

				int   nchannel = k_int();
				char *chan[nchannel] = { NULL };

				if ( nchannel ) {
					for ( i = 0; i < nchannel; i++ ) {
						str = k_str();
						chan[i] = malloc(TRACE2_CHAN_LEN);
						strcpy(chan[i], str);
					}
				}
				palert2ew_list_station_add( &_Root, serial, sta, net, loc, nchannel, chan );
			/* */
				for ( i = 0; i < nchannel; i++ )
					free(chan[i]);
			}
		 /* Unknown command
		  *****************/
			else {
				logit( "e", "palert2ew: <%s> Unknown command in <%s>.\n",
						 com, configfile );
				continue;
			}

		/* See if there were any errors processing the command
		 *****************************************************/
			if( k_err() ) {
			   logit( "e",
					   "palert2ew: Bad <%s> command in <%s>; exiting!\n",
						com, configfile );
			   exit( -1 );
			}
		}
		nfiles = k_close();
   }

/* After all files are closed, check init flags for missed commands
 ******************************************************************/
	nmiss = 0;
	for ( i=0; i<ncommand; i++ )  if( !init[i] ) nmiss++;
	if ( nmiss ) {
		logit( "e", "palert2ew: ERROR, no " );
		if ( !init[0] )  logit( "e", "<LogFile> "           );
		if ( !init[1] )  logit( "e", "<MyModuleId> "        );
		if ( !init[2] )  logit( "e", "<OutWaveRing> "       );
		if ( !init[3] )  logit( "e", "<HeartBeatInterval> " );
		if ( !init[4] )  logit( "e", "<QueueSize> "         );
		if ( !init[5] )  logit( "e", "<MaxStationNum> "     );
		if ( !init[6] )  logit( "e", "<ServerSwitch> "      );
		if ( !init[7] )  logit( "e", "<ServerIP> "          );
		if ( !init[8] )  logit( "e", "<ServerPort> "        );
		if ( !init[9] )  logit( "e", "<SQLPort> "           );
		if ( !init[10] ) logit( "e", "<SQLUser> "           );
		if ( !init[11] ) logit( "e", "<SQLPassword> "       );
		if ( !init[12] ) logit( "e", "<SQLDatabase> "       );
		if ( !init[13] ) logit( "e", "<SQLStationTable> "   );

		logit( "e", "command(s) in <%s>; exiting!\n", configfile );
		exit( -1 );
	}

	return;
}

/*
 * palert2ew_lookup() - Look up important info from earthworm.h tables
 */
static void palert2ew_lookup( void )
{
/* Look up keys to shared memory regions */
	if( ( RingKey[WAVE_MSG_LOGO] = GetKey(&RingName[WAVE_MSG_LOGO][0]) ) == -1 ) {
		fprintf( stderr,
				"palert2ew:  Invalid ring name <%s>; exiting!\n", &RingName[WAVE_MSG_LOGO][0]);
		exit( -1 );
	}
	if ( RawOutputSwitch ) {
		if( ( RingKey[RAW_MSG_LOGO] = GetKey(&RingName[RAW_MSG_LOGO][0]) ) == -1 ) {
			fprintf( stderr,
					"palert2ew:  Invalid ring name <%s>; exiting!\n", &RingName[RAW_MSG_LOGO][0]);
			exit( -1 );
		}
	}
	else RingKey[RAW_MSG_LOGO] = -1;

/* Look up installations of interest
*********************************/
	if ( GetLocalInst( &InstId ) != 0 ) {
		fprintf( stderr,
				"palert2ew: error getting local installation id; exiting!\n" );
		exit( -1 );
	}

/* Look up modules of interest
***************************/
	if ( GetModId( MyModName, &MyModId ) != 0 ) {
		fprintf( stderr,
				"palert2ew: Invalid module name <%s>; exiting!\n", MyModName );
		exit( -1 );
	}

/* Look up message types of interest
*********************************/
	if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
		fprintf( stderr,
				"palert2ew: Invalid message type <TYPE_HEARTBEAT>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
		fprintf( stderr,
				"palert2ew: Invalid message type <TYPE_ERROR>; exiting!\n" );
		exit( -1 );
	}
	if ( GetType( "TYPE_TRACEBUF2", &TypeTracebuf2 ) != 0 ) {
		fprintf( stderr,
				"palert2ew: Invalid message type <TYPE_TRACEBUF2>; exiting!\n" );
		exit( -1 );
	}
	if ( RawOutputSwitch ) {
		if ( GetType( "TYPE_PALERTRAW", &TypePalertRaw ) != 0 ) {
			fprintf( stderr,
					"palert2ew: Invalid message type <TYPE_PALERTRAW>; exiting!\n" );
			exit( -1 );
		}
	}
	return;
}

/*
 * palert2ew_status() - builds a heartbeat or error message & puts it into
 *                      shared memory.  Writes errors to log file & screen.
 */
static void palert2ew_status( unsigned char type, short ierr, char *note )
{
	MSG_LOGO    logo;
	char        msg[256];
	uint64_t    size;
	time_t      t;

/* Build the message */
	logo.instid = InstId;
	logo.mod    = MyModId;
	logo.type   = type;

	time(&t);

	if( type == TypeHeartBeat ) {
		sprintf( msg, "%ld %ld\n", (long)t, (long)MyPid);
	}
	else if( type == TypeError ) {
		sprintf( msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit( "et", "palert2ew: %s\n", note );
	}

	size = strlen(msg);   /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg( &Region[0], &logo, size, msg ) != PUT_OK ) {
		if( type == TypeHeartBeat ) {
			logit("et","palert2ew:  Error sending heartbeat.\n" );
		}
		else if( type == TypeError ) {
			logit("et","palert2ew:  Error sending error:%d.\n", ierr );
		}
	}

	return;
}

/*
 * palert2ew_end() - free all the local memory & close socket
 */
static void palert2ew_end( void )
{
	tport_detach( &Region[WAVE_MSG_LOGO] );
	if ( RawOutputSwitch )
		tport_detach( &Region[RAW_MSG_LOGO] );

	MsgQueueEnd();
	palert2ew_list_end();

	return;
}

/*
 *
 */
static void check_thread_client( void )
{
	if ( MessageReceiverFStatus != THREAD_ALIVE ) {
		if ( StartThread( MessageReceiverF, (uint32_t)THREAD_STACK, &tid[0] ) == -1 ) {
			logit("e", "palert2ew: Error starting MessageReceiverF thread; exiting!\n");
			palert2ew_end();
			exit(-1);
		}
		MessageReceiverFStatus = THREAD_ALIVE;
	}

	return;
}

/*
 *
 */
static void check_thread_server( void )
{
	int i;

	for ( i = 0; i < 2; i++ ) {
		if ( MessageReceiverPStatus[i] != THREAD_ALIVE ) {
			if ( StartThreadWithArg( MessageReceiverP, (void *)&number[i], (uint32_t)THREAD_STACK, &tid[i] ) == -1 ) {
				logit("e", "palert2ew: Error starting MessageReceiverP %d thread; exiting!\n", i);
				palert2ew_end();
				exit(-1);
			}
			MessageReceiverPStatus[i] = THREAD_ALIVE;
		}
	}
	if ( timeNow - timeLastCheck >= 60 ) {
		timeLastCheck = timeNow;
		CheckPalertConn();
	}

	return;
}

/******************************************************************************
 * MessageReceiverF() Receive the messages from the socket of forward server  *
 *                    and send it to the MessageStacker.                      *
 ******************************************************************************/
static thr_ret
MessageReceiverF ( void *dummy )
{
	int ret;

/* Initialize the connection */
	if ( PalertClientInit( ServerIP, ServerPort ) < 0 ) {
		logit("e", "palert2ew: Cannot initialize the connection!\n");
		MessageReceiverFStatus = THREAD_ERR; /* file a complaint to the main thread */
		KillSelfThread(); /* main thread will restart us */
		return NULL;
	}

/* Tell the main thread we're ok
 ********************************/
	MessageReceiverFStatus = THREAD_ALIVE;

	do
	{
		if ( (ret = ReadServerData()) != 0 ) {
			if ( ret == -1 ) {
				if ( UpdateStatus == 0 ) UpdateStatus = 1;
			}
			else if ( ret == -2 ) {
				sleep_ew(50);
			}
			else if ( ret == -3 ) {
				logit("e", "palert2ew: Can't connect to the Palert server!\n");
				break;
			}
		}
	} while ( Finish );

/* we're quitting
 *****************/
	PalertClientEnd();
	if ( Finish ) MessageReceiverFStatus = THREAD_ERR; /* file a complaint to the main thread */
	KillSelfThread(); /* main thread will restart us */

	return NULL;
}

/******************************************************************************
 * MessageReceiverP() Receive the messages from the socket of all the Palerts *
 *                    and send it to the MessageStacker.                      *
 ******************************************************************************/
static thr_ret
MessageReceiverP ( void *arg )
{
	int ret;
	const uint8_t countindex = *((uint8_t *)arg);

/* Initialize the connection */
	if ( countindex ) {
		if ( PalertServerInit( MaxStationNum ) < 0 ) {
			MessageReceiverPStatus[countindex] = THREAD_ERR; /* file a complaint to the main thread */
			KillSelfThread(); /* main thread will restart us */
			return NULL;
		}
	}
	else {
	/* Test if initialization is ready */
		while( GetAcceptSocket() < 0 ) sleep_ew(100);
	}

/* Tell the main thread we're ok
 ********************************/
	MessageReceiverPStatus[countindex] = THREAD_ALIVE;

	do
	{
		if ( (ret = ReadPalertsData(countindex, 1000)) != 0 ) {
			if ( ret == -1 ) {
				if ( UpdateStatus == 0 ) UpdateStatus = 1;
			}
		}
	} while ( Finish );

/* we're quitting
 *****************/
	if ( countindex ) PalertServerEnd();

	if ( Finish ) MessageReceiverPStatus[countindex] = THREAD_ERR; /* file a complaint to the main thread */
	KillSelfThread(); /* main thread will restart us */

	return NULL;
}

/*
 * update_list_thread() -
 */
static thr_ret update_list_thread( void *dummy )
{
	logit("o", "palert2ew: Start to updating the list of Palerts.\n");

	if ( palert2ew_list_db_fetch( &_Root, SQLStationTable, SQLChannelTable, &DBInfo ) <= 0 )
		logit("e", "palert2ew: Fetching list from remote database error!\n");

/* Just wait for 30 seconds */
	sleep_ew(30000);
/* Tell other threads that update is finshed */
	UpdateStatus = 0;
	KillSelfThread(); /* Just exit this thread */
	return NULL;
}

/*
 * enrich_tracebuf_header_pmode1() -
 */
static TRACE2_HEADER *enrich_tracebuf_header_pmode1(
	TRACE2_HEADER *trh2, const _STAINFO *staptr, const PALERTMODE1_HEADER *pah
) {
/* */
	trh2->pinno     = 0;
	trh2->nsamp     = PALERTMODE1_SAMPLE_NUMBER;
	trh2->samprate  = (double)PALERTMODE1_HEADER_GET_SAMPRATE( pah );
	trh2->starttime = palert_get_systime( pah, TimeShift );
	trh2->endtime   = trh2->starttime + (trh2->nsamp - 1) * 1.0 / trh2->samprate;
/* */
	strcpy(trh2->sta, staptr->sta);
	strcpy(trh2->net, staptr->net);
	strcpy(trh2->loc, staptr->loc);
/* */
	trh2->version[0] = TRACE2_VERSION0;
	trh2->version[1] = TRACE2_VERSION1;

	strcpy(trh2->quality , TRACE2_NO_QUALITY);
	strcpy(trh2->pad     , TRACE2_NO_PAD    );

#if defined( _SPARC )
	strcpy(trh2->datatype, "s4");   /* SUN IEEE integer */
#elif defined( _INTEL )
	strcpy(trh2->datatype, "i4");   /* VAX/Intel IEEE integer */
#else
	logit("e", "palert2ew warning: _INTEL and _SPARC are both undefined.");
#endif

	return trh2;
}

/*
 * copydata_to_traceubf_pmode1() -
 */
static int32_t *copydata_to_traceubf_pmode1(
	TracePacket *tracebuffer, const PalertPacket *palertp, const PALERTMODE1_CHANNEL chan_seq
) {
	return palert_get_data( palertp, chan_seq, (int32_t *)((&tracebuffer->trh2) + 1) );
}
