#include"siowrap.h"

#include<stdlib.h>
#include<string.h>
#include<errno.h>

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max)
{
	const struct SoundIoChannelLayout *layout = &outstream->layout;
	struct SoundIoChannelArea *areas;
	siowrap_p s = outstream->userdata;
	FILE *log_fp = s->log_fp;
	int i, j;
	int err;

	if (s->channel_count != layout->channel_count || !s->pointers_of_channels || !s->sample_pointer_steps)
	{
		s->channel_count = 0;
		free(s->pointers_of_channels);
		free(s->sample_pointer_steps);
		s->sample_pointer_steps = NULL;
		s->pointers_of_channels = calloc(layout->channel_count, sizeof s->pointers_of_channels[0]);
		if (!s->pointers_of_channels)
		{
			fprintf(log_fp, "libsoundio: unable to allocate channel data pointers: %s\n", strerror(errno));
			goto FailExit;
		}
		s->sample_pointer_steps = calloc(layout->channel_count, sizeof s->sample_pointer_steps[0]);
		if (!s->sample_pointer_steps)
		{
			free(s->pointers_of_channels);
			s->pointers_of_channels = NULL;
			fprintf(log_fp, "libsoundio: unable to allocate channel data pointers: %s\n", strerror(errno));
			goto FailExit;
		}
		s->channel_count = layout->channel_count;
	}

	for (i = 0; i < frame_count_max; i++)
	{
		int frame_count = frame_count_max - i;
		err = soundio_outstream_begin_write(outstream, &areas, &frame_count);
		if (err)
		{
			fprintf(log_fp, "libsoundio: soundio_outstream_begin_write() failed: %s\n", soundio_strerror(err));
			goto FailExit;
		}

		if (!frame_count) break;

		for (j = 0; j < s->channel_count; j++)
		{
			s->pointers_of_channels[j] = areas[j].ptr;
			s->sample_pointer_steps[j] = areas[j].step;
		}

		s->write_cb(s, outstream->sample_rate, s->channel_count, s->pointers_of_channels, s->sample_pointer_steps, frame_count);

		err = soundio_outstream_end_write(outstream);
		if (err)
		{
			fprintf(log_fp, "libsoundio: soundio_outstream_end_write() failed: %s\n", soundio_strerror(err));
			goto FailExit;
		}
	}
	return;
FailExit:
	return;
}

static void on_devices_change(struct SoundIo *soundio)
{
	siowrap_p s = soundio->userdata;
	FILE *log_fp = s->log_fp;
	int err;

	s->default_out_device_index = soundio_default_output_device_index(s->soundio);
	if (s->default_out_device_index < 0)
	{
		fprintf(log_fp, "libsoundio: no output device found.\n");
		goto FailExit;
	}

	s->device = soundio_get_output_device(s->soundio, s->default_out_device_index);
	if (!s->device)
	{
		fprintf(log_fp, "libsoundio: soundio_get_output_device() failed: '%s'.\n", strerror(errno));
		goto FailExit;
	}

	fprintf(log_fp, "libsoundio: output device: %s\n", s->device->name);

	s->outstream = soundio_outstream_create(s->device);
	if (!s->outstream)
	{
		fprintf(log_fp, "libsoundio: soundio_outstream_create() failed: '%s'.\n", strerror(errno));
		goto FailExit;
	}

	s->outstream->userdata = s;
	s->outstream->format = s->format;
	s->outstream->write_callback = write_callback;
	err = soundio_outstream_open(s->outstream);
	if (err)
	{
		fprintf(log_fp, "libsoundio: unable to open devive: %s\n", soundio_strerror(err));
		goto FailExit;
	}

	if (s->outstream->layout_error)
	{
		fprintf(log_fp, "libsoundio: unable to set channel layout: %s\n", soundio_strerror(s->outstream->layout_error));
		goto FailExit;
	}

	s->channel_count = s->outstream->layout.channel_count;
	s->pointers_of_channels = calloc(s->channel_count, sizeof s->pointers_of_channels[0]);
	s->sample_pointer_steps = calloc(s->channel_count, sizeof s->sample_pointer_steps[0]);

	err = soundio_outstream_start(s->outstream);
	if (err)
	{
		fprintf(log_fp, "libsoundio: unable to start devive: %s\n", soundio_strerror(err));
		goto FailExit;
	}

	return;
FailExit:
	if (s->outstream)
	{
		soundio_outstream_destroy(s->outstream);
		s->outstream = NULL;
	}
	if (s->device)
	{
		soundio_device_unref(s->device);
		s->device = NULL;
	}
	free(s->pointers_of_channels);
	s->pointers_of_channels = NULL;
	return;
}

siowrap_p siowrap_create(FILE *log_fp, enum SoundIoFormat format, int sample_rate, on_write_sample write_cb)
{
	siowrap_p s = NULL;
	int err;
	
	if (!log_fp) log_fp = stderr;
	
	s = calloc(1, sizeof s[0]);
	if (!s) return s;

	s->log_fp = log_fp;

	s->soundio = soundio_create();
	if (!s->soundio)
	{
		fprintf(log_fp, "libsoundio: soundio_create() failed: '%s'\n", strerror(errno));
		goto FailExit;
	}
	s->soundio->userdata = s;

	err = soundio_connect(s->soundio);
	if (err)
	{
		fprintf(log_fp, "libsoundio: error connecting: %s\n", soundio_strerror(err));
		goto FailExit;
	}

	soundio_flush_events(s->soundio);

	s->format = format;
	s->sample_rate = sample_rate;
	s->write_cb = write_cb;
	on_devices_change(s->soundio);

	if (!s->device || !s->outstream) goto FailExit;

	return s;
FailExit:
	siowrap_destroy(s);
	return NULL;
}

void siowrap_wait_events(siowrap_p s)
{
	soundio_wait_events(s->soundio);
}

void siowrap_destroy(siowrap_p s)
{
	if (!s) return;

	if (s->outstream) soundio_outstream_destroy(s->outstream);
	if (s->device) soundio_device_unref(s->device);
	if (s->soundio) soundio_destroy(s->soundio);
	free(s->pointers_of_channels);

	free(s);
}

