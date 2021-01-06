/*
 *
 */

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
#include <ctype.h>
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
/* Local header include */
#include <palert2ew_list.h>
#include <palert2ew_client.h>
#include <palert2ew_server.h>
#include <palert2ew_msg_queue.h>

/* Functions prototype in this source file
 *******************************/
static void palert2ew_config( char * );
static void palert2ew_lookup( void );
static void palert2ew_status( unsigned char, short, char * );
static void palert2ew_end( void );                /* Free all the local memory & close socket */

static void    check_receiver_client( const int );
static void    check_receiver_server( const int );
static thr_ret receiver_client_thread( void * );  /* Read messages from the socket of forward server */
static thr_ret receiver_server_thread( void * );  /* Read messages from the socket of Palerts */
static thr_ret update_list_thread( void * );
static int     load_list_configfile( void **, char * );

static void           process_packet_pm1( PalertPacket *, _STAINFO * );
static int            examine_ntp_sync_pm1( _STAINFO *, const PALERTMODE1_HEADER * );
static TRACE2_HEADER *enrich_trh2_pm1( TRACE2_HEADER *, const _STAINFO *, const PALERTMODE1_HEADER * );

/* Ring messages things */
#define WAVE_MSG_LOGO  0
#define RAW_MSG_LOGO   1

static SHM_INFO Region[2];      /* shared memory region to use for i/o    */
static MSG_LOGO Putlogo[2];     /* array for requesting module, type, instid */
static pid_t    MyPid;          /* for restarts by startstop               */

/* Thread things */
#define THREAD_STACK 8388608         /* 8388608 Byte = 8192 Kilobyte = 8 Megabyte */
#define THREAD_OFF    0              /* Thread has not been started      */
#define THREAD_ALIVE  1              /* Thread alive and well            */
#define THREAD_ERR   -1              /* Thread encountered error quit    */
static volatile int     ReceiverThreadsNum = 0;
static volatile int8_t *MessageReceiverStatus = NULL;
#if defined( _V710 )
static ew_thread_t      UpdateThreadID   = 0;          /* Thread id for updating the Palert list */
static ew_thread_t     *ReceiverThreadID = NULL;       /* Thread id for receiving messages from TCP/IP */
#else
static unsigned         UpdateThreadID   = 0;          /* Thread id for updating the Palert list */
static unsigned        *ReceiverThreadID = NULL;       /* Thread id for receiving messages from TCP/IP */
#endif

/* Things to read or derive from configuration file
 **************************************************/
static char     RingName[2][MAX_RING_STR];  /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];     /* speak as this module name/id      */
static uint8_t  LogSwitch;                  /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;          /* seconds between heartbeats        */
static uint64_t UpdateInterval = 0;         /* seconds between updating check    */
static uint64_t QueueSize;                  /* max messages in output circular buffer */
static uint8_t  ServerSwitch;               /* 0 connect to Palert server; 1 as the server of Palert */
static uint8_t  RawOutputSwitch = 0;
static char     ServerIP[INET6_ADDRSTRLEN];
static char     ServerPort[8];
static uint64_t MaxStationNum;
static uint32_t UniSampRate = 0;
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

/* Update flag used by palert2ew */
#define  LIST_IS_UPDATED      0
#define  LIST_NEED_UPDATED    1
#define  LIST_UNDER_UPDATE    2

static volatile void   *_Root  = NULL;
static volatile _Bool   Finish = 0;
static volatile uint8_t UpdateFlag = LIST_IS_UPDATED;

static int64_t LocalTimeShift = 0;            /* Time difference between UTC & local timezone */

/* Macro */
#define COPYDATA_TRACEBUF_PM1(TBUF, PM1, SEQ) \
		(palert_get_data( (PM1), (SEQ), (int32_t *)((&(TBUF)->trh2) + 1) ))

/*
 *
 */
int main ( int argc, char **argv )
{
	int      i;
	time_t   timeNow;          /* current time                    */
	time_t   timeLastBeat;     /* time last heartbeat was sent    */
	time_t   timeLastUpd;      /* time last checked updating list */
	char    *lockfile;
	int32_t  lockfile_fd;
	uint32_t count    = 0;
	size_t   msg_size = 0;
	PACKET   packet = { 0 };

	PALERTMODE1_HEADER * const pah1 = (PALERTMODE1_HEADER *)packet.data;
	void (*check_receiver_func)( const int ) = NULL;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: palert2ew <configfile>\n");
		exit(0);
	}
	Finish = 1;
	UpdateFlag = LIST_IS_UPDATED;

/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);
/* Read the configuration file(s) */
	palert2ew_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Read the station list from remote database */
	pa2ew_list_db_fetch( (void **)&_Root, SQLStationTable, SQLChannelTable, &DBInfo );
	pa2ew_list_root_reg( (void *)_Root );
	if ( !(i = pa2ew_list_total_station()) ) {
		fprintf(stderr, "There is not any station in the list after fetching, exiting!\n");
		exit(-1);
	}
	else {
		logit("o", "palert2ew: There are total %d stations in the list.\n", i);
	}

/* Look up important info from earthworm.h tables */
	palert2ew_lookup();
/* Reinitialize logit to desired logging level */
	logit_init(argv[1], 0, 256, LogSwitch);
	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "One instance of %s is already running, exiting!\n", argv[0]);
		pa2ew_list_end();
		exit(-1);
	}
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e","palert2ew: Cannot get pid. Exiting!\n");
		pa2ew_list_end();
		exit(-1);
	}

/* Initialize the connection process either server or client */
	if ( ServerSwitch == 0 ) {
		ReceiverThreadsNum  = 1;
		if ( pa2ew_client_init( ServerIP, ServerPort ) < 0 ) {
			logit("e","palert2ew: Cannot initialize the connection client process. Exiting!\n");
			pa2ew_list_end();
			exit(-1);
		}
		check_receiver_func = check_receiver_client;
	}
	else {
		if ( (ReceiverThreadsNum = pa2ew_server_init( MaxStationNum )) < 1 ) {
			logit("e","palert2ew: Cannot initialize the connection server process. Exiting!\n");
			pa2ew_list_end();
			exit(-1);
		}
		check_receiver_func = check_receiver_server;
	}

/* Build the message */
	Putlogo[WAVE_MSG_LOGO].instid = InstId;
	Putlogo[WAVE_MSG_LOGO].mod    = MyModId;
	Putlogo[WAVE_MSG_LOGO].type   = TypeTracebuf2;
	Putlogo[RAW_MSG_LOGO].instid  = InstId;
	Putlogo[RAW_MSG_LOGO].mod     = MyModId;
	Putlogo[RAW_MSG_LOGO].type    = TypePalertRaw;

/* Attach to Output shared memory ring */
	for ( i = 0; i < 2; i++ ) {
		if ( RingKey[i] == -1 ) {
			Region[i].key = RingKey[i];
		}
		else {
			tport_attach(&Region[i], RingKey[i]);
			logit("", "palert2ew: Attached to public memory region %s: %ld\n", &RingName[i][0], RingKey[i]);
		}
	}
/* Initialize the message queue */
	pa2ew_msgqueue_init( (unsigned long)QueueSize );

/* Initialize the threads' parameters */
	MessageReceiverStatus = calloc(ReceiverThreadsNum, sizeof(int8_t));
#if defined( _V710 )
	ReceiverThreadID      = calloc(ReceiverThreadsNum, sizeof(ew_thread_t));
#else
	ReceiverThreadID      = calloc(ReceiverThreadsNum, sizeof(unsigned));
#endif

/* Force a heartbeat to be issued in first pass thru main loop */
	timeLastBeat = time(&timeNow) - HeartBeatInterval - 1;
	timeLastUpd  = timeNow;
/* Initialize the timezone shift */
	LocalTimeShift = -(localtime(&timeNow)->tm_gmtoff);
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send palert2ew's heartbeat */
		if ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			palert2ew_status( TypeHeartBeat, 0, "" );
		}
	/* Start the check of updating list thread */
		if ( UpdateInterval &&
			UpdateFlag == LIST_NEED_UPDATED &&
			timeNow - timeLastUpd >= (int64_t)UpdateInterval
		) {
			timeLastUpd = timeNow;
			if ( StartThreadWithArg(update_list_thread, argv[1], (uint32_t)THREAD_STACK, &UpdateThreadID) == -1 )
				logit("e", "palert2ew: Error starting update_list thread, just skip it!\n");
		}
	/* Start the message receiving thread if it isn't running. */
		check_receiver_func( 50 );

	/* Process all new messages */
		count    = 0;
		msg_size = 0;
		do {
		/* See if a termination has been requested */
			if ( tport_getflag( &Region[0] ) == TERMINATE ||
				tport_getflag( &Region[0] ) == MyPid ) {
			/* write a termination msg to log file */
				logit("t", "palert2ew: Termination requested; exiting!\n");
				fflush(stdout);
			/* should check the return of these if we really care */
				Finish = 0;
				sleep_ew(500);
			/* detach from shared memory */
				palert2ew_end();
				ew_unlockfile(lockfile_fd);
				ew_unlink_lockfile(lockfile);
				exit(0);
			}

			if ( pa2ew_msgqueue_dequeue( &packet, &msg_size ) < 0 ) {
				break;
			}
			else {
				count++;
			/* Put the raw data to the raw ring */
				if ( RawOutputSwitch )
					if ( tport_putmsg(&Region[RAW_MSG_LOGO], &Putlogo[RAW_MSG_LOGO], PALERTMODE1_PACKET_LENGTH, (char *)packet.data ) != PUT_OK )
						logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[RAW_MSG_LOGO]);
			}

		/* Process the message */
			if ( PALERT_IS_MODE1_HEADER( pah1 ) ) {
				process_packet_pm1( (PalertPacket *)packet.data, (_STAINFO *)packet.sptr );
			}
		} while ( count < MaxStationNum );  /* end of message-processing-loop */
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
/* */
	DBINFO_INIT( DBInfo );
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
			else if( k_its("UniSampRate") ) {
				UniSampRate = k_int();
				logit(
					"o", "palert2ew: Change to unified sampling rate mode, the unified sampling rate is %d Hz!\n",
					UniSampRate
				);
			}
			else if( k_its("UpdateInterval") ) {
				UpdateInterval = k_long();
				if ( UpdateInterval )
					logit(
						"o", "palert2ew: Change to auto updating mode, the updating interval is %d seconds!\n",
						UpdateInterval
					);
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
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( pa2ew_list_station_line_parse( (void **)&_Root, str ) ) {
					logit(
						"e", "palert2ew: ERROR, lack of some station information for in <%s>, exiting!\n",
						configfile
					);
					exit(-1);
				}
			}
		/* Unknown command */
			else {
				logit("e", "palert2ew: <%s> Unknown command in <%s>.\n", com, configfile);
				continue;
			}

		/* See if there were any errors processing the command
		 *****************************************************/
			if ( k_err() ) {
			   logit("e", "palert2ew: Bad <%s> command in <%s>; exiting!\n", com, configfile);
			   exit(-1);
			}
		}
		nfiles = k_close();
	}

/* After all files are closed, check init flags for missed commands */
	nmiss = 0;
	for ( i = 0; i < ncommand; i++ ) if ( !init[i] ) nmiss++;
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

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/*
 * palert2ew_lookup() - Look up important info from earthworm.h tables
 */
static void palert2ew_lookup( void )
{
/* Look up keys to shared memory regions */
	if( (RingKey[WAVE_MSG_LOGO] = GetKey(&RingName[WAVE_MSG_LOGO][0])) == -1 ) {
		fprintf(
			stderr, "palert2ew: Invalid ring name <%s>; exiting!\n", &RingName[WAVE_MSG_LOGO][0]
		);
		exit(-1);
	}
	if ( RawOutputSwitch ) {
		if ( (RingKey[RAW_MSG_LOGO] = GetKey(&RingName[RAW_MSG_LOGO][0])) == -1 ) {
			fprintf(
				stderr, "palert2ew: Invalid ring name <%s>; exiting!\n", &RingName[RAW_MSG_LOGO][0]
			);
			exit(-1);
		}
	}
	else RingKey[RAW_MSG_LOGO] = -1;

/* Look up installations of interest */
	if ( GetLocalInst( &InstId ) != 0 ) {
		fprintf(stderr, "palert2ew: error getting local installation id; exiting!\n");
		exit(-1);
	}

/* Look up modules of interest */
	if ( GetModId( MyModName, &MyModId ) != 0 ) {
		fprintf(stderr, "palert2ew: Invalid module name <%s>; exiting!\n", MyModName);
		exit(-1);
	}

/* Look up message types of interest */
	if ( GetType( "TYPE_HEARTBEAT", &TypeHeartBeat ) != 0 ) {
		fprintf(stderr, "palert2ew: Invalid message type <TYPE_HEARTBEAT>; exiting!\n");
		exit(-1);
	}
	if ( GetType( "TYPE_ERROR", &TypeError ) != 0 ) {
		fprintf(stderr, "palert2ew: Invalid message type <TYPE_ERROR>; exiting!\n");
		exit(-1);
	}
	if ( GetType( "TYPE_TRACEBUF2", &TypeTracebuf2 ) != 0 ) {
		fprintf(stderr, "palert2ew: Invalid message type <TYPE_TRACEBUF2>; exiting!\n");
		exit(-1);
	}
	if ( RawOutputSwitch ) {
		if ( GetType( "TYPE_PALERTRAW", &TypePalertRaw ) != 0 ) {
			fprintf(stderr, "palert2ew: Invalid message type <TYPE_PALERTRAW>; exiting!\n");
			exit(-1);
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

	if ( type == TypeHeartBeat ) {
		sprintf(msg, "%ld %ld\n", (long)t, (long)MyPid);
	}
	else if ( type == TypeError ) {
		sprintf(msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit("et", "palert2ew: %s\n", note);
	}

	size = strlen(msg);   /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&Region[0], &logo, size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat ) {
			logit("et","palert2ew: Error sending heartbeat.\n");
		}
		else if ( type == TypeError ) {
			logit("et","palert2ew: Error sending error:%d.\n", ierr);
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

	pa2ew_msgqueue_end();
	pa2ew_list_end();
	if ( ServerSwitch == 0 )
		pa2ew_client_end();
	else
		pa2ew_server_end();

	free(ReceiverThreadID);
	free((int8_t *)MessageReceiverStatus);

	return;
}

/*
 *
 */
static void check_receiver_client( const int wait_msec )
{
	if ( MessageReceiverStatus[0] != THREAD_ALIVE ) {
		if ( StartThread(receiver_client_thread, (uint32_t)THREAD_STACK, ReceiverThreadID) == -1 ) {
			logit("e", "palert2ew: Error starting receiver_client thread; exiting!\n");
			palert2ew_end();
			exit(-1);
		}
		MessageReceiverStatus[0] = THREAD_ALIVE;
	}
	sleep_ew(wait_msec);

	return;
}

/*
 *
 */
static void check_receiver_server( const int wait_msec )
{
	static uint8_t *number     = NULL;
	static time_t   time_check = 0;

	int    i;
	time_t time_now;

/* */
	if ( number == NULL ) {
		time(&time_check);
		number = calloc(ReceiverThreadsNum, sizeof(uint8_t));
		for ( i = 0; i < ReceiverThreadsNum; i++ )
			number[i] = i;
	}
/* */
	pa2ew_server_palert_accept( wait_msec );
/* */
	for ( i = 0; i < ReceiverThreadsNum; i++ ) {
		if ( MessageReceiverStatus[i] != THREAD_ALIVE ) {
			if ( StartThreadWithArg(
					receiver_server_thread, number + i, (uint32_t)THREAD_STACK, ReceiverThreadID + i
				) == -1
			) {
				logit("e", "palert2ew: Error starting receiver_server thread(%d); exiting!\n", i);
				palert2ew_end();
				exit(-1);
			}
			MessageReceiverStatus[i] = THREAD_ALIVE;
		}
	}
/* */
	if ( (time(&time_now) - time_check) >= 60 ) {
		time_check = time_now;
		pa2ew_server_conn_check();
	}

	return;
}

/*
 * receiver_client_thread() - Receive the messages from the socket of forward server
 *                            and send it to the MessageStacker.
 */
static thr_ret receiver_client_thread( void *dummy )
{
	int ret;

/* Tell the main thread we're ok */
	MessageReceiverStatus[0] = THREAD_ALIVE;
/* Main service loop */
	do {
		if ( (ret = pa2ew_client_stream()) ) {
			if ( ret == PA2EW_RECV_NEED_UPDATE ) {
				if ( UpdateFlag == LIST_IS_UPDATED )
					UpdateFlag = LIST_NEED_UPDATED;
				continue;
			}
			break;
		}
	} while ( Finish );
/* we're quitting */
	if ( Finish )
		MessageReceiverStatus[0] = THREAD_ERR;

	KillSelfThread(); /* main thread will restart us */

	return NULL;
}

/*
 * receiver_server_thread() - Receive the messages from the socket of all the Palerts
 *                            and send it to the MessageStacker.
 */
static thr_ret receiver_server_thread( void *arg )
{
	int           ret;
	const uint8_t countindex = *((uint8_t *)arg);

/* Tell the main thread we're ok */
	MessageReceiverStatus[countindex] = THREAD_ALIVE;
/* Main service loop */
	do {
		if ( (ret = pa2ew_server_stream(countindex, 1000)) )
			if ( ret == PA2EW_RECV_NEED_UPDATE )
				if ( UpdateFlag == LIST_IS_UPDATED )
					UpdateFlag = LIST_NEED_UPDATED;
	} while ( Finish );
/* file a complaint to the main thread */
	if ( Finish )
		MessageReceiverStatus[countindex] = THREAD_ERR;

	KillSelfThread();

	return NULL;
}

/*
 * update_list_thread() -
 */
static thr_ret update_list_thread( void *arg )
{
	void *root = NULL;

	logit("o", "palert2ew: Updating the Palert list...\n");
	UpdateFlag = LIST_UNDER_UPDATE;
/* */
	if ( load_list_configfile( &root, (char *)arg ) ) {
		logit("e", "palert2ew: Fetching Palert list from local file error!\n");
		pa2ew_list_root_destroy( root );
	}
	else {
	/* */
		if ( pa2ew_list_db_fetch( &root, SQLStationTable, SQLChannelTable, &DBInfo ) < 0 ) {
			logit("e", "palert2ew: Fetching Palert list from remote database error!\n");
			pa2ew_list_root_destroy( root );
		}
		else {
			if ( root != NULL ) {
				logit("o", "palert2ew: Successfully updated the Palert list!\n");
				pa2ew_list_root_reg( root );
				logit(
					"o", "palert2ew: There are total %d stations in the new Palert list.\n", pa2ew_list_total_station()
				);
			}
		}
	}
/* */
	UpdateFlag = LIST_IS_UPDATED;
/* Just exit this thread */
	KillSelfThread();

	return NULL;
}

/*
 *
 */
static int load_list_configfile( void **root, char *configfile )
{
	char *com;
	char *str;
	int   nfiles;
	int   success;

/* Open the main configuration file */
	nfiles = k_open( configfile );
	if ( nfiles == 0 ) {
		logit("e","palert2ew: Error opening command file <%s> when updating!\n", configfile);
		return -1;
	}

/* Process all command files */
	while ( nfiles > 0 )   /* While there are command files open */
	{
		while ( k_rd() )        /* Read next line from active file  */
		{
			com = k_str();         /* Get the first token from line */
		/* Ignore blank lines & comments */
			if ( !com )          continue;
			if ( com[0] == '#' ) continue;
		/* Open a nested configuration file */
			if ( com[0] == '@' ) {
				success = nfiles + 1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "palert2ew: Error opening command file <%s> when updating!\n", &com[1]);
					return -1;
				}
				continue;
			}

		/* Process only "Palert" command */
			if ( k_its("Palert") ) {
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( pa2ew_list_station_line_parse( root, str ) ) {
					logit(
						"e", "palert2ew: Some errors occured in <%s> when updating!\n",
						configfile
					);
					return -1;
				}
			}
		/* See if there were any errors processing the command */
			if ( k_err() ) {
			   logit("e", "palert2ew: Bad <%s> command in <%s> when updating!\n", com, configfile);
			   return -1;
			}
		}
		nfiles = k_close();
	}

	return 0;
}

/*
 *
 */
static void process_packet_pm1( PalertPacket *packet, _STAINFO *stainfo )
{
	int             i, msg_size;
	TracePacket     tracebuf;  /* message which is sent to share ring    */
	const _CHAINFO *chaptr = (_CHAINFO *)stainfo->chaptr;

/* Examine the NTP sync. status */
	if ( examine_ntp_sync_pm1( stainfo, &packet->pah ) ) {
	/* Common part */
		enrich_trh2_pm1( &tracebuf.trh2, stainfo, &packet->pah );
		msg_size = (tracebuf.trh2.nsamp << 2) + sizeof(TRACE2_HEADER);
	/* Each channel part */
		for ( i = 0; i < stainfo->nchannel; i++, chaptr++ ) {
			strcpy(tracebuf.trh2.chan, chaptr->chan);
			COPYDATA_TRACEBUF_PM1( &tracebuf, packet, chaptr->seq );
			if ( tport_putmsg(&Region[WAVE_MSG_LOGO], &Putlogo[WAVE_MSG_LOGO], msg_size, tracebuf.msg) != PUT_OK )
			logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[WAVE_MSG_LOGO]);
		}
	}

	return;
}

/*
 * examine_ntp_sync_pm1() -
 */
static int examine_ntp_sync_pm1( _STAINFO *stainfo, const PALERTMODE1_HEADER *pah )
{
	uint8_t *ntp_errors = &stainfo->param.ntp_errors;

/* Check NTP SYNC. */
	if ( PALERTMODE1_HEADER_CHECK_NTP( pah ) ) {
		if ( *ntp_errors >= PA2EW_NTP_SYNC_ERR_LIMIT )
			logit("o", "palert2ew: Station %s NTP resync, now back online.\n", stainfo->sta);
		*ntp_errors = 0;
	}
	else {
		if ( *ntp_errors >= 25 ) {
			if ( *ntp_errors < PA2EW_NTP_SYNC_ERR_LIMIT ) {
				printf("palert2ew: Station %s NTP sync error, please check it!\n", stainfo->sta);
			}
			else {
				if ( *ntp_errors == PA2EW_NTP_SYNC_ERR_LIMIT ) {
					logit("e", "palert2ew: Station %s NTP sync error, drop the packet.\n", stainfo->sta);
					(*ntp_errors)++;
				}
				return 0;
			}
		}
		(*ntp_errors)++;
	}

	return 1;
}

/*
 * enrich_trh2_pm1() -
 */
static TRACE2_HEADER *enrich_trh2_pm1(
	TRACE2_HEADER *trh2, const _STAINFO *staptr, const PALERTMODE1_HEADER *pah
) {
/* */
	trh2->pinno     = 0;
	trh2->nsamp     = PALERTMODE1_SAMPLE_NUMBER;
	trh2->samprate  = UniSampRate ? (double)UniSampRate : (double)PALERTMODE1_HEADER_GET_SAMPRATE( pah );
	trh2->starttime = palert_get_systime( pah, LocalTimeShift );
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
	logit("e", "palert2ew: warning _INTEL and _SPARC are both undefined.");
#endif

	return trh2;
}
