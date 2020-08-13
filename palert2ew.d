
# This is palert2ew's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_PALERT2EW  # module id for this instance of template
OutWaveRing        WAVE_RING      # shared memory ring for output wave trace
OutRawRing         RPALERT_RING   # shared memory ring for output raw packet;
                                  # if not define, will close this function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
HeartBeatInterval  15             # seconds between heartbeats

QueueSize          1000           # max messages in internal circular msg buffer

# Palert server setup:
#
ServerSwitch      0               # 0 connect to Palert server; 1 as the server of Palert; 2 hybrid mode
                                  # Warning: Do not receive the same station by two ways simultaneously!!
ServerIP          127.0.0.1       # Server IP address of reciving socket
ServerPort        23000           # Server port of reciving socket
MaxStationNum     1024            # max number of stations which will receive data from

# MySQL server setup:
#
SQLHost         127.0.0.1         # The maximum length is 36 words
SQLPort         3306              # Port number between 1 to 65536
SQLDatabase     EEW	              # The maximum length is 36 words

@LoginInfo_sql                    # Please keep the security of the SQL login information

# Login information example:
#
# SQLUser       test
# SQLPassword   123456

# List the stations list to grab from MySQL server
#
StationTable      PalertList
ChannelTable      PalertChannelList
