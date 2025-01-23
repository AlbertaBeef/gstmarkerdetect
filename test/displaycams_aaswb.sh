#!/bin/bash

# Opsero Electronic Design Inc. 2024
#-----------------------------------
# This script goes through all of the media devices found and uses media-ctl
# to filter out the devices that are not attached to the xilinx-video driver.
# This way we attempt to target only the cameras that are connected to the 
# RPi Camera FMC, and ignore any USB (or other) cameras that are connected.
# We also use media-ctl to determine the video device that is associated
# with each media device, and we keep these values in an array.
# The second part of the script goes through the array of media devices and
# configures the associated video pipe with values for resolution, format
# and frame rate, according to a set of variables defined at the top of this
# script.
# The next part of the script prints a list of the cameras that were found
# and configured, showing the port (CAM0,CAM1,CAM2,CAM3), the media device
# (eg. /dev/media0) and the video device (eg. /dev/video0) for each.
# The last part of the script launches gstreamer to display all four video
# streams on a single display.

# This dictionary associates GStreamer pixel formats with those used with media-ctl
declare -A format_dict
format_dict["NV12"]="VYYUYY8_1X24"
format_dict["YUY2"]="UYVY8_1X16"
format_dict["BGR"]="RBG888_1X24"

#--------------------------------------------------------------------------------
# Example settings - the script will configure ALL video pipelines to these specs
#--------------------------------------------------------------------------------
# Resolution of RPi V2 cameras (must be a resolution supported by the IMX219 Linux driver 640x480, 1640x1232, 1920x1080)
IMX219_RES_W=1920
IMX219_RES_H=1080
# Resolution of RPi HQ cameras (must be a resolution supported by the IMX477 Linux driver 1332x990)
IMX477_RES_W=1332
IMX477_RES_H=990
# Resolution of RPi HQ cameras (must be a resolution supported by the IMX500 Linux driver 2028x1520)
IMX500_RES_W=2028
IMX500_RES_H=1520
# Resolution of RPi AI cameras (must be a resolution supported by the IMX708 Linux driver 1536x864)
IMX708_RES_W=1536
IMX708_RES_H=864
# Resolution of RPi camera pipelines (after Video Processing Subsystem IP)
OUT_RES_W=960
OUT_RES_H=540
# Output format of the RPi camera pipelines (use a GStreamer pixel format from the dict above)
#OUT_FORMAT=YUY2
OUT_FORMAT=BGR
# Frame rate (fps)
FRM_RATE=30
#--------------------------------------------------------------------------------
# End of example settings
#--------------------------------------------------------------------------------

# Find the vmixer
VMIX_PATH=$(find /sys/bus/platform/devices/ -name "*.v_mix" | head -n 1)
VMIX=$(basename "$VMIX_PATH")

# Find out the monitor's highest resolution
output=$(modetest -c -M xlnx | grep "#0")
DISP_RES=$(echo "$output" | awk '{print $2}')

echo "-------------------------------------------------"
echo " Capture pipeline init: RPi cam -> Scaler -> DDR"
echo "-------------------------------------------------"

# Print the settings
echo "Configuring IMX219 video capture pipelines to:"
echo " - RPi V2 Camera output : $IMX219_RES_W x $IMX219_RES_H"
echo " - RPi V3 Camera output : $IMX708_RES_W x $IMX708_RES_H"
echo " - RPi AI Camera output : $IMX500_RES_W x $IMX500_RES_H"
echo " - RPi HQ Camera output : $IMX477_RES_W x $IMX477_RES_H"
echo " - Scaler (VPSS) output : $OUT_RES_W x $OUT_RES_H $OUT_FORMAT"
echo " - Frame rate           : $FRM_RATE fps"

# Print the bus_id of the video mixer
echo "Video Mixer found here:"
echo " - $VMIX"

echo "Monitor resolution:"
echo " - $DISP_RES"

# Find all the media devices
media_devices=($(ls /dev/media*))

# Declare a associative arrays
declare -A unique_video_devices
declare -A media_to_video_mapping
declare -A media_to_cam_interface
declare -A media_to_cam_sensor
declare -A media_to_cam_resolution

# For each media device, find its associated video devices
for media in "${media_devices[@]}"; do
        output=$(media-ctl -d "$media" -p)
        # Check if the media device is of type "xilinx-video"
        if echo "$output" | grep -q "driver          xilinx-video"; then
                video_device=$(echo "$output" | grep "dev/video")
                # Extract video device path from the grep result
                if [[ $video_device =~ (/dev/video[0-9]+) ]]; then
                        unique_video_devices["${BASH_REMATCH[1]}"]=1
                        # Store the media to video relationship
                        media_to_video_mapping["$media"]="${BASH_REMATCH[1]}"

                        # Extract X from the string "vcap_mipi_X_v_proc"
                        if [[ $output =~ vcap_mipi_([0-9])_v_proc ]]; then
                                cam_interface="CAM${BASH_REMATCH[1]}"
                                media_to_cam_interface["$media"]="$cam_interface"
                        fi
                        
                        # Determine sensor type
                        if [[ $output =~ imx219 ]]; then
                                i2c_bus=$(echo "$output" | grep '.*- entity.*imx219' | awk -F' ' '{print $5}')                          
                                media_to_cam_sensor["$media"]="imx219 $i2c_bus"
                                media_to_cam_resolution["$media"]="${IMX219_RES_W}x${IMX219_RES_H}"
                        elif [[ $output =~ imx477 ]]; then
                                i2c_bus=$(echo "$output" | grep '.*- entity.*imx477' | awk -F' ' '{print $5}')                          
                                media_to_cam_sensor["$media"]="imx477 $i2c_bus"
                                media_to_cam_resolution["$media"]="${IMX477_RES_W}x${IMX477_RES_H}"
                        elif [[ $output =~ imx500 ]]; then
                                i2c_bus=$(echo "$output" | grep '.*- entity.*imx500' | awk -F' ' '{print $5}')                          
                                media_to_cam_sensor["$media"]="imx500 $i2c_bus"
                                media_to_cam_resolution["$media"]="${IMX500_RES_W}x${IMX500_RES_H}"
                        elif [[ $output =~ imx708_wide_noir ]]; then
                                media_to_cam_sensor["$media"]="imx708_wide_noir"
                                media_to_cam_resolution["$media"]="${IMX708_RES_W}x${IMX708_RES_H}"
                        elif [[ $output =~ imx708_wide ]]; then
                                media_to_cam_sensor["$media"]="imx708_wide"
                                media_to_cam_resolution["$media"]="${IMX708_RES_W}x${IMX708_RES_H}"
                        elif [[ $output =~ imx708_noir ]]; then
                                media_to_cam_sensor["$media"]="imx708_noir"
                                media_to_cam_resolution["$media"]="${IMX708_RES_W}x${IMX708_RES_H}"
                        elif [[ $output =~ imx708 ]]; then
                                media_to_cam_sensor["$media"]="imx708"
                                media_to_cam_resolution["$media"]="${IMX708_RES_W}x${IMX708_RES_H}"
                        else
                                media_to_cam_sensor["$media"]="unknown"
                                media_to_cam_resolution["$media"]="WxH"
                        fi
                        
                fi
        fi
done

#-------------------------------------------------------------------------------
# The section below serves as an example for configuring the video pipelines with
# media-ctl. In this example, we set all video pipelines to the same specs.
# See the documentation for help on these commands.
# https://rpi.camerafmc.com/ (PetaLinux -> Debugging tips section)
#-------------------------------------------------------------------------------
for media in "${!media_to_video_mapping[@]}"; do
        OUTPUT=$(media-ctl -d $media -p)
        echo ""
        echo "${media_to_cam_interface[$media]} (${media_to_cam_sensor[$media]}) : $media = ${media_to_video_mapping[$media]}"
        echo "=================================================================================================================="
        IMX_SENSOR=${media_to_cam_sensor["$media"]}
        IMX_RESOLUTION=${media_to_cam_resolution["$media"]}
        echo media-ctl -V "\"${IMX_SENSOR}\":0 [fmt:SRGGB10_1X10/${IMX_RESOLUTION}]" -d $media
        media-ctl -V "\"${IMX_SENSOR}\":0 [fmt:SRGGB10_1X10/${IMX_RESOLUTION}]" -d $media
        MIPI_CSI=$(echo "$OUTPUT" | grep '.*- entity.*mipi_csi2_rx_subsystem' | awk -F' ' '{print $4}')
        echo media-ctl -V "\"${MIPI_CSI}\":0 [fmt:SRGGB10_1X10/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        media-ctl -V "\"${MIPI_CSI}\":0 [fmt:SRGGB10_1X10/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        echo media-ctl -V "\"${MIPI_CSI}\":1  [fmt:SRGGB10_1X10/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        media-ctl -V "\"${MIPI_CSI}\":1  [fmt:SRGGB10_1X10/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        ISP_PIPE=$(echo "$OUTPUT" | grep '.*- entity.*ISPPipeline_accel' | awk -F' ' '{print $4}')
        echo media-ctl -V "\"${ISP_PIPE}\":0  [fmt:SRGGB10_1X10/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        media-ctl -V "\"${ISP_PIPE}\":0  [fmt:SRGGB10_1X10/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        echo media-ctl -V "\"${ISP_PIPE}\":1  [fmt:RBG888_1X24/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        media-ctl -V "\"${ISP_PIPE}\":1  [fmt:RBG888_1X24/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        V_PROC=$(echo "$OUTPUT" | grep '.*- entity.*.v_proc_ss ' | awk -F' ' '{print $4}')
        echo media-ctl -V "\"${V_PROC}\":0  [fmt:RBG888_1X24/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        media-ctl -V "\"${V_PROC}\":0  [fmt:RBG888_1X24/${IMX_RESOLUTION} field:none colorspace:srgb]" -d $media
        echo media-ctl -V "\"${V_PROC}\":1  [fmt:${format_dict[$OUT_FORMAT]}/${OUT_RES_W}x${OUT_RES_H} field:none colorspace:srgb]" -d $media
        media-ctl -V "\"${V_PROC}\":1  [fmt:${format_dict[$OUT_FORMAT]}/${OUT_RES_W}x${OUT_RES_H} field:none colorspace:srgb]" -d $media
done
echo ""
        
#-------------------------------------------------------------------------------
# List the media devices and their associated video devices
#-------------------------------------------------------------------------------
echo "Detected and configured the following cameras on RPi Camera FMC:"
for media in "${!media_to_video_mapping[@]}"; do
        echo " - ${media_to_cam_interface[$media]} (${media_to_cam_sensor[$media]}) : $media = ${media_to_video_mapping[$media]}"
done

#-------------------------------------------------------------------------------
# Setup the display pipeline
#-------------------------------------------------------------------------------
# Initialize the display pipeline
#echo | modetest -M xlnx -D ${VMIX} -s 60@46:${DISP_RES}@NV16
echo | modetest -M xlnx -D ${VMIX} -s 68@54:${DISP_RES}@NV16

#------------------------------------------------------------------------------
# Run GStreamer to combine all videos and display on the screen
#-------------------------------------------------------------------------------
full_command="gst-launch-1.0"

# Screen quadrants: TOP-LEFT, TOP-RIGHT, BOTTOM-LEFT, BOTTOM-RIGHT
quadrants_yuyv=(
        "plane-id=34 render-rectangle=\"<0,0,${OUT_RES_W},${OUT_RES_H}>\""
        "plane-id=36 render-rectangle=\"<${OUT_RES_W},0,${OUT_RES_W},${OUT_RES_H}>\""
        "plane-id=38 render-rectangle=\"<0,${OUT_RES_H},${OUT_RES_W},${OUT_RES_H}>\""
        "plane-id=40 render-rectangle=\"<${OUT_RES_W},${OUT_RES_H},${OUT_RES_W},${OUT_RES_H}>\""
)
quadrants_bgr=(
        "plane-id=44 render-rectangle=\"<0,0,${OUT_RES_W},${OUT_RES_H}>\""
        "plane-id=46 render-rectangle=\"<${OUT_RES_W},0,${OUT_RES_W},${OUT_RES_H}>\""
        "plane-id=48 render-rectangle=\"<0,${OUT_RES_H},${OUT_RES_W},${OUT_RES_H}>\""
        "plane-id=50 render-rectangle=\"<${OUT_RES_W},${OUT_RES_H},${OUT_RES_W},${OUT_RES_H}>\""
)

index=0

# For each connected camera, add pipeline to gstreamer command
for media in "${!media_to_video_mapping[@]}"; do
        # Append the specific command for the current iteration to the full command
        full_command+=" v4l2src device=${media_to_video_mapping[$media]} io-mode=mmap"
        full_command+=" ! video/x-raw, width=${OUT_RES_W}, height=${OUT_RES_H}, format=${OUT_FORMAT}, framerate=${FRM_RATE}/1"
        full_command+=" ! markerdetect wb-script=./rpicam_aaswb.sh wb-extra-args=${media_to_video_mapping[$media]} wb-skip-frames=0"
        full_command+=" ! kmssink bus-id=${VMIX} ${quadrants_bgr[$index]} show-preroll-frame=false sync=false can-scale=false"

        ((index++))
done

# Display the command being run
echo "GStreamer command:"
echo "--------------------------"
echo "${full_command}"
echo "--------------------------"

# Execute the command
eval "${full_command}"
