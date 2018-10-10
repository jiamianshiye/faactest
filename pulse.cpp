#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "pulse.h"
#include "memkit.h"

#define CLEAR_LINE "\n"
#define BUFF_LEN 4096

// From pulsecore/macro.h
pa_mainloop *pa_ml = NULL;
pa_mainloop_api *pa_mlapi = NULL;
static int retval;
int verbose = 1;
static pa_stream_flags_t flags;
static int pcm_fd;

#define pa_memzero(x,l) (memset((x), 0, (l)))
#define pa_zero(x) (pa_memzero(&(x), sizeof(x)))

static pa_stream *stream = NULL;
static pa_stream *ostream = NULL;

static pa_context *context;
static pa_sample_spec sample_spec = {
  .format = PA_SAMPLE_S16LE,
  .rate = 44100,
  .channels = 2
};

struct pulseUnit g_pulseunit;
callback_get_pcm cb_get_pcm = NULL;

//int fdout;
//char *fname = "tmp.s16";

static size_t latency = 0, process_time = 0;
static int32_t latency_msec = 1, process_time_msec = 0;


extern pthread_mutex_t audio_cache_lock;
static struct timeval tv1, tv2;
unsigned char pcmbuf[40*1024];
unsigned int pcmoffset = 0;
unsigned int pcmlength = 0;
/* This is called whenever new data is available */
static void stream_read_callback(pa_stream *s, size_t length, void *userdata)
{

    assert(s);
    assert(length > 0);

    gettimeofday(&tv2, NULL);
    tv1 = tv2;
    int i = 0;
    char *ptr = NULL;
    int lout;
    int ret = 0;
       
    while (pa_stream_readable_size(s) > 0)
    {
        const void *data;
        size_t length;

        // peek actually creates and fills the data vbl
        if (pa_stream_peek(s, &data, &length) < 0)
        {
            fprintf(stderr, "Read failed\n");
            exit(1);
            return;
        }

        lout =  pa_stream_writable_size(ostream);
        //fprintf(stderr, "Writable: %lu\n", lout);
        if (lout == 0) {
            fprintf(stderr, "can't write, zero writable\n");
            return;
        }

        if (lout < length) {
            fprintf(stderr, "Truncating read length:%d,lout:%d\n", length, lout);
            length = lout;
        }

        if(g_pulseunit.writefile && pcm_fd > 0)
        {
            write(pcm_fd, data, length);
        }

        ret = cb_get_pcm((unsigned char *)data, (unsigned int)length);

        if (pa_stream_write(ostream, (uint8_t*) data, length, NULL, 0, PA_SEEK_RELATIVE) < 0) {
            fprintf(stderr, "pa_stream_write() failed\n");
            exit(1);
            return;
        }
        // swallow the data peeked at before
        pa_stream_drop(s);
    }
}
/* This is called whenever new data may be written to the stream */
// We don't actually write anything this time
static void stream_write_callback(pa_stream *s, size_t length, void *userdata) {
  //assert(s);
  //assert(length > 0);

  printf("Stream write callback: Ready to write %lu bytes\n", length);
 }

void stream_state_callback(pa_stream *s, void *userdata)
{
    assert(s);

    int status = pa_stream_get_state(s);
    printf("get status : %d\n", status);
    switch (status)
    {
    case PA_STREAM_CREATING:
        // The stream has been created, so
        // let's open a file to record to
        printf("Creating stream\n");
        //fdout = creat(fname, 0711);
        break;

    case PA_STREAM_TERMINATED:
        //close(fdout);
        break;

    case PA_STREAM_READY:

        // Just for info: no functionality in this branch
        if (verbose)
        {
            const pa_buffer_attr *a;
            //char cmt[PA_CHANNEL_MAP_SNPRINT_MAX], sst[PA_SAMPLE_SPEC_SNPRINT_MAX];

            printf("Stream successfully created.");

            if (!(a = pa_stream_get_buffer_attr(s)))
                printf("pa_stream_get_buffer_attr() failed: %s", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
            else
            {
                printf("Buffer metrics: maxlength=%u, fragsize=%u", a->maxlength, a->fragsize);
            }

            printf("Connected to device %s (%u, %ssuspended).",
                   pa_stream_get_device_name(s),
                   pa_stream_get_device_index(s),
                   pa_stream_is_suspended(s) ? "" : "not ");
        }

        break;

    case PA_STREAM_FAILED:
    default:
        printf("Stream error: %s", pa_strerror(pa_context_errno(pa_stream_get_context(s))));
        exit(1);
    }
}

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
void state_cb(pa_context *c, void *userdata)
{
    pa_context_state_t state;
    int *pa_ready = (int *)userdata;
    state = pa_context_get_state(c);
    switch(state)
    {
        // There are just here for reference
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        default:
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            *pa_ready = 2;
            break;    
        case PA_CONTEXT_READY:
        {
            pa_buffer_attr buffer_attr;

            if (verbose)
                printf("Connection established.%s\n", CLEAR_LINE);

            if (!(stream = pa_stream_new(c, "JanCapture", &sample_spec, NULL)))
            {
                printf("pa_stream_new() failed: %s", pa_strerror(pa_context_errno(c)));
                exit(1);
            }
            if (!(ostream = pa_stream_new(c, "JanPlayback", &sample_spec, NULL))) {
            printf("pa_stream_new() failed: %s", pa_strerror(pa_context_errno(c)));
            exit(1);
            }

            // Watch for changes in the stream state to create the output file
            pa_stream_set_state_callback(stream, stream_state_callback, NULL);

            // Watch for changes in the stream's read state to write to the output file
            pa_stream_set_read_callback(stream, stream_read_callback, NULL);

            pa_stream_set_write_callback(ostream, stream_write_callback, NULL);
            // Set properties of the record buffer
            pa_zero(buffer_attr);
            buffer_attr.maxlength = (uint32_t)-1;
            buffer_attr.prebuf = (uint32_t)-1;

            if (latency_msec > 0)
            {
                buffer_attr.fragsize = buffer_attr.tlength = pa_usec_to_bytes(latency_msec * PA_USEC_PER_MSEC, &sample_spec);
                //flags |= (pa_stream_flags_t)PA_STREAM_ADJUST_LATENCY;
                flags = static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY);
            }
            else if (latency > 0)
            {
                buffer_attr.fragsize = buffer_attr.tlength = (uint32_t)latency;
                flags = PA_STREAM_ADJUST_LATENCY;
            }
            else
                buffer_attr.fragsize = buffer_attr.tlength = (uint32_t)-1;

            printf("buffer fragsize:%d\n", buffer_attr.fragsize);

            if (process_time_msec > 0)
            {
                buffer_attr.minreq = pa_usec_to_bytes(process_time_msec * PA_USEC_PER_MSEC, &sample_spec);
            }
            else if (process_time > 0)
                buffer_attr.minreq = (uint32_t)process_time;
            else
                buffer_attr.minreq = (uint32_t)-1;

            printf("buffer fragsize:%d\n", buffer_attr.fragsize);
            printf("buffer minreq:%d\n", buffer_attr.minreq);
            buffer_attr.minreq = 1024;
            buffer_attr.fragsize  = 2048;

            sleep(3);
            // and start recording
            if (pa_stream_connect_record(stream, NULL, &buffer_attr, flags) < 0)
            {
                printf("pa_stream_connect_record() failed: %s", pa_strerror(pa_context_errno(c)));
                exit(1);
            }

            if(pa_stream_connect_playback(ostream, NULL, &buffer_attr, flags, NULL, NULL) < 0)
            {
                printf("pa_stream_connect_playback failed:%s\n", pa_strerror(pa_context_errno(c)));
                exit(1);
            }
        }
    }
}
int pulse_init(struct pulseUnit *pObj)
{
    pa_context *pa_ctx = NULL;
// Create a mainloop API and connection to the default server
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "test");

    if(NULL == pObj)
    {
        ERROR("Wrong params pObj:%p!\n", pObj);
        return -1;
    }

    memset(&g_pulseunit, 0, sizeof(struct pulseUnit));

    sample_spec.channels = pObj->channels;
    sample_spec.rate = pObj->rate;
    sample_spec.format = (pa_sample_format_t)pObj->format;
    cb_get_pcm = pObj->cb_pcm_data;

    if(pObj->writefile == 1)
    {
        pcm_fd = open(pObj->filename, O_RDWR | O_CREAT | O_TRUNC);
        if(pcm_fd < 0)
        {
            ERROR("Cannot open file : %s\n", pObj->filename);
            pcm_fd = -1;
        }
    }

    memcpy(&g_pulseunit, pObj, sizeof(struct pulseUnit));

  // This function connects to the pulse server
    pa_context_connect(pa_ctx, NULL, (pa_context_flags_t)0, NULL);
    pa_context_set_state_callback(pa_ctx, state_cb, NULL);

    return 0;
}

int pulse_loop_in()
{
    if (pa_mainloop_run(pa_ml, &retval) < 0)
    {
        printf("pa_mainloop_run() failed.");
        exit(1);
    }

    return -1;
}