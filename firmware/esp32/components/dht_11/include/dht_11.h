#ifndef DHT_11_H
#define DHT_11_H

typedef struct {
    float humidity;
    float temperature;
} reading;

reading dht_11_read(reading);

#endif