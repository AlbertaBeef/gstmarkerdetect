#!/bin/bash

b_mean=$1
g_mean=$2
r_mean=$3
#dev='/dev/video0'
dev=$4

#echo "White Balance script"
#echo "   Color averages (B/G/R) = $b_mean/$g_mean/$r_mean"

#####################
## SENSOR CONTROLS ##
#####################
RED_GAIN_VALUE="150"
BLUE_GAIN_VALUE="150"

#v4l2-ctl  -d /dev/video0 --set-ctrl blue_gain=150
#v4l2-ctl  -d /dev/video0 --set-ctrl red_gain=150
#v4l2-ctl  -d /dev/video0 --get-ctrl blue_gain
#v4l2-ctl  -d /dev/video0 --get-ctrl red_gain


# Get current values
b_gain_str=$(v4l2-ctl  -d $dev --get-ctrl blue_gain 2>&1)
r_gain_str=$(v4l2-ctl  -d $dev --get-ctrl red_gain 2>&1)
#exposure_str=$(v4l2-ctl  -d $dev --get-ctrl exposure 2>&1)
#echo "   Blue Gain = $b_gain_str"
#echo "   Red Gain = $r_gain_str"
b_gain=$(echo $b_gain_str | cut -d' ' -f 2)
r_gain=$(echo $r_gain_str | cut -d' ' -f 2)
#exposure=$(echo $exposure_str | cut -d' ' -f 2)
#echo "   Current Color Gains (B/G/R) = $b_gain/-/$r_gain"
#echo "   Current Exposure = $exposure"

# Define targeted color average
#x_mean=150
x_mean=g_mean

# Adjust gains according to color averages
b_delta=$(( x_mean - b_mean ))
r_delta=$(( x_mean - r_mean ))
# large values cause oscillations, need to dampen adjustments
b_delta=$(( b_delta > +50 ? +50 : b_delta ))
b_delta=$(( b_delta < -50 ? -50 : b_delta ))
r_delta=$(( r_delta > +50 ? +50 : r_delta ))
r_delta=$(( r_delta < -50 ? -50 : r_delta ))
b_adjust=$(( b_delta/4 ))
r_adjust=$(( r_delta/4 ))
# apply adjustments to color gains
BLUE_GAIN_VALUE=$((  b_gain + b_adjust ))
RED_GAIN_VALUE=$((   r_gain + r_adjust ))
#echo "   Updated Color Gains (B/G/R) = $BLUE_GAIN_VALUE/--/$RED_GAIN_VALUE ($b_adjust/--/$r_adjust)"

echo "AASWB : BGR Averages = $b_mean/$g_mean/$r_mean | BGR Gains = $BLUE_GAIN_VALUE/----/$RED_GAIN_VALUE ($b_adjust/--/$r_adjust)"

#echo -n "Setting sensor controls... "
v4l2-ctl  -d $dev --set-ctrl blue_gain=$BLUE_GAIN_VALUE > /dev/null 2>&1
v4l2-ctl  -d $dev --set-ctrl red_gain=$RED_GAIN_VALUE > /dev/null 2>&1
#v4l2-ctl  -d $dev --set-ctrl exposure=$EXPOSURE_VALUE > /dev/null 2>&1
#echo "Done!"