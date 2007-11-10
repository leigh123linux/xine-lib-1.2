
#ifndef __POST_VDR_H
#define __POST_VDR_H



typedef struct vdr_set_video_window_data_s {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  int32_t w_ref;
  int32_t h_ref;

} vdr_set_video_window_data_t;



typedef struct vdr_frame_size_changed_data_s {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
  double r;

} vdr_frame_size_changed_data_t;



typedef struct vdr_select_audio_data_s {
  uint8_t channels;

} vdr_select_audio_data_t;



inline static int vdr_is_vdr_stream(xine_stream_t *stream)
{
  if (!stream
      || !stream->input_plugin
      || !stream->input_plugin->input_class)
  {
    return 0;
  }

  {
    input_class_t *input_class = stream->input_plugin->input_class;

    if (input_class->get_identifier)
    {
      const char *identifier = input_class->get_identifier(input_class);
      if (identifier
          && 0 == strcmp(identifier, "VDR"))
      {
        return 1;
      }
    }
  }

  return 0;
}



/* plugin class initialization function */
void *vdr_video_init_plugin(xine_t *xine, void *);
void *vdr_audio_init_plugin(xine_t *xine, void *);



#endif /* __POST_VDR_H */

