/**
 * @file palert2ew.c
 * @author Benjamin Ming Yang @ Department of Geology, National Taiwan University
 * @brief
 * @date 2024-06-06
 * @copyright Copyright (c) 2024
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
#include <search.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
/* Earthworm environment header include */
#include <earthworm.h>
#include <trace_buf.h>
#include <kom.h>
#include <transport.h>
#include <lockfile.h>
#include <libmseed.h>
/* Local header include */
#include <libpalertc/libpalertc.h>
#include <palert2ew.h>
#include <palert2ew_misc.h>
#include <palert2ew_list.h>
#include <palert2ew_client.h>
#include <palert2ew_server.h>
#include <palert2ew_msg_queue.h>

/* Internal stack related struct */
typedef struct {
/* */
	LABEL   label;
/* */
	uint8_t buffer[65536];
} LABELED_DATA;

/* Functions prototype in this source file */
static void palert2ew_config( char * );
static void palert2ew_lookup( void );
static void palert2ew_status( unsigned char, short, char * );
static void palert2ew_end( void );                /* Free all the local memory & close socket */

static void    check_receiver_client( const int );
static void    check_receiver_server( const int );
static thr_ret receiver_client_thread( void * );  /* Read messages from the socket of forward server */
static thr_ret receiver_server_thread( void * );  /* Read messages from the socket of Palerts */
static thr_ret update_list_thread( void * );

static int     update_list_configfile( char * );
static void    process_packet_pm1( const void *, _STAINFO *, const char [2] );
static void    process_packet_pm4( const void *, _STAINFO *, const char [2] );
static void    process_packet_pm16( const void *, _STAINFO *, const char [2] );
static int     examine_ntp_status( _STAINFO *, const void *, const int );
static int     check_pkt_crc( const void *, const int );
static void    handle_signal( void );

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
static ew_thread_t      UpdateThreadID      = 0;          /* Thread id for updating the Palert list */
static ew_thread_t     *ReceiverThreadID    = NULL;       /* Thread id for receiving messages from TCP/IP */
#else
static unsigned         UpdateThreadID      = 0;          /* Thread id for updating the Palert list */
static unsigned        *ReceiverThreadID    = NULL;       /* Thread id for receiving messages from TCP/IP */
#endif

/* Things to read or derive from configuration file */
static char     RingName[2][MAX_RING_STR];   /* name of transport ring for i/o    */
static char     MyModName[MAX_MOD_STR];      /* speak as this module name/id      */
static uint8_t  LogSwitch;                   /* 0 if no logfile should be written */
static uint64_t HeartBeatInterval;           /* seconds between heartbeats        */
static uint64_t UpdateInterval = 0;          /* seconds between updating check    */
static uint64_t QueueSize;                   /* max messages in output circular buffer */
static uint8_t  ServerSwitch;                /* 0 connect to Palert server; 1 as the server of Palert */
static uint8_t  RawOutputSwitch = 0;
static uint8_t  CheckCRCSwitch = 1;          /* 0 disable the CRC checking; 1 enable the CRC checking */
static uint8_t  OutputTimeQuestionable = 0;  /* 0 filter out NTP unsychronized stations; 1 allow these stations */
static char     ServerIP[INET6_ADDRSTRLEN];
static char     ServerPort[8] = { 0 };
static uint64_t MaxStationNum;
static uint32_t UniSampRate = 0;
static DBINFO   DBInfo;
static char     SQLStationTable[MAX_TABLE_LEGTH];
static char     SQLChannelTable[MAX_TABLE_LEGTH];

/* Things to look up in the earthworm.h tables with getutil.c functions */
static int64_t RingKey[2];      /* key of transport ring for i/o     */
static uint8_t InstId;          /* local installation id             */
static uint8_t MyModId;         /* Module Id for this program        */
static uint8_t TypeHeartBeat;
static uint8_t TypeError;
static uint8_t TypeTracebuf2 = 0;
static uint8_t TypePalertRaw = 0;

/* Error messages used by palert2ew */
#define  ERR_MISSMSG       0   /* message missed in transport ring       */
#define  ERR_TOOBIG        1   /* retreived msg too large for buffer     */
#define  ERR_NOTRACK       2   /* msg retreived; tracking limit exceeded */
#define  ERR_QUEUE         3   /* error queueing message for sending      */

/* Update flag used by palert2ew */
#define  LIST_IS_UPDATED      0
#define  LIST_NEED_UPDATED    1
#define  LIST_UNDER_UPDATE    2

static volatile _Bool   Finish = 1;
static volatile uint8_t UpdateFlag = LIST_IS_UPDATED;

/**
 * @brief
 *
 * @param argc
 * @param argv
 * @return int
 */
int main ( int argc, char **argv )
{
	int      i;
	time_t   timeNow;          /* current time                              */
	time_t   timeLastBeat;     /* time last heartbeat was sent              */
	time_t   timeLastUpd;      /* time last checked updating list           */
	char    *lockfile;
	int32_t  lockfile_fd;

	uint8_t *buffer     = NULL;
	uint32_t count      = 0;
	size_t   msg_size   = 0;
	MSG_LOGO msg_logo   = { 0 };
	char     idatatype[2];
	char     fdatatype[2];

	LABELED_DATA *data_ptr = NULL;
	void (*check_receiver_func)( const int ) = NULL;

/* Check command line arguments */
	if ( argc != 2 ) {
		fprintf(stderr, "Usage: palert2ew <configfile>\n");
		exit(0);
	}
	UpdateFlag = LIST_IS_UPDATED;
/* */
	handle_signal();
/* Define the first byte of the datatype depends on the system endian */
	if ( pa2ew_endian_get() == PA2EW_BIG_ENDIAN ) {
		idatatype[0] = 's';
		fdatatype[0] = 't';
		idatatype[1] = fdatatype[1] = '4';
	}
	else {
		idatatype[0] = 'i';
		fdatatype[0] = 'f';
		idatatype[1] = fdatatype[1] = '4';
	}

/* Initialize name of log-file & open it */
	logit_init(argv[1], 0, 256, 1);
/* Read the configuration file(s) */
	palert2ew_config( argv[1] );
	logit("" , "%s: Read command file <%s>\n", argv[0], argv[1]);
/* Read the station list from remote database */
	if ( pa2ew_list_db_fetch( SQLStationTable, SQLChannelTable, &DBInfo, PA2EW_LIST_INITIALIZING ) < 0 ) {
		fprintf(stderr, "Something error when fetching station list. Exiting!\n");
		exit(-1);
	}
/* Checking total station number again */
	if ( !(i = pa2ew_list_total_station_get()) ) {
		fprintf(stderr, "There is not any station in the list after fetching. Exiting!\n");
		exit(-1);
	}
	else {
		logit("o", "palert2ew: There are total %d stations in the list.\n", i);
		pa2ew_list_tree_activate();
	}

/* Look up important info from earthworm.h tables */
	palert2ew_lookup();
/* Reinitialize logit to desired logging level */
	logit_init(argv[1], 0, 256, LogSwitch);
	lockfile = ew_lockfile_path(argv[1]);
	if ( (lockfile_fd = ew_lockfile(lockfile) ) == -1 ) {
		fprintf(stderr, "One instance of %s is already running. Exiting!\n", argv[0]);
		pa2ew_list_end();
		exit(-1);
	}
/* Get process ID for heartbeat messages */
	MyPid = getpid();
	if ( MyPid == -1 ) {
		logit("e", "palert2ew: Cannot get pid. Exiting!\n");
		pa2ew_list_end();
		exit(-1);
	}

/* Initialize the receiver thread number and function pointer */
	ReceiverThreadsNum  = pa2ew_recv_thrdnum_eval( MaxStationNum, ServerSwitch );
	check_receiver_func = ServerSwitch ? check_receiver_server : check_receiver_client;

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
	pa2ew_msgqueue_init( (unsigned long)QueueSize, sizeof(LABELED_DATA) );
/* */
	buffer   = calloc(1, sizeof(LABELED_DATA));
	data_ptr = (LABELED_DATA *)buffer;

/* Initialize the threads' parameters */
	MessageReceiverStatus = calloc(ReceiverThreadsNum, sizeof(int8_t));
#if defined( _V710 )
	ReceiverThreadID      = calloc(ReceiverThreadsNum, sizeof(ew_thread_t));
#else
	ReceiverThreadID      = calloc(ReceiverThreadsNum, sizeof(unsigned));
#endif

/* Force a heartbeat to be issued in first pass thru main loop */
	timeLastBeat   = time(&timeNow) - HeartBeatInterval - 1;
	timeLastUpd    = timeNow + 1;
/*----------------------- setup done; start main loop -------------------------*/
	while ( 1 ) {
	/* Send palert2ew's heartbeat */
		if ( time(&timeNow) - timeLastBeat >= (int64_t)HeartBeatInterval ) {
			timeLastBeat = timeNow;
			palert2ew_status( TypeHeartBeat, 0, "" );
		}
	/* Start the check of updating list thread */
		if ( UpdateInterval && UpdateFlag == LIST_NEED_UPDATED && (timeNow - timeLastUpd) >= (int64_t)UpdateInterval ) {
			timeLastUpd = timeNow;
			if ( StartThreadWithArg(update_list_thread, argv[1], (uint32_t)THREAD_STACK, &UpdateThreadID) == -1 )
				logit("e", "palert2ew: Error starting update_list thread, just skip it!\n");
		}
	/* Start the message receiving thread if it isn't running. */
		check_receiver_func( 50 );

	/* Process all new messages */
		count = 0;
		do {
		/* See if a termination has been requested */
			i = tport_getflag( &Region[0] );
			if ( i == TERMINATE || i == MyPid ) {
			/* Write a termination msg to log file */
				logit("t", "palert2ew: Termination requested; exiting!\n");
				fflush(stdout);
				goto exit_procedure;
			}
		/* */
			if ( pa2ew_msgqueue_dequeue( buffer, &msg_size, &msg_logo ) < 0 )
				break;
		/* Just in case */
			if ( data_ptr->label.staptr == NULL )
				continue;
		/* Process the raw packet */
			if ( msg_logo.type == PA2EW_MSG_CLIENT_STREAM || msg_logo.type == PA2EW_MSG_SERVER_NORMAL ) {
				count++;
				msg_size -= data_ptr->buffer - (uint8_t *)data_ptr;
			/* Check the CRC of the packet if enable this function */
				if ( CheckCRCSwitch && !check_pkt_crc( data_ptr->buffer, data_ptr->label.packmode ) )
					continue;
			/* Put the raw data to the raw ring */
				if (
					RawOutputSwitch &&
					tport_putmsg(&Region[RAW_MSG_LOGO], &Putlogo[RAW_MSG_LOGO], msg_size, (char *)data_ptr->buffer) != PUT_OK
				) {
					logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[RAW_MSG_LOGO]);
				}
			/* Examine the NTP status */
				if ( examine_ntp_status( data_ptr->label.staptr, data_ptr->buffer, data_ptr->label.packmode ) || OutputTimeQuestionable ) {
				/* Parse the raw packet to trace buffer */
					switch ( data_ptr->label.packmode ) {
					case PALERT_PKT_MODE1:
					/* We only deal with the Normal Streaming packet(1) in this program!! */
						if ( PALERT_M1_PACKETTYPE_GET( (PALERT_M1_HEADER *)data_ptr->buffer ) == PALERT_M1_PACKETTYPE_NORMAL )
							process_packet_pm1( data_ptr->buffer, (_STAINFO *)data_ptr->label.staptr, idatatype );
						break;
					case PALERT_PKT_MODE4:
						process_packet_pm4( data_ptr->buffer, (_STAINFO *)data_ptr->label.staptr, idatatype );
						break;
					case PALERT_PKT_MODE16:
						process_packet_pm16( data_ptr->buffer, (_STAINFO *)data_ptr->label.staptr, fdatatype );
						break;
					default:
						break;
					}
				}
			}
		} while ( count < MaxStationNum ); /* end of message-processing-loop */
	}
/*-----------------------------end of main loop-------------------------------*/
exit_procedure:
	Finish = 0;
	sleep_ew(1000);
/* Free local memory */
	free(buffer);
/* Detach from all the shared memory */
	palert2ew_end();
/* Close & remove the locking file descriptor */
	ew_unlockfile(lockfile_fd);
	ew_unlink_lockfile(lockfile);

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

/* Set to zero one init flag for each required command */
	ncommand = 14;
	for ( int i = 0; i < ncommand; i++ ) {
		if ( i < 9 )
			init[i] = 0;
		else
			init[i] = 1;
	}
/* */
	DBINFO_INIT( DBInfo );
/* Open the main configuration file */
	nfiles = k_open(configfile);
	if ( nfiles == 0 ) {
		logit("e","palert2ew: Error opening command file <%s>; exiting!\n", configfile);
		exit(-1);
	}

/* Process all command files */
/* While there are command files open */
	while ( nfiles > 0 ) {
	/* Read next line from active file  */
		while ( k_rd() ) {
		/* Get the first token from line */
			com = k_str();
		/* Ignore blank lines & comments */
			if ( !com || com[0] == '#' )
				continue;
		/* Open a nested configuration file s*/
			if ( com[0] == '@' ) {
				success = nfiles + 1;
				nfiles  = k_open(&com[1]);
				if ( nfiles != success ) {
					logit("e", "palert2ew: Error opening command file <%s>; exiting!\n", &com[1]);
					exit(-1);
				}
				continue;
			}

		/* Process anything else as a command */
		/* 0 */
			if ( k_its("LogFile") ) {
				LogSwitch = k_int();
				init[0] = 1;
			}
		/* 1 */
			else if ( k_its("MyModuleId") ) {
				str = k_str();
				if ( str )
					strcpy(MyModName, str);
				init[1] = 1;
			}
		/* 2 */
			else if ( k_its("OutWaveRing") ) {
				str = k_str();
				if ( str )
					strcpy(&RingName[WAVE_MSG_LOGO][0], str);
				init[2] = 1;
			}
			else if ( k_its("OutRawRing") ) {
				str = k_str();
				if ( str )
					strcpy(&RingName[RAW_MSG_LOGO][0], str);
				RawOutputSwitch = 1;
			}
		/* 3 */
			else if ( k_its("HeartBeatInterval") ) {
				HeartBeatInterval = k_long();
				init[3] = 1;
			}
		/* 4 */
			else if ( k_its("QueueSize") ) {
				QueueSize = k_long();
				init[4] = 1;
			}
		/* 5 */
			else if ( k_its("MaxStationNum") ) {
				MaxStationNum = k_long();
				init[5] = 1;
			}
			else if ( k_its("UniSampRate") ) {
				UniSampRate = k_int();
				logit(
					"o", "palert2ew: Change to unified sampling rate mode, the unified sampling rate is %d Hz!\n",
					UniSampRate
				);
			}
			else if ( k_its("UpdateInterval") ) {
				UpdateInterval = k_long();
				if ( UpdateInterval )
					logit(
						"o", "palert2ew: Change to auto updating mode, the updating interval is %ld seconds!\n",
						UpdateInterval
					);
			}
			else if ( k_its("CheckCRC16") ) {
				CheckCRCSwitch = k_int();
				if ( CheckCRCSwitch )
					logit("o", "palert2ew: Turn on the CRC-16 checking function.\n");
			}
			else if ( k_its("OutputTimeQuestionable") ) {
				OutputTimeQuestionable = k_int();
				if ( OutputTimeQuestionable )
					logit("o", "palert2ew: NOTICE!! Those waveforms with questionable timestamp will be output!\n");
			}
		/* 6 */
			else if ( k_its("ServerSwitch") ) {
				if ( (ServerSwitch = k_int()) >= 1 ) {
					ServerSwitch = PA2EW_RECV_SERVER_ON;
					for ( int i = 7; i < 9; i++ )
						init[i] = 1;
				}
				else {
					ServerSwitch = PA2EW_RECV_SERVER_OFF;
				}
				init[6] = 1;
			}
		/* 7 */
			else if ( k_its("ServerIP") ) {
				str = k_str();
				if ( str )
					strcpy( ServerIP, str );
				init[7] = 1;
			}
		/* 8 */
			else if ( k_its("ServerPort") ) {
				str = k_str();
				if ( str )
					strcpy( ServerPort, str );
				init[8] = 1;
			}
			else if ( k_its("SQLHost") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.host, str);
#if defined( _USE_SQL )
				for ( int i = 9; i < 14; i++ )
					init[i] = 0;
#endif
			}
		/* 9 */
			else if ( k_its("SQLPort") ) {
				DBInfo.port = k_long();
				init[9] = 1;
			}
		/* 10 */
			else if ( k_its("SQLUser") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.user, str);
				init[10] = 1;
			}
		/* 11 */
			else if ( k_its("SQLPassword") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.password, str);
				init[11] = 1;
			}
		/* 12 */
			else if ( k_its("SQLDatabase") ) {
				str = k_str();
				if ( str )
					strcpy(DBInfo.database, str);
				init[12] = 1;
			}
		/* 13 */
			else if ( k_its("SQLStationTable") ) {
				str = k_str();
				if ( str )
					strcpy(SQLStationTable, str);
				init[13] = 1;
			}
			else if ( k_its("SQLChannelTable") ) {
				str = k_str();
				if ( str )
					strcpy(SQLChannelTable, str);
			}
			else if ( k_its("Palert") ) {
				str = k_get();
				for ( str += strlen(str) + 1; isspace(*str); str++ );
				if ( pa2ew_list_station_line_parse( str, PA2EW_LIST_INITIALIZING ) ) {
					logit(
						"e", "palert2ew: ERROR, lack of some station information for in <%s>. Exiting!\n",
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
		/* See if there were any errors processing the command */
			if ( k_err() ) {
				logit("e", "palert2ew: Bad <%s> command in <%s>; exiting!\n", com, configfile);
				exit(-1);
			}
		}
		nfiles = k_close();
	}

/* After all files are closed, check init flags for missed commands */
	nmiss = 0;
	for ( int i = 0; i < ncommand; i++ )
		if ( !init[i] )
			nmiss++;
/* */
	if ( nmiss ) {
		logit("e", "palert2ew: ERROR, no ");
		if ( !init[0] )  logit("e", "<LogFile> "          );
		if ( !init[1] )  logit("e", "<MyModuleId> "       );
		if ( !init[2] )  logit("e", "<OutWaveRing> "      );
		if ( !init[3] )  logit("e", "<HeartBeatInterval> ");
		if ( !init[4] )  logit("e", "<QueueSize> "        );
		if ( !init[5] )  logit("e", "<MaxStationNum> "    );
		if ( !init[6] )  logit("e", "<ServerSwitch> "     );
		if ( !init[7] )  logit("e", "<ServerIP> "         );
		if ( !init[8] )  logit("e", "<ServerPort> "       );
		if ( !init[9] )  logit("e", "<SQLPort> "          );
		if ( !init[10] ) logit("e", "<SQLUser> "          );
		if ( !init[11] ) logit("e", "<SQLPassword> "      );
		if ( !init[12] ) logit("e", "<SQLDatabase> "      );
		if ( !init[13] ) logit("e", "<SQLStationTable> "  );

		logit("e", "command(s) in <%s>; exiting!\n", configfile);
		exit(-1);
	}

	return;
}

/**
 * @brief Look up important info from earthworm.h tables
 *
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
	if ( RawOutputSwitch && (RingKey[RAW_MSG_LOGO] = GetKey(&RingName[RAW_MSG_LOGO][0])) == -1 ) {
		fprintf(
			stderr, "palert2ew: Invalid ring name <%s>; exiting!\n", &RingName[RAW_MSG_LOGO][0]
		);
		exit(-1);
	}
	else if ( !RawOutputSwitch ) {
		RingKey[RAW_MSG_LOGO] = -1;
	}

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
	if ( RawOutputSwitch && GetType( "TYPE_PALERTRAW", &TypePalertRaw ) != 0 ) {
		fprintf(stderr, "palert2ew: Invalid message type <TYPE_PALERTRAW>; exiting!\n");
		exit(-1);
	}

	return;
}

/**
 * @brief Builds a heartbeat or error message & puts it into shared memory. Writes errors to log file & screen.
 *
 * @param type
 * @param ierr
 * @param note
 */
static void palert2ew_status( unsigned char type, short ierr, char *note )
{
/* Build the message */
	MSG_LOGO logo = {
		.instid = InstId,
		.mod    = MyModId,
		.type   = type
	};

	char   msg[512];
	size_t size;
	time_t t;

	time(&t);

	if ( type == TypeHeartBeat ) {
		sprintf(msg, "%ld %ld\n", (long)t, (long)MyPid);
	}
	else if ( type == TypeError ) {
		sprintf(msg, "%ld %hd %s\n", (long)t, ierr, note);
		logit("et", "palert2ew: %s\n", note);
	}

	size = strlen(msg);  /* don't include the null byte in the message */

/* Write the message to shared memory */
	if ( tport_putmsg(&Region[0], &logo, (long)size, msg) != PUT_OK ) {
		if ( type == TypeHeartBeat )
			logit("et","palert2ew: Error sending heartbeat.\n");
		else if ( type == TypeError )
			logit("et","palert2ew: Error sending error:%d.\n", ierr);
	}

	return;
}

/**
 * @brief Free all the allocated memory & close socket
 *
 */
static void palert2ew_end( void )
{
	tport_detach( &Region[WAVE_MSG_LOGO] );
	if ( RawOutputSwitch )
		tport_detach( &Region[RAW_MSG_LOGO] );

	pa2ew_msgqueue_end();
	pa2ew_list_end();
/* */
	if ( ServerSwitch )
		pa2ew_server_end();

	free(ReceiverThreadID);
	free((int8_t *)MessageReceiverStatus);

	return;
}

/**
 * @brief
 *
 * @param wait_msec
 */
static void check_receiver_client( const int wait_msec )
{
	if ( MessageReceiverStatus[0] != THREAD_ALIVE ) {
		if ( pa2ew_client_init( ServerIP, ServerPort ) < 0 ) {
			if ( MessageReceiverStatus[0] != THREAD_ERR ) {
				logit("e", "palert2ew: Cannot initialize the connection to Palert server. Exiting!\n");
				palert2ew_end();
				exit(-1);
			}
			else {
			/* We might encounter some disconnection situations, then try to reconnect until it recover */
				logit("et", "palert2ew: Re-initialize the connection to Palert server failed, try next time!\n");
				sleep_ew(PA2EW_RECONNECT_INTERVAL);
				return;
			}
		}
		if ( StartThread(receiver_client_thread, (uint32_t)THREAD_STACK, ReceiverThreadID) == -1 ) {
			logit("e", "palert2ew: Error starting receiver_client thread. Exiting!\n");
			palert2ew_end();
			exit(-1);
		}
		MessageReceiverStatus[0] = THREAD_ALIVE;
	}
/* */
	sleep_ew(wait_msec);

	return;
}

/**
 * @brief
 *
 * @param wait_msec
 */
static void check_receiver_server( const int wait_msec )
{
	static uint8_t *number     = NULL;
	static time_t   time_check = 0;
	static int      thread_num = 0;

	time_t time_now;

/* */
	if ( !thread_num ) {
		time(&time_check);
		thread_num = ReceiverThreadsNum;
	/* */
		number = calloc(thread_num, sizeof(uint8_t));
		for ( int i = 0; i < thread_num; i++ )
			number[i] = i;
	/*
	 * 'cause these sockets are local, it should be much more stable.
	 * Therefore we just need to check once in the beginning
	 */
		if ( pa2ew_server_init( MaxStationNum, PA2EW_PALERT_PORT ) < 1 ) {
			logit("e","palert2ew: Cannot initialize the Palert server process. Exiting!\n");
			palert2ew_end();
			exit(-1);
		}
	}
/* */
	pa2ew_server_palerts_accept( wait_msec );
/* */
	for ( int i = 0; i < thread_num; i++ ) {
		if ( MessageReceiverStatus[i] != THREAD_ALIVE ) {
			if (
				StartThreadWithArg(receiver_server_thread, number + i, (uint32_t)THREAD_STACK, ReceiverThreadID + i) == -1
			) {
				logit("e", "palert2ew: Error starting receiver_server thread(%d). Exiting!\n", i);
				palert2ew_end();
				exit(-1);
			}
			MessageReceiverStatus[i] = THREAD_ALIVE;
		}
	}
/* */
	if ( (time(&time_now) - time_check) >= 60 ) {
		time_check = time_now;
		pa2ew_server_pconnect_check();
	}

	return;
}

/**
 * @brief Receive the messages from the socket of forward server & send it to the MessageStacker.
 *
 * @param dummy
 * @return thr_ret
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
	pa2ew_client_end();
/* File a complaint to the main thread */
	if ( Finish ) {
		sleep_ew(1000);
		MessageReceiverStatus[0] = THREAD_ERR;
	}

	KillSelfThread(); /* main thread will restart us */

	return NULL;
}

/**
 * @brief Receive the messages from the socket of all the Palerts & send it to the MessageStacker.
 *
 * @param arg
 * @return thr_ret
 */
static thr_ret receiver_server_thread( void *arg )
{
	int           ret;
	const uint8_t countindex = *((uint8_t *)arg);

/* Tell the main thread we're ok */
	MessageReceiverStatus[countindex] = THREAD_ALIVE;
/* Main service loop */
	do {
		if ( (ret = pa2ew_server_proc(countindex, 1000)) )
			if ( ret == PA2EW_RECV_NEED_UPDATE )
				if ( UpdateFlag == LIST_IS_UPDATED )
					UpdateFlag = LIST_NEED_UPDATED;
	} while ( Finish );
/* File a complaint to the main thread */
	if ( Finish )
		MessageReceiverStatus[countindex] = THREAD_ERR;

	KillSelfThread();

	return NULL;
}

/**
 * @brief
 *
 * @param arg
 * @return thr_ret
 */
static thr_ret update_list_thread( void *arg )
{
	int update_flag = 0;

	logit("ot", "palert2ew: Updating the Palert list...\n");
	UpdateFlag = LIST_UNDER_UPDATE;
/* */
	pa2ew_list_update_status_set( PA2EW_PALERT_INFO_OBSOLETE );
	if ( pa2ew_list_db_fetch( SQLStationTable, SQLChannelTable, &DBInfo, PA2EW_LIST_UPDATING ) < 0 ) {
		logit("e", "palert2ew: Fetching Palert list from remote database error!\n");
		update_flag = 1;
	}
/* */
	if ( update_list_configfile( (char *)arg ) ) {
		logit("e", "palert2ew: Fetching Palert list from local file error!\n");
		update_flag = 1;
	}
/* */
	if ( update_flag ) {
		pa2ew_list_update_status_set( PA2EW_PALERT_INFO_UPDATED );
		pa2ew_list_tree_abandon();
		logit("e", "palert2ew: Failed to update the Palert list!\n");
		logit("ot", "palert2ew: Keep using the previous Palert list(%.6lf)!\n", pa2ew_list_timestamp_get());
	}
	else {
		pa2ew_list_tree_activate();
		//pa2ew_list_obsolete_clear(); /* Still under testing... */
		logit("ot", "palert2ew: Successfully updated the Palert list(%.6lf)!\n", pa2ew_list_timestamp_get());
		logit(
			"ot", "palert2ew: There are total %d stations in the new Palert list.\n", pa2ew_list_total_station_get()
		);
	}

/* */
	UpdateFlag = LIST_IS_UPDATED;
/* Just exit this thread */
	KillSelfThread();

	return NULL;
}

/**
 * @brief
 *
 * @param configfile
 * @return int
 */
static int update_list_configfile( char *configfile )
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
	while ( nfiles > 0 )  /* While there are command files open */
	{
		while ( k_rd() )  /* Read next line from active file  */
		{
			com = k_str();  /* Get the first token from line */
		/* Ignore blank lines & comments */
			if ( !com || com[0] == '#' )
				continue;
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
				if ( pa2ew_list_station_line_parse( str, PA2EW_LIST_UPDATING ) ) {
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

/**
 * @brief
 *
 * @param packet
 * @param stainfo
 * @param datatype
 */
static void process_packet_pm1( const void *packet, _STAINFO *stainfo, const char datatype[2] )
{
	TracePacket tracebuf;  /* Trace message which is sent to share ring    */
	size_t      data_size  = PALERT_M1_SAMPLE_NUMBER << 2;
	size_t      total_size = data_size + sizeof(TRACE2_HEADER);
	_CHAINFO   *chaptr     = (_CHAINFO *)stainfo->chaptr;
	int32_t    *tb_data    = (int32_t *)(&tracebuf.trh2 + 1);
	int32_t    *_databuf[PALERT_M1_CHAN_COUNT] = {
		tb_data + PALERT_M1_SAMPLE_NUMBER * 0,
		tb_data + PALERT_M1_SAMPLE_NUMBER * 1,
		tb_data + PALERT_M1_SAMPLE_NUMBER * 2,
		tb_data + PALERT_M1_SAMPLE_NUMBER * 3,
		tb_data + PALERT_M1_SAMPLE_NUMBER * 4
	};

/* Common information part */
	pa2ew_trh2_init( &tracebuf.trh2 );
	pa2ew_trh2_scn_enrich( &tracebuf.trh2, stainfo->sta, stainfo->net, stainfo->loc );
	pa2ew_trh2_sampinfo_enrich(
		&tracebuf.trh2,
		PALERT_M1_SAMPLE_NUMBER,
		UniSampRate ? (double)UniSampRate : (double)PALERT_M1_SAMPRATE_GET( (PALERT_M1_HEADER *)packet ),
		pac_m1_systime_get( packet, stainfo->timeshift ),
		datatype
	);
/* Time sync. tag */
	tracebuf.trh2.quality[0] |= stainfo->ntp_errors >= PA2EW_NTP_SYNC_ERR_LIMIT ? TIME_TAG_QUESTIONABLE : 0;
/* Extract all the channels' data */
	pac_m1_data_extract( packet, _databuf );
/* Each channel part */
	for ( int i = 0; i < stainfo->nchannel && i < PALERT_M1_CHAN_COUNT; i++, chaptr++ ) {
	/* First, enrich the channel code */
		memcpy(tracebuf.trh2.chan, chaptr->chan, TRACE2_CHAN_LEN);
		if ( tport_putmsg(&Region[WAVE_MSG_LOGO], &Putlogo[WAVE_MSG_LOGO], total_size, tracebuf.msg) != PUT_OK )
			logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[WAVE_MSG_LOGO]);
	/*
	 * 'cause the total size of tracepacket is 4096 bytes,
	 * larger than total occupied size of mode 1 data which is 2000 bytes.
	 * therefore, here we don't need to care about the edge condition (i == 4).
	 */
		memcpy(tb_data, _databuf[i + 1], data_size);
	/* Only keep the end time that is larger than the last end time */
		chaptr->last_endtime =
			tracebuf.trh2.endtime > chaptr->last_endtime ? tracebuf.trh2.endtime : chaptr->last_endtime;
	}

	return;
}

/**
 * @brief
 *
 * @param packet
 * @param stainfo
 * @param datatype
 */
static void process_packet_pm4( const void *packet, _STAINFO *stainfo, const char datatype[2] )
{
	int               msg_size;
	TracePacket       tracebuf;  /* message which is sent to share ring    */
	_CHAINFO         *chaptr   = (_CHAINFO *)stainfo->chaptr;
	_CHAINFO         *cha_last = (_CHAINFO *)stainfo->chaptr + stainfo->nchannel;
	PALERT_M4_HEADER *pah4     = (PALERT_M4_HEADER *)packet;
	uint8_t          *dataptr  = (uint8_t *)(pah4 + 1);
	uint8_t          *endptr   = (uint8_t *)pah4 + PALERT_M4_PACKETLEN_GET( pah4 );
	int32_t          *tb_data  = (int32_t *)(&tracebuf.trh2 + 1);
/* */
	uint8_t    msbuffer[512];
	uint16_t   msrlength;
	MSRecord  *msr  = NULL;
	SMSRECORD *smsr = NULL;

/* Common information part */
	pa2ew_trh2_init( &tracebuf.trh2 );
	pa2ew_trh2_scn_enrich( &tracebuf.trh2, stainfo->sta, stainfo->net, stainfo->loc );
/* Time sync. tag */
	tracebuf.trh2.quality[0] |= stainfo->ntp_errors >= PA2EW_NTP_SYNC_ERR_LIMIT ? TIME_TAG_QUESTIONABLE : 0;
/* */
	do {
		smsr = (SMSRECORD *)dataptr;
	/* Need to check the byte order */
		msrlength = (smsr->smsrlength[0] << 8) + smsr->smsrlength[1];
	/* */
		if ( msrlength > sizeof(msbuffer) ) {
			logit("et", "palert2ew: Unexpected error with the mode 4 packet from %s, skip it!\n", stainfo->sta);
			break;
		}
	/* */
		memset(msbuffer, 0, sizeof(msbuffer));
		memcpy(msbuffer, dataptr, msrlength);
	/* */
		if ( msr_parse((char *)msbuffer, msrlength, &msr, msrlength, 1, 0) )
			continue;
	/* */
		pa2ew_trh2_sampinfo_enrich(
			&tracebuf.trh2, msr->numsamples, msr->samprate, (double)(MS_HPTIME2EPOCH(msr->starttime)), datatype
		);
		memcpy(tracebuf.trh2.chan, chaptr->chan, TRACE2_CHAN_LEN);
	/* */
		msg_size = tracebuf.trh2.nsamp * ms_samplesize(msr->sampletype);
		memcpy(tb_data, msr->datasamples, msg_size);
		msr_free(&msr);
		msg_size += sizeof(TRACE2_HEADER);
		if ( tport_putmsg(&Region[WAVE_MSG_LOGO], &Putlogo[WAVE_MSG_LOGO], msg_size, tracebuf.msg) != PUT_OK )
			logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[WAVE_MSG_LOGO]);
	/* Only keep the end time that is larger than the last end time */
		chaptr->last_endtime =
			tracebuf.trh2.endtime > chaptr->last_endtime ? tracebuf.trh2.endtime : chaptr->last_endtime;
		chaptr++;
	} while ( (dataptr += msrlength) < endptr && chaptr < cha_last );

	return;
}

/**
 * @brief
 *
 * @param packet
 * @param stainfo
 * @param datatype
 */
static void process_packet_pm16( const void *packet, _STAINFO *stainfo, const char datatype[2] )
{
/* */
	static uint8_t databuf[sizeof(PALERT_M16_PACKET)];
/* */
	TracePacket    tracebuf;  /* Trace message which is sent to share ring    */
	uint16_t       nsamp      = PALERT_M16_SAMPNUM_GET( (PALERT_M16_HEADER *)packet );
	size_t         data_size  = nsamp << 2;
	size_t         total_size = data_size + sizeof(TRACE2_HEADER);
	_CHAINFO      *chaptr     = (_CHAINFO *)stainfo->chaptr;
	int32_t       *tb_data    = (int32_t *)(&tracebuf.trh2 + 1);
	float         *_databuf[stainfo->nchannel];

/* Common information part */
	pa2ew_trh2_init( &tracebuf.trh2 );
	pa2ew_trh2_scn_enrich( &tracebuf.trh2, stainfo->sta, stainfo->net, stainfo->loc );
/* */
	pa2ew_trh2_sampinfo_enrich(
		&tracebuf.trh2,
		nsamp,
		PALERT_M16_SAMPRATE_GET( (PALERT_M16_HEADER *)packet ),
		pac_m16_sptime_get( (PALERT_M16_HEADER *)packet ),
		datatype
	);
/* Time sync. tag */
	tracebuf.trh2.quality[0] |= stainfo->ntp_errors >= PA2EW_NTP_SYNC_ERR_LIMIT ? TIME_TAG_QUESTIONABLE : 0;
/* Extract all the channels' data */
	for ( int i = 0; i < stainfo->nchannel; i++ )
		_databuf[i] = (float *)&databuf + (nsamp * i);
	pac_m16_data_extract( packet, stainfo->nchannel, _databuf );
/* Each channel part */
	for ( int i = 0; i < stainfo->nchannel; i++, chaptr++ ) {
	/* First, enrich the channel code */
		memcpy(tracebuf.trh2.chan, chaptr->chan, TRACE2_CHAN_LEN);
	/* Then, copy the data from the buffer to the trace buffer */
		memcpy(tb_data, _databuf[i], data_size);
	/* Put it into the share ring */
		if ( tport_putmsg(&Region[WAVE_MSG_LOGO], &Putlogo[WAVE_MSG_LOGO], total_size, tracebuf.msg) != PUT_OK )
			logit("e", "palert2ew: Error putting message in region %ld\n", RingKey[WAVE_MSG_LOGO]);
	/* Only keep the end time that is larger than the last end time */
		chaptr->last_endtime =
			tracebuf.trh2.endtime > chaptr->last_endtime ? tracebuf.trh2.endtime : chaptr->last_endtime;
	}

	return;
}

/**
 * @brief
 *
 * @param stainfo
 * @param packet
 * @param packet_mode
 * @return int
 */
static int examine_ntp_status( _STAINFO *stainfo, const void *packet, const int packet_mode )
{
	static const uint8_t pre_threshold = PA2EW_NTP_SYNC_ERR_LIMIT * 0.8;
	uint8_t * const      ntp_errors    = &stainfo->ntp_errors;

/* Check NTP status. depends on its packet mode */
	switch ( packet_mode ) {
	case PALERT_PKT_MODE1: default:
		if ( !PALERT_M1_NTP_CHECK( (PALERT_M1_HEADER *)packet ) )
			goto not_sync;
		break;
	case PALERT_PKT_MODE4:
		if ( !PALERT_M4_NTP_CHECK( (PALERT_M4_HEADER *)packet ) )
			goto not_sync;
		break;
	case PALERT_PKT_MODE16:
		if ( !PALERT_M16_NTP_CHECK( (PALERT_M16_HEADER *)packet ) )
			goto not_sync;
		break;
	}

/* Good NTP status */
	if ( *ntp_errors >= PA2EW_NTP_SYNC_ERR_LIMIT )
		logit("ot", "palert2ew: Station %s reconnect to NTP server & time re-synchronized!\n", stainfo->sta);
	*ntp_errors = 0;
/* If the NTP status is good, we should accept this waveform */
	goto pass;

not_sync:
	if ( (*ntp_errors)++ >= pre_threshold ) {
		if ( *ntp_errors < PA2EW_NTP_SYNC_ERR_LIMIT ) {
			printf("palert2ew: Station %s lost connection to NTP server, please check it!\n", stainfo->sta);
			goto pass;
		}
		else if ( *ntp_errors == PA2EW_NTP_SYNC_ERR_LIMIT ) {
			logit(
				"et",
				OutputTimeQuestionable ?
				"palert2ew: NOTICE!! Station %s time unsynchronized, the waveforms will be marked.\n" :
				"palert2ew: NOTICE!! Station %s time unsynchronized, reject the waveforms.\n",
				stainfo->sta
			);
		}
	/* Keep the error count equal to limit plus 1 */
		*ntp_errors = PA2EW_NTP_SYNC_ERR_LIMIT + 1;
	/* NTP status is not good and it exceed the error limit, we will reject this waveform */
		return 0;
	}

pass:
/* Even NTP status is not good but it still under the error limit, we also accept this waveform */
	return 1;
}

/**
 * @brief
 *
 * @param packet
 * @param packet_mode
 * @return int
 */
static int check_pkt_crc( const void *packet, const int packet_mode )
{
/* Check CRC number. depends on its packet mode */
	switch ( packet_mode ) {
	case PALERT_PKT_MODE1: default:
		if ( pac_m1_crc_check( packet ) )
			return 1;
		break;
	case PALERT_PKT_MODE4:
		if ( pac_m4_crc_check( packet ) )
			return 1;
		break;
	case PALERT_PKT_MODE16:
		if ( pac_m16_crc_check( packet ) )
			return 1;
		break;
	}

	return 0;
}

/**
 * @brief
 *
 */
static void handle_signal( void )
{
	struct sigaction   act;

/* Signal handling */
	sigemptyset(&act.sa_mask);
	act.sa_sigaction = NULL;
	act.sa_flags     = 0;
	act.sa_handler   = SIG_IGN;

	sigaction(SIGPIPE, &act, (struct sigaction *)NULL);

	return;
}
