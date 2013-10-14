# WinKey Protols

sn | code            | return |descrition
---+-----------------+--------+----
1  |00 00 <100ms> FF |        |calibrate
2  |00 01            |        |reset
3  |00 02            |version |host on
4  |00 03            |        |host off
5  |00 04 <cc>       |<cc>    |echo test
6  |00 05            |<nn>    |paddle ad value
7  |00 06            |<nn>    |speed ad value, 0-63
8  |00 07            |<...>   |return all parameters
9  |00 09            |<nn>    |return calibrate value
10 |01 <nn>          |        |side tone freq, 1-10
11 |02 <nn>          |        |WPM speed, 0-99
12 |03 <nn>          |        |weight, 10-90
13 |04 <n1> <n2>     |        |PTT lead/tail time
14 |05 <n1> <n2> <n3>|        |speed pot range
15 |06 <nn>          |        |pause sending,0 or 1
16 |07               |<nn>    |get speed pot
17 |08               |        |put a byte back into buf
18 |09 <nn>          |        |config output and more
19 |0A               |        |clear buf
20 |0B <nn>          |        |key down,0 or 1
21 |0C <nn>          |        |high speed CW
22 |0D <nn>          |        |Farns WPM, 10-99
23 |0E <nn>          |        |set mode
24 |0F 15<bb>        |        |15 binary value
25 |10 <nn>          |        |first dit extension
26 |11 <nn>          |        |key componsation
27 |12 <nn>          |        |paddle switchpoint,10-90
28 |13               |        |NOP
29 |14 <nn>          |        |software paddle
30 |15               |<bb>    |request winkey status
31 |16 <nn>          |        |buf pointer cmd
32 |16 03 <nn>       |        |add multiple nulls
33 |17 <nn>          |        |set dit/dah ratio
   |below are buffered commands|
34 |18 <nn>          |        |PTT on/off
35 |19 <nn>          |        |key down <nn> seconds
36 |1A <nn>          |        |delay
37 |1B <cc><cc>      |        |mergy two letters
38 |1C <nn>          |        |change speed
39 |1D <nn>          |        |change high speed
40 |1E               |        |cancel buffered speed 
41 |1F               |        |NOP

0x22-0x40 are some prosign keys

Whenever the speed pot is moved the new value will be sent to the host, as well as the internal status register changes. The MSB should be set to indicate such an unsolicted byte, and the 6th bit is to be set to indicate a speed or status byte.





