import matplotlib
import numpy as np

#define range of values we are interested in
values_index = range(60)

# normatlize these values in 0.0 - 1.0 range
norm = matplotlib.colors.Normalize(vmin=0.0, vmax=60.0)
#print(values_index)
values_norm = norm(values_index)
#print(values_norm)

# define colormap (Red=>Yellow=>Green)
cmap = matplotlib.cm.get_cmap('RdYlGn')
#print(cmap)

# get color map for our range of values
values_color = cmap(values_norm)
#print(values_color)

# convert colormap from 0.0-1.0 range to 0-255 range
colormap_rgb = np.uint8(values_color[:,0:3]*255.0)
#print(colormap_rgb)
#[[165   0  38]
# ...
# [  4 111  58]]
# this is in RGB format, need to convert to BGR
colormap_bgr = colormap_rgb[:,::-1]
#colormap_bgr = colormap_rgb.copy()
#colormap_bgr[:,0] = colormap_rgb[:,2]
#colormap_bgr[:,1] = colormap_rgb[:,1]
#colormap_bgr[:,2] = colormap_rgb[:,0]
#print(colormap_bgr)
#[[ 38   0 165]
# ...
# [ 58 111   4]]

# inverse order of colormap (make Red=max, Green=min)
colormap_bgr = colormap_bgr[::-1,:]
#print(colormap_bgr)
#[[ 58 111   4]
# ...
# [ 38   0 165]]

# Generate structure for use with C++
print("// BGR values for GrYlRd colormap")
print("std::vector<cv::Scalar> colormap_GrYlRd =")
print("{")
for i in range(60):
  print("   { ", str(colormap_bgr[i,0]), ", ", str(colormap_bgr[i,1]), ", ", str(colormap_bgr[i,2]), " },")
print("};")