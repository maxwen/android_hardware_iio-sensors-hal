/*
 * Copyright (C) 2014 Intel Corporation.
 */

#include <stdlib.h>
#include <math.h>
#include <utils/Log.h>
#include <cutils/properties.h>
#include <hardware/sensors.h>
#include "common.h"
#include "transform.h"


static int64_t sample_as_int64(unsigned char* sample, struct datum_info_t* type)
{
	uint16_t u16;
	uint32_t u32;
	uint64_t u64;
	int i;

	switch (type->storagebits) {
		case 64:
			u64 = 0;

			if (type->endianness == 'b')
				for (i=0; i<8; i++)
					u64 = (u64 << 8) | sample[i];
			else
				for (i=7; i>=0; i--)
					u64 = (u64 << 8) | sample[i];

			if (type->sign == 'u')
				return (int64_t) (u64 >> type->shift);

			return ((int64_t) u64) >> type->shift;

		case 32:
			if (type->endianness == 'b')
				u32 = (sample[0] << 24) | (sample[1] << 16) |
					(sample[2] << 8) | sample[3];
			else
				u32 = (sample[3] << 24) | (sample[2] << 16) |
					(sample[1] << 8) | sample[0];

			if (type->sign == 'u')
				return u32 >> type->shift;

			return ((int32_t) u32) >> type->shift;

		case 16:
			if (type->endianness == 'b')
				u16 = (sample[0] << 8) | sample[1];
			else
				u16 = (sample[1] << 8) | sample[0];

			if (type->sign == 'u')
				return u16 >> type->shift;

			return  ((int16_t) u16) >> type->shift;
	}

	ALOGE("Unhandled sample storage size\n");
	return 0;
}


static void finalize_sample_default(int s, struct sensors_event_t* data)
{
	int i		= sensor_info[s].catalog_index;
	int sensor_type	= sensor_catalog[i].type;

	switch (sensor_type) {
		case SENSOR_TYPE_ACCELEROMETER:
			/*
			 * Invert x axis orientation from SI units - see
			 * /hardware/libhardware/include/hardware/sensors.h
			 * for a discussion of what Android expects
			 */
			data->data[0] = -data->data[0];
			break;

		case SENSOR_TYPE_GYROSCOPE:
			/* Limit drift */
			if (abs(data->data[0]) < .05 && abs(data->data[1]) < .05
				&& abs(data->data[2]) < .05) {
					data->data[0] = 0;
					data->data[1] = 0;
					data->data[2] = 0;
				}
			break;
	}
}


static float transform_sample_default(int s, int c, unsigned char* sample_data)
{
	struct datum_info_t* sample_type = &sensor_info[s].channel[c].type_info;
	int64_t		     s64 = sample_as_int64(sample_data, sample_type);

	/* Apply default scaling rules */
	return (sensor_info[s].offset + s64) * sensor_info[s].scale;
}


static void finalize_sample_ISH(int s, struct sensors_event_t* data)
{
}


static float transform_sample_ISH(int s, int c, unsigned char* sample_data)
{
	return 0;
}


void select_transform (int s)
{
	char prop_name[PROP_NAME_MAX];
	char prop_val[PROP_VALUE_MAX];
	int i			= sensor_info[s].catalog_index;
	const char *prefix	= sensor_catalog[i].tag;

	sprintf(prop_name, PROP_BASE, prefix, "transform");

	if (property_get(prop_name, prop_val, "")) {
		if (!strcmp(prop_val, "ISH")) {
			ALOGI(	"Using Intel Sensor Hub semantics on %s\n",
				sensor_info[s].friendly_name);

			sensor_info[s].ops.transform = transform_sample_ISH;
			sensor_info[s].ops.finalize = finalize_sample_ISH;
			return;
		}
	}

	sensor_info[s].ops.transform = transform_sample_default;
	sensor_info[s].ops.finalize = finalize_sample_default;
}
