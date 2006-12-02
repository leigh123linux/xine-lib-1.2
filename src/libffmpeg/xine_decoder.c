/*
 * Copyright (C) 2001-2004 the xine project
 * 
 * This file is part of xine, a free video player.
 * 
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: xine_decoder.c,v 1.171 2006/10/11 12:53:12 dgp85 Exp $
 *
 * xine decoder plugin using ffmpeg
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xine_internal.h"

#include "xine_decoder.h"

/*
 * common initialisation
 */

pthread_once_t once_control = PTHREAD_ONCE_INIT;
pthread_mutex_t ffmpeg_lock;

#ifndef HAVE_FFMPEG
void avcodec_register_all(void)
{
    static int inited = 0;
    
    if (inited != 0)
	return;
    inited = 1;

    /* decoders */
    register_avcodec(&h263_decoder);
    register_avcodec(&mpeg4_decoder);
    register_avcodec(&msmpeg4v1_decoder);
    register_avcodec(&msmpeg4v2_decoder);
    register_avcodec(&msmpeg4v3_decoder);
    register_avcodec(&wmv1_decoder);
    register_avcodec(&wmv2_decoder);
    register_avcodec(&h263i_decoder);
    register_avcodec(&rv10_decoder);
    register_avcodec(&rv20_decoder);
    register_avcodec(&svq1_decoder);
    register_avcodec(&svq3_decoder);
    register_avcodec(&wmav1_decoder);
    register_avcodec(&wmav2_decoder);
    register_avcodec(&indeo3_decoder);
    register_avcodec(&mpeg1video_decoder);
    register_avcodec(&dvvideo_decoder);
    register_avcodec(&pcm_s16le_decoder);
    register_avcodec(&mjpeg_decoder);
    register_avcodec(&mjpegb_decoder);
    register_avcodec(&mp2_decoder);
    register_avcodec(&mp3_decoder);
    register_avcodec(&mace3_decoder);
    register_avcodec(&mace6_decoder);
    register_avcodec(&huffyuv_decoder);
    register_avcodec(&cyuv_decoder);
    register_avcodec(&h264_decoder);
    register_avcodec(&vp3_decoder);
    register_avcodec(&fourxm_decoder);
    register_avcodec(&ra_144_decoder);
    register_avcodec(&ra_288_decoder);
    register_avcodec(&adpcm_ms_decoder);
    register_avcodec(&adpcm_ima_qt_decoder);
    register_avcodec(&adpcm_ima_wav_decoder);
    register_avcodec(&adpcm_ima_dk3_decoder);
    register_avcodec(&adpcm_ima_dk4_decoder);
    register_avcodec(&adpcm_ima_ws_decoder);
    register_avcodec(&adpcm_ima_smjpeg_decoder);
    register_avcodec(&adpcm_xa_decoder);
    register_avcodec(&adpcm_4xm_decoder);
    register_avcodec(&adpcm_ea_decoder);
    register_avcodec(&pcm_alaw_decoder);
    register_avcodec(&pcm_mulaw_decoder);
    register_avcodec(&roq_dpcm_decoder);
    register_avcodec(&interplay_dpcm_decoder);
    register_avcodec(&cinepak_decoder);
    register_avcodec(&msvideo1_decoder);
    register_avcodec(&msrle_decoder);
    register_avcodec(&rpza_decoder);
    register_avcodec(&roq_decoder);
    register_avcodec(&idcin_decoder);
    register_avcodec(&xan_wc3_decoder);
    register_avcodec(&vqa_decoder);
    register_avcodec(&interplay_video_decoder);
    register_avcodec(&flic_decoder);
    register_avcodec(&smc_decoder);
    register_avcodec(&eightbps_decoder);
    register_avcodec(&vmdvideo_decoder);
    register_avcodec(&vmdaudio_decoder);
    register_avcodec(&truemotion1_decoder);
    register_avcodec(&mszh_decoder);
    register_avcodec(&zlib_decoder);
    register_avcodec(&xan_dpcm_decoder);
    register_avcodec(&asv1_decoder);
    register_avcodec(&asv2_decoder);
    register_avcodec(&vcr1_decoder);
    register_avcodec(&flv_decoder);
    register_avcodec(&qtrle_decoder);
    register_avcodec(&flac_decoder);
    register_avcodec(&aasc_decoder);
    register_avcodec(&alac_decoder);
    register_avcodec(&h261_decoder);
    register_avcodec(&loco_decoder);
    register_avcodec(&qdraw_decoder);
    register_avcodec(&qpeg_decoder);
    register_avcodec(&tscc_decoder);
    register_avcodec(&ulti_decoder);
    register_avcodec(&wnv1_decoder);
    register_avcodec(&xl_decoder);
    register_avcodec(&indeo2_decoder);
    register_avcodec(&fraps_decoder);
    register_avcodec(&shorten_decoder);
    register_avcodec(&qdm2_decoder);
    register_avcodec(&truemotion2_decoder);
    register_avcodec(&wmv3_decoder);
    register_avcodec(&cscd_decoder);
    register_avcodec(&mmvideo_decoder);
    register_avcodec(&zmbv_decoder);
    register_avcodec(&avs_decoder);
    register_avcodec(&smacker_decoder);
    register_avcodec(&smackaud_decoder);
    register_avcodec(&nuv_decoder);
    register_avcodec(&kmvc_decoder);
    register_avcodec(&flashsv_decoder);
    register_avcodec(&cavs_decoder);
    register_avcodec(&cook_decoder);
    register_avcodec(&truespeech_decoder);
    register_avcodec(&tta_decoder);
}
#endif

void init_once_routine(void) {
  pthread_mutex_init(&ffmpeg_lock, NULL);
  avcodec_init();
  avcodec_register_all();
}

/*
 * exported plugin catalog entry
 */

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_VIDEO_DECODER | PLUGIN_MUST_PRELOAD, 18, "ffmpegvideo", XINE_VERSION_CODE, &dec_info_ffmpeg_video, init_video_plugin },
  { PLUGIN_VIDEO_DECODER, 18, "ffmpeg-wmv8", XINE_VERSION_CODE, &dec_info_ffmpeg_wmv8, init_video_plugin },
  { PLUGIN_VIDEO_DECODER, 18, "ffmpeg-wmv9", XINE_VERSION_CODE, &dec_info_ffmpeg_wmv9, init_video_plugin },
  { PLUGIN_AUDIO_DECODER, 15, "ffmpegaudio", XINE_VERSION_CODE, &dec_info_ffmpeg_audio, init_audio_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
