#ifndef SENSOR_BASE_H
#define SENSOR_BASE_H
typedef enum
{
    SENSOR_OK = 0,
    SENSOR_ERROR = -1,
    SENSOR_NOT_FOUND = -2,
    SENSOR_TIMEOUT = -3
} SensorStatus_t;

struct sensor_base
{
    SensorStatus_t (*read_data)(struct sensor_base *self, void *data);
    SensorStatus_t (*deinit)(struct sensor_base *self);
    void *context;
};

typedef struct sensor_base sensor_base_t;
#endif // SENSOR_BASE_H