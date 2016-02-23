
/* 
 * *** Fine Offset WH1080 Weather Station ***
 * (aka Watson W-8681)
 * (aka Digitech XC0348 Weather Station)
 * (aka PCE-FWS 20) 
 * (aka Elecsa AstroTouch 6975)
 * (aka Froggit WH1080)
 * (aka .....)
 *
 * Hacked rtl_433 version for the RaspberryPi with BMP085/BMP180 or similar pressure sensor.
 *
 *********************
 * 
 * This module is based on Stanisław Pitucha ('viraptor' https://github.com/viraptor ) earl code 
 * for the Digitech XC0348 Weather Station, a rebranded Fine Offset WH1080 Weather Station. 
 *
 * Some info and code derived from Kevin Sangelee's page: 
 * http://www.susa.net/wordpress/2012/08/raspberry-pi-reading-wh1081-weather-sensors-using-an-rfm01-and-rfm12b/ .
 *
 * See also Frank 'SevenW' page ( https://www.sevenwatt.com/main/wh1080-protocol-v2-fsk/ ) for some other useful info.
 *
 * BMP085 code taken from https://www.john.geek.nz/2013/02/update-bosch-bmp085-source-raspberry-pi/
 *
 * I have re-elaborated and merged their works. Credits (and kudos) should go to them all (and to many others too).
 *
 *********************
 *
 * This weather station is based on an indoor touchscreen receiver, and on a 5+1 outdoor wireless sensors group 
 * (rain, wind speed, wind direction, temperature, humidity, plus a DCF77 time signal decoder, 
 * maybe capable to decode some other time signal standard).
 * See the product page here: http://www.foshk.com/weather_professional/wh1080.htm . 
 * It's a very popular weather station, you can easily find it on eBay or Amazon (just do a search for 'WH1080').
 *
 * The module seems to work fine, decoding all of the data as read into the original console 
 * (there is some minimal difference sometime on the decimals due to the different architecture
 * of the console processor, which is a little less precise).
 *
 * Data are trasmitted in a 48 seconds cycle (data packet, then wait 48 seconds, then data packet...).
 * 
 * This module is also capable to decode the DCF77 time signal sent by the wireless time signal decoder: 
 * around the minute 59 of (most of) the even hours the sensor's TX stops sending weather data, 
 * probably to receive (and sync with) DCF77 signal.
 * After around 3-4 minutes of silence it starts to send just time data for some minute, 
 * then it starts again with weather data as usual.
 *
 * To recognize message type (weather or time) you can use the 'msg_type' field on json output:
 * msg_type 0 = weather data
 * msg_type 1 = time data
 *
 * By living in Europe I can only test DCF77 time decoding, so if you live outside Europe and you find garbage instead of correct time,
 * you should disable time decoding (or, better, try to implement a more complete time decoding system :) ).
 *
 * The 'Total rainfall' field is a cumulative counter, increased by 0.3 millimeters of rain at once.
 *
 * The station comes in three TX operating frequency versions: 433, 868.3 and 915 Mhz. 
 * I've had tested the module with a 'Froggit WH1080' on 868.3 Mhz, using '-f 868140000' as frequency parameter and it works fine 
 * I don't know if it works also with other versions and, generally speaking, with ALL of the rebranded versions of this weather station. 
 * I guess it *should* do... Just give it a try! :)
 *
 * The WH1080's pressure sensor (barometer) is enclosed into the indoor console unit, NOT in the outdoor wireless sensors group.
 * That's why you will NOT find any pressure data into the RF packets coming from sensors... But WE MISS THAT PRESSURE DATA!!...
 * There's a solution: a (cheap) Raspberry Pi, a (supercheap) BMP085/BMP180 sensor wired to the Rasp,
 * and some line of code placed somewhere to read data coming from the pressure sensor itself.
 * So I've hacked a snapshot of rtl_433: I have stripped all of its device modules except for the 'Fine Offset WH1080' and added 
 * the pressure read code. No need anymore to mess around with external code to get all of the weather data from the station.
 * Now you will find all of data (pressure included) on rtl_433 json output, ready for your logging pleasures! :)
 * (Note: BMP180 is a pin-to-pin compatible replacement for the obsolete BMP085 sensor) 
 *
 * IMPORTANT NOTE: before compiling, edit 'station_altitude' value to reflect yours (the value should be in METERS).
 * If you don't, your pressure read will be wrong. You are warned!
 *
 * Now search Google for 'Raspberry BMP085 wiring', there are only 4 wires to connect. Just **REMEMBER**
 * to check two, three times to connect the sensor's Vcc wire to the 3.3V pin, ***NOT** to the 5V pin! Ok?
 * 
 *
 * ***TODO***: check if negative temperature values (and sign) are OK (no real winter this year where I live, so cannot test...) .
 * 
 * Have fun!
 *
 * 2016 Nicola Quiriti ('ovrheat')
 *
 *
 */
 

#include "data.h"
#include "rtl_433.h"
#include "util.h"
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <stdlib.h>

#include <unistd.h>
#include <linux/i2c-dev.h>
//#include <linux/i2c.h>
#include <sys/ioctl.h>


#define CRC_POLY 0x31
#define CRC_INIT 0xff

#define BMP085_I2C_ADDRESS 0x77


const unsigned char station_altitude = 10;  // <----- Edit this value entering YOUR station altitude!

const unsigned char BMP085_OVERSAMPLING_SETTING = 3;


// Calibration values - These are stored in the BMP085
short int ac1;
short int ac2; 
short int ac3; 
unsigned short int ac4;
unsigned short int ac5;
unsigned short int ac6;
short int b1; 
short int b2;
short int mb;
short int mc;
short int md;

int b5; 

unsigned int temperature, pressure;


// Open a connection to the bmp085
// Returns a file id
int bmp085_i2c_Begin()
{
	int fd;
	char *fileName = "/dev/i2c-1"; //<------- If your Raspberry is an older model and pressure doesn't work, 
	// try changing '1' to '0'. Also change it to '2' if you are using a BananaPi! ("/dev/i2c-2";)
	
	// Open port for reading and writing
	if ((fd = open(fileName, O_RDWR)) < 0)
		exit(1);
	
	// Set the port options and set the address of the device
	if (ioctl(fd, I2C_SLAVE, BMP085_I2C_ADDRESS) < 0) {					
		close(fd);
		exit(1);
	}

	return fd;
}

// Read two words from the BMP085 and supply it as a 16 bit integer
__s32 bmp085_i2c_Read_Int(int fd, __u8 address)
{
	__s32 res = i2c_smbus_read_word_data(fd, address);
	if (res < 0) {
		close(fd);
		exit(1);
	}

	// Convert result to 16 bits and swap bytes
	res = ((res<<8) & 0xFF00) | ((res>>8) & 0xFF);

	return res;
}

//Write a byte to the BMP085
void bmp085_i2c_Write_Byte(int fd, __u8 address, __u8 value)
{
	if (i2c_smbus_write_byte_data(fd, address, value) < 0) {
		close(fd);
		exit(1);
	}
}

// Read a block of data BMP085
void bmp085_i2c_Read_Block(int fd, __u8 address, __u8 length, __u8 *values)
{
	if(i2c_smbus_read_i2c_block_data(fd, address,length,values)<0) {
		close(fd);
		exit(1);
	}
}


void bmp085_Calibration()
{
	int fd = bmp085_i2c_Begin();
	ac1 = bmp085_i2c_Read_Int(fd,0xAA);
	ac2 = bmp085_i2c_Read_Int(fd,0xAC);
	ac3 = bmp085_i2c_Read_Int(fd,0xAE);
	ac4 = bmp085_i2c_Read_Int(fd,0xB0);
	ac5 = bmp085_i2c_Read_Int(fd,0xB2);
	ac6 = bmp085_i2c_Read_Int(fd,0xB4);
	b1 = bmp085_i2c_Read_Int(fd,0xB6);
	b2 = bmp085_i2c_Read_Int(fd,0xB8);
	mb = bmp085_i2c_Read_Int(fd,0xBA);
	mc = bmp085_i2c_Read_Int(fd,0xBC);
	md = bmp085_i2c_Read_Int(fd,0xBE);
	close(fd);
}

// Read the uncompensated temperature value
unsigned int bmp085_ReadUT()
{
	unsigned int ut = 0;
	int fd = bmp085_i2c_Begin();

	// Write 0x2E into Register 0xF4
	// This requests a temperature reading
	bmp085_i2c_Write_Byte(fd,0xF4,0x2E);
	
	// Wait at least 4.5ms
	usleep(5000);

	// Read the two byte result from address 0xF6
	ut = bmp085_i2c_Read_Int(fd,0xF6);

	// Close the i2c file
	close (fd);
	
	return ut;
}

// Read the uncompensated pressure value
unsigned int bmp085_ReadUP()
{
	unsigned int up = 0;
	int fd = bmp085_i2c_Begin();

	// Write 0x34+(BMP085_OVERSAMPLING_SETTING<<6) into register 0xF4
	// Request a pressure reading w/ oversampling setting
	bmp085_i2c_Write_Byte(fd,0xF4,0x34 + (BMP085_OVERSAMPLING_SETTING<<6));

	// Wait for conversion, delay time dependent on oversampling setting
	usleep((2 + (3<<BMP085_OVERSAMPLING_SETTING)) * 1000);

	// Read the three byte result from 0xF6
	// 0xF6 = MSB, 0xF7 = LSB and 0xF8 = XLSB
	__u8 values[3];
	bmp085_i2c_Read_Block(fd, 0xF6, 3, values);

	up = (((unsigned int) values[0] << 16) | ((unsigned int) values[1] << 8) | (unsigned int) values[2]) >> (8-BMP085_OVERSAMPLING_SETTING);

	// Close the i2c file
	close (fd);
	
	return up;
}

// Calculate pressure given uncalibrated pressure
// Value returned will be in units of Pa
unsigned int bmp085_GetPressure(unsigned int up)
{
	int x1, x2, x3, b3, b6, p;
	unsigned int b4, b7;
  
	b6 = b5 - 4000;
	// Calculate B3
	x1 = (b2 * (b6 * b6)>>12)>>11;
	x2 = (ac2 * b6)>>11;
	x3 = x1 + x2;
	b3 = (((((int)ac1)*4 + x3)<<BMP085_OVERSAMPLING_SETTING) + 2)>>2;
  
	// Calculate B4
	x1 = (ac3 * b6)>>13;
	x2 = (b1 * ((b6 * b6)>>12))>>16;
	x3 = ((x1 + x2) + 2)>>2;
	b4 = (ac4 * (unsigned int)(x3 + 32768))>>15;
  
	b7 = ((unsigned int)(up - b3) * (50000>>BMP085_OVERSAMPLING_SETTING));
	if (b7 < 0x80000000)
		p = (b7<<1)/b4;
	else
		p = (b7/b4)<<1;
	
	x1 = (p>>8) * (p>>8);
	x1 = (x1 * 3038)>>16;
	x2 = (-7357 * p)>>16;
	p += (x1 + x2 + 3791)>>4;
  
	return p;
}

// Calculate temperature given uncalibrated temperature
// Value returned will be in units of 0.1 deg C
unsigned int bmp085_GetTemperature(unsigned int ut)
{
	int x1, x2;
  
	x1 = (((int)ut - (int)ac6)*(int)ac5) >> 15;
	x2 = ((int)mc << 11)/(x1 + md);
	b5 = x1 + x2;

	unsigned int result = ((b5 + 8)>>4);  

	return result;
}


// --------- Get PRESSURE -------------------------------------------------------------------


double read_int_temp()
{
	bmp085_Calibration();
	temperature = bmp085_GetTemperature(bmp085_ReadUT());
	pressure = bmp085_GetPressure(bmp085_ReadUP());
	
	return ((double)temperature)/10;	
}


double read_press()
{
	bmp085_Calibration();
	temperature = bmp085_GetTemperature(bmp085_ReadUT());
	pressure = bmp085_GetPressure(bmp085_ReadUP());

	return ((((double)pressure)/100)/ pow(1.0 - station_altitude/44330.0, 5.255));
	
	//Relative pressure calculated from 'station_altitude' value. See https://en.wikipedia.org/wiki/Barometric_formula 
	//See also: https://www.mkompf.com/weather/pibaro.html
	//
	//Remember to change 'station_altitude' value at the top of this file to reflect YOUR station altitude!
}


//-------------------------------------------------------------------------------------------


unsigned short msg_type = 0; // 0=Weather   1=Time


static const char* wind_dir_string[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW",};
static const char* wind_dir_degr[]= {"0", "23", "45", "68", "90", "113", "135", "158", "180", "203", "225", "248", "270", "293", "315", "338",};

static unsigned short get_device_id(const uint8_t* br) {
	return (br[1] << 4 & 0xf0 ) | (br[2] >> 4);
}

static char* get_battery(const uint8_t* br) {  // Disabled - Still unknown if it's right
	if ((br[9] >> 4) == 0) {
		return "OK";
	} else {
		return "LOW";
	}	
}
	
// ------------ WEATHER SENSORS DECODING ----------------------------------------------------

static float get_temperature(const uint8_t* br) {
    const int temp_raw = (br[2] << 8) + br[3];
    return ((temp_raw & 0x0fff) - 0x190) / 10.0;
}

static int get_humidity(const uint8_t* br) {
    return br[4];
}

static const char* get_wind_direction_str(const uint8_t* br) {
    return wind_dir_string[br[9] & 0x0f];
}

static const char* get_wind_direction_deg(const uint8_t* br) {
    return wind_dir_degr[br[9] & 0x0f];
}

static float get_wind_speed_raw(const uint8_t* br) {
    return br[5]; // Raw
}

static float get_wind_avg_ms(const uint8_t* br) {
    return (br[5] * 34.0f) / 100; // Meters/sec.
}

static float get_wind_avg_mph(const uint8_t* br) {
    return ((br[5] * 34.0f) / 100) * 2.23693629f; // Mph
}

static float get_wind_avg_kmh(const uint8_t* br) {
    return ((br[5] * 34.0f) / 100) * 3.6f; // Km/h
}

static float get_wind_avg_knot(const uint8_t* br) {
    return ((br[5] * 34.0f) / 100) * 1.94384f; // Knots
}

static float get_wind_gust_raw(const uint8_t* br) {
    return br[6]; // Raw
}

static float get_wind_gust_ms(const uint8_t* br) {
    return (br[6] * 34.0f) / 100; // Meters/sec.
}

static float get_wind_gust_mph(const uint8_t* br) {
    return ((br[6] * 34.0f) / 100) * 2.23693629f; // Mph
	
}

static float get_wind_gust_kmh(const uint8_t* br) {
    return ((br[6] * 34.0f) / 100) * 3.6f; // Km/h
}

static float get_wind_gust_knot(const uint8_t* br) {
    return ((br[6] * 34.0f) / 100) * 1.94384f; // Knots
}

static float get_rainfall(const uint8_t* br) {
	return ((((unsigned short)br[7] & 0x0f) << 8) | br[8]) * 0.3f;
}


//----------------- TIME DECODING ----------------------------------------------------

static int get_hours(const uint8_t* br) {
	return ((br[3] >> 4 & 0x03) * 10) + (br[3] & 0x0F);
}

static int get_minutes(const uint8_t* br) {
	return (((br[4] & 0xF0) >> 4) * 10) + (br[4] & 0x0F);
}

static int get_seconds(const uint8_t* br) {
	return (((br[5] & 0xF0) >> 4) * 10) + (br[5] & 0x0F);
}

static int get_year(const uint8_t* br) {
	return (((br[6] & 0xF0) >> 4) * 10) + (br[6] & 0x0F);
}
	
static int get_month(const uint8_t* br) {
	return ((br[7] >> 4 & 0x01) * 10) + (br[7] & 0x0F);	
}

static int get_day(const uint8_t* br) {
	return (((br[8] & 0xF0) >> 4) * 10) + (br[8] & 0x0F);
}

//-------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------

static char temp_date = '0';

static int fineoffset_wh1080_callback(bitbuffer_t *bitbuffer) {
    data_t *data;
    char time_str[LOCAL_TIME_BUFLEN];
    local_time_str(0, time_str);

    if (bitbuffer->num_rows != 1) {
        return 0;
    }
    if (bitbuffer->bits_per_row[0] != 88) {
        return 0;
    }

    const uint8_t *br = bitbuffer->bb[0];

    if (br[0] != 0xff) {
        // preamble missing
        return 0;
    }

    if (br[10] != crc8(br, 10, CRC_POLY, CRC_INIT)) {
        // crc mismatch
        return 0;
    }
	
	if (br[0] == 0xff && br[1] == 0xa0) {
	msg_type = 0;
	} else if (br[0] == 0xff && br[1] == 0xb0) {
	msg_type = 1;
	}
	
	

//---------------------------------------------------------------------------------------	
//-------- GETTING WEATHER SENSORS DATA -------------------------------------------------
	
    const float temperature = get_temperature(br);
    const int humidity = get_humidity(br);
    const char* direction_str = get_wind_direction_str(br);
	const char* direction_deg = get_wind_direction_deg(br);	
	const float pressure = read_press();
	const float int_temp = read_int_temp();
	
	// Select which metric system for *wind avg speed* and *wind gust* :
	
	// Wind average speed :
	
	//const float speed = get_wind_avg_ms((br)   // <--- Data will be shown in Meters/sec.
	//const float speed = get_wind_avg_mph((br)  // <--- Data will be shown in Mph
	const float speed = get_wind_avg_kmh(br);  // <--- Data will be shown in Km/h
	//const float speed = get_wind_avg_knot((br) // <--- Data will be shown in Knots
	
	
	// Wind gust speed :
	
    //const float gust = get_wind_gust_ms(br);   // <--- Data will be shown in Meters/sec.
	//const float gust = get_wind_gust_mph(br);  // <--- Data will be shown in Mph
	const float gust = get_wind_gust_kmh(br);  // <--- Data will be shown in km/h
	//const float gust = get_wind_gust_knot(br); // <--- Data will be shown in Knots	
	
    const float rain = get_rainfall(br);
    const int device_id = get_device_id(br);
	const char* battery = get_battery(br);

//---------------------------------------------------------------------------------------	
//-------- GETTING TIME DATA ------------------------------------------------------------

	const int the_hours = get_hours(br);
	const int the_minutes =	get_minutes(br);
	const int the_seconds = get_seconds(br);
	const int the_year = get_year(br);
	const int the_month = get_month(br);
	const int the_day = get_day(br);
	//printf("test");

//--------- PRESENTING DATA --------------------------------------------------------------
	
if (msg_type == 0) {
	
    data = data_make("time",			"",				DATA_STRING, time_str,
                     "model",			"",				DATA_STRING, "Fine Offset WH1080 weather station",
					 "msg_type",	  "Msg type",		DATA_INT,    msg_type,	
                     "id",            "StationID",		DATA_FORMAT, "%04X",	DATA_INT,    device_id,
                     "temperature_C", "Temperature",	DATA_FORMAT, "%.01f C",	DATA_DOUBLE, temperature,
                     "humidity",      "Humidity",		DATA_FORMAT, "%u %%",	DATA_INT,    humidity,
					 "pressure", 		"Pressure",		DATA_FORMAT, "%.02f hPa",	DATA_DOUBLE, pressure,
                     "direction_str", "Wind string",	DATA_STRING, direction_str,
					 "direction_deg", "Wind degrees",	DATA_STRING, direction_deg,
                     "speed",         "Wind avg speed",		DATA_FORMAT, "%.02f",	DATA_DOUBLE, speed,
                     "gust",          "Wind gust",		DATA_FORMAT, "%.02f",	DATA_DOUBLE, gust,
                     "rain",          "Total rainfall",	DATA_FORMAT, "%.01f",	DATA_DOUBLE, rain,
					 "int_temp",	  "Internal temp.",	DATA_FORMAT, "%.01f C",	DATA_DOUBLE, int_temp,
					 //"battery",	  	  "Battery",		DATA_STRING, battery, // Unsure about Battery byte...
                     NULL);
    data_acquired_handler(data);
    return 1; 
	} else {
		
	data = data_make("time",          "",               DATA_STRING,	time_str,
                     "model",         "",               DATA_STRING,	"Fine Offset WH1080 weather station",
					 "msg_type",	  "Msg type",		DATA_INT,		msg_type,	
                     "id",            "StationID",      DATA_FORMAT,	"%04X",	DATA_INT,	device_id,
                     "the_hours",		"Hours",			DATA_FORMAT,	"%02d",	DATA_INT,	the_hours,
                     "the_minutes",		"Minutes",       	DATA_FORMAT,	"%02d",	DATA_INT,	the_minutes,
                     "the_seconds",		"Seconds", 		DATA_FORMAT,	"%02d",	DATA_INT,	the_seconds,
					 "the_year",		"Year", 			DATA_FORMAT,	"20%02d",	DATA_INT,	the_year,
                     "the_month",		"Month",     		DATA_FORMAT,	"%02d",	DATA_INT,	the_month,
                     "the_day",			"Day",      		DATA_FORMAT,	"%02d",	DATA_INT,	the_day,
                     NULL);
    data_acquired_handler(data);
    return 1; 
	}	
}

static char *output_fields[] = {
	"time",
	"model",
	"id",
	"temperature_C",
	"humidity",
	"pressure",
	"direction_str",
	"direction_deg",
	"speed",
	"gust",
	"rain",
	"msg_type",
	"the_hours",
	"the_minutes",
	"the_seconds",
	"the_year",
	"the_month",
	"the_day",
	"int_temp",
	//"battery", // Unsure about Battery byte...
	NULL
};

r_device fineoffset_wh1080 = {
    .name           = "Fine Offset WH1080 Weather Station",
    .modulation     = OOK_PULSE_PWM_RAW,
    .short_limit    = 976,
    .long_limit     = 2400,
    .reset_limit    = 10520,
    .json_callback  = &fineoffset_wh1080_callback,
    .disabled       = 0,
    .demod_arg      = 0,
    .fields         = output_fields,
};
