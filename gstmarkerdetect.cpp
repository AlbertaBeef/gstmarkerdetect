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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "gstmarkerdetect.h"

/* OpenCV header files */
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

/* Aruco Markers */
#include <opencv2/aruco.hpp>

GST_DEBUG_CATEGORY_STATIC (gst_markerdetect_debug_category);
#define GST_CAT_DEFAULT gst_markerdetect_debug_category

/* prototypes */


static void gst_markerdetect_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_markerdetect_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_markerdetect_dispose (GObject * object);
static void gst_markerdetect_finalize (GObject * object);

static gboolean gst_markerdetect_start (GstBaseTransform * trans);
static gboolean gst_markerdetect_stop (GstBaseTransform * trans);
static gboolean gst_markerdetect_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info);
static GstFlowReturn gst_markerdetect_transform_frame (GstVideoFilter * filter,
    GstVideoFrame * inframe, GstVideoFrame * outframe);
static GstFlowReturn gst_markerdetect_transform_frame_ip (GstVideoFilter * filter,
    GstVideoFrame * frame);

enum
{
  PROP_0,
  PROP_CC_SCRIPT,
  PROP_CC_EXTRA_ARGS,
  PROP_CC_SKIP_FRAMES,
  PROP_CC_SHOW_GT, // ground truth colors
  PROP_CC_SHOW_EC, // error color code (GnYlRd)
  PROP_WB_SCRIPT,
  PROP_WB_EXTRA_ARGS,
  PROP_WB_SKIP_FRAMES
};

/* pad templates */

/* Input format */
#define VIDEO_SRC_CAPS \
    GST_VIDEO_CAPS_MAKE("{ BGR }")

/* Output format */
#define VIDEO_SINK_CAPS \
    GST_VIDEO_CAPS_MAKE("{ BGR }")


/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstMarkerDetect, gst_markerdetect, GST_TYPE_VIDEO_FILTER,
  GST_DEBUG_CATEGORY_INIT (gst_markerdetect_debug_category, "markerdetect", 0,
  "debug category for markerdetect element"));

static void
gst_markerdetect_class_init (GstMarkerDetectClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS (klass);
  GstVideoFilterClass *video_filter_class = GST_VIDEO_FILTER_CLASS (klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
    gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      //gst_caps_from_string (VIDEO_SRC_CAPS ",width = (int) [1, 1920], height = (int) [1, 1080]")));
      gst_caps_from_string (VIDEO_SRC_CAPS ",width = (int) [1, 3840], height = (int) [1, 2160]")));
  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass),
    gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      //gst_caps_from_string (VIDEO_SINK_CAPS ", width = (int) [1, 1920], height = (int) [1, 1080]")));
      gst_caps_from_string (VIDEO_SINK_CAPS ", width = (int) [1, 3840], height = (int) [1, 2160]")));

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
    "Marker detection using the OpenCV Library", 
    "Video Filter", 
    "Marker Detection",
    "AlbertaBeef <grouby177@gmail.com>");

  gobject_class->set_property = gst_markerdetect_set_property;
  gobject_class->get_property = gst_markerdetect_get_property;

  g_object_class_install_property (gobject_class, PROP_CC_SCRIPT,
      g_param_spec_string ("cc-script", "cc-script",
          "Color Checker script.", 
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_CC_EXTRA_ARGS,
      g_param_spec_string ("cc-extra-args", "cc-extra-args",
          "Extra arguments for Color Checker script.", 
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  
  g_object_class_install_property (gobject_class, PROP_CC_SKIP_FRAMES,
      g_param_spec_int ("cc-skip-frames", "cc-skip-frames",
          "Color Checker skip frames.", 0, G_MAXINT,
          0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_CC_SHOW_GT,
      g_param_spec_boolean ("cc-show-gt", "cc-show-gt",
          "Color Checker show grounth truth (on right half of color patch).",
          FALSE, 
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_CC_SHOW_EC,
      g_param_spec_boolean ("cc-show-ec", "cc-show-ec",
          "Color Checker show error color (on a GrYlRd scale for values 0-59).",
          FALSE, 
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_WB_SCRIPT,
      g_param_spec_string ("wb-script", "wb-script",
          "White Balance script.", 
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_WB_EXTRA_ARGS,
      g_param_spec_string ("wb-extra-args", "wb-extra-args",
          "Extra arguments for White Balance script.", 
          NULL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_WB_SKIP_FRAMES,
      g_param_spec_int ("wb-skip-frames", "wb-skip-frames",
          "White Balance skip frames.", 0, G_MAXINT,
          0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
            
  gobject_class->dispose = gst_markerdetect_dispose;
  gobject_class->finalize = gst_markerdetect_finalize;
  base_transform_class->start = GST_DEBUG_FUNCPTR (gst_markerdetect_start);
  base_transform_class->stop = GST_DEBUG_FUNCPTR (gst_markerdetect_stop);
  video_filter_class->set_info = GST_DEBUG_FUNCPTR (gst_markerdetect_set_info);
  video_filter_class->transform_frame_ip = GST_DEBUG_FUNCPTR (gst_markerdetect_transform_frame_ip);

}

static void
gst_markerdetect_init (GstMarkerDetect *markerdetect)
{
   markerdetect->iterations = 0;

   markerdetect->cc_script = NULL;
   markerdetect->cc_extra_args = NULL;
   markerdetect->cc_skip_frames = 0;
   markerdetect->cc_frame_count = 0;
   markerdetect->cc_show_gt = FALSE;
   markerdetect->cc_show_ec = FALSE;

   markerdetect->wb_script = NULL;
   markerdetect->wb_extra_args = NULL;
   markerdetect->wb_skip_frames = 0;
   markerdetect->wb_frame_count = 0;
}

void
gst_markerdetect_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (object);

  GST_DEBUG_OBJECT (markerdetect, "set_property");

  switch (property_id) {
    case PROP_CC_SCRIPT:
      g_free (markerdetect->cc_script);
      markerdetect->cc_script = g_value_dup_string (value);
      break;
    case PROP_CC_EXTRA_ARGS:
      g_free (markerdetect->cc_extra_args);
      markerdetect->cc_extra_args = g_value_dup_string (value);
      break;
    case PROP_CC_SKIP_FRAMES:
      markerdetect->cc_skip_frames = g_value_get_int (value);
      break;
    case PROP_CC_SHOW_GT:
      markerdetect->cc_show_gt = g_value_get_boolean (value);
      break;
    case PROP_CC_SHOW_EC:
      markerdetect->cc_show_ec = g_value_get_boolean (value);
      break;
    case PROP_WB_SCRIPT:
      g_free (markerdetect->wb_script);
      markerdetect->wb_script = g_value_dup_string (value);
      break;
    case PROP_WB_EXTRA_ARGS:
      g_free (markerdetect->wb_extra_args);
      markerdetect->wb_extra_args = g_value_dup_string (value);
      break;
    case PROP_WB_SKIP_FRAMES:
      markerdetect->wb_skip_frames = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_markerdetect_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (object);

  GST_DEBUG_OBJECT (markerdetect, "get_property");

  switch (property_id) {
    case PROP_CC_SCRIPT:
      g_value_set_string (value, markerdetect->cc_script);
      break;
    case PROP_CC_EXTRA_ARGS:
      g_value_set_string (value, markerdetect->cc_extra_args);
      break;
    case PROP_CC_SKIP_FRAMES:
      g_value_set_int (value, markerdetect->cc_skip_frames);
      break;      
    case PROP_CC_SHOW_GT:
      g_value_set_boolean (value, markerdetect->cc_show_gt);
      break;      
    case PROP_CC_SHOW_EC:
      g_value_set_boolean (value, markerdetect->cc_show_ec);
      break;      
    case PROP_WB_SCRIPT:
      g_value_set_string (value, markerdetect->wb_script);
      break;
    case PROP_WB_EXTRA_ARGS:
      g_value_set_string (value, markerdetect->wb_extra_args);
      break;
    case PROP_WB_SKIP_FRAMES:
      g_value_set_int (value, markerdetect->wb_skip_frames);
      break;      
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_markerdetect_dispose (GObject * object)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (object);

  GST_DEBUG_OBJECT (markerdetect, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_markerdetect_parent_class)->dispose (object);
}

void
gst_markerdetect_finalize (GObject * object)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (object);

  GST_DEBUG_OBJECT (markerdetect, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_markerdetect_parent_class)->finalize (object);
}

static gboolean
gst_markerdetect_start (GstBaseTransform * trans)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (trans);

  GST_DEBUG_OBJECT (markerdetect, "start");

  return TRUE;
}

static gboolean
gst_markerdetect_stop (GstBaseTransform * trans)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (trans);

  GST_DEBUG_OBJECT (markerdetect, "stop");

  return TRUE;
}

static gboolean
gst_markerdetect_set_info (GstVideoFilter * filter, GstCaps * incaps,
    GstVideoInfo * in_info, GstCaps * outcaps, GstVideoInfo * out_info)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (filter);

  GST_DEBUG_OBJECT (markerdetect, "set_info");

  return TRUE;
}

/* transform */
static GstFlowReturn
gst_markerdetect_transform_frame (GstVideoFilter * filter, GstVideoFrame * inframe,
    GstVideoFrame * outframe)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (filter);

  GST_DEBUG_OBJECT (markerdetect, "transform_frame");

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_markerdetect_transform_frame_ip (GstVideoFilter * filter, GstVideoFrame * frame)
{
  GstMarkerDetect *markerdetect = GST_MARKERDETECT (filter);

  markerdetect->iterations++;
  markerdetect->cc_frame_count++;
  markerdetect->wb_frame_count++;

  /* Setup an OpenCV Mat with the frame data */
  int width = GST_VIDEO_FRAME_WIDTH(frame);
  int height = GST_VIDEO_FRAME_HEIGHT(frame);
  cv::Mat img(height, width, CV_8UC3, GST_VIDEO_FRAME_PLANE_DATA(frame, 0));

  //
  // Detect ARUCO markers
  //   ref : https://docs.opencv.org/master/d5/dae/tutorial_aruco_detection.html
  //
  
  std::vector<int> markerIds;
  std::vector<std::vector<cv::Point2f>> markerCorners, rejectedCandidates;
  cv::Ptr<cv::aruco::DetectorParameters> parameters = cv::aruco::DetectorParameters::create();
  cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_ARUCO_ORIGINAL);
  cv::aruco::detectMarkers(img, dictionary, markerCorners, markerIds, parameters, rejectedCandidates);

  if ( markerIds.size() > 0 )
  {
    cv::aruco::drawDetectedMarkers(img, markerCorners, markerIds);
  }
  
  if (markerIds.size() >= 4 )
  {
    int tl_id = 0;
    int tr_id = 0;
    int bl_id = 0;
    int br_id = 0;
    cv::Point2f tl_xy, tr_xy, bl_xy, br_xy;
    for ( unsigned i = 0; i < markerIds.size(); i++ )
    {
      switch ( markerIds[i] )
      {
      case 923:
        tl_id = markerIds[i];
        //tl_xy = markerCorners[i][2]; // bottom right corner of top left marker
        tl_xy = markerCorners[i][3]; // bottom left corner of top left marker
        break;
      case 1001:
      case 1002:
      case 1003:
      case 1004:
      case 1005:
      case 1006:
        tr_id = markerIds[i];
        //tr_xy = markerCorners[i][3]; // bottom left corner of top right marker
        tr_xy = markerCorners[i][2]; // bottom right corner of top right marker
        break;
      case 1007:
        bl_id = markerIds[i];
        //bl_xy = markerCorners[i][1]; // top right corner of bottom left marker
        bl_xy = markerCorners[i][0]; // top left corner of bottom left marker
        break;
      case 241:
        br_id = markerIds[i];
        //br_xy = markerCorners[i][0]; // top left corner of bottom right marker
        br_xy = markerCorners[i][1]; // top right corner of bottom right marker
        break;
      default:
        break;
      }
    }
    // Chart 1 - Color Checker CLASSIC
    if ( (tl_id==923) && (tr_id==1001) && (bl_id==1007) && (br_id==241) )
    {
      // Reference coordinates manually taken from 608x512 image (ROI from ArUco markers)
      std::vector<cv::Point2f> arucoCornersRef = {
        {  0,  0}, {607,  0}, {607,511}, {  0,511}
      };
      std::vector<cv::Point2f> chartCornersRef = {
        {  0, 57}, {607, 57}, {607,455}, {  0,455}
      };
      std::vector<cv::Point2f> chartCentroidsRef = {
        { 46,103}, {150,103}, {252,103}, {355,103}, {458,103}, {561,103},
        { 46,205}, {150,205}, {252,205}, {355,205}, {458,205}, {561,205},
        { 46,307}, {150,307}, {252,307}, {355,307}, {458,307}, {561,307},
        { 46,409}, {150,409}, {252,409}, {355,409}, {458,409}, {561,409}
      };
      // Reference width/height of color patches is approximately 89/88, so take safe subset of this
      float colorPatchFullWidth = 88.0;
      float colorPatchFullHeight = 88.0;
      float colorPatchWidth = 50.0;
      float colorPatchHeight = 50.0;
      
      // Ground Truth BGR values for 24 Color Patches
      //std::vector<std::array<float, 3>> chartColorsRef =
      std::vector<cv::Scalar> chartColorsRef =
      { 
      // Dark Skin      Light Skin     Blue Sky       Foliage        Blue Flower    Bluish Green   
        { 68, 82,115}, {130,150,192}, {157,122, 98}, { 67,108, 87}, {177,128,133}, {170,189,103},
      // Orange         Purple Red     Moderate Red   Purple         Yellow Green   Orange Yello
        { 44,126,214}, {166, 91, 80}, { 99, 90,193}, {108, 60, 94}, { 64,188,157}, { 46,163,224},
      // Blue           Green          Red            Yellow         Magenta        Cyan
        {150, 61, 56}, { 73,148, 70}, { 60, 54,175}, { 31,199,231}, {149, 86,187}, {161,133,  8},
      // White          Neutral 8      Neutral 65     Neutral 5      Neutral 35     Black
        {242,243,243}, {200,200,200}, {160,160,160}, {121,122,122}, { 85, 85, 85}, { 52, 52, 52}
      };
      
      // BGR values for GrYlRd colormap
      // (generated with colormap_GrYlRd.py)
      std::vector<cv::Scalar> colormap_GrYlRd =
      {
         {   58 ,  111 ,    4  },
         {   62 ,  119 ,    8  },
         {   66 ,  126 ,   12  },
         {   71 ,  136 ,   17  },
         {   75 ,  143 ,   21  },
         {   79 ,  151 ,   25  },
         {   82 ,  157 ,   36  },
         {   86 ,  164 ,   51  },
         {   89 ,  170 ,   63  },
         {   92 ,  175 ,   75  },
         {   95 ,  181 ,   87  },
         {   99 ,  189 ,  102  },
         {  100 ,  193 ,  112  },
         {  101 ,  197 ,  122  },
         {  102 ,  202 ,  132  },
         {  103 ,  207 ,  144  },
         {  104 ,  212 ,  154  },
         {  105 ,  216 ,  164  },
         {  111 ,  220 ,  175  },
         {  117 ,  224 ,  183  },
         {  122 ,  227 ,  191  },
         {  127 ,  231 ,  199  },
         {  133 ,  235 ,  209  },
         {  139 ,  239 ,  217  },
         {  147 ,  241 ,  222  },
         {  155 ,  244 ,  228  },
         {  165 ,  247 ,  236  },
         {  173 ,  249 ,  242  },
         {  181 ,  252 ,  248  },
         {  189 ,  254 ,  254  },
         {  181 ,  249 ,  254  },
         {  173 ,  244 ,  254  },
         {  165 ,  239 ,  254  },
         {  155 ,  233 ,  254  },
         {  147 ,  228 ,  254  },
         {  139 ,  224 ,  254  },
         {  132 ,  216 ,  253  },
         {  124 ,  206 ,  253  },
         {  117 ,  198 ,  253  },
         {  110 ,  190 ,  253  },
         {  104 ,  182 ,  253  },
         {   96 ,  172 ,  252  },
         {   91 ,  162 ,  251  },
         {   86 ,  152 ,  250  },
         {   82 ,  142 ,  248  },
         {   76 ,  129 ,  246  },
         {   71 ,  119 ,  245  },
         {   67 ,  109 ,  244  },
         {   61 ,   97 ,  238  },
         {   57 ,   87 ,  233  },
         {   52 ,   77 ,  229  },
         {   48 ,   68 ,  224  },
         {   42 ,   56 ,  218  },
         {   38 ,   47 ,  214  },
         {   38 ,   39 ,  206  },
         {   38 ,   32 ,  198  },
         {   38 ,   22 ,  188  },
         {   38 ,   15 ,  180  },
         {   38 ,    7 ,  172  },
         {   38 ,    0 ,  165  }
      };
      unsigned colormap_size = colormap_GrYlRd.size();
      //printf("[INFO] colormap_size = %d\n\r",colormap_size);
      
      // Calculate transformation matrix based on ROI defined by ArUco markers
      std::vector<cv::Point2f> srcPoints;
      std::vector<cv::Point2f> dstPoints;
      srcPoints.push_back(arucoCornersRef[0]);
      srcPoints.push_back(arucoCornersRef[1]);
      srcPoints.push_back(arucoCornersRef[2]);
      srcPoints.push_back(arucoCornersRef[3]);      
      dstPoints.push_back(tl_xy);
      dstPoints.push_back(tr_xy);
      dstPoints.push_back(br_xy);
      dstPoints.push_back(bl_xy);
      cv::Mat warpMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);

      // Calculate real coordinates for corners and color patch centroids      
      std::vector<cv::Point2f> chartCorners;
      std::vector<cv::Point2f> chartCentroids;
      cv::perspectiveTransform(chartCornersRef, chartCorners, warpMatrix);
      cv::perspectiveTransform(chartCentroidsRef, chartCentroids, warpMatrix);

      std::vector<cv::Mat> bgr_planes;
      cv::split( img, bgr_planes );

      // Create string of bgr values for each color patch
      std::stringstream color_patch_bgr_values;
      color_patch_bgr_values << "";

      // Cumulate color patch errors
      float chartErrorBGR = 0.0;
      float chartErrorB = 0.0;
      float chartErrorG = 0.0;
      float chartErrorR = 0.0;

      // YUV Color Space
      float chartErrorYUV = 0.0;
      float chartErrorY = 0.0;
      float chartErrorU = 0.0;
      float chartErrorV = 0.0;

      // LAB Color Space
      float chartErrorLAB = 0.0;
      float chartErrorL = 0.0;
      float chartErrorA = 0.0;
      float chartErrorBB = 0.0;

      // HSV Color Space
      float chartErrorHSV = 0.0;
      float chartErrorH = 0.0;
      float chartErrorS = 0.0;
      float chartErrorVV = 0.0;

      // XYZ Color Space
      float chartErrorXYZ = 0.0;
      float chartErrorX = 0.0;
      float chartErrorYY = 0.0;
      float chartErrorZ = 0.0;
      
      for ( int i = 0; i < 24; i++ )
      {
        // Define corner points for each color patch
        std::vector<cv::Point2f> patchCornersRef;
        std::vector<cv::Point2f> patchCorners;
        patchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x-(colorPatchWidth/2),chartCentroidsRef[i].y-(colorPatchHeight/2)));
        patchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x+(colorPatchWidth/2),chartCentroidsRef[i].y-(colorPatchHeight/2)));
        patchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x+(colorPatchWidth/2),chartCentroidsRef[i].y+(colorPatchHeight/2)));  
        patchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x-(colorPatchWidth/2),chartCentroidsRef[i].y+(colorPatchHeight/2))); 
        cv::perspectiveTransform(patchCornersRef, patchCorners, warpMatrix);
        
        //
        // Calculate color gains
        //   ref : https://stackoverflow.com/questions/32466616/finding-the-average-color-within-a-polygon-bound-in-opencv
        //

        // Create mask for color patch
        cv::Point pts[1][4];
        pts[0][0] = cv::Point(patchCorners[0].x,patchCorners[0].y);
        pts[0][1] = cv::Point(patchCorners[1].x,patchCorners[1].y);
        pts[0][2] = cv::Point(patchCorners[2].x,patchCorners[2].y);
        pts[0][3] = cv::Point(patchCorners[3].x,patchCorners[3].y);
        const cv::Point* points[1] = {pts[0]};
        cv::Mat1b mask(img.rows, img.cols, uchar(0));
        int npoints = 4;
        cv::fillPoly(mask, points, &npoints, 1, cv::Scalar(255));
        
        // Calculate mean in masked area
        //auto bgr_mean = cv::mean( img, mask );
        cv::Scalar bgr_mean1 = cv::mean( img, mask );
        float b_mean = bgr_mean1(0);
        float g_mean = bgr_mean1(1);
        float r_mean = bgr_mean1(2);
        float b_error = chartColorsRef[i][0]-b_mean;
        float g_error = chartColorsRef[i][1]-g_mean;
        float r_error = chartColorsRef[i][2]-r_mean;

        // Create string of bgr values for each color patch
        color_patch_bgr_values << int(b_mean) << " " << int(g_mean) << " " << int(r_mean) << " ";
        
        // Cumulate color patch errors
        float patchErrorBGR = std::sqrt(pow(b_error,2) + pow(g_error,2) + pow(r_error,2));
        chartErrorBGR += patchErrorBGR;
        chartErrorB += abs(b_error);
        chartErrorG += abs(g_error);
        chartErrorR += abs(r_error);

        // Explore different color spaces        
        cv::Mat3f bgr_mean2(cv::Vec3f(b_mean, g_mean, r_mean));
        cv::Mat3f bgr_patch(cv::Vec3f(chartColorsRef[i][0],chartColorsRef[i][1],chartColorsRef[i][2]));

        // YUV Color Space
        cv::Mat3f yuv_mean;
        cv::Mat3f yuv_patch;
        cv::cvtColor(bgr_mean2, yuv_mean, cv::COLOR_BGR2YUV);
        cv::cvtColor(bgr_patch, yuv_patch, cv::COLOR_BGR2YUV);
        cv::Vec3f yuv_mean_pixel = yuv_mean[0][0];
        cv::Vec3f yuv_patch_pixel = yuv_patch[0][0];
        float y_error = yuv_patch_pixel[0]-yuv_mean_pixel[0];
        float u_error = yuv_patch_pixel[1]-yuv_mean_pixel[1];
        float v_error = yuv_patch_pixel[2]-yuv_mean_pixel[2];        
        // Cumulate color patch errors
        //float patchErrorYUV = std::sqrt(pow(y_error,2) + pow(u_error,2) + pow(v_error,2));
        float patchErrorYUV = std::sqrt(pow(u_error,2) + pow(v_error,2));
        chartErrorYUV += patchErrorYUV;
        chartErrorY += abs(y_error);
        chartErrorU += abs(u_error);
        chartErrorV += abs(v_error);

        // LAB Color Space
        cv::Mat3f lab_mean;
        cv::Mat3f lab_patch;
        cv::cvtColor(bgr_mean2, lab_mean, cv::COLOR_BGR2Lab);
        cv::cvtColor(bgr_patch, lab_patch, cv::COLOR_BGR2Lab);
        cv::Vec3f lab_mean_pixel = lab_mean[0][0];
        cv::Vec3f lab_patch_pixel = lab_patch[0][0];
        float l_error = lab_patch_pixel[0]-lab_mean_pixel[0];
        float a_error = lab_patch_pixel[1]-lab_mean_pixel[1];
        float bb_error = lab_patch_pixel[2]-lab_mean_pixel[2];        
        // Cumulate color patch errors
        float patchErrorLAB = std::sqrt(pow(l_error,2) + pow(a_error,2) + pow(bb_error,2));
        chartErrorLAB += patchErrorLAB;
        chartErrorL += abs(l_error);
        chartErrorA += abs(a_error);
        chartErrorBB += abs(bb_error);

        // HSV Color Space
        cv::Mat3f hsv_mean;
        cv::Mat3f hsv_patch;
        cv::cvtColor(bgr_mean2, hsv_mean, cv::COLOR_BGR2HSV);
        cv::cvtColor(bgr_patch, hsv_patch, cv::COLOR_BGR2HSV);
        cv::Vec3f hsv_mean_pixel = hsv_mean[0][0];
        cv::Vec3f hsv_patch_pixel = hsv_patch[0][0];
        float h_error = hsv_patch_pixel[0]-hsv_mean_pixel[0];
        float s_error = hsv_patch_pixel[1]-hsv_mean_pixel[1];
        float vv_error = hsv_patch_pixel[2]-hsv_mean_pixel[2];        
        // Cumulate color patch errors
        float patchErrorHSV = std::sqrt(pow(h_error,2) + pow(s_error,2) + pow(vv_error,2));
        chartErrorHSV += patchErrorHSV;
        chartErrorH += abs(h_error);
        chartErrorS += abs(s_error);
        chartErrorVV += abs(vv_error);

        // XYZ Color Space
        cv::Mat3f xyz_mean;
        cv::Mat3f xyz_patch;
        cv::cvtColor(bgr_mean2, xyz_mean, cv::COLOR_BGR2XYZ);
        cv::cvtColor(bgr_patch, xyz_patch, cv::COLOR_BGR2XYZ);
        cv::Vec3f xyz_mean_pixel = xyz_mean[0][0];
        cv::Vec3f xyz_patch_pixel = xyz_patch[0][0];
        float x_error = xyz_patch_pixel[0]-xyz_mean_pixel[0];
        float yy_error = xyz_patch_pixel[1]-xyz_mean_pixel[1];
        float z_error = xyz_patch_pixel[2]-xyz_mean_pixel[2];        
        // Cumulate color patch errors
        float patchErrorXYZ = std::sqrt(pow(x_error,2) + pow(yy_error,2) + pow(z_error,2));
        chartErrorXYZ += patchErrorXYZ;
        chartErrorX += abs(x_error);
        chartErrorYY += abs(yy_error);
        chartErrorZ += abs(z_error);
      
#if 0
        //
        // This all seemed like a good idea, but the graphs are too small to be of any use
        //
        
        // Draw bars of deltas between measured and ground truth
        int plot_w = 100, plot_h = 100;
        cv::Mat plotImage( plot_h, plot_w, CV_8UC3, cv::Scalar(255,255,255) );
  #if 0
        // Use these lines to display BGR average values
        float b_value = b_mean;
        float g_value = g_mean;
        float r_value = r_mean;
  #else
        // Use these lines to display BGR error (with ground truth)
        float b_value = b_error;
        float g_value = g_error;
        float r_value = r_error;
  #endif
        int b_bar = int((abs(b_value)/256.0)*80.0);
        int g_bar = int((abs(g_value)/256.0)*80.0);
        int r_bar = int((abs(r_value)/256.0)*80.0);
        // layout of bars : |<-10->|<---20-->|<-10->|<---20-->|<-10->|<---20-->|<-10->|
        cv::rectangle(plotImage, cv::Rect(10,(80-b_bar),20,b_bar), cv::Scalar(255, 0, 0), cv::FILLED, cv::LINE_8);
        cv::rectangle(plotImage, cv::Rect(40,(80-g_bar),20,g_bar), cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_8);
        cv::rectangle(plotImage, cv::Rect(70,(80-r_bar),20,r_bar), cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_8);
        //printf( "Stats : BGR=%5.3f,%5.3f,%5.3f (%d,%d,%d) => Kbgr=%5.3f,%5.3f,%5.3f\n", b_mean, g_mean, r_mean, b_bar, g_bar, r_bar, Kb, Kg, Kr );
        std::stringstream b_str;
        std::stringstream g_str;
        std::stringstream r_str;
        b_str << int(b_value);
        g_str << int(g_value);
        r_str << int(r_value);
        cv::putText(plotImage, b_str.str(), cv::Point(10,90), cv::FONT_HERSHEY_PLAIN, 0.75, cv::Scalar(255,0,0), 1, cv::LINE_AA);
        cv::putText(plotImage, g_str.str(), cv::Point(40,90), cv::FONT_HERSHEY_PLAIN, 0.75, cv::Scalar(0,255,0), 1, cv::LINE_AA);
        cv::putText(plotImage, r_str.str(), cv::Point(70,90), cv::FONT_HERSHEY_PLAIN, 0.75, cv::Scalar(0,0,255), 1, cv::LINE_AA);
  
        // Calculate transformation matrix
        std::vector<cv::Point2f> srcPoints;
        std::vector<cv::Point2f> dstPoints;
        srcPoints.push_back(cv::Point(       0,       0)); // top left
        srcPoints.push_back(cv::Point(plot_w-1,       0)); // top right
        srcPoints.push_back(cv::Point(plot_w-1,plot_h-1)); // bottom right
        srcPoints.push_back(cv::Point(       0,plot_h-1)); // bottom left
        dstPoints.push_back(patchCorners[0]);
        dstPoints.push_back(patchCorners[1]);
        dstPoints.push_back(patchCorners[2]);
        dstPoints.push_back(patchCorners[3]);
        cv::Mat h = cv::findHomography(srcPoints,dstPoints);
        // Warp plot image onto video frame
        cv::Mat img_temp = img.clone();
        cv::warpPerspective(plotImage, img_temp, h, img_temp.size());
        cv::Point pts_dst[4];
        for( int i = 0; i < 4; i++)
        {
          pts_dst[i] = dstPoints[i];
        }
        cv::fillConvexPoly(img, pts_dst, 4, cv::Scalar(0), cv::LINE_AA);
        img = img + img_temp;
#endif        

        if ( markerdetect->cc_show_gt == TRUE )
        {
          // Overlay ground truth on right half of color patch (for visual comparison)
          std::vector<cv::Point2f> halfPatchCornersRef;
          std::vector<cv::Point2f> halfPatchCorners;
          halfPatchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x                        ,chartCentroidsRef[i].y-(colorPatchFullHeight/2)));
          halfPatchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x+(colorPatchFullWidth/2),chartCentroidsRef[i].y-(colorPatchFullHeight/2)));
          halfPatchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x+(colorPatchFullWidth/2),chartCentroidsRef[i].y+(colorPatchFullHeight/2)));  
          halfPatchCornersRef.push_back(cv::Point2f(chartCentroidsRef[i].x                        ,chartCentroidsRef[i].y+(colorPatchFullHeight/2))); 
          cv::perspectiveTransform(halfPatchCornersRef, halfPatchCorners, warpMatrix);
          std::vector<cv::Point> halfPatchCornersFixpt;
          halfPatchCornersFixpt.push_back( cv::Point(halfPatchCorners[0].x,halfPatchCorners[0].y) );
          halfPatchCornersFixpt.push_back( cv::Point(halfPatchCorners[1].x,halfPatchCorners[1].y) );
          halfPatchCornersFixpt.push_back( cv::Point(halfPatchCorners[2].x,halfPatchCorners[2].y) );
          halfPatchCornersFixpt.push_back( cv::Point(halfPatchCorners[3].x,halfPatchCorners[3].y) );
          cv::fillPoly(img, halfPatchCornersFixpt, chartColorsRef[i]);        
        }
        if ( markerdetect->cc_show_ec == TRUE )
        {
          // Overlay correctness score in ROI region        
          std::vector<cv::Point> patchCornersFixpt;
          patchCornersFixpt.push_back( cv::Point(patchCorners[0].x,patchCorners[0].y) );
          patchCornersFixpt.push_back( cv::Point(patchCorners[1].x,patchCorners[1].y) );
          patchCornersFixpt.push_back( cv::Point(patchCorners[2].x,patchCorners[2].y) );
          patchCornersFixpt.push_back( cv::Point(patchCorners[3].x,patchCorners[3].y) );
          unsigned colormap_index = (unsigned)patchErrorYUV;
          if (colormap_index >= colormap_size) colormap_index = colormap_size-1;
          cv::fillPoly(img, patchCornersFixpt, colormap_GrYlRd[colormap_index]);
          std::stringstream e_str;        
          e_str << unsigned(patchErrorYUV);
          cv::putText(img, e_str.str(), cv::Point(patchCornersFixpt[3].x+5,patchCornersFixpt[3].y-5), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
        }
        else
        {
          // Draw Color Patch ROI
          std::vector<cv::Point> patchCornersFixpt;
          patchCornersFixpt.push_back( cv::Point(patchCorners[0].x,patchCorners[0].y) );
          patchCornersFixpt.push_back( cv::Point(patchCorners[1].x,patchCorners[1].y) );
          patchCornersFixpt.push_back( cv::Point(patchCorners[2].x,patchCorners[2].y) );
          patchCornersFixpt.push_back( cv::Point(patchCorners[3].x,patchCorners[3].y) );
          cv::polylines(img, patchCornersFixpt, true, cv::Scalar (163, 0, 255), 2, 16);
          std::stringstream e_str;
          //e_str << "E=" << unsigned(patchErrorBGR) << "|" << unsigned(patchErrorYUV);
          //e_str << "E[BGR|YUV]=" << unsigned(patchErrorBGR) << "|" << unsigned(patchErrorYUV);
          //e_str << " E[B]=" << int(b_error) << " E[G]=" << int(g_error)  << " E[R]=" << int(r_error); 
          //e_str << unsigned(patchErrorBGR) << "|" << unsigned(patchErrorYUV);
          e_str << "E[UV]" << unsigned(patchErrorYUV);
          cv::putText(img, e_str.str(), patchCornersFixpt[0], cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);        
          cv::putText(img, e_str.str(), patchCornersFixpt[3], cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255,255,255), 1, cv::LINE_AA);
        }
        
      }
      
      // BGR color space
      unsigned int y_offset = 20;
      std::stringstream e_str, eb_str, eg_str, er_str;
      e_str << "E[BGR]=" << unsigned(chartErrorBGR);
      cv::putText(img, e_str.str(), cv::Point(10,y_offset+20), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(99,31,0), 1, cv::LINE_AA);
      eb_str << " E[B]=" << unsigned(chartErrorB);
      cv::putText(img, eb_str.str(), cv::Point(10,y_offset+40), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255,0,0), 1, cv::LINE_AA);
      eg_str << " E[G]=" << unsigned(chartErrorG);
      cv::putText(img, eg_str.str(), cv::Point(10,y_offset+60), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,255,0), 1, cv::LINE_AA);
      er_str << " E[R]=" << unsigned(chartErrorR);
      cv::putText(img, er_str.str(), cv::Point(10,y_offset+80), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,255), 1, cv::LINE_AA);

      // YUV color space
      y_offset += 100;
      std::stringstream eyuv_str, ey_str, eu_str, ev_str;
      eyuv_str << "E[UV]=" << unsigned(chartErrorYUV);
      cv::putText(img, eyuv_str.str(), cv::Point(10,y_offset+20), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(99,31,0), 1, cv::LINE_AA);
      ey_str << " E[Y]=" << unsigned(chartErrorY);
      cv::putText(img, ey_str.str(), cv::Point(10,y_offset+40), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      eu_str << " E[U]=" << unsigned(chartErrorU);
      cv::putText(img, eu_str.str(), cv::Point(10,y_offset+60), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      ev_str << " E[V]=" << unsigned(chartErrorV);
      cv::putText(img, ev_str.str(), cv::Point(10,y_offset+80), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);

      // LAB color space
      y_offset += 100;
      std::stringstream elab_str, el_str, ea_str, ebb_str;
      elab_str << "E[LAB]=" << unsigned(chartErrorLAB);
      cv::putText(img, elab_str.str(), cv::Point(10,y_offset+20), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(99,31,0), 1, cv::LINE_AA);
      el_str << " E[L]=" << unsigned(chartErrorL);
      cv::putText(img, el_str.str(), cv::Point(10,y_offset+40), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      ea_str << " E[A]=" << unsigned(chartErrorA);
      cv::putText(img, ea_str.str(), cv::Point(10,y_offset+60), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      ebb_str << " E[B]=" << unsigned(chartErrorBB);
      cv::putText(img, ebb_str.str(), cv::Point(10,y_offset+80), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);

      // HSV color space
      y_offset += 100;
      std::stringstream ehsv_str, eh_str, es_str, evv_str;
      ehsv_str << "E[HSV]=" << unsigned(chartErrorHSV);
      cv::putText(img, ehsv_str.str(), cv::Point(10,y_offset+20), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(99,31,0), 1, cv::LINE_AA);
      eh_str << " E[H]=" << unsigned(chartErrorH);
      cv::putText(img, eh_str.str(), cv::Point(10,y_offset+40), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      es_str << " E[S]=" << unsigned(chartErrorS);
      cv::putText(img, es_str.str(), cv::Point(10,y_offset+60), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      evv_str << " E[V]=" << unsigned(chartErrorVV);
      cv::putText(img, evv_str.str(), cv::Point(10,y_offset+80), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      
      // XYZ color space
      y_offset += 100;
      std::stringstream exyz_str, ex_str, eyy_str, ez_str;
      exyz_str << "E[XYZ]=" << unsigned(chartErrorXYZ);
      cv::putText(img, exyz_str.str(), cv::Point(10,y_offset+20), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(99,31,0), 1, cv::LINE_AA);
      ex_str << " E[X]=" << unsigned(chartErrorX);
      cv::putText(img, ex_str.str(), cv::Point(10,y_offset+40), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      eyy_str << " E[Y]=" << unsigned(chartErrorYY);
      cv::putText(img, eyy_str.str(), cv::Point(10,y_offset+60), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      ez_str << " E[Z]=" << unsigned(chartErrorZ);
      cv::putText(img, ez_str.str(), cv::Point(10,y_offset+80), cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0,0,0), 1, cv::LINE_AA);
      
      // Draw border around "color checker" area
      std::vector<cv::Point> polygonPoints;
      polygonPoints.push_back(cv::Point(chartCorners[0].x,chartCorners[0].y));
      polygonPoints.push_back(cv::Point(chartCorners[1].x,chartCorners[1].y));
      polygonPoints.push_back(cv::Point(chartCorners[2].x,chartCorners[2].y));
      polygonPoints.push_back(cv::Point(chartCorners[3].x,chartCorners[3].y));
      cv::polylines(img, polygonPoints, true, cv::Scalar (0, 255, 0), 2, 16);
      //for ( int i = 0; i < 24; i++ ) {
      //    cv::circle(img, chartCentroids[i] ,5, cv::Scalar(163, 0, 255),cv::FILLED, 8,0);
      //};

      // Call Color Checker Script (if specified)
      if ( markerdetect->cc_script != NULL )
      { 
        if ( (markerdetect->iterations > 50) && (markerdetect->cc_frame_count > markerdetect->cc_skip_frames) )
        {
          char szCommand[1024];
          if ( markerdetect->cc_extra_args != NULL )
          {
            sprintf(szCommand,"%s %s %s\n", markerdetect->cc_script, color_patch_bgr_values.str().c_str(), markerdetect->cc_extra_args );
          }
          else {
            sprintf(szCommand,"%s %s\n", markerdetect->cc_script, color_patch_bgr_values.str().c_str() );
          }
          //printf(szCommand);
          system(szCommand);
          
          markerdetect->cc_frame_count = 0;
        }
      }

    }
    // Chart 2 - White Reference
    if ( (tl_id==923) && (tr_id==1002) && (bl_id==1007) && (br_id==241) )
    {
      // Extract ROI (area, ideally within 4 markers)
      std::vector<cv::Point> polygonPoints;
      polygonPoints.push_back(cv::Point(tl_xy.x,tl_xy.y));
      polygonPoints.push_back(cv::Point(tr_xy.x,tr_xy.y));
      polygonPoints.push_back(cv::Point(br_xy.x,br_xy.y));
      polygonPoints.push_back(cv::Point(bl_xy.x,bl_xy.y));
      std::vector<cv::Mat> bgr_planes;
      cv::split( img, bgr_planes );

      //
      // Calculate color gains
      //   ref : https://stackoverflow.com/questions/32466616/finding-the-average-color-within-a-polygon-bound-in-opencv
      //
      cv::Point pts[1][4];
      pts[0][0] = cv::Point(tl_xy.x,tl_xy.y);
      pts[0][1] = cv::Point(tr_xy.x,tr_xy.y);
      pts[0][2] = cv::Point(br_xy.x,br_xy.y);
      pts[0][3] = cv::Point(bl_xy.x,bl_xy.y);
      const cv::Point* points[1] = {pts[0]};
      int npoints = 4;
      // Create the mask with the polygon
      cv::Mat1b mask(img.rows, img.cols, uchar(0));
      cv::fillPoly(mask, points, &npoints, 1, cv::Scalar(255));
      // Calculate mean in masked area
      auto bgr_mean = cv::mean( img, mask );
      double b_mean = bgr_mean(0);
      double g_mean = bgr_mean(1);
      double r_mean = bgr_mean(2);
      // Find the gain of a channel
      //double K = (b_mean+g_mean+r_mean)/3;
      //double Kb = K/b_mean;
      //double Kg = K/g_mean;
      //double Kr = K/r_mean;
      //printf( "Stats : B=%5.3f G=%5.3f R=%5.3f > Kb=%5.3f Kg=%5.3f Kr=%5.3f\n", b_mean, g_mean, r_mean, Kb, Kg, Kr );
      
      // Draw bars 
      int plot_w = 100, plot_h = 100;
      cv::Mat plotImage( plot_h, plot_w, CV_8UC3, cv::Scalar(255,255,255) );
      int b_bar = int((b_mean/256.0)*80.0);
      int g_bar = int((g_mean/256.0)*80.0);
      int r_bar = int((r_mean/256.0)*80.0);
      // layout of bars : |<-10->|<---20-->|<-10->|<---20-->|<-10->|<---20-->|<-10->|
      cv::rectangle(plotImage, cv::Rect(10,(80-b_bar),20,b_bar), cv::Scalar(255, 0, 0), cv::FILLED, cv::LINE_8);
      cv::rectangle(plotImage, cv::Rect(40,(80-g_bar),20,g_bar), cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_8);
      cv::rectangle(plotImage, cv::Rect(70,(80-r_bar),20,r_bar), cv::Scalar(0, 0, 255), cv::FILLED, cv::LINE_8);
      //printf( "Stats : BGR=%5.3f,%5.3f,%5.3f (%d,%d,%d) => Kbgr=%5.3f,%5.3f,%5.3f\n", b_mean, g_mean, r_mean, b_bar, g_bar, r_bar, Kb, Kg, Kr );
      std::stringstream b_str;
      std::stringstream g_str;
      std::stringstream r_str;
      b_str << int(b_mean);
      g_str << int(g_mean);
      r_str << int(r_mean);
      cv::putText(plotImage, b_str.str(), cv::Point(10,90), cv::FONT_HERSHEY_PLAIN, 0.75, cv::Scalar(255,0,0), 1, cv::LINE_AA);
      cv::putText(plotImage, g_str.str(), cv::Point(40,90), cv::FONT_HERSHEY_PLAIN, 0.75, cv::Scalar(0,255,0), 1, cv::LINE_AA);
      cv::putText(plotImage, r_str.str(), cv::Point(70,90), cv::FONT_HERSHEY_PLAIN, 0.75, cv::Scalar(0,0,255), 1, cv::LINE_AA);

      // Calculate transformation matrix
      std::vector<cv::Point2f> srcPoints;
      std::vector<cv::Point2f> dstPoints;
      srcPoints.push_back(cv::Point(       0,       0)); // top left
      srcPoints.push_back(cv::Point(plot_w-1,       0)); // top right
      srcPoints.push_back(cv::Point(plot_w-1,plot_h-1)); // bottom right
      srcPoints.push_back(cv::Point(       0,plot_h-1)); // bottom left
      dstPoints.push_back(tl_xy);
      dstPoints.push_back(tr_xy);
      dstPoints.push_back(br_xy);
      dstPoints.push_back(bl_xy);
      cv::Mat h = cv::findHomography(srcPoints,dstPoints);
      // Warp plot image onto video frame
      cv::Mat img_temp = img.clone();
      cv::warpPerspective(plotImage, img_temp, h, img_temp.size());
      cv::Point pts_dst[4];
      for( int i = 0; i < 4; i++)
      {
        pts_dst[i] = dstPoints[i];
      }
      cv::fillConvexPoly(img, pts_dst, 4, cv::Scalar(0), cv::LINE_AA);
      img = img + img_temp;

      // Draw border around "white reference" area
      cv::polylines(img, polygonPoints, true, cv::Scalar (0, 255, 0), 2, 16);
      
      // Call White Balance Script (if specified)
      if ( markerdetect->wb_script != NULL )
      { 
        if ( (markerdetect->iterations > 50) && (markerdetect->wb_frame_count > markerdetect->wb_skip_frames) )
        {
          char szCommand[256];
          if ( markerdetect->wb_extra_args != NULL )
          {
            sprintf(szCommand,"%s %d %d %d %s\n", markerdetect->wb_script, int(b_mean), int(g_mean), int(r_mean), markerdetect->wb_extra_args );
          }
          else {
            sprintf(szCommand,"%s %d %d %d\n", markerdetect->wb_script, int(b_mean), int(g_mean), int(r_mean) );
          }
          //printf(szCommand);
          system(szCommand);
          
          markerdetect->wb_frame_count = 0;
        }
      }

    }

    // Chart 3 - Histogram
    if ( (tl_id==923) && (tr_id==1003) && (bl_id==1007) && (br_id==241) )
    {
      // Extract ROI (area, ideally within 4 markers)
      std::vector<cv::Point> polygonPoints;
      polygonPoints.push_back(cv::Point(tl_xy.x,tl_xy.y));
      polygonPoints.push_back(cv::Point(tr_xy.x,tr_xy.y));
      polygonPoints.push_back(cv::Point(br_xy.x,br_xy.y));
      polygonPoints.push_back(cv::Point(bl_xy.x,bl_xy.y));
      std::vector<cv::Mat> bgr_planes;
      cv::split( img, bgr_planes );

      cv::Point pts[1][4];
      pts[0][0] = cv::Point(tl_xy.x,tl_xy.y);
      pts[0][1] = cv::Point(tr_xy.x,tr_xy.y);
      pts[0][2] = cv::Point(br_xy.x,br_xy.y);
      pts[0][3] = cv::Point(bl_xy.x,bl_xy.y);
      const cv::Point* points[1] = {pts[0]};
      int npoints = 4;
      // Create the mask with the polygon
      cv::Mat1b mask(img.rows, img.cols, uchar(0));
      cv::fillPoly(mask, points, &npoints, 1, cv::Scalar(255));

      //
      // Calculate color histograms
      //    https://github.com/opencv/opencv/blob/3.4/samples/cpp/tutorial_code/Histograms_Matching/calcHist_Demo.cpp
      //
      int hist_w = 512, hist_h = 400;
      cv::Mat histImage( hist_h, hist_w, CV_8UC3, cv::Scalar( 0,0,0) );
      int histSize = 256; // number of bins
      int bin_w = cvRound( (double) hist_w/histSize );
      float range[] = { 0, 256 }; // ranges for B,G,R (the upper boundary is exclusive)
      const float* histRange = { range };
      bool uniform = true, accumulate = false;
      cv::Mat b_hist, g_hist, r_hist;
      cv::calcHist( &bgr_planes[0], 1, 0, mask, b_hist, 1, &histSize, &histRange, uniform, accumulate );
      cv::calcHist( &bgr_planes[1], 1, 0, mask, g_hist, 1, &histSize, &histRange, uniform, accumulate );
      cv::calcHist( &bgr_planes[2], 1, 0, mask, r_hist, 1, &histSize, &histRange, uniform, accumulate );
      // Draw the histograms for B, G and R
      // Normalize the result to ( 0, histImage.rows )
      cv::normalize(b_hist, b_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat() );
      cv::normalize(g_hist, g_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat() );
      cv::normalize(r_hist, r_hist, 0, histImage.rows, cv::NORM_MINMAX, -1, cv::Mat() );
      // Draw for each channel
      for( int i = 1; i < histSize; i++ )
      {
          cv::line( histImage, 
                cv::Point( bin_w*(i-1), hist_h - cvRound(b_hist.at<float>(i-1)) ),
                cv::Point( bin_w*(i), hist_h - cvRound(b_hist.at<float>(i)) ),
                cv::Scalar( 255, 0, 0), 2, 8, 0  );
          cv::line( histImage, 
                cv::Point( bin_w*(i-1), hist_h - cvRound(g_hist.at<float>(i-1)) ),
                cv::Point( bin_w*(i), hist_h - cvRound(g_hist.at<float>(i)) ),
                cv::Scalar( 0, 255, 0), 2, 8, 0  );
          cv::line( histImage,
                cv::Point( bin_w*(i-1), hist_h - cvRound(r_hist.at<float>(i-1)) ),
                cv::Point( bin_w*(i), hist_h - cvRound(r_hist.at<float>(i)) ),
                cv::Scalar( 0, 0, 255), 2, 8, 0  );
      }

      // Draw border around ROI used for color histogram
      //cv::rectangle(img, roi, cv::Scalar (0, 255, 0), 2, cv::LINE_AA);

      // Calculate transformation matrix
      std::vector<cv::Point2f> srcPoints;
      std::vector<cv::Point2f> dstPoints;
      srcPoints.push_back(cv::Point(       0,       0)); // top left
      srcPoints.push_back(cv::Point(hist_w-1,       0)); // top right
      srcPoints.push_back(cv::Point(hist_w-1,hist_h-1)); // bottom right
      srcPoints.push_back(cv::Point(       0,hist_h-1)); // bottom left
      dstPoints.push_back(tl_xy);
      dstPoints.push_back(tr_xy);
      dstPoints.push_back(br_xy);
      dstPoints.push_back(bl_xy);
      cv::Mat h = cv::findHomography(srcPoints,dstPoints);
      // Warp histogram image onto video frame
      cv::Mat img_temp = img.clone();
      cv::warpPerspective(histImage, img_temp, h, img_temp.size());
      cv::Point pts_dst[4];
      for( int i = 0; i < 4; i++)
      {
        pts_dst[i] = dstPoints[i];
      }
      cv::fillConvexPoly(img, pts_dst, 4, cv::Scalar(0), cv::LINE_AA);
      img = img + img_temp;
      
      // Draw border around "histgramm" area
      cv::polylines(img, polygonPoints, true, cv::Scalar (0, 255, 0), 2, 16);
      
    }
  }

  GST_DEBUG_OBJECT (markerdetect, "transform_frame_ip");

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{

  /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register (plugin, "markerdetect", GST_RANK_NONE,
      GST_TYPE_MARKERDETECT);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "0.0.0"
#endif
#ifndef PACKAGE
#define PACKAGE "markerdetect"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "OpenCV Library"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://avnet.com"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    markerdetect,
    "Marker detection using the OpenCV Library",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)  


