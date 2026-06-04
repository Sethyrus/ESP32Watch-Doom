#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_codec_dev.h"
#include "doomgeneric.h"

#include "i_sound.h"
#include "doomtype.h"
#include "w_wad.h"
#include "z_zone.h"

static const char *TAG = "doom_sound";

#define NUM_CHANNELS 8
#define SAMPLE_RATE 22050
#define BUFFER_SAMPLES 512
#define SFX_HEADER_BYTES 8
#define SFX_PAD_BYTES 16
#define SFX_MIN_DMX_LENGTH 49

typedef struct {
    const uint8_t *samples;
    uint32_t sample_count;
    uint32_t sample_rate;
} sfx_lump_view_t;

typedef struct {
    sfxinfo_t *sfxinfo;
    const uint8_t *data;
    uint32_t length;
    uint32_t pos_fp; // Fixed point 16.16 position
    uint32_t step_fp; // Fixed point 16.16 step
    int vol;
    bool active;
} channel_t;

static channel_t channels[NUM_CHANNELS];
static esp_codec_dev_handle_t s_codec;
static TaskHandle_t s_audio_task;
static SemaphoreHandle_t s_audio_mutex;
static bool s_sound_initialized = false;
static bool s_use_sfx_prefix = false;
static int s_sfx_load_logs;

// Dummy sound devices
static snddevice_t sound_devices[] = { SNDDEVICE_SB };

// Convert doom volume (0-255) to a multiplier (0-256)
static inline int vol_to_mult(int vol) {
    return vol;
}

static uint16_t read_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static bool parse_dmx_sfx(sfxinfo_t *sfxinfo, sfx_lump_view_t *view)
{
    if (sfxinfo == NULL || view == NULL || sfxinfo->lumpnum < 0) {
        return false;
    }

    const uint8_t *raw_data = (const uint8_t *)sfxinfo->driver_data;
    if (raw_data == NULL) {
        raw_data = (const uint8_t *)W_CacheLumpNum(sfxinfo->lumpnum, PU_STATIC);
        sfxinfo->driver_data = (void *)raw_data;
    }

    const uint32_t lump_len = W_LumpLength(sfxinfo->lumpnum);
    const uint16_t format = (lump_len >= sizeof(uint16_t)) ? read_le16(raw_data) : 0xffff;
    if (lump_len < SFX_HEADER_BYTES || format != 3) {
        ESP_LOGW(TAG, "Invalid SFX lump %s: lump=%d len=%" PRIu32 " fmt=0x%04x",
                 sfxinfo->name, sfxinfo->lumpnum, lump_len, format);
        return false;
    }

    const uint32_t dmx_len = read_le32(raw_data + 4);
    if (dmx_len > lump_len - SFX_HEADER_BYTES || dmx_len < SFX_MIN_DMX_LENGTH) {
        ESP_LOGW(TAG, "Invalid SFX length %s: lump=%d lump_len=%" PRIu32 " dmx_len=%" PRIu32,
                 sfxinfo->name, sfxinfo->lumpnum, lump_len, dmx_len);
        return false;
    }

    view->sample_rate = read_le16(raw_data + 2);
    view->sample_count = dmx_len - (SFX_PAD_BYTES * 2);
    view->samples = raw_data + SFX_HEADER_BYTES + SFX_PAD_BYTES;

    if (view->sample_rate == 0 || view->sample_rate > 48000 || view->sample_count == 0) {
        ESP_LOGW(TAG, "Unsupported SFX %s: rate=%" PRIu32 " samples=%" PRIu32,
                 sfxinfo->name, view->sample_rate, view->sample_count);
        return false;
    }

    if (s_sfx_load_logs < 8) {
        ESP_LOGI(TAG, "SFX %s lump=%d rate=%" PRIu32 " samples=%" PRIu32,
                 sfxinfo->name, sfxinfo->lumpnum, view->sample_rate, view->sample_count);
        s_sfx_load_logs++;
    }

    return true;
}

static void audio_task(void *arg)
{
    int16_t mix_buffer[BUFFER_SAMPLES * 2]; // Stereo buffer

    while (1) {
        if (!s_sound_initialized) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        memset(mix_buffer, 0, sizeof(mix_buffer));

        if (xSemaphoreTake(s_audio_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            for (int i = 0; i < NUM_CHANNELS; i++) {
                if (!channels[i].active || !channels[i].data) continue;

                channel_t *ch = &channels[i];
                for (int s = 0; s < BUFFER_SAMPLES; s++) {
                    uint32_t pos = ch->pos_fp >> 16;
                    if (pos >= ch->length) {
                        ch->active = false;
                        break;
                    }

                    // Doom sounds are 8-bit unsigned. Convert to 16-bit signed.
                    int16_t sample = (int16_t)((ch->data[pos] - 128) << 8);

                    // Apply volume
                    int32_t mixed = mix_buffer[s * 2] + ((sample * ch->vol) / 256);

                    // Clip
                    if (mixed > 32767) mixed = 32767;
                    else if (mixed < -32768) mixed = -32768;

                    // Write to both Left and Right channels (Stereo)
                    mix_buffer[s * 2] = (int16_t)mixed;
                    mix_buffer[s * 2 + 1] = (int16_t)mixed;

                    ch->pos_fp += ch->step_fp;
                }
            }
            xSemaphoreGive(s_audio_mutex);
        }

        if (s_codec) {
            // Write to I2S via esp_codec_dev
            int ret = esp_codec_dev_write(s_codec, mix_buffer, sizeof(mix_buffer));
            if (ret != ESP_CODEC_DEV_OK) {
                // If it fails, wait a bit so we don't spin endlessly
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(10)); // Shouldn't happen, but just in case
        }
    }
}

static boolean I_ESP32_Init(boolean use_sfx_prefix)
{
    ESP_LOGI(TAG, "I_ESP32_Init called");
    s_use_sfx_prefix = use_sfx_prefix;

    memset(channels, 0, sizeof(channels));
    s_audio_mutex = xSemaphoreCreateMutex();
    if (s_audio_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create audio mutex");
        return false;
    }

    BaseType_t created = xTaskCreatePinnedToCore(audio_task, "doom_audio", 6144, NULL, 5, &s_audio_task, 0);
    if (created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio task");
        vSemaphoreDelete(s_audio_mutex);
        s_audio_mutex = NULL;
        return false;
    }

    s_sound_initialized = true;
    return true;
}

static void I_ESP32_Shutdown(void)
{
    s_sound_initialized = false;
    if (s_audio_task) {
        vTaskDelete(s_audio_task);
        s_audio_task = NULL;
    }
    if (s_audio_mutex) {
        vSemaphoreDelete(s_audio_mutex);
        s_audio_mutex = NULL;
    }
}

static int I_ESP32_GetSfxLumpNum(sfxinfo_t *sfx)
{
    if (sfx->link != NULL) {
        sfx = sfx->link;
    }

    char namebuf[16];
    if (s_use_sfx_prefix) {
        snprintf(namebuf, sizeof(namebuf), "ds%s", sfx->name);
    } else {
        snprintf(namebuf, sizeof(namebuf), "%s", sfx->name);
    }
    return W_GetNumForName(namebuf);
}

static void I_ESP32_Update(void)
{
    // Nothing to do periodically
}

static void I_ESP32_UpdateSoundParams(int channel, int vol, int sep)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return;

    if (xSemaphoreTake(s_audio_mutex, portMAX_DELAY) == pdTRUE) {
        channels[channel].vol = vol_to_mult(vol);
        xSemaphoreGive(s_audio_mutex);
    }
}

static int I_ESP32_StartSound(sfxinfo_t *sfxinfo, int channel, int vol, int sep)
{
    if (!s_sound_initialized) return -1;

    if (channel < 0 || channel >= NUM_CHANNELS) {
        // Find a free channel
        for (int i = 0; i < NUM_CHANNELS; i++) {
            if (!channels[i].active) {
                channel = i;
                break;
            }
        }
    }

    if (channel < 0 || channel >= NUM_CHANNELS) return -1; // No free channels

    sfx_lump_view_t sfx_view;
    if (!parse_dmx_sfx(sfxinfo, &sfx_view)) {
        return -1;
    }

    if (xSemaphoreTake(s_audio_mutex, portMAX_DELAY) == pdTRUE) {
        channel_t *ch = &channels[channel];
        ch->sfxinfo = sfxinfo;

        ch->data = sfx_view.samples;
        ch->length = sfx_view.sample_count;
        ch->pos_fp = 0;

        const uint32_t pitch = (sfxinfo->pitch > 0) ? (uint32_t)sfxinfo->pitch : 128;
        const uint64_t actual_rate = ((uint64_t)sfx_view.sample_rate * pitch) / 128;
        ch->step_fp = (uint32_t)((actual_rate << 16) / SAMPLE_RATE);
        if (ch->step_fp == 0) {
            ch->step_fp = 1;
        }

        ch->vol = vol_to_mult(vol);
        ch->active = true;
        xSemaphoreGive(s_audio_mutex);
    }

    return channel;
}

static void I_ESP32_StopSound(int channel)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return;

    if (xSemaphoreTake(s_audio_mutex, portMAX_DELAY) == pdTRUE) {
        channels[channel].active = false;
        xSemaphoreGive(s_audio_mutex);
    }
}

static boolean I_ESP32_SoundIsPlaying(int channel)
{
    if (channel < 0 || channel >= NUM_CHANNELS) return false;

    bool playing = false;
    if (xSemaphoreTake(s_audio_mutex, portMAX_DELAY) == pdTRUE) {
        playing = channels[channel].active;
        xSemaphoreGive(s_audio_mutex);
    }
    return playing;
}

static void I_ESP32_CacheSounds(sfxinfo_t *sounds, int num_sounds)
{
    if (sounds == NULL) {
        return;
    }

    for (int i = 0; i < num_sounds; i++) {
        sounds[i].driver_data = NULL;
    }
}

// Global module definition for doomgeneric
sound_module_t DG_sound_module = {
    sound_devices,
    1,
    I_ESP32_Init,
    I_ESP32_Shutdown,
    I_ESP32_GetSfxLumpNum,
    I_ESP32_Update,
    I_ESP32_UpdateSoundParams,
    I_ESP32_StartSound,
    I_ESP32_StopSound,
    I_ESP32_SoundIsPlaying,
    I_ESP32_CacheSounds
};

// Dummy music module
static boolean I_ESP32_MusicInit(void) { return true; }
static void I_ESP32_MusicShutdown(void) {}
static void I_ESP32_SetMusicVolume(int volume) {}
static void I_ESP32_PauseSong(void) {}
static void I_ESP32_ResumeSong(void) {}
static void *I_ESP32_RegisterSong(void *data, int len) { return NULL; }
static void I_ESP32_UnRegisterSong(void *handle) {}
static void I_ESP32_PlaySong(void *handle, boolean looping) {}
static void I_ESP32_StopSong(void) {}
static boolean I_ESP32_MusicIsPlaying(void) { return false; }
static void I_ESP32_Poll(void) {}

music_module_t DG_music_module = {
    sound_devices,
    1,
    I_ESP32_MusicInit,
    I_ESP32_MusicShutdown,
    I_ESP32_SetMusicVolume,
    I_ESP32_PauseSong,
    I_ESP32_ResumeSong,
    I_ESP32_RegisterSong,
    I_ESP32_UnRegisterSong,
    I_ESP32_PlaySong,
    I_ESP32_StopSong,
    I_ESP32_MusicIsPlaying,
    I_ESP32_Poll
};

// Globals expected by i_sound.c
int use_libsamplerate = 0;
float libsamplerate_scale = 1.0f;

// Expose a setter for doom_app to provide the codec handle
void doom_app_set_audio_codec(esp_codec_dev_handle_t codec)
{
    s_codec = codec;
}
