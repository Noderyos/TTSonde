#ifndef GPS_H
#define GPS_H
#include <time.h>

struct gps_data_t {
    float lat;      /* Latitude, deg */
    float lon;      /* Longitude, deg */
    float alt;      /* Altitude, m */
    float dop;      /* Dilution of precision */

    float speed;    /* Speed, m/s */
    float heading;  /* Heading, deg */
    float magvar;   /* Magnetic variation, deg */

    float sep;      /* Geoidal separation, m */

    time_t time;    /* Timestamp, s */
};

void gps_init(void);

#endif
