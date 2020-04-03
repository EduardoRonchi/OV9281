#include "arducam_mipicamera.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define LOG(fmt, args...) fprintf(stderr, fmt "\n", ##args)
#define SOFTWARE_AE_AWB
#define ENABLE_PREVIEW

FILE *fd;
int frame_count = 0;
int video_callback(BUFFER *buffer) {
    if (TIME_UNKNOWN == buffer->pts) {
        // Frame data in the second half
    }
    // LOG("buffer length = %d, pts = %llu, flags = 0x%X", buffer->length, buffer->pts, buffer->flags);

    if (buffer->length) {
        if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CONFIG) {
            // SPS PPS
            if (fd) {
                fwrite(buffer->data, 1, buffer->length, fd);
                fflush(fd);
            }
        }
        if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) {
            /// Encoder outputs inline Motion Vectors
        } else {
            // MMAL_BUFFER_HEADER_FLAG_KEYFRAME
            // MMAL_BUFFER_HEADER_FLAG_FRAME_END
            if (fd) {
                int bytes_written = fwrite(buffer->data, 1, buffer->length, fd);
                fflush(fd);
            }
            // Here may be just a part of the data, we need to check it.
            if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
                frame_count++;
        }
    }
    return 0;
}
static void default_status(VIDEO_ENCODER_STATE *state) {
    // Default everything to zero
    memset(state, 0, sizeof(VIDEO_ENCODER_STATE));
    state->encoding = VIDEO_ENCODING_H264;
    state->bitrate = 17000000;
    state->immutableInput = 1; // Not working
    /**********************H264 only**************************************/
    state->intraperiod = -1;                  // Not set
                                              // Specify the intra refresh period (key frame rate/GoP size).
                                              // Zero to produce an initial I-frame and then just P-frames.
    state->quantisationParameter = 0;         // Quantisation parameter. Use approximately 10-40. Default 0 (off)
    state->profile = VIDEO_PROFILE_H264_HIGH; // Specify H264 profile to use for encoding
    state->level = VIDEO_LEVEL_H264_4;        // Specify H264 level to use for encoding
    state->bInlineHeaders = 0;                // Insert inline headers (SPS, PPS) to stream
    state->inlineMotionVectors = 0;           // output motion vector estimates
    state->intra_refresh_type = -1;           // Set intra refresh type
    state->addSPSTiming = 0;                  // zero or one
    state->slices = 1;
    /**********************H264 only**************************************/
}

// ROTINA DE Escrita
struct reg {
    uint16_t address;
    uint16_t value;
};

struct reg regs[] = {
    {0x3006, 0x08}, //Strobe register as Output
    {0x3921, 0x00}, //Strobe register
    {0x3922, 0x00}, //Strobe register
    {0x3923, 0x00}, //Strobe register
    {0x3924, 0xFF}, //Strobe register
    {0x3925, 0x00}, //Strobe register
    {0x3926, 0x00}, //Strobe register
    {0x3927, 0x00}, //Strobe register
    {0x3928, 0xFF}, //Strobe register
    {0x3912, 0x08}, //Strobe register
    {0x3913, 0x00}  //Strobe register
};

static const int regs_size = sizeof(regs) / sizeof(regs[0]);


int write_regs(CAMERA_INSTANCE camera_instance, struct reg regs[], int length){
    int status = 0;
    for(int i = 0; i < length; i++){
	uint16_t valueRegs = 0;
        if (arducam_write_sensor_reg(camera_instance, regs[i].address, regs[i].value)) {
            LOG("Failed to write register: 0x%02X, 0x%02X.", regs[i].address, regs[i].value);
            status += 1;
        }
    }
    return status;
}

int main(int argc, char **argv) {
    CAMERA_INSTANCE camera_instance;
    int count = 0;
    int width = 0, height = 0;

    LOG("Open camera...");
    int res = arducam_init_camera(&camera_instance);
    if (res) {
        LOG("init camera status = %d", res);
        return -1;
    }

    width = 1920;
    height = 1080;
    LOG("Setting the resolution...");
    res = arducam_set_resolution(camera_instance, &width, &height);
    if (res) {
        LOG("set resolution status = %d", res);
        return -1;
    } else {
        LOG("Current resolution is %dx%d", width, height);
        LOG("Notice:You can use the list_format sample program to see the resolution and control supported by the camera.");
    }
#if defined(ENABLE_PREVIEW)
    LOG("Start preview...");
    PREVIEW_PARAMS preview_params = {
        .fullscreen = 0,             // 0 is use previewRect, non-zero to use full screen
        .opacity = 255,              // Opacity of window - 0 = transparent, 255 = opaque
        .window = {0, 0, 1280, 720}, // Destination rectangle for the preview window.
    };
    res = arducam_start_preview(camera_instance, &preview_params);
    if (res) {
        LOG("start preview status = %d", res);
        return -1;
    }
#endif

    //EDITED by EDUARDO - Write Strobes' registers
    write_regs(camera_instance, regs, regs_size);

    //EDITED by EDUARDO - Reads the registers' values (Just sequencies)
    struct reg2 {
    uint16_t address2;
    };
    struct reg2 regs2[] = {
    {0x3920},
    {0x3921},
    {0x3922},
    {0x3923},
    {0x3924},
    {0x3925},
    {0x3926},
    {0x3927},
    {0x3928}
    };
    for(int k = 0; k < 0x9; k++){
    uint16_t value_2 = 0;
        if (arducam_read_sensor_reg(camera_instance, regs2[k].address2, &value_2)) {
            LOG("Failed to read register: 0x%02X, 0x%02X.", regs2[k].address2, value_2);
        } else {
            LOG("Read 0x%02X = 0x%02X", regs2[k].address2, value_2);
        }
    }
   
    uint16_t value_3006 = 0;
        if (arducam_read_sensor_reg(camera_instance, 0x3006, &value_3006)) {
            LOG("Failed to read register: 0x3006, 0x%02X.", value_3006);
        } else {
            LOG("Read 0x3006 = 0x%02X", value_3006);
        }
    // EDN OF EDITION

#if defined(SOFTWARE_AE_AWB)
    LOG("Enable Software Auto Exposure...");
    arducam_software_auto_exposure(camera_instance, 1);
    LOG("Enable Software Auto White Balance...");
    arducam_software_auto_white_balance(camera_instance, 1);
#endif

    fd = fopen("test.h264", "wb");
    VIDEO_ENCODER_STATE video_state;
    default_status(&video_state);
    // start video callback
    // Set video_state to NULL, using default parameters
    LOG("Start video encoding...");
    res = arducam_set_video_callback(camera_instance, &video_state, video_callback, NULL);
    if (res) {
        LOG("Failed to start video encoding, probably due to resolution greater than 1920x1080 or video_state setting error.");
        return -1;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_REALTIME, &start);
    usleep(1000 * 1000 * 20); //Controla o tempo de duração do programa. Nesta configuração são 20 segundos.
    clock_gettime(CLOCK_REALTIME, &end);

    double timeElapsed = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1000000000.0;
    LOG("Total frame count = %d", frame_count);
    LOG("TimeElapsed = %f", timeElapsed);

    // stop video callback
    LOG("Stop video encoding...");
    arducam_set_video_callback(camera_instance, NULL, NULL, NULL);
    fclose(fd);

    LOG("Close camera...");
    res = arducam_close_camera(camera_instance);
    if (res) {
        LOG("close camera status = %d", res);
    }
    return 0;
}
