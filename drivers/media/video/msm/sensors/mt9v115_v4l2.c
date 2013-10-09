/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 * Copyright(C) 2013 Foxconn International Holdings, Ltd. All rights 
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
#define SENSOR_NAME "mt9v115"
#define PLATFORM_DRIVER_NAME "msm_camera_mt9v115"
#define mt9v115_obj mt9v115_##obj

DEFINE_MUTEX(mt9v115_mut);
static struct msm_sensor_ctrl_t mt9v115_s_ctrl;

static struct msm_camera_i2c_reg_conf mt9v115_start_settings[] = {
	{0x8400, 0x02, MSM_CAMERA_I2C_BYTE_DATA, MSM_CAMERA_I2C_CMD_WRITE},
	{0x8401, 0x02, MSM_CAMERA_I2C_BYTE_DATA, MSM_CAMERA_I2C_CMD_POLL},
};

static struct msm_camera_i2c_reg_conf mt9v115_stop_settings[] = {
	{0x8400, 0x01, MSM_CAMERA_I2C_BYTE_DATA, MSM_CAMERA_I2C_CMD_WRITE},
	{0x8401, 0x01, MSM_CAMERA_I2C_BYTE_DATA, MSM_CAMERA_I2C_CMD_POLL},
};

static struct msm_camera_i2c_reg_conf mt9v115_recommend_part1_settings[] = {
    /* Porting revision = 0701 */
    { 0x001A, 0x0106},
    { 0x0010, 0x052C},
    { 0x0012, 0x0800},
    { 0x0014, 0x2047},    
    { 0x0014, 0x2046},    
    { 0x0018, 0x4505},    
    { 0x0018, 0x4504},
};

static struct msm_camera_i2c_reg_conf mt9v115_recommend_part2_settings[] = {
    { 0x0042, 0xFFF3},
    { 0x3C00, 0x0004},
    { 0x001A, 0x0520},
    { 0x001A, 0x0564},
};

static struct msm_camera_i2c_reg_conf mt9v115_recommend_part3_settings[] = {
    { 0x0012, 0x0200}, 
    { 0x300A, 0x01F9}, 
    { 0x300C, 0x02D6}, 
    { 0x3010, 0x0012}, 
    { 0x3040, 0x0041}, 
    { 0x098E, 0x9803},
    { 0x9803, 0x0730}, 
    { 0xA06E, 0x0098}, 
    { 0xA070, 0x007E}, 
    { 0xA072, 0x1113}, 
    { 0xA074, 0x1416}, 
    { 0xA076, 0x000D}, 
    { 0xA078, 0x0010},
    { 0xA01A, 0x000D},
};

static struct msm_camera_i2c_reg_conf mt9v115_full_settings[] = {
	/*sub_init_settings*/
    //Char_settings
    { 0x3168, 0x84F8},
    { 0x316A, 0x028A},
    { 0x316C, 0xB477},
    { 0x316E, 0x8268},
    { 0x3180, 0x87FF},
    { 0x3E02, 0x0600},
    { 0x3E04, 0x221C},
    { 0x3E06, 0x3632},
    { 0x3E08, 0x3204},
    { 0x3E0A, 0x3106},
    { 0x3E0C, 0x3025},
    { 0x3E0E, 0x190B},
    { 0x3E10, 0x0700},
    { 0x3E12, 0x24FF},
    { 0x3E14, 0x3731},
    { 0x3E16, 0x0401},
    { 0x3E18, 0x211E},
    { 0x3E1A, 0x3633},
    { 0x3E1C, 0x3107},
    { 0x3E1E, 0x1A16},
    { 0x3E20, 0x312D},
    { 0x3E22, 0x3303},
    { 0x3E24, 0x1401},
    { 0x3E26, 0x0600},
    { 0x3E30, 0x0037},
    { 0x3E32, 0x1638},
    { 0x3E90, 0x0E05},
    { 0x3E92, 0x1310},
    { 0x3E94, 0x0904},
    { 0x3E96, 0x0B00},
    { 0x3E98, 0x130B},
    { 0x3E9A, 0x0C06},
    { 0x3E9C, 0x1411},
    { 0x3E9E, 0x0E01},
    { 0x3ECC, 0x4091},
    { 0x3ECE, 0x430D},
    { 0x3ED0, 0x1817},
    { 0x3ED2, 0x8504},
    //load_and_go patch1
    { 0x0982, 0x0000},
    { 0x098A, 0x0000},
    { 0x8251, 0x3C3C},
    { 0x8253, 0xBDD1},
    { 0x8255, 0xF2D6},
    { 0x8257, 0x15C1},
    { 0x8259, 0x0126},
    { 0x825B, 0x3ADC},
    { 0x825D, 0x0A30},
    { 0x825F, 0xED02},
    { 0x8261, 0xDC08},
    { 0x8263, 0xED00},
    { 0x8265, 0xFC01},
    { 0x8267, 0xFCBD},
    { 0x8269, 0xF5FC},
    { 0x826B, 0x30EC},
    { 0x826D, 0x02FD},
    { 0x826F, 0x0344},
    { 0x8271, 0xB303},
    { 0x8273, 0x4025},
    { 0x8275, 0x0DCC},
    { 0x8277, 0x3180},
    { 0x8279, 0xED00},
    { 0x827B, 0xCCA0},
    { 0x827D, 0x00BD},
    { 0x827F, 0xFBFB},
    { 0x8281, 0x2013},
    { 0x8283, 0xFC03},
    { 0x8285, 0x44B3},
    { 0x8287, 0x0342},
    { 0x8289, 0x220B},
    { 0x828B, 0xCC31},
    { 0x828D, 0x80ED},
    { 0x828F, 0x00CC},
    { 0x8291, 0xA000},
    { 0x8293, 0xBDFC},
    { 0x8295, 0x1738},
    { 0x8297, 0x3839},
    { 0x8299, 0x3CD6},
    { 0x829B, 0x15C1},
    { 0x829D, 0x0126},
    { 0x829F, 0x70FC},
    { 0x82A1, 0x0344},
    { 0x82A3, 0xB303},
    { 0x82A5, 0x4025},
    { 0x82A7, 0x13FC},
    { 0x82A9, 0x7E26},
    { 0x82AB, 0x83FF},
    { 0x82AD, 0xFF27},
    { 0x82AF, 0x0BFC},
    { 0x82B1, 0x7E26},
    { 0x82B3, 0xFD03},
    { 0x82B5, 0x4CCC},
    { 0x82B7, 0xFFFF},
    { 0x82B9, 0x2013},
    { 0x82BB, 0xFC03},
    { 0x82BD, 0x44B3},
    { 0x82BF, 0x0342},
    { 0x82C1, 0x220E},
    { 0x82C3, 0xFC7E},
    { 0x82C5, 0x2683},
    { 0x82C7, 0xFFFF},
    { 0x82C9, 0x2606},
    { 0x82CB, 0xFC03},
    { 0x82CD, 0x4CFD},
    { 0x82CF, 0x7E26},
    { 0x82D1, 0xFC7E},
    { 0x82D3, 0x2683},
    { 0x82D5, 0xFFFF},
    { 0x82D7, 0x2605},
    { 0x82D9, 0xFC03},
    { 0x82DB, 0x4A20},
    { 0x82DD, 0x03FC},
    { 0x82DF, 0x0348},
    { 0x82E1, 0xFD7E},
    { 0x82E3, 0xD0FC},
    { 0x82E5, 0x7ED2},
    { 0x82E7, 0x5F84},
    { 0x82E9, 0xF030},
    { 0x82EB, 0xED00},
    { 0x82ED, 0xDC0A},
    { 0x82EF, 0xB303},
    { 0x82F1, 0x4625},
    { 0x82F3, 0x10EC},
    { 0x82F5, 0x0027},
    { 0x82F7, 0x0CFD},
    { 0x82F9, 0x034E},
    { 0x82FB, 0xFC7E},
    { 0x82FD, 0xD284},
    { 0x82FF, 0x0FED},
    { 0x8301, 0x0020},
    { 0x8303, 0x19DC},
    { 0x8305, 0x0AB3},
    { 0x8307, 0x0346},
    { 0x8309, 0x2415},
    { 0x830B, 0xEC00},
    { 0x830D, 0x8300},
    { 0x830F, 0x0026},
    { 0x8311, 0x0EFC},
    { 0x8313, 0x7ED2},
    { 0x8315, 0x840F},
    { 0x8317, 0xFA03},
    { 0x8319, 0x4FBA},
    { 0x831B, 0x034E},
    { 0x831D, 0xFD7E},
    { 0x831F, 0xD2BD},
    { 0x8321, 0xD2AD},
    { 0x8323, 0x3839},
    { 0x098E, 0x0000},
    //patch1 thresholds
    { 0x0982, 0x0000},
    { 0x098A, 0x0000},
    { 0x8340, 0x0048},
    { 0x8342, 0x0040},
    { 0x8344, 0x0000},
    { 0x8346, 0x0040},
    { 0x8348, 0x1817},
    { 0x834A, 0x1857},
    { 0x098E, 0x0000},
    //enable patch1
    { 0x0982, 0x0000},
    { 0x098A, 0x0000},
    { 0x824D, 0x0251},
    { 0x824F, 0x0299},
    { 0x098E, 0x0000},
    //Lens Correction 85%
    { 0x3210, 0x00B8},
    { 0x3640, 0x0030},
    { 0x3642, 0x3608},
    { 0x3644, 0x24F0},
    { 0x3646, 0xFC0D},
    { 0x3648, 0xB94C},
    { 0x364A, 0x0090},
    { 0x364C, 0x26A8},
    { 0x364E, 0x3250},
    { 0x3650, 0xA5AD},
    { 0x3652, 0xB82A},
    { 0x3654, 0x0090},
    { 0x3656, 0x88AC},
    { 0x3658, 0x0190},
    { 0x365A, 0xA76B},
    { 0x365C, 0x872C},
    { 0x365E, 0x01F0},
    { 0x3660, 0x0D0A},
    { 0x3662, 0x26B0},
    { 0x3664, 0xF10D},
    { 0x3666, 0x988C},
    { 0x3680, 0x670B},
    { 0x3682, 0xEDC9},
    { 0x3684, 0x720A},
    { 0x3686, 0xD92D},
    { 0x3688, 0xDCED},
    { 0x368A, 0x54EB},
    { 0x368C, 0x1084},
    { 0x368E, 0x3408},
    { 0x3690, 0xE00D},
    { 0x3692, 0x8E6E},
    { 0x3694, 0x230B},
    { 0x3696, 0x8826},
    { 0x3698, 0x418A},
    { 0x369A, 0xC6CD},
    { 0x369C, 0x950B},
    { 0x369E, 0x510B},
    { 0x36A0, 0xE5A4},
    { 0x36A2, 0x48EB},
    { 0x36A4, 0xE9AD},
    { 0x36A6, 0xAE4D},
    { 0x36C0, 0x1AB0},
    { 0x36C2, 0xA1AE},
    { 0x36C4, 0x9650},
    { 0x36C6, 0xCB0E},
    { 0x36C8, 0x3AF1},
    { 0x36CA, 0x2330},
    { 0x36CC, 0xCC2C},
    { 0x36CE, 0x7C0E},
    { 0x36D0, 0xCCCC},
    { 0x36D2, 0x57AE},
    { 0x36D4, 0x0D10},
    { 0x36D6, 0x336D},
    { 0x36D8, 0x8A4E},
    { 0x36DA, 0xE590},
    { 0x36DC, 0x8AB0},
    { 0x36DE, 0x1670},
    { 0x36E0, 0xE24E},
    { 0x36E2, 0x9C2E},
    { 0x36E4, 0x196E},
    { 0x36E6, 0x0470},
    { 0x3700, 0xF98A},
    { 0x3702, 0x060A},
    { 0x3704, 0x45CA},
    { 0x3706, 0xE64A},
    { 0x3708, 0x7ACD},
    { 0x370A, 0xB64C},
    { 0x370C, 0x9D0D},
    { 0x370E, 0x87ED},
    { 0x3710, 0x2F8E},
    { 0x3712, 0x0BF0},
    { 0x3714, 0xAF6A},
    { 0x3716, 0x83ED},
    { 0x3718, 0x576C},
    { 0x371A, 0x088D},
    { 0x371C, 0xDD8F},
    { 0x371E, 0x080C},
    { 0x3720, 0x950D},
    { 0x3722, 0xAE6E},
    { 0x3724, 0x08EF},
    { 0x3726, 0x0670},
    { 0x3740, 0xE86D},
    { 0x3742, 0x4E4E},
    { 0x3744, 0x2B92},
    { 0x3746, 0x5F2F},
    { 0x3748, 0x83D4},
    { 0x374A, 0x30A9},
    { 0x374C, 0x9CCD},
    { 0x374E, 0xD2AF},
    { 0x3750, 0xBF30},
    { 0x3752, 0xC872},
    { 0x3754, 0xCBAF},
    { 0x3756, 0x90B0},
    { 0x3758, 0x222B},
    { 0x375A, 0x3C72},
    { 0x375C, 0x11D1},
    { 0x375E, 0xAAAC},
    { 0x3760, 0x30D0},
    { 0x3762, 0x5EB0},
    { 0x3764, 0xF051},
    { 0x3766, 0xA4F3},
    { 0x3782, 0x00F0},
    { 0x3784, 0x0144},
    { 0x3210, 0x00B8},
    { 0x3210, 0x00B8},
	//AWB_CCM
    { 0x9416, 0x3895},
    { 0x9418, 0x1561},
    { 0x2110, 0x0024},
	//Load = Color Correction Matrices for AAC
    { 0xA02F, 0x0201},
    { 0xA031, 0xFEC9},
    { 0xA033, 0x0036},
    { 0xA035, 0xFFCB},
    { 0xA037, 0x0133},
    { 0xA039, 0x0001},
    { 0xA03B, 0xFFA4},
    { 0xA03D, 0xFF10},
    { 0xA03F, 0x024C},
    { 0xA041, 0x001B},
    { 0xA043, 0x004B},
    { 0xA045, 0xFFEC},
    { 0xA047, 0x008A},
    { 0xA049, 0xFF8A},
    { 0xA04B, 0xFFDD},
    { 0xA04D, 0x0044},
    { 0xA04F, 0xFFDF},
    { 0xA051, 0x0049},
    { 0xA053, 0x0071},
    { 0xA055, 0xFF46},
    { 0xA057, 0x0011},
    { 0xA059, 0xFFD8},
    { 0xA061, 0x003C},
    { 0xA063, 0x0035},
    { 0xA065, 0x0302},
    { 0x2112, 0x9C2B},
    { 0x2114, 0xF7D9},
    { 0x2116, 0x1FC9},
    { 0x2118, 0x2CA4},
    { 0x211A, 0xFB9A},
    { 0x211C, 0xDF46},
    { 0x211E, 0xCBEE},
    { 0x2120, 0x00C7},
    //Load = Color Correction for AAC
    { 0x098E, 0xA06D},
    { 0xA06D, 0x7400},
    { 0x098E, 0xA068},
    { 0xA068, 0x7080},
    //PA Settings
    { 0xA020, 0x4600},
    { 0xA07A, 0x0114},
    { 0xA081, 0x1E50},
    { 0xA0B1, 0x105A},
    { 0xA0B3, 0x105A},
    { 0xA0B5, 0x105A},
    { 0xA0B7, 0x1050},
    { 0xA05F, 0x9600},
    { 0xA0B9, 0x0028},
    { 0xA0BB, 0x0080},
    { 0xA085, 0x0008},
    { 0xA07C, 0x0701},
    //AE Settings
    { 0x9003, 0x2001},
    { 0xA027, 0x002A},
    { 0xA029, 0x0110},
    { 0xA01C, 0x0060},
    { 0xA023, 0x0080},
    { 0xA025, 0x0080},
    { 0xA01E, 0x0080},
    //CPIPE_Preference
    { 0x326E, 0x0006},
    { 0x33F4, 0x000B},
    { 0x35A2, 0x00B2},
    { 0x35A4, 0x0594},
    { 0xA087, 0x000D},
    { 0xA089, 0x223A},
    { 0xA08B, 0x5972},
    { 0xA08D, 0x8799},
    { 0xA08F, 0xA8B5},
    { 0xA091, 0xC1CB},
    { 0xA093, 0xD4DD},
    { 0xA095, 0xE5EC},
    { 0xA097, 0xF3F9},
    { 0xA099, 0xFF00},
    { 0xA0AD, 0x0003},
    { 0xA0AF, 0x0008},
    { 0x9003, 0x2001},
    { 0x8C00, 0x0201},
    { 0x8C03, 0x0103},
    { 0x8C05, 0x0500},
    { 0x329E, 0x0000},
    { 0x0018, 0x0002},
};

static struct v4l2_subdev_info mt9v115_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array mt9v115_init_conf[] = {
	{mt9v115_recommend_part1_settings,
	ARRAY_SIZE(mt9v115_recommend_part1_settings), 100, MSM_CAMERA_I2C_WORD_DATA},
    {mt9v115_recommend_part2_settings,
    ARRAY_SIZE(mt9v115_recommend_part2_settings), 50, MSM_CAMERA_I2C_WORD_DATA},
    {mt9v115_recommend_part3_settings,
    ARRAY_SIZE(mt9v115_recommend_part3_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_camera_i2c_conf_array mt9v115_confs[] = {
	{mt9v115_full_settings,
	ARRAY_SIZE(mt9v115_full_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

/* VFE maximum clock is 266 MHz.
    For sensor bringup you can keep .op_pixel_clk = maximum VFE clock.
    and .vt_pixel_clk = line_length_pclk * frame_length_lines * FPS.         */
static struct msm_sensor_output_info_t mt9v115_dimensions[] = {
	/* 30 fps */
	{
		.x_output = 0x280, //640
		.y_output = 0x1E0, //480
		.line_length_pclk = 0x02D6,   //726
		.frame_length_lines = 0x01F9, //505
        .vt_pixel_clk = 11000000,//FIH-SW-MM-MC-BringUpCameraYUVSensorForHM03D5-00*
        .op_pixel_clk = 22000000,//FIH-SW-MM-MC-BringUpCameraYUVSensorForHM03D5-00*
		.binning_factor = 1,
	},
};

static struct msm_cam_clk_info cam_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_cam_clk_info cam_8974_clk_info[] = {
	{"cam_src_clk", 19200000},
	{"cam_clk", -1},
};

int32_t mt9v115_power_up(struct msm_sensor_ctrl_t *s_ctrl)
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

int32_t mt9v115_power_down(struct msm_sensor_ctrl_t *s_ctrl)
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

static struct msm_sensor_output_reg_addr_t mt9v115_reg_addr = {
	.x_output = 0xA000,
	.y_output = 0xA002,
	.line_length_pclk = 0x300C,
	.frame_length_lines = 0x300A,
};

static enum msm_camera_vreg_name_t mt9v115_veg_seq[] = {
	CAM_VIO,
	CAM_VDIG,
	CAM_VANA,
};

static struct msm_sensor_id_info_t mt9v115_id_info = {
    .sensor_id_reg_addr = 0x0000,
    .sensor_id = 0x2284,
};

static const struct i2c_device_id mt9v115_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&mt9v115_s_ctrl},
	{ }
};

static struct i2c_driver mt9v115_i2c_driver = {
	.id_table = mt9v115_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9v115_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&mt9v115_i2c_driver);
}

static struct v4l2_subdev_core_ops mt9v115_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops mt9v115_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops mt9v115_subdev_ops = {
	.core = &mt9v115_subdev_core_ops,
	.video  = &mt9v115_subdev_video_ops,
};

static struct msm_sensor_fn_t mt9v115_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_setting = msm_sensor_setting,
    .sensor_csi_setting = msm_sensor_setting1,//No use.
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = mt9v115_power_up,
	.sensor_power_down = mt9v115_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t mt9v115_regs = {
	.default_data_type = MSM_CAMERA_I2C_WORD_DATA,
	.start_stream_conf = mt9v115_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(mt9v115_start_settings),
    .stop_stream_conf = mt9v115_stop_settings,
    .stop_stream_conf_size = ARRAY_SIZE(mt9v115_stop_settings),
	.init_settings = &mt9v115_init_conf[0],
	.init_size = ARRAY_SIZE(mt9v115_init_conf),
	.mode_settings = &mt9v115_confs[0],
	.output_settings = &mt9v115_dimensions[0],
	.num_conf = ARRAY_SIZE(mt9v115_confs),
};

static struct msm_sensor_ctrl_t mt9v115_s_ctrl = {
	.msm_sensor_reg = &mt9v115_regs,
	.sensor_i2c_client = &mt9v115_sensor_i2c_client,
	.sensor_i2c_addr = 0x7A,
	.vreg_seq = mt9v115_veg_seq,
	.num_vreg_seq = ARRAY_SIZE(mt9v115_veg_seq),
	.sensor_output_reg_addr = &mt9v115_reg_addr,
	.sensor_id_info = &mt9v115_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.min_delay = 30,
	.power_seq_delay = 60,
	.msm_sensor_mutex = &mt9v115_mut,
	.sensor_i2c_driver = &mt9v115_i2c_driver,
	.sensor_v4l2_subdev_info = mt9v115_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9v115_subdev_info),
	.sensor_v4l2_subdev_ops = &mt9v115_subdev_ops,
	.func_tbl = &mt9v115_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Aptina VGA YUV sensor driver");
MODULE_LICENSE("GPL v2");
