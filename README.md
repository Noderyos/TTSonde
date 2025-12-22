# TT-Sonde

## Installation

### Requirements

* [ESP-IDF](https://github.com/espressif/esp-idf)

### Build and Flash

1. Configure the project:

   ```bash
   idf.py menuconfig
   ```

2. Build the firmware:

   ```bash
   idf.py build
   ```

3. Flash the firmware to the device:

   ```bash
   idf.py flash
   ```

## Supported Sondes

The following radiosonde models are currently supported:

* **M20**

## Configuration

### User Interface

All user interfaces are configurable via JSON. Configuration can be applied:

* At flash time
* Later, through the TT-Sonde WebUI

The table below lists all supported fields, their types, default formatting, and descriptions.
If you are unfamiliar with printf-style format string, check below.

| Field Name  | Type  | Default Format      | Description                                    |
|-------------|-------|---------------------|------------------------------------------------|
| text        | char* |                     | Plain text (expected in `value` key)           |
| lat         | float | `%.5fN`             | Sonde latitude                                 |
| lon         | float | `%.5fE`             | Sonde longitude                                |
| alt         | float | `%.1fm`             | Sonde altitude                                 |
| climb       | float | `%.1fm/s`           | Sonde vertical speed                           |
| speed       | float | `%.1fm/s`           | Sonde horizontal speed                         |
| heading     | float | `%d'`               | Sonde heading in degrees                       |
| temp        | float | `%.2f'C`            | Sonde temperature                              |
| rh_temp     | float | `%.2f'C`            | Relative humidity sensor temperature           |
| rh          | float | `%.1f%%`            | Sonde relative humidity                        |
| pressure    | float | `%.1fhPa`           | Atmospheric pressure                           |
| vbat        | float | `%.2fV`             | Sonde battery voltage                          |
| seq         | int   |                     | Sonde sequence number                          |
| serial      | char* |                     | Sonde serial number                            |
| time        | long  | `%Y-%m-%d %H:%M:%S` | Sonde timestamp (formatted with `strftime`)    |
| model       | char* |                     | Sonde model (set internally)                   |
| dev_ssid    | char* |                     | TT-Sonde Wi-Fi SSID                            |
| dev_ip      | char* |                     | TT-Sonde Wi-Fi IP address                      |
| dev_vbat    | float | `%.2f`              | TT-Sonde battery voltage                       |
| dev_time    | long  | `%Y-%m-%d %H:%M:%S` | TT-Sonde timestamp (formatted with `strftime`) |
| dev_lat     | float | `%.5fN`             | TT-Sonde GPS latitude                          |
| dev_lon     | float | `%.5fE`             | TT-Sonde GPS longitude                         |
| dev_alt     | float | `%.1fm`             | TT-Sonde GPS altitude                          |
| dev_speed   | float | `%.1fm/s`           | TT-Sonde GPS horizontal speed                  |
| dev_heading | float | `%d'`               | TT-Sonde GPS heading in degrees                |
| dev_dop     | float | `%.1f`              | TT-Sonde GPS dilution of precision             |
| dev_magvar  | float | `%.1f`              | TT-Sonde GPS magnetic declination              |
| dev_sep     | float | `%.1f`              | TT-Sonde GPS geoidal separation                |

### Value Formatting

Values are rendered using **printf-style format specifiers**, following standard C formatting conventions.
- `%d` Integer
- `%s` String
- `%f` Floating-point value
- `%.2f` Floating-point value with 2 decimal places
- `%%` Literal percent sign

> For example `%s car can go over %.2fkm/h` 
> with `F1` and `3.1415` parameters 
> will show `F1 car can go over 3.14km/h`

I recommend you reading [this article](https://cplusplus.com/reference/cstdio/printf/) for further explanations.
