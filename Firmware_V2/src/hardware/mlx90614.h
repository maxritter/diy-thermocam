/*
*
* MLX90614 - Access the MLX90614 spot IR sensor
*
* DIY-Thermocam Firmware
*
* GNU General Public License v3.0
*
* Copyright by Max Ritter
*
* http://www.diy-thermocam.net
* https://github.com/maxritter/DIY-Thermocam
*
*/

#ifndef MLX90614_H
#define MLX90614_H

/*########################## PUBLIC PROCEDURES ################################*/

boolean mlx90614_checkEmissivity();
boolean mlx90614_checkFilter();
boolean mlx90614_checkMax();
boolean mlx90614_checkMin();
char mlx90614_crc8(byte buffer[], int len);
float mlx90614_getAmb();
uint16_t mlx90614_getRawData(boolean TaTo, boolean* check);
float mlx90614_getTemp();
void mlx90614_init();
float mlx90614_measure(boolean TaTo, boolean* check);
uint16_t mlx90614_receive(byte address, byte* error = NULL);
void mlx90614_send(byte address, byte LSB, byte MSB);
void mlx90614_setEmissivity();
void mlx90614_setFilter();
void mlx90614_setMax();
void mlx90614_setMin();

#endif /* MLX90614_H */
