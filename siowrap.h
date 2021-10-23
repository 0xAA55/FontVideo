#ifndef _SIOWRAP_H_
#define _SIOWRAP_H_ 1

#include<soundio/soundio.h>

#include<stdio.h>

typedef struct siowrap_struct siowrap_t, *siowrap_p;

typedef void (*on_write_sample)(siowrap_p s, int sample_rate, int channel_count, void **pointers_per_channel, size_t *sample_pointer_steps, size_t samples_to_write_per_channel);

struct siowrap_struct
{
	FILE *log_fp;
	int default_out_device_index;
	enum SoundIoFormat format;
	int sample_rate;
	on_write_sample write_cb;
	int channel_count;
	void **pointers_of_channels;
	size_t *sample_pointer_steps; // in bytes

	struct SoundIo *soundio;
	struct SoundIoDevice *device;
	struct SoundIoOutStream *outstream;
};

siowrap_p siowrap_create(FILE *log_fp, enum SoundIoFormat format, int sample_rate, on_write_sample write_cb);
void siowrap_wait_events(siowrap_p s);
void siowrap_destroy(siowrap_p s);

#endif
