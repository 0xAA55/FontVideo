#include"siowrap.h"

#include<stdlib.h>
#include<string.h>
#include<errno.h>

static void on_devices_change(struct SoundIo *soundio);
static void close_device(siowrap_p s)
{
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
	free(s->sample_pointer_steps);
	s->sample_pointer_steps = NULL;
	s->channel_count = 0;
}

static void write_callback(struct SoundIoOutStream *outstream, int frame_count_min, int frame_count_max)
{
	const struct SoundIoChannelLayout *layout = &outstream->layout;
	struct SoundIoChannelArea *areas;
	siowrap_p s = outstream->userdata;
	FILE *log_fp = s->log_fp;
	int i, j;
	int err;

	frame_count_min;
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

		frame_count = (int)s->write_cb(s, outstream->sample_rate, s->channel_count, s->pointers_of_channels, s->sample_pointer_steps, frame_count);

		err = soundio_outstream_end_write(outstream);
		if (err)
		{
			fprintf(log_fp, "libsoundio: soundio_outstream_end_write() failed: %s\n", soundio_strerror(err));
			goto FailExit;
		}

		if (!frame_count) break;
		i += frame_count;
	}
	return;
FailExit:
	close_device(s);
	return;
}

static void on_devices_change(struct SoundIo *soundio)
{
	siowrap_p s = soundio->userdata;
	FILE *log_fp = s->log_fp;
	int err;

	close_device(s);

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
	s->outstream->sample_rate = s->sample_rate;
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
	close_device(s);
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
	s->format = format;
	s->sample_rate = sample_rate;
	s->write_cb = write_cb;

	s->soundio = soundio_create();
	if (!s->soundio)
	{
		fprintf(log_fp, "libsoundio: soundio_create() failed: '%s'\n", strerror(errno));
		goto FailExit;
	}
	s->soundio->userdata = s;
	s->soundio->on_devices_change = on_devices_change;

	err = soundio_connect(s->soundio);
	if (err)
	{
		fprintf(log_fp, "libsoundio: error connecting: %s\n", soundio_strerror(err));
		goto FailExit;
	}

	soundio_flush_events(s->soundio);

	if (!s->device || !s->outstream) goto FailExit;

	return s;
FailExit:
	siowrap_destroy(s);
	return NULL;
}

void siowrap_wait_events(siowrap_p s)
{
	if (!s) return;

	soundio_wait_events(s->soundio);
	if (!s->device || !s->outstream)
	{
		on_devices_change(s->soundio);
	}
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

#ifdef _WIN32
#include<Windows.h>

// And some GUID are never implemented (Ignoring the INITGUID define)
// extern const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
extern const CLSID   CLSID_MMDeviceEnumerator = {
	//MIDL_INTERFACE("BCDE0395-E52F-467C-8E3D-C4579291692E")
	0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}
};
extern const IID   IID_IMMDeviceEnumerator = {
	//MIDL_INTERFACE("A95664D2-9614-4F35-A746-DE8DB63617E6")
	0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
};
extern const IID   IID_IMMNotificationClient = {
	//MIDL_INTERFACE("7991EEC9-7E89-4D85-8390-6C703CEC60C0")
	0x7991eec9, 0x7e89, 0x4d85, {0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0}
};
extern const IID   IID_IAudioClient = {
	//MIDL_INTERFACE("1CB9AD4C-DBFA-4c32-B178-C2F568A703B2")
	0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}
};
extern const IID   IID_IAudioRenderClient = {
	//MIDL_INTERFACE("F294ACFC-3146-4483-A7BF-ADDCA7C260E2")
	0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}
};
extern const IID   IID_IAudioSessionControl = {
	//MIDL_INTERFACE("F4B1A599-7266-4319-A8CA-E70ACB11E8CD")
	0xf4b1a599, 0x7266, 0x4319, {0xa8, 0xca, 0xe7, 0x0a, 0xcb, 0x11, 0xe8, 0xcd}
};
extern const IID   IID_IAudioSessionEvents = {
	//MIDL_INTERFACE("24918ACC-64B3-37C1-8CA9-74A66E9957A8")
	0x24918acc, 0x64b3, 0x37c1, {0x8c, 0xa9, 0x74, 0xa6, 0x6e, 0x99, 0x57, 0xa8}
};
extern const IID IID_IMMEndpoint = {
	//MIDL_INTERFACE("1BE09788-6894-4089-8586-9A2A6C265AC5")
	0x1be09788, 0x6894, 0x4089, {0x85, 0x86, 0x9a, 0x2a, 0x6c, 0x26, 0x5a, 0xc5}
};
extern const IID IID_IAudioClockAdjustment = {
	//MIDL_INTERFACE("f6e4c0a0-46d9-4fb8-be21-57a3ef2b626c")
	0xf6e4c0a0, 0x46d9, 0x4fb8, {0xbe, 0x21, 0x57, 0xa3, 0xef, 0x2b, 0x62, 0x6c}
};
extern const IID IID_IAudioCaptureClient = {
	//MIDL_INTERFACE("C8ADBD64-E71E-48a0-A4DE-185C395CD317")
	0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}
};
extern const IID IID_ISimpleAudioVolume = {
	//MIDL_INTERFACE("87ce5498-68d6-44e5-9215-6da47ef883d8")
	0x87ce5498, 0x68d6, 0x44e5,{ 0x92, 0x15, 0x6d, 0xa4, 0x7e, 0xf8, 0x83, 0xd8 }
};
#endif // _WIN32
