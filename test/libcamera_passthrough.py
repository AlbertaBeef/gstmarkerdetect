'''
Copyright 2024 Tria Technologies Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
'''

import numpy as np
import cv2
import argparse
import sys
import os
from datetime import datetime


# USAGE
# python3 libcamera_passthrough.py [--input (camera name}] [--width 640] [--height 480]

# construct the argument parse and parse the arguments
ap = argparse.ArgumentParser()
ap.add_argument("-i", "--input", required=False,
	help = "input camera name (query camera names with cam -l")
ap.add_argument("-W", "--width", required=False,
	help = "input width (default = 640)")
ap.add_argument("-H", "--height", required=False,
	help = "input height (default = 480)")
ap.add_argument("-f", "--fps", required=False, default=False, action="store_true",
	help = "fps overlay (default = off")
args = vars(ap.parse_args())

if not args.get("input",False):
  input = None
else:
  input = args["input"]

if not args.get("width",False):
  width = 640
else:
  width = int(args["width"])
if not args.get("height",False):
  height = 480
else:
  height = int(args["height"])
print('[INFO] input resolution = ',width,'X',height)

if not args.get("fps",False):
  fps_overlay = False 
else:
  fps_overlay = True
print('[INFO] fps overlay =  ',fps_overlay)


# init the real-time FPS display
rt_fps_count = 0;
rt_fps_time = cv2.getTickCount()
rt_fps_valid = False
rt_fps = 0.0
rt_fps_message = "FPS: {0:.2f}".format(rt_fps)
rt_fps_x = 10
rt_fps_y = height-10

def ignore(x):
    pass

dev_video = "/dev/video0" # need to figure out how to obtain this ... 
print("[INFO] dev_video = ",dev_video)
    

gst_pipeline = "libcamerasrc "
if input != None:
	gst_pipeline = gst_pipeline + "camera-name=" + input
gst_pipeline = gst_pipeline + " ! video/x-raw,width="+str(width)+",height="+str(height)+",format=BGR"
gst_pipeline = gst_pipeline + " ! videoconvert"
gst_pipeline = gst_pipeline + " ! queue"
gst_pipeline = gst_pipeline + " ! appsink"
print("[INFO] gst_pipeline = ",gst_pipeline)
  
cam = cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)
#cam.set(cv2.CAP_PROP_FRAME_WIDTH,width)
#cam.set(cv2.CAP_PROP_FRAME_HEIGHT,height)  
    
app_main_title = "tria aaswb example"
cv2.namedWindow(app_main_title)


while(True):
	# Update the real-time FPS counter
	if rt_fps_count == 0:
		rt_fps_time = cv2.getTickCount()

	# camera input
	ret,frame = cam.read()
	if ret == False:
		print("[ERROR] Failed to capture video from input ",input)

	#print("[INFO] frame.size=",frame.size)

	# passthrough
	output = frame

	# Display status messages
	status = ""
	if fps_overlay == True and rt_fps_valid == True:
		status = status + " " + rt_fps_message
	cv2.putText(output, status, (rt_fps_x,rt_fps_y), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0,255,0), 1, cv2.LINE_AA)

	# Display output
	cv2.imshow(app_main_title,output)

	key = cv2.waitKey(1) & 0xFF

	# if the ESC or 'q' key was pressed, break from the loop
	if key == 27 or key == ord("q"):
		break

	if key == ord("w"):
		datetime_object = datetime.now()
		format_string = "%Y%m%d_%H%M%S"
		date_string = datetime_object.strftime(format_string)
		capture_filename = "capture_"+date_string+".png"
		cv2.imwrite(capture_filename,output)
		print("[INFO] Image captured ... ",capture_filename)
   
	# Update the real-time FPS counter
	rt_fps_count = rt_fps_count + 1
	if rt_fps_count >= 10:
		t = (cv2.getTickCount() - rt_fps_time)/cv2.getTickFrequency()
		rt_fps_valid = True
		rt_fps = 10.0/t
		rt_fps_message = "FPS: {0:.2f}".format(rt_fps)
		#print("[INFO] ",rt_fps_message)
		rt_fps_count = 0

# When everything done, release the capture
cam.release()
cv2.destroyAllWindows()

