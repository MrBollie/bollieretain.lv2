/**
    Bollie Retain - (c) 2016 Thomas Ebeling https://ca9.eu

    This file is part of bollieretain.lv2

    bolliedelay.lv2 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    bolliedelay.lv2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
* \file bollie-retain.c
* \author Bollie
* \date 11 Jun 2017
* \brief An LV2 sound retainer
*/

#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define URI "https://ca9.eu/lv2/bollieretain"

#define MAX_TAPE_LEN 960000


/**
* Make a bool type available. ;)
*/
typedef enum { false, true } bool;


/**
* Enumeration of LV2 ports
*/
typedef enum {
    BRT_BLEND       = 0,
    BRT_TRIGGER     = 1,
    BRT_INPUT_L     = 2,
    BRT_INPUT_R     = 3,
    BRT_OUTPUT_L    = 4,
    BRT_OUTPUT_R    = 5,
} PortIdx;


/**
* Struct for THE BollieRetain instance, the host is going to use.
*/
typedef struct {
    const float* ctl_blend;     ///< Tempo in BPM from host
    const float* ctl_trigger;   ///< Tempo in BPM set by user
    const float* input_l;       ///< input0, left side
    const float* input_r;       ///< input1, right side
    float* output_l;            ///< output1, left side
    float* output_r;            ///< output2, right side

    double rate;                ///< Current sample rate

    int n_loop_samples;         ///< Numbers of samples for the loop
    int n_fade_samples;         ///< Numbers of samples for fade

    int pos_w;                  ///< Write position
    int pos_r;                  ///< Read position

    int listening;              ///< State listening
    int looping;                ///< State looping

    float dry_gain;             ///< State leading towards target dry gain
    float wet_gain;             ///< State leading towards target dry gain

    float buffer_l[MAX_TAPE_LEN];   ///< delay buffer left
    float buffer_r[MAX_TAPE_LEN];   ///< delay buffer right

} BollieRetain;


/**
* Instantiates the plugin
* Allocates memory for the BollieRetain object and returns a pointer as
* LV2Handle.
*/
static LV2_Handle instantiate(const LV2_Descriptor * descriptor, double rate,
    const char* bundle_path, const LV2_Feature* const* features) {
    
    BollieRetain *self = (BollieRetain*)calloc(1, sizeof(BollieRetain));

    // Memorize sample rate for calculation
    self->rate = rate;
    self->n_fade_samples = ceil(0.2f * rate);
    self->n_fade_samples = rate;
    self->n_loop_samples = ceil(0.5f * rate);
    self->n_loop_samples = 5*rate;

    return (LV2_Handle)self;
}


/**
* Used by the host to connect the ports of this plugin.
* \param instance current LV2_Handle (will be cast to BollieRetain*)
* \param port LV2 port index, maches the enum above.
* \param data Pointer to the actual port data.
*/
static void connect_port(LV2_Handle instance, uint32_t port, void *data) {
    BollieRetain *self = (BollieRetain*)instance;

    switch ((PortIdx)port) {
        case BRT_BLEND:
            self->ctl_blend = data;
            break;
        case BRT_TRIGGER:
            self->ctl_trigger = data;
            break;
        case BRT_INPUT_L:
            self->input_l = data;
            break;
        case BRT_INPUT_R:
            self->input_r = data;
            break;
        case BRT_OUTPUT_L:
            self->output_l = data;
            break;
        case BRT_OUTPUT_R:
            self->output_r = data;
            break;
    }
}
    

/**
* This has to reset all the internal states of the plugin
* \param instance pointer to current plugin instance
*/
static void activate(LV2_Handle instance) {
    BollieRetain* self = (BollieRetain*)instance;
    // Let's remove all that noise
    for (int i = 0 ; i < MAX_TAPE_LEN ; ++i) {
        self->buffer_l[i] = 0;
        self->buffer_r[i] = 0;
    }

    // Reset state variables
    self->pos_r = 0;
    self->pos_w = 0;
    self->dry_gain = 0;
    self->wet_gain = 0;
    self->listening = false;
    self->looping = true;
}

/**
* Main process function of the plugin.
* \param instance  handle of the current plugin
* \param n_samples number of samples in this current input block.
*/
static void run(LV2_Handle instance, uint32_t n_samples) {
    BollieRetain* self = (BollieRetain*)instance;

    float dry_gain = self->dry_gain;
    float wet_gain = self->wet_gain;
    int pos_w = self->pos_w;
    int pos_r = self->pos_r;
    int n_fade_samples = self->n_fade_samples;
    int n_loop_samples = self->n_loop_samples;
    int listening = self->listening;
    int looping = self->looping;
    float ctl_blend = *self->ctl_blend;

    // Now listen
    if (*(self->ctl_trigger) > 0 && !listening) {
        listening = true;
    }
    
    // Gain calculation
    float target_dry_gain = 1;
    float target_wet_gain = 0;
    if (ctl_blend > 0 && ctl_blend < 50) {
        target_wet_gain = powf(10.0f, (ctl_blend-50) * 0.04f);
    }
    else if (ctl_blend < 100 && ctl_blend > 50) {
        target_wet_gain = 1;
        target_dry_gain = powf(10.0f, (ctl_blend-50) * -0.04f);
    }
    else if (ctl_blend == 50) {
        target_wet_gain = 1;
    }
    else if (ctl_blend == 100) {
        target_wet_gain = 1;
        target_dry_gain = 0;
    }

    // Loop over the block of audio we got
    for (unsigned int i = 0 ; i < n_samples ; ++i) {
        
        // Paraemter smoothing for wet and dry gain
        wet_gain = target_wet_gain * 0.01f + wet_gain * 0.99f;
        dry_gain = target_dry_gain * 0.01f + dry_gain * 0.99f;

        // Current samples
        float cur_s_l = self->input_l[i];
        float cur_s_r = self->input_r[i];
        float wet_s_l = 0; // Wet sample left
        float wet_s_r = 0; // Wet sample right
        float coeff = 1.0f;
        if (listening && !looping) {
            if (pos_w < n_loop_samples) {
                if (pos_w < n_fade_samples) {
                    coeff = 1/n_fade_samples * pos_w;
                }
                else if (pos_w >= n_loop_samples - n_fade_samples) {
                    coeff = 1/n_fade_samples * (n_loop_samples-1 - pos_w);
                }

                self->buffer_l[pos_w] = cur_s_l * coeff;
                self->buffer_r[pos_w++] = cur_s_r * coeff;;
            }
            else {
                listening = false;
                looping = true;
                pos_w = 0;
            }
        }
        else if (looping) {
            // buffer size - fade offset needs a crossfade
            if (pos_r >= n_loop_samples - n_fade_samples && !listening) {
                int p = pos_r - (n_loop_samples - n_fade_samples); 
                wet_s_l = self->buffer_l[pos_r] + self->buffer_l[p];
                wet_s_r = self->buffer_r[pos_r] + self->buffer_r[p];
            }
            else {
                // Simply copy
                wet_s_l = self->buffer_l[pos_r];
                wet_s_r = self->buffer_r[pos_r];
            }
            pos_r++;
            // reset to fade offset at the end of the buffer
            if (pos_r >= n_loop_samples) {
                if (listening) {
                    looping = false;
                    pos_r = 0;
                }
                else {
                    pos_r = n_fade_samples;
                }
            }
        }
        self->output_l[i] = cur_s_l * dry_gain +  wet_s_l * wet_gain;
        self->output_r[i] = cur_s_r * dry_gain +  wet_s_r * wet_gain;
    }
    self->pos_w = pos_w;
    self->pos_r = pos_r;
    self->dry_gain = dry_gain;
    self->wet_gain = wet_gain;
    self->looping = looping;
    self->listening = listening;
}


/**
* Called, when the host deactivates the plugin.
*/
static void deactivate(LV2_Handle instance) {
}


/**
* Cleanup, freeing memory and stuff
*/
static void cleanup(LV2_Handle instance) {
    free(instance);
}


/**
* extension stuff for additional interfaces
*/
static const void* extension_data(const char* uri) {
    return NULL;
}


/**
* Descriptor linking our methods.
*/
static const LV2_Descriptor descriptor = {
    URI,
    instantiate,
    connect_port,
    activate,
    run,
    deactivate,
    cleanup,
    extension_data
};


/**
* Symbol export using the descriptor above
*/
LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    switch (index) {
        case 0:  return &descriptor;
        default: return NULL;
    }
}
