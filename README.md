# raspberry_rtl_wh1080
[RaspberryPi] (https://www.raspberrypi.org/) specific rtl-sdr solution to decode [Fine Offset WH1080 weather station] (http://www.foshk.com/weather_professional/wh1080.htm) data and [BMP085] (https://www.google.com/search?q=BMP085)/[BMP180] (https://www.google.com/search?q=BMP180) barometric sensor (using  [rtl_433] (https://github.com/merbanan/rtl_433)).
------------------------------

- The [Raspberry Pi] (https://www.raspberrypi.org/) is a tiny and affordable computer with a nice [GPIO] (https://en.wikipedia.org/wiki/General-purpose_input/output) interface.
- [rtl_433] (https://github.com/merbanan/rtl_433) is a fantastic tool to turn your [Realtek RTL2832 based DVB dongle] (https://www.google.com/search?q=Realtek+RTL2832+based+DVB+dongle&source=lnms&tbm=isch&sa=X&ved=0) into a generic data receiver.
- The [Fine Offset WH1080] (http://www.foshk.com/weather_professional/wh1080.htm) is a relatively low-cost weather station and is also sold rebranded with many names: Watson W-8681, Digitech XC0348 Weather Station, PCE-FWS 20, Elecsa AstroTouch 6975, Froggit WH1080 and many others.
 
--
The WH1080 weather station it's composed by an indoor touchscreen control panel and an outdoor wireless sensors group. The latter sends periodically data packets to the indoor console, containing weather measurements about wind speed, wind direction, temperature, humidity and rain. It periodically sends also time signals ([DCF77] (https://en.wikipedia.org/wiki/DCF77) and maybe other time signals standards) to keep the console clock perfectly synced with an atomic clock.
The indoor console itself contains a barometric sensor and another hygro and temperature sensor. All of these data are available thru the console's USB port: by connecting a PC and by using some opportune software it's possible to keep track of the weather conditions on your area.





Unfortunately the USB connection is not much reliable in the longtime and it tends to stall every often, giving the need for a console's reset. Furthermore, having a pc connected to the console with a (short) USB cable makes impossible to keep the console handy, so you have to choose if you want to see around the console or if you want to record its data.

A solution would be to grab the radio signals sent by the outdoor sensors with some kind of receiver, opportunely decoding and saving them. But there is a disadvantage: because the barometric sensor is enclosed into the indoor console unit and not in the outdoor sensors group, pressure data is NOT available into these radio signals. So we should connect a barometric sensor to this receiver to integrate the missing pressure data.

The RaspberryPi mini computer fits perfectly to this purpose: it's cheap, it has GPIO ports to connect sensors, its low power is more than enough for this task and it will not weigh down your electric bill even with 24/7 service (it's much less than 5 watts). The pressure sensor (barometer) it's a cheap BMP085 or BMP180 and its wiring requires only four wires.

To receive WH1080's data with a RaspberryPi I have first tested using an RFM01 module: it's a tiny radio data receiver that works fine in normal conditions. Some line of code found on the web, some more wiring, and the ensemble 'Rasp + RFM01 + BMP085' has worked for a season... Or so. 

Unfortunately, with season changes I've found that WH1080 tends to drift in frequency, probably because of the lack of frequency/temperature compensation on the external sensors' TX: the more the temperature was lowering in winter, the more the signal was drifting and my 'ensemble' was unable to cope with it. So during the most interesting part of winter I was unable to track down data!

Trying to recalibrate frequency on RFM01 was an option, but as soon as summer approaching, the same problem obviously rised again with the same result: no data, need to recalibrate back again...

Furthermore the C code solution used to read data from the RFM01 module was built to write the received data into a file. The file was then read by my Python datalogger script to get the data contained within. 
This process was happening every 48 seconds (the WH1080 sends its data every 48 seconds), so this means that in a year that file was overwritten more than 500,000 times! It's way too much for the poor SDcard which is the 'hard disk' of the Rasp (as you know there is a finite number of times for write-cycles in such a media). 
Hacking the C code has been necessary so the program was passing its values 'on-the-fly' to a Python script instead of writing data to a file. But this approach, summed up to the frequency problems, somehow introduced new instability behaviours...

There was a need for another way to receive data by using a different approach: [rtl-sdr] (http://sdr.osmocom.org/trac/wiki/rtl-sdr).

There is much documentation on the web about it, and the **rtl_433** project, which relies on rtl-sdr libraries, is a **perfect** solution for this task. 

By testing I've found that:

- the rtl-sdr dongle is far more sensible than the RFM01;
- it's more able to cope with frequency drifting;
- the reading process takes much less CPU power than RFM01 (my rtl_433 process is around 15% on an 'old' Rasp model B);
- It's plug & play! Just insert the dongle in the Rasp USB port!

But we still have the same problem about lack of pressure data so we still need a BMP085 or compatible sensor, with relative data reading code, and being rtl_433 an agnostic project, it does not contains specific code for the Raspberry and/or sensors.

So that's my try to integrate some borrowed from the web code to read BMP085 data to an rtl_433 snapshot to build this all-in-one solution for the WH1080. 

Looking at the code you'll may find that it's not such elegant, but it's a kind of test and it's working fine to me. It's tested with a RaspberryPi Model B, also with a B+ model, Raspbian Jessie (2015-11-21), a nameless USB DVB-T RTL2832U dongle and a BMP180 sensor. It should compile and work happily to the Raspberry Pi 2 too.

I have stripped all of the devices modules from rtl_433 source, leaving active only the **'Fine Offset WH1080 weather station'** one to keep the resources use at minimum, but I think that to re-add modules to support some other device should be not so difficult.


So this software can:
----------------

- Get your WH1080 weather data: wind dir and speed, temp, hum, rain, and pressure (from the wired BMP085/BMP180 sensor);
- Get the exact time (DCF77 time system and maybe more) from the station. By using some script you can easily set the Rasp internal clock without the need of NTP or RTC. No data connection required!
- Give you a valid json data output for your Python (or other programming languages) needs;
- Give you the flexibility of rtl_433 thanks to its options: you can optimize data receiving, frequency etc. ...


--


Installation instructions (tested on Raspbian Jessie (2015-11-21)):
--------------------------

Plug the USB dongle into the Rasp and connect the pressure sensor to the GPIO port (search Google for how to do that on your RaspberryPi model. There are just 4 wires to connect. Just **BE AWARE** to use the 3.3V pin, **NOT** the 5V pin).
 
As this work is derived from rtl_433, the same compilation and installation procedures applies, but because of the barometric sensor you need some extra operation:

First of all SPI and I2C on the Rasp must be enabled. Use *sudo raspi-config* and go to the 'Advanced Options' and enable both. Answer 'Yes' to the question about kernel module to be loaded by default, but do not reboot at the moment.

Then:

--

sudo apt-get update

sudo apt-get install libusb-1.0-0-dev i2c-tools libi2c-dev cmake git


cd /home/pi

git clone git://git.osmocom.org/rtl-sdr.git

cd rtl-sdr

mkdir build

cd build

cmake ../ -DINSTALL_UDEV_RULES=ON -DDETACH_KERNEL_DRIVER=ON

make

sudo make install

sudo reboot


--
Now that we have the rtl-sdr base installed we can proceed with **raspberry_rtl_wh1080**:


--
cd /home/pi

git clone https://github.com/ovrheat/raspberry_rtl_wh1080.git

cd raspberry_rtl_wh1080


*Now an important part: you MUST edit the file:*
--------

~/raspberry_wh_1080/src/devices/fineoffset_wh1080.c

find the line containing:

--
const unsigned char station_altitude = 10;  // <----- Edit this value entering YOUR station altitude!
--

'10' is my station altitude in meters. You must change this to YOUR station altitude (in meters), otherwise your pressure reading will be incorrect.


Another thing to look for is this line:

--
char *fileName = "/dev/i2c-1"; //<------- If your Raspberry is an older model and pressure doesn't work, try changing '1' to '0'
--

It's self-explaining, I hope. If something doesn't work with pressure and you are sure of your BMP085 wiring, then try changing that '/dev/i2c-1' in '/dev/i2c-0' . 


--
After that, save the file and go back to the root of the source directory:

--
cd /home/pi/raspberry_rtl_wh1080

mkdir build

cd build

cmake ../

make

sudo make install


--
You are done. Now we need to know what frequencies your WH1080 is using. This station TX comes in (at least) three different frequencies models: 868 Mhz, 433 Mhz and 915 Mhz.
My station sends data on 868.3 Mhz, so my command line is:

*rtl_433 -f 868300000 -l 0*


If your station transmits on 433 Mhz you can omit the '-f' part, as rtl_433 defaults to that frequency, but leave the '-l 0' parameter:

*rtl_433 -l 0*


If you want json data output, use -F json parameter:

*rtl_433 -f 868300000 -F json -l 0*

--
The WH1080 sends time packets on the start of (mostly) every even hour: at the minute 59 of the odd hour the station stops sending weather data. After some minute of silence, probably used to sync purpose, the station starts to send time data for around three minutes or so. Then it restarts to send weather data as usual.


To recognize message type (weather or time) and adapt your data acquisition, you can look at the 'msg_type' field on json output:

*msg_type 0 = weather data*

*msg_type 1 = time data*

--
For specific usage of rtl_433 (and other relative options) you can look at the [project page] (https://github.com/merbanan/rtl_433). Just don't bother them with questions related to Raspberry and pressure sensors... :)


------------------------------------------------------------------

Notes:
- this is just a 'hacked' version of rtl_433; credits and kudos should go to this fantastic tool's authors.
- BMP085 code comes from https://www.john.geek.nz/2013/02/update-bosch-bmp085-source-raspberry-pi/ . Kudos!






