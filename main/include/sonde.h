#ifndef SONDE_H
#define SONDE_H

#include <time.h>

typedef struct sonde_data {
	char model[16];  /* Sonde model, TTSonde internal variable */

	int seq;         /* Sequence number */
	char serial[32]; /* Serial number */

	float lat;       /* Latitude, degrees */
	float lon;       /* Longitude, degrees */
	float alt;       /* Altitude, m */
	float speed;     /* Horizontal speed, m/s */
	float climb;     /* Climb rate, m/s */
	float heading;   /* Heading, degrees */

	time_t time;     /* GPS time, seconds since Jan 1 1970 */

	float temp;      /* Temperature, 'C */
	float rh_temp;   /* Relavite humidity temperature, 'C */
	float rh;        /* Relavite humidity, % */
	float pressure;  /* Pressure, hPa */

	float vbat;      /* Battery voltage, V */
} sonde_data_t;

typedef enum sonde_model {
	SONDE_M20 = 0,
	_SONDE_COUNT
} sonde_model_t;

typedef struct {
	char name[16];

	// ret = 0 on success, negative otherwise
	int (*setup)(float frequency);

	/* Return value
	 *  -1 Invalid packet (sonde is not modified) (i.e. invalid CRC)
	 *   0 No      packet (sonde is not modified)
	 *   1 Valid   packet (sonde is modified)
	 */
	int (*receive)(sonde_data_t *sonde);
} sonde_t;

#endif