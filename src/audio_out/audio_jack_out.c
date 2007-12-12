
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>

#include "xine_internal.h"
#include "xineutils.h"
#include "audio_out.h"

#include <jack/jack.h>

#define AO_OUT_JACK_IFACE_VERSION 9

#define GAP_TOLERANCE        AO_MAX_GAP
#define BUFSIZE              81920

typedef struct jack_driver_s {

    ao_driver_t    ao_driver;
    xine_t        *xine;

    int            capabilities;

    int32_t        sample_rate;
    uint32_t       num_channels;
    uint32_t       bits_per_sample;
    uint32_t       bytes_per_frame;

    jack_client_t *client;
    jack_port_t   *port_1;
    jack_port_t   *port_2;
    float          buf_1[BUFSIZE];
    float          buf_2[BUFSIZE];
    uint32_t       buf_read;
    uint32_t       buf_write;

    uint32_t       volume;
    uint32_t       mute;

} jack_driver_t;

typedef struct {
    audio_driver_class_t  driver_class;
    config_values_t      *config;
    xine_t               *xine;
} jack_class_t;


static int jack_process(jack_nframes_t nframes, void *arg)
{
    jack_driver_t *this = (jack_driver_t *)arg;
    uint32_t local_buf_read = this->buf_read;
    uint32_t local_buf_write = this->buf_write;
    uint32_t src_channel, target_channel;
    uint32_t frame;
    float *buf, *out;
    float gain = 0;

    if (!this->client) return 0;

    if (!this->mute) {
	gain = (float)this->volume / 100.0;
    }

    for (target_channel = 0; target_channel < 2; ++target_channel) {
	    
	if (target_channel < this->num_channels) src_channel = target_channel;
	else src_channel = 0;

        jack_port_t *port = (target_channel ? this->port_2 : this->port_1);
        if (!port) continue;

        buf = (src_channel ? this->buf_2 : this->buf_1);
        out = (float *)jack_port_get_buffer(port, nframes);

        local_buf_read = this->buf_read;
        frame = 0;

        while (frame < nframes && local_buf_read != local_buf_write) {

            // local_buf_write doesn't change during this process,
            // so we can safely defer updating buf_read until after

            out[frame++] = buf[local_buf_read] * gain;
            if (++local_buf_read == BUFSIZE) local_buf_read = 0;
        }

	if (frame < nframes) {
//	    printf("jack_process: underrun: %u required, %u available\n",
//		   nframes, frame);
	    while (frame < nframes) {
		out[frame++] = 0.0f;
	    }
	}
    }

    this->buf_read = local_buf_read;


//    printf("jack_process: buf_read %u, buf_write %u\n", this->buf_read, this->buf_write);

    return 0;
}
    

static void jack_shutdown(void *arg)
{
    jack_driver_t *this = (jack_driver_t *)arg;
    this->client = 0;
}

/*
 * open the audio device for writing to
 */
static int ao_jack_open(ao_driver_t *this_gen, uint32_t bits, uint32_t rate, int mode)
{
    jack_driver_t *this = (jack_driver_t *) this_gen;

    if (bits != 16) {
	fprintf(stderr, "ao_jack_open: bits=%u expected %u\n", bits, 16);
	return 0;
    }

    rate = jack_get_sample_rate(this->client);
    fprintf(stderr, "ao_jack_open: JACK sample rate is %u\n", rate);

    switch (mode) {
    case AO_CAP_MODE_MONO:
	this->num_channels = 1;
	break;
    case AO_CAP_MODE_STEREO:
	this->num_channels = 2;
	break;
    }

    this->buf_read = this->buf_write = 0;
    this->sample_rate = rate;
    this->bits_per_sample = bits;
    this->capabilities = AO_CAP_16BITS | AO_CAP_MODE_MONO | \
	AO_CAP_MODE_STEREO | AO_CAP_MIXER_VOL | AO_CAP_MUTE_VOL;
    this->bytes_per_frame = this->num_channels * (bits / 8);

    fprintf(stderr, "ao_jack_open: bits=%d rate=%d, mode=%d OK\n", bits, rate, mode);

    return rate;
}


static int ao_jack_num_channels(ao_driver_t *this_gen)
{
    jack_driver_t *this = (jack_driver_t *) this_gen;
    return this->num_channels;
}

static int ao_jack_bytes_per_frame(ao_driver_t *this_gen)
{
    jack_driver_t *this = (jack_driver_t *) this_gen;
    return this->bytes_per_frame;
}

static int ao_jack_get_gap_tolerance (ao_driver_t *this_gen)
{
    return GAP_TOLERANCE;
}

static int last_write_space = 0;

static int ao_jack_write(ao_driver_t *this_gen, int16_t *data,
                         uint32_t num_frames)
{
    jack_driver_t *this = (jack_driver_t *) this_gen;
    uint32_t frame, channel;

    uint32_t local_buf_read = this->buf_read;
    uint32_t local_buf_write = this->buf_write;
    uint32_t space = (local_buf_read + BUFSIZE - local_buf_write - 1) % BUFSIZE;    
    uint32_t first_frame = 0;

    int c = 0;
    while (space < num_frames) {
	if (++c == 10) return 0;
	usleep(10000);
	local_buf_read = this->buf_read;
 	space = (local_buf_read + BUFSIZE - local_buf_write - 1) % BUFSIZE; 
    }
	
//    if (space < num_frames) return 0;

//    printf("ao_jack_write: %u frames on %u channels, space is %u\n", num_frames, this->num_channels, space);
    last_write_space = space;

    for (frame = first_frame; frame < num_frames; ++frame) {
	for (channel = 0; channel < this->num_channels; ++channel) {
	    float *buf = (channel ? this->buf_2 : this->buf_1);
	    int16_t sample = data[frame * this->num_channels + channel];
	    buf[local_buf_write] = ((float)sample) / 32767.0f;
//	    printf("%6f ", buf[local_buf_write]);
//	    if (++c == 8) { printf("\n"); c = 0; }
	}
	if (++local_buf_write == BUFSIZE) local_buf_write = 0;
    }

    this->buf_write = local_buf_write;

//    printf("ao_jack_write: buf_read %u, buf_write %u\n", this->buf_read, this->buf_write);

    return 1;
}

static int ao_jack_delay (ao_driver_t *this_gen)
{
    jack_driver_t *this = (jack_driver_t *) this_gen;

    uint32_t local_buf_read = this->buf_read;
    uint32_t local_buf_write = this->buf_write;

    int delay = 0;

    if (local_buf_write > local_buf_read) {
        delay = local_buf_write - local_buf_read;
    } else {
        delay = ((local_buf_write + BUFSIZE - local_buf_read) % BUFSIZE);
    }

    return delay;// - jack_get_buffer_size(this->client);
}

static void ao_jack_close(ao_driver_t *this_gen)
{
    // nothing
}

static uint32_t ao_jack_get_capabilities (ao_driver_t *this_gen) {
    jack_driver_t *this = (jack_driver_t *) this_gen;
    return this->capabilities;
}

static void ao_jack_exit(ao_driver_t *this_gen)
{
    jack_driver_t *this = (jack_driver_t *) this_gen;
    jack_client_t *client = this->client;
    ao_jack_close(this_gen);
    this->client = 0;
    if (client) jack_client_close(client);
    free (this);
}

static int ao_jack_get_property (ao_driver_t *this_gen, int property) {
    jack_driver_t *this = (jack_driver_t *) this_gen;

    switch(property) {
    case AO_PROP_PCM_VOL:
    case AO_PROP_MIXER_VOL:
//	printf("ao_jack_get_property(AO_PROP_MIXER_VOL): %u\n", this->volume);
	return this->volume;
	break;
    case AO_PROP_MUTE_VOL:
//	printf("ao_jack_get_property(AO_PROP_MUTE_VOL): %u\n", this->mute);
	return this->mute;
	break;
    }

    return 0;
}

static int ao_jack_set_property (ao_driver_t *this_gen, int property, int value) {
    jack_driver_t *this = (jack_driver_t *) this_gen;

    switch(property) {
    case AO_PROP_PCM_VOL:
    case AO_PROP_MIXER_VOL:
//	printf("ao_jack_set_property(AO_PROP_MIXER_VOL): %u\n", value);
	this->volume = value;
	break;
    case AO_PROP_MUTE_VOL:
//	printf("ao_jack_get_property(AO_PROP_MUTE_VOL): %u\n", value);
	this->mute = value;
	break;
    }

    return ~value;
}

static int ao_jack_ctrl(ao_driver_t *this_gen, int cmd, ...) {
    jack_driver_t *this = (jack_driver_t *) this_gen;

    switch (cmd) {

    case AO_CTRL_PLAY_PAUSE:
	break;

    case AO_CTRL_PLAY_RESUME:
	break;

    case AO_CTRL_FLUSH_BUFFERS:
//	fprintf(stderr, "ao_jack_ctrl(AO_CTRL_FLUSH_BUFFERS)\n");
	this->buf_write = this->buf_read = 0;
	break;
    }

    return 0;
}

static ao_driver_t *open_jack_plugin (audio_driver_class_t *class_gen,
				      const void *data)
{
    jack_class_t  *class = (jack_class_t *) class_gen;
    jack_driver_t *this;
    jack_client_t *client;
    uint32_t rate;
    const char **port_names;
    int i;

    if ((client = jack_client_new("xine")) == 0) {

	char name[20];
	sprintf(name, "xine (%d)", (int)getpid());

	if ((client = jack_client_new(name)) == 0) {
	    fprintf(stderr, "\nopen_jack_plugin: Error: Failed to connect to JACK server\n");
	    fprintf(stderr, "open_jack_plugin: (did you start 'jackd' server?)\n");
	    return 0;
	}
    }

    this = (jack_driver_t *) xine_xmalloc (sizeof (jack_driver_t));
    
    this->client = client;

    jack_set_process_callback(client, jack_process, this);
    jack_on_shutdown(client, jack_shutdown, this);

    rate = jack_get_sample_rate(client);
    fprintf(stderr, "open_jack_plugin: JACK sample rate is %u\n", rate);

    // We support up to 2-channel output

    for (i = 0; i < 2; ++i) {
	jack_port_t *port = jack_port_register
	    (client, (i ? "out_r" : "out_l"),
	     JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	if (!port) {
	    fprintf(stderr, "ao_jack_open: failed to register port %u\n", i);
	}
	if (i) this->port_2 = port;
	else   this->port_1 = port;
    }

    this->buf_read = this->buf_write = 0;
    this->volume = 100;
    this->mute = 0;

    if (jack_activate(client)) {
	fprintf(stderr, "ao_jack_open: failed to activate JACK client\n");
	return 0;
    }

    if ((port_names = jack_get_ports(client, NULL, NULL,
				     JackPortIsPhysical | JackPortIsInput)) != NULL) {
	if (port_names[0]) {
	    jack_connect(client, jack_port_name(this->port_1), port_names[0]);
	}
	if (port_names[1] && this->port_2) {
	    jack_connect(client, jack_port_name(this->port_2), port_names[1]);
	}
	free(port_names);
    }

    this->sample_rate = rate;

    this->xine = class->xine;
    this->capabilities = AO_CAP_FLOAT32 | AO_CAP_MODE_MONO | 
	AO_CAP_MODE_STEREO | AO_CAP_MIXER_VOL | AO_CAP_MUTE_VOL;

    this->ao_driver.get_capabilities    = ao_jack_get_capabilities;
    this->ao_driver.get_property        = ao_jack_get_property;
    this->ao_driver.set_property        = ao_jack_set_property;
    this->ao_driver.open                = ao_jack_open;
    this->ao_driver.num_channels        = ao_jack_num_channels;
    this->ao_driver.bytes_per_frame     = ao_jack_bytes_per_frame;
    this->ao_driver.delay               = ao_jack_delay;
    this->ao_driver.write               = ao_jack_write;
    this->ao_driver.close               = ao_jack_close;
    this->ao_driver.exit                = ao_jack_exit;
    this->ao_driver.get_gap_tolerance   = ao_jack_get_gap_tolerance;
    this->ao_driver.control             = ao_jack_ctrl;

    fprintf(stderr, "jack open_jack_plugin returning %p\n", (void *)(&this->ao_driver));
    return &this->ao_driver;
}

/*
 * class functions
 */
static void *init_class (xine_t *xine, void *data) {

    jack_class_t        *this;

    this = (jack_class_t *) xine_xmalloc (sizeof (jack_class_t));

    this->driver_class.open_plugin     = open_jack_plugin;
    this->driver_class.identifier      = "jack";
    this->driver_class.description     = N_("xine output plugin for JACK Audio Connection Kit");
    this->driver_class.dispose         = default_audio_driver_class_dispose;

    this->config = xine->config;
    this->xine   = xine;

    fprintf(stderr, "jack init_class returning %p\n", (void *)this);

    return this;
}

static ao_info_t ao_info_jack = {
    6
};

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
    /* type, API, "name", version, special_info, init_function */
    { PLUGIN_AUDIO_OUT, AO_OUT_JACK_IFACE_VERSION, "jack", XINE_VERSION_CODE /* XINE_VERSION_CODE */, &ao_info_jack, init_class },
    { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

