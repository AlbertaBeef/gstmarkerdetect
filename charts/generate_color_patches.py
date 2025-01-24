import cv2
import numpy as np

size = (512,608)

#      // Reference coordinates manually taken from 608x512 image (ROI from ArUco markers)
#      std::vector<cv::Point2f> arucoCornersRef = {
#        {  0,  0}, {607,  0}, {607,511}, {  0,511}
#      };
#      std::vector<cv::Point2f> chartCornersRef = {
#        {  0, 57}, {607, 57}, {607,455}, {  0,455}
#      };
#      std::vector<cv::Point2f> chartCentroidsRef = {
#        { 46,103}, {150,103}, {252,103}, {355,103}, {458,103}, {561,103},
#        { 46,205}, {150,205}, {252,205}, {355,205}, {458,205}, {561,205},
#        { 46,307}, {150,307}, {252,307}, {355,307}, {458,307}, {561,307},
#        { 46,409}, {150,409}, {252,409}, {355,409}, {458,409}, {561,409}
#      };
#      std::vector<cv::Scalar> chartColorsRef =
#      { 
#      // Dark Skin      Light Skin     Blue Sky       Foliage        Blue Flower    Bluish Green   
#        { 68, 82,115}, {130,150,192}, {157,122, 98}, { 67,108, 87}, {177,128,133}, {170,189,103},
#      // Orange         Purple Red     Moderate Red   Purple         Yellow Green   Orange Yello
#        { 44,126,214}, {166, 91, 80}, { 99, 90,193}, {108, 60, 94}, { 64,188,157}, { 46,163,224},
#      // Blue           Green          Red            Yellow         Magenta        Cyan
#        {150, 61, 56}, { 73,148, 70}, { 60, 54,175}, { 31,199,231}, {149, 86,187}, {161,133,  8},
#      // White          Neutral 8      Neutral 65     Neutral 5      Neutral 35     Black
#        {242,243,243}, {200,200,200}, {160,160,160}, {121,122,122}, { 85, 85, 85}, { 52, 52, 52}
#      };


blank_image = np.zeros((size[0], size[1], 4), np.uint8)

chartCornersRef = np.array([(  0, 57), (607, 57), (607,455), (  0,455)])
chartCentroidsRef = np.array([
        ( 46,103), (150,103), (252,103), (355,103), (458,103), (561,103),
        ( 46,205), (150,205), (252,205), (355,205), (458,205), (561,205),
        ( 46,307), (150,307), (252,307), (355,307), (458,307), (561,307),
        ( 46,409), (150,409), (252,409), (355,409), (458,409), (561,409)
])
chartColorsRef = np.array([
       # Dark Skin      Light Skin     Blue Sky       Foliage        Blue Flower    Bluish Green   
        ( 68, 82,115), (130,150,192), (157,122, 98), ( 67,108, 87), (177,128,133), (170,189,103),
       # Orange         Purple Red     Moderate Red   Purple         Yellow Green   Orange Yello
        ( 44,126,214), (166, 91, 80), ( 99, 90,193), (108, 60, 94), ( 64,188,157), ( 46,163,224),
       # Blue           Green          Red            Yellow         Magenta        Cyan
        (150, 61, 56), ( 73,148, 70), ( 60, 54,175), ( 31,199,231), (149, 86,187), (161,133,  8),
       # White          Neutral 8      Neutral 65     Neutral 5      Neutral 35     Black
        (242,243,243), (200,200,200), (160,160,160), (121,122,122), ( 85, 85, 85), ( 52, 52, 52)
])

for i,cornerPoint in enumerate(chartCornersRef):
   print(cornerPoint)
   x = chartCornersRef[i,0]
   y = chartCornersRef[i,1]
   x1 = max(x-5,0)
   x2 = min(x+5,608)
   y1 = y-5
   y2 = y+5
   print( x,y, " => ", x1,x2, y1,y2 )
   blank_image[y1:y2,x] = [255,255,255,255]
   blank_image[y,x1:x2] = [255,255,255,255]

for i,centroid in enumerate(chartCentroidsRef):
   point = [centroid[1],centroid[0]]
   color = [chartColorsRef[i][0],chartColorsRef[i][1],chartColorsRef[i][2],255]
   print( centroid, " => ", point, color )
   blank_image[point[0]-44:point[0]+44,point[1]-44:point[1]+44] = color

cv2.imwrite('test.png', blank_image)