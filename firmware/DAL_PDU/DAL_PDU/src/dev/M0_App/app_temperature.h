#ifndef APP_TEMPERATURE_H_
#define APP_TEMPERATURE_H_

/* Initializes the temperature ADC and publishes one reading. A failed sensor
 * leaves the reading invalid and does not stop startup. */
void AppTemperature_Init(void);

/* Samples the board temperature into SystemData. Scheduled every 1000 ms. */
void AppTemperature_Update(void);

#endif /* APP_TEMPERATURE_H_ */
