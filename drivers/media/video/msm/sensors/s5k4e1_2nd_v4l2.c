/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright(C) 2013 Foxconn International Holdings, Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "fih_read_otp.h"//MM-MC-ImplementReadOtpDataFeature-00+
#define SENSOR_NAME "s5k4e1_2nd"
#define PLATFORM_DRIVER_NAME "msm_camera_s5k4e1_2nd"
#define MSB                             1
#define LSB                             0

DEFINE_MUTEX(s5k4e1_2nd_mut);
static struct msm_sensor_ctrl_t s5k4e1_2nd_s_ctrl;

static struct msm_camera_i2c_reg_conf s5k4e1_2nd_start_settings[] = {
	{0x0100, 0x01},
    //{0x0601, 0x05},//Test patten 0: black, 5: white
};

static struct msm_camera_i2c_reg_conf s5k4e1_2nd_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k4e1_2nd_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e1_2nd_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k4e1_2nd_prev_settings[] = {
    {0x0100, 0x00},

    /* [20120321][4E1EVT3][MIPI2lane][Mclk=24Mhz-612Mbps][1304x980][30fps][non continuous] */
    // Integration setting ...
    /*Timing configuration*/
    {0x0202, 0x01}, //coarse_integration_time_H
    {0x0203, 0x11}, //coarse_integration_time_L
    {0x0204, 0x02}, //analogue_gain_code_global_H
    {0x0205, 0x00}, //analogue_gain_code_global_L
    //Frame Length
    {0x0340, 0x04},
    {0x0341, 0xC1},
    //Line Length
    {0x0342, 0x0D}, //3352
    {0x0343, 0x18},

    // PLL setting ...
    //// input clock 24MHz
    ////// (3) MIPI 1-lane Serial(TST = 0000b or TST = 0010b), 30 fps
    {0x0305, 0x04},
    {0x0306, 0x00},
    {0x0307, 0x66},
    {0x30B5, 0x01},
    {0x30E2, 0x02}, //num lanes[1:0] = 2
    {0x30F1, 0xA0},
    {0x30e8, 0x07}, //default =0f continuous ; 07= non-continuous

    // MIPI Size Setting ...
    // 1304 x 980
    {0x30A9, 0x02}, //Horizontal Binning On
    {0x300E, 0xEB}, //Vertical Binning On
    {0x0387, 0x03}, //y_odd_inc 03(10b AVG)
    {0x0344, 0x00}, //x_addr_start 0
    {0x0345, 0x00},
    {0x0348, 0x0A}, //x_addr_end 2607
    {0x0349, 0x2F},
    {0x0346, 0x00}, //y_addr_start 0
    {0x0347, 0x00},
    {0x034A, 0x07}, //y_addr_end 1959
    {0x034B, 0xA7},
    {0x0380, 0x00}, //x_even_inc 1
    {0x0381, 0x01},
    {0x0382, 0x00}, //x_odd_inc 1
    {0x0383, 0x01},
    {0x0384, 0x00}, //y_even_inc 1
    {0x0385, 0x01},
    {0x0386, 0x00}, //y_odd_inc 3
    {0x0387, 0x03},
    {0x034C, 0x05}, //x_output_size 1304
    {0x034D, 0x18},
    {0x034E, 0x03}, //y_output_size 980
    {0x034F, 0xd4},
    {0x30BF, 0xAB}, //outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
    {0x30C0, 0xA0}, //video_offset[7:4] 3260%12
    {0x30C8, 0x06}, //video_data_length 1600 = 1304 * 1.25
    {0x30C9, 0x5E},
    {0x301B, 0x83}, //full size =77 , binning size = 83 greg0315 
    {0x3017, 0x84}, //full size =94 , binning size = 84 greg0315
    //{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e1_2nd_snap_settings[] = {
    {0x0100, 0x00},
        
    /*[20120314][4E1EVT3][MIPI2lane][Mclk=24Mhz-440Mbps][2608x1960][15fps][non continuous]*/
    // Integration setting ... 
    /*Timing configuration*/
    {0x0202, 0x04},
    {0x0203, 0x12},
    {0x0204, 0x00},
    {0x0205, 0x80},
    // Frame Length
    {0x0340, 0x07},
    {0x0341, 0xef},
    // Line Length
    {0x0342, 0x0A}, //2738
    {0x0343, 0xF0},

    // PLL setting ...
    //// input clock 24MHz
    ////// (3) MIPI 1-lane Serial(TST = 0000b or TST = 0010b), 15 fps
    {0x0305, 0x04},
    {0x0306, 0x00},
    {0x0307, 0x49}, //66:612MHz, 46:440MHz
    {0x30B5, 0x01},
    {0x30E2, 0x02}, //num lanes[1:0] = 2
    {0x30F1, 0xA0},
    {0x30e8, 0x07}, //default =0f continuous ; 07= non-continuous

    // MIPI Size Setting
    //2608 x 1960
    {0x30A9, 0x01}, //03greg0315 //Horizontal Binning Off 
    {0x300E, 0xE9}, //E8greg0315  //Vertical Binning Off  
    {0x0387, 0x01}, //y_odd_inc
    {0x0344, 0x00}, //x_addr_start 0
    {0x0345, 0x00},
    {0x0348, 0x0A}, //x_addr_end 2607
    {0x0349, 0x2F},
    {0x0346, 0x00}, //y_addr_start 0
    {0x0347, 0x00},
    {0x034A, 0x07}, //y_addr_end 1959
    {0x034B, 0xA7},
    {0x0380, 0x00}, //x_even_inc 1
    {0x0381, 0x01},
    {0x0382, 0x00}, //x_odd_inc 1
    {0x0383, 0x01},
    {0x0384, 0x00}, //y_even_inc 1
    {0x0385, 0x01},
    {0x0386, 0x00}, //y_odd_inc 1
    {0x0387, 0x01},
    {0x034C, 0x0A}, //x_output_size 2608
    {0x034D, 0x30},
    {0x034E, 0x07}, //y_output_size 1960
    {0x034F, 0xA8},
    {0x30BF, 0xAB}, //outif_enable[7], data_type[5:0](2Bh = bayer 10bit)
    {0x30C0, 0x80}, //video_offset[7:4] 3260%12
    {0x30C8, 0x0C}, //video_data_length 3260 = 2608 * 1.25
    {0x30C9, 0xBC},
    {0x301B, 0x77}, //full size =77 , binning size = 83 greg0315 
    {0x3017, 0x94}, //full size =94 , binning size = 84 greg0315 
    //{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e1_2nd_recommend_settings[] = {
    {0x0100, 0x00},
    {0x3030, 0x06},
    
    /* [4E1EVT3][GlobalV1][MIPI2lane][Mclk=24Mhz] */
    /* Reset setting */
    //{0x0103, 0x01},

    // Analog Setting
    //// CDS timing setting ...                                    
    {0x3000, 0x05},
    {0x3001, 0x03},
    {0x3002, 0x08},
    {0x3003, 0x09},
    {0x3004, 0x2E},
    {0x3005, 0x06},
    {0x3006, 0x34},
    {0x3007, 0x00},
    {0x3008, 0x3C},
    {0x3009, 0x3C},
    {0x300A, 0x28},
    {0x300B, 0x04},
    {0x300C, 0x0A},
    {0x300D, 0x02},
    {0x300F, 0x82},
    {0x3010, 0x00},
    {0x3011, 0x4C},
    {0x3012, 0x30},
    {0x3013, 0xC0},
    {0x3014, 0x00},
    {0x3015, 0x00},
    {0x3016, 0x2C},
    {0x3017, 0x94},
    {0x3018, 0x78},
    //{0x301B, 0x77},	 //full size =77 , binning size = 83
    {0x301C, 0x04},
    {0x301D, 0xD4},
    {0x3021, 0x02},
    {0x3022, 0x24},
    {0x3024, 0x40},
    {0x3027, 0x08},
    {0x3029, 0xC6},
    {0x30BC, 0x98},
    {0x302B, 0x01},
    {0x30D8, 0x3F},

    // ADLC setting ...
    {0x3070, 0x5F},
    {0x3071, 0x00},
    {0x3080, 0x04},
    {0x3081, 0x38},

    // MIPI setting
    {0x30BD, 0x00}, //SEL_CCP[0]
    {0x3084, 0x15}, //SYNC Mode
    {0x30BE, 0x1A}, //M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]
    {0x30C1, 0x01}, //pack video enable [0]
    {0x3111, 0x86}, //Embedded data off [5]

    //For MIPI T8 T9 add by greg0809 
    {0x30E3, 0x38}, //outif_mld_ulpm_rxinit_limit[15:8]                                          
    {0x30E4, 0x40}, //outif_mld_ulpm_rxinit_limit[7:0]                                           
    {0x3113, 0x70}, //outif_enable_time[15:8]                                                    
    {0x3114, 0x80}, //outif_enable_time[7:0]                                                     
    {0x3115, 0x7B}, //streaming_enalbe_time[15:8]                                                
    {0x3116, 0xC0}, //streaming_enalbe_time[7:0]                                                 
    {0x30EE, 0x12}, //[5:4]esc_ref_div, [3] dphy_ulps_auto, [1]dphy_enable  
    {0x0101, 0x03}, //image_orientation, mirror & flip on //MM-UW-Change slave address-00+{
};

static struct v4l2_subdev_info s5k4e1_2nd_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array s5k4e1_2nd_init_conf[] = {
	{&s5k4e1_2nd_recommend_settings[0],
	ARRAY_SIZE(s5k4e1_2nd_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array s5k4e1_2nd_confs[] = {
	{&s5k4e1_2nd_snap_settings[0],
	ARRAY_SIZE(s5k4e1_2nd_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k4e1_2nd_prev_settings[0],
	ARRAY_SIZE(s5k4e1_2nd_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

//FIH-SW-MM-MC-UpdateCameraSettingForS5K4E1_2ND-00*{
static struct msm_sensor_output_info_t s5k4e1_2nd_dimensions[] = {
	{
		.x_output = 0xA30,//2608
		.y_output = 0x7A8,//1960
		.line_length_pclk = 0xAF0,//2800
		.frame_length_lines = 0x7EF,//2031
		//vt_pixel_clk* op_pixel_clk*fps
		.vt_pixel_clk = 85300000,//85302000
		.op_pixel_clk = 85300000,//85302000
		.binning_factor = 0,
	},
	{
		.x_output = 0x518,//1304
		.y_output = 0x3D4,//980
		.line_length_pclk = 0xD18,//3352
		.frame_length_lines = 0x4C1,//1217
		.vt_pixel_clk = 122400000,//
		.op_pixel_clk = 122400000,//
		.binning_factor = 1,
	},
};
//FIH-SW-MM-MC-UpdateCameraSettingForS5K4E1_2ND-00*}

static struct msm_sensor_output_reg_addr_t s5k4e1_2nd_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t s5k4e1_2nd_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x4E10,
};

static struct msm_sensor_exp_gain_info_t s5k4e1_2nd_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,
	.vert_offset = 4,
};

static inline uint8_t s5k4e1_2nd_byte(uint16_t word, uint8_t offset)
{
	return word >> (offset * BITS_PER_BYTE);
}

static int32_t s5k4e1_2nd_write_prev_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
						uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	int32_t rc = 0;
	static uint32_t fl_lines, offset;

	pr_info("s5k4e1_2nd_write_prev_exp_gain :%d %d\n", gain, line);

	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

	/* Analogue Gain */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr,
		s5k4e1_2nd_byte(gain, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr + 1,
		s5k4e1_2nd_byte(gain, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);

	if (line > (s_ctrl->curr_frame_length_lines - offset)) {
		fl_lines = line + offset;
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			s5k4e1_2nd_byte(fl_lines, MSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines + 1,
			s5k4e1_2nd_byte(fl_lines, LSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			s5k4e1_2nd_byte(line, MSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
			s5k4e1_2nd_byte(line, LSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	} else if (line < (fl_lines - offset)) {
		fl_lines = line + offset;
		if (fl_lines < s_ctrl->curr_frame_length_lines)
			fl_lines = s_ctrl->curr_frame_length_lines;

		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			s5k4e1_2nd_byte(line, MSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
			s5k4e1_2nd_byte(line, LSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			s5k4e1_2nd_byte(fl_lines, MSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines + 1,
			s5k4e1_2nd_byte(fl_lines, LSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	} else {
		fl_lines = line+4;
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			s5k4e1_2nd_byte(line, MSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
			s5k4e1_2nd_byte(line, LSB),
			MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	}
	return rc;
}

static int32_t s5k4e1_2nd_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	uint16_t min_ll_pck = 0x0AB2;
	uint32_t ll_pck, fl_lines;
	uint32_t ll_ratio;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t ll_pck_msb, ll_pck_lsb;

	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

	pr_info("s5k4e1_2nd_write_exp_gain : gain = %d line = %d\n", gain, line);
	line = (uint32_t) (line * s_ctrl->fps_divider);
	fl_lines = s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider / Q10;
	ll_pck = s_ctrl->curr_line_length_pclk;

	if (fl_lines < (line / Q10))
		ll_ratio = (line / (fl_lines - 4));
	else
		ll_ratio = Q10;

	ll_pck = ll_pck * ll_ratio / Q10;
	line = line / ll_ratio;
	if (ll_pck < min_ll_pck)
		ll_pck = min_ll_pck;

	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);

	intg_time_msb = (uint8_t) ((line & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (line & 0x00FF);

	ll_pck_msb = (uint8_t) ((ll_pck & 0xFF00) >> 8);
	ll_pck_lsb = (uint8_t) (ll_pck & 0x00FF);

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr,
		gain_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr + 1,
		gain_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk,
		ll_pck_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk + 1,
		ll_pck_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* Coarse Integration Time */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		intg_time_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
		intg_time_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

	return 0;
}

//FIH-SW-MM-MC-BringUpCameraRawSensorS5k4e1-00+{
static struct msm_cam_clk_info cam_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_cam_clk_info cam_8974_clk_info[] = {
	{"cam_src_clk", 19200000},
	{"cam_clk", -1},
};

int32_t s5k4e1_2nd_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;
	
	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->sensor_device_type == MSM_SENSOR_I2C_DEVICE) {
		if (s_ctrl->clk_rate != 0)
			cam_8960_clk_info->clk_rate = s_ctrl->clk_rate;

		rc = msm_cam_clk_enable(dev, cam_8960_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_8960_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: clk enable failed\n", __func__);
			goto enable_clk_failed;
		}
	} else {
		rc = msm_cam_clk_enable(dev, cam_8974_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_8974_clk_info), 1);
		if (rc < 0) {
			pr_err("%s: clk enable failed\n", __func__);
			goto enable_clk_failed;
		}
	}

	if (!s_ctrl->power_seq_delay)
		usleep_range(1000, 2000);
	else if (s_ctrl->power_seq_delay < 20)
		usleep_range((s_ctrl->power_seq_delay * 1000),
			((s_ctrl->power_seq_delay * 1000) + 1000));
	else
		msleep(s_ctrl->power_seq_delay);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE) {
		rc = msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto cci_init_failed;
		}
	}
	s_ctrl->curr_res = MSM_SENSOR_INVALID_RES;
	return rc;

cci_init_failed:
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:

	return rc;
}

int32_t s5k4e1_2nd_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct device *dev = NULL;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE)
		dev = &s_ctrl->pdev->dev;
	else
		dev = &s_ctrl->sensor_i2c_client->client->dev;
	if (s_ctrl->sensor_device_type == MSM_SENSOR_PLATFORM_DEVICE) {
		msm_sensor_cci_util(s_ctrl->sensor_i2c_client,
			MSM_CCI_RELEASE);
	}

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
	if (s_ctrl->sensor_device_type == MSM_SENSOR_I2C_DEVICE)
		msm_cam_clk_enable(dev, cam_8960_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_8960_clk_info), 0);
	else
		msm_cam_clk_enable(dev, cam_8974_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_8974_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);

	msm_camera_request_gpio_table(data, 0);
	return 0;
}

//MM-UW-Change slave address-00+{
int32_t s5k4e1_2nd_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *s_info  = NULL;
    struct msm_sensor_ctrl_t *s_ctrl       = NULL;

   //MM-MC-ImplementReadOtpDataFeature-00*{
    if (g_IsFihMainCamProbe == 1)
    {
        printk("s5k4e1_2nd_sensor_i2c_probe: Main camera has probe success before !\n");
        return -EFAULT;
    }

    s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	rc = msm_sensor_i2c_probe(client, id);//Probe for 0x20
    if(rc < 0){
#if 0/* MM-MC-FixMainCameraI2cBusy-00-{ */
        s_ctrl->sensor_i2c_addr = 0x6C;
        rc = msm_sensor_i2c_probe(client, id);//Probe for 0x6c
        if(rc < 0)
            return rc;
#else
        pr_err("%s %s_i2c_probe failed\n", __func__, client->name);
        return rc;
#endif/* MM-MC-FixMainCameraI2cBusy-00-} */
    }
    g_IsFihMainCamProbe = 1;
    printk("s5k4e1_2nd_sensor_i2c_probe: sensor_i2c_addr(0x20/0x6c) = 0x%x, g_IsFihMainCamProbe = %d.\n", s_ctrl->sensor_i2c_addr, g_IsFihMainCamProbe);
    //MM-MC-ImplementReadOtpDataFeature-00*}

	s_info = client->dev.platform_data;
	if (s_info == NULL) {
		pr_err("%s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

	if (s_info->actuator_info->vcm_enable) {
		rc = gpio_request(s_info->actuator_info->vcm_pwd,
				"msm_actuator");
		if (rc < 0)
			pr_err("%s: gpio_request:msm_actuator %d failed\n",
				__func__, s_info->actuator_info->vcm_pwd);
		rc = gpio_direction_output(s_info->actuator_info->vcm_pwd, 0);
		if (rc < 0)
			pr_err("%s: gpio:msm_actuator %d direction can't be set\n",
				__func__, s_info->actuator_info->vcm_pwd);
		gpio_free(s_info->actuator_info->vcm_pwd);
	}

	return rc;
}
//MM-UW-Change slave address-00+}

//MM-MC-ImplementReadOtpDataFeature-00+{
int32_t S5k4e1_2nd_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
    int32_t productid = 0;
	uint16_t chipid = 0;
    
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensor_id_info->sensor_id);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("S5k4e1_2nd_match_id: chip id doesnot match\n");
		return -ENODEV;
	}

    if (s_ctrl->sensor_i2c_addr == 0x20)/* MM-MC-ImplementOtpReadFunctionForS5k4E1_2ND-00* */
    {
        rc = fih_init_otp(s_ctrl);
        if (rc < 0)
            return rc;

        rc = fih_get_otp_data(OTP_MI_PID , &productid);
        if (rc < 0)
            return rc;
        
        if (productid != PID_S5K4E1_2ND)
        {
            pr_err("S5k4e1_2nd_match_id: Product id doesnot match\n");
            return -ENODEV;
        }
    }
    
	return rc;
}
//MM-MC-ImplementReadOtpDataFeature-00+}

static enum msm_camera_vreg_name_t s5k4e1_2nd_veg_seq[] = {
	CAM_VDIG,
	CAM_VIO,
	CAM_VANA,
	CAM_VAF,
};

static const struct i2c_device_id s5k4e1_2nd_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k4e1_2nd_s_ctrl},
	{ }
};

static struct i2c_driver s5k4e1_2nd_i2c_driver = {
	.id_table = s5k4e1_2nd_i2c_id,
	.probe  = s5k4e1_2nd_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k4e1_2nd_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&s5k4e1_2nd_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k4e1_2nd_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k4e1_2nd_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k4e1_2nd_subdev_ops = {
	.core = &s5k4e1_2nd_subdev_core_ops,
	.video  = &s5k4e1_2nd_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k4e1_2nd_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = s5k4e1_2nd_write_prev_exp_gain,
	.sensor_write_snapshot_exp_gain = s5k4e1_2nd_write_pict_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = s5k4e1_2nd_power_up,//,msm_sensor_power_up,//FIH-SW-MM-MC-BringUpCameraRawSensorS5k4e1-00*
	.sensor_power_down = s5k4e1_2nd_power_down,//msm_sensor_power_down,//FIH-SW-MM-MC-BringUpCameraRawSensorS5k4e1-00*
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_match_id = S5k4e1_2nd_match_id,//MM-MC-ImplementReadOtpDataFeature-00+
};

static struct msm_sensor_reg_t s5k4e1_2nd_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = s5k4e1_2nd_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k4e1_2nd_start_settings),
	.stop_stream_conf = s5k4e1_2nd_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k4e1_2nd_stop_settings),
	.group_hold_on_conf = s5k4e1_2nd_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(s5k4e1_2nd_groupon_settings),
	.group_hold_off_conf = s5k4e1_2nd_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(s5k4e1_2nd_groupoff_settings),
	.init_settings = &s5k4e1_2nd_init_conf[0],
	.init_size = ARRAY_SIZE(s5k4e1_2nd_init_conf),
	.mode_settings = &s5k4e1_2nd_confs[0],
	.output_settings = &s5k4e1_2nd_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k4e1_2nd_confs),
};

static struct msm_sensor_ctrl_t s5k4e1_2nd_s_ctrl = {
	.msm_sensor_reg = &s5k4e1_2nd_regs,
	.sensor_i2c_client = &s5k4e1_2nd_sensor_i2c_client,
	.sensor_i2c_addr = 0x20, //0x6C
	.vreg_seq = s5k4e1_2nd_veg_seq,
	.num_vreg_seq = ARRAY_SIZE(s5k4e1_2nd_veg_seq),
	.sensor_output_reg_addr = &s5k4e1_2nd_reg_addr,
	.sensor_id_info = &s5k4e1_2nd_id_info,
	.sensor_exp_gain_info = &s5k4e1_2nd_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &s5k4e1_2nd_mut,
	.sensor_i2c_driver = &s5k4e1_2nd_i2c_driver,
	.sensor_v4l2_subdev_info = s5k4e1_2nd_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4e1_2nd_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k4e1_2nd_subdev_ops,
	.func_tbl = &s5k4e1_2nd_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Samsung 5MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
