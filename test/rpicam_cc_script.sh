#!/bin/bash

arg_array=( "$@" )
arg_count=${#arg_array[@]}

# Select one of the Color Patches

#id=0  # Dark Skin
#id=1  # Light Skin
#id=2  # Blue Sky
#id=3  # Foliage
#id=4  # Blue Flower
#id=5  # Bluish Green
   
#id=6  # Orange
#id=7  # Purple Red
#id=8  # Moderate Red
#id=9  # Purple
#id=10 # Yellow Green
#id=11 # Orange Yello

#id=12 # Blue
#id=13 # Green
#id=14 # Red
#id=15 # Yellow
#id=16 # Magenta
#id=17 # Cyan

#id=18 # White
#id=19 # Neutral 8
id=20 # Neutral 65
#id=21 # Neutral 5
#id=22 # Neutral 35
#id=23 # Black

# Get Color Averages (BGR) for specified Color Patch

b_id=3*id+0
g_id=3*id+1
r_id=3*id+2
b_mean=${arg_array[$b_id]}
g_mean=${arg_array[$g_id]}
r_mean=${arg_array[$r_id]}

# Get last argument (cc-ext-args)

dev=${arg_array[$arg_count-1]}

# Display Color averages (BGR) for Specified Color Patch

echo "[$dev] Patch[$id] Color averages (B/G/R) = $b_mean/$g_mean/$r_mean"

