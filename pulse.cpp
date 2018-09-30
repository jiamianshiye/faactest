#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>

pa_mainloop *pa_ml = NULL;
pa_mainloop_api *pa_mlapi = NULL;


int pulse_init()
{
    pa_operation *pa_op = NULL;
    pa_context *pa_ctx = NULL;
// Create a mainloop API and connection to the default server
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "test");

  // This function connects to the pulse server
    pa_context_connect(pa_ctx, NULL, (pa_context_flags_t)0, NULL);


    return 0;
}

int pulse_loop_in()
{
    if (pa_mainloop_run(pa_ml, &ret) < 0)
    {
        printf("pa_mainloop_run() failed.");
        exit(1);
    }
}