/* 
 * Copyright 2025 Tria Technologies Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _GST_MARKERDETECT_H_
#define _GST_MARKERDETECT_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_MARKERDETECT   (gst_markerdetect_get_type())
#define GST_MARKERDETECT(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MARKERDETECT,GstMarkerDetect))
#define GST_MARKERDETECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MARKERDETECT,GstMarkerDetectClass))
#define GST_IS_MARKERDETECT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MARKERDETECT))
#define GST_IS_MARKERDETECT_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MARKERDETECT))

typedef struct _GstMarkerDetect GstMarkerDetect;
typedef struct _GstMarkerDetectClass GstMarkerDetectClass;

struct _GstMarkerDetect
{
  GstVideoFilter base_markerdetect;

  unsigned iterations;

  gchar *cc_script;
  gchar *cc_extra_args;
  unsigned cc_skip_frames;
  unsigned cc_frame_count; 
  
  gchar *wb_script;
  gchar *wb_extra_args;
  unsigned wb_skip_frames;
  unsigned wb_frame_count; 
};

struct _GstMarkerDetectClass
{
  GstVideoFilterClass base_markerdetect_class;
};

GType gst_markerdetect_get_type (void);

G_END_DECLS

#endif
