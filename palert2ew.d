
# This is palert2ew's parameter file

# Basic Earthworm setup:
#
MyModuleId         MOD_PALERT2EW  # module id for this instance of template
OutWaveRing        WAVE_RING      # shared memory ring for output wave trace
#OutRawRing         RPALERT_RING   # shared memory ring for output raw packet;
                                  # if not define, it will close this output function
#OutExtendRing      PEXTEND_RING   # shared memory ring for output extension messages
                                  # like, retransmissioned wave trace, if not define,
                                  # it will close this output function
LogFile            1              # 0 to turn off disk log file; 1 to turn it on
                                  # to log to module log but not stderr/stdout
ExtFunc            0              # 0 to turn off extension function; 1 to turn it on
                                  # like, retransmissioned wave trace,
HeartBeatInterval  15             # seconds between heartbeats

QueueSize          1000           # max messages in internal circular msg buffer
MaxStationNum      1024           # max number of stations which will receive data from
UniSampRate        100            # setting for unified sampling rate (Hz), if set this parameter,
                                  # all of the P-alerts will be applied by this value; or just
                                  # comment it out, let the program detect the sampling rate
                                  # from the packet.
UpdateInterval     0              # setting for automatical updating interval (seconds). If set this
                                  # parameter larger than 0, the program will update the P-alerts
                                  # list with this interval; or the program will ignore the new
                                  # incoming P-alerts' packets.
# Palert server setup:
#
# There are two independent mode in this module: first, as a client-side program connect to the P-alert
# Core; second, as server-side program accept those P-alerts connections. While setting second mode, the
# P-alert Core server informations are neccessary.
#
ServerSwitch      0               # 0 connect to Palert server; 1 as the server of Palert
ServerIP          127.0.0.1       # Server IP address of P-alert Core server
ServerPort        23000           # Server port of P-alert Core server

# MySQL server information:
#
# If you setup the follow parameter especially SQLHost, this program will fetch
# list from MySQL server or you can just comment all of them, then it will turn
# off this function.
#
SQLHost         127.0.0.1         # The maximum length is 36 words
SQLPort         3306              # Port number between 1 to 65536
SQLDatabase     EEW	              # The maximum length is 36 words

# Login information example
#
# SQLUser       test
# SQLPassword   123456
#@LoginInfo_sql                    # Please keep the security of the SQL login information

# List the stations list that will grab from MySQL server
#
# Even when you using MySQL server to fetch station information, the channel table is
# optional. Once you comment the option, the channel information will be filled by
# default value(HLZ, HLN & HLE).
#
SQLStationTable    PalertList
SQLChannelTable    PalertChannelList

# Local station list:
#
# The local list for P-alerts that will receive. By the way, the priority of local list
# is higher than the one from remote data. The channel codes are optional, and the value
# will be filled by the default value. And the layout should be like these example:
#
# Palert   Serial   Station   Network   Location   Nchannel   Channel_0   Channel_1   Channel_2
# Palert    1993      TEST      TW         --         3         HLZ          HLN        HLE      # First example
# Palert    1993      TEST      TW         --         0                                          # Optional example
#
#@palertlist_p2ew
