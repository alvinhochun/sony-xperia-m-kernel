/* drivers/input/misc/cm36283.c - cm36283 optical sensors driver
 *
 * Copyright (C) 2012 Capella Microsystems Inc.
 * Copyright(C) 2013 Foxconn International Holdings, Ltd. All rights reserved.
 * Author: Capella
 *                                    
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <linux/cm36283.h>
#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/regulator/consumer.h>
/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#include <linux/fih_hw_info.h>
/*MTD-PERIPHERAL-CH-PS_conf00++]*/

#define D(x...) pr_info(x)

#define I2C_RETRY_COUNT 10

#define NEAR_DELAY_TIME ((100 * HZ) / 1000)

#define CONTROL_INT_ISR_REPORT        0x00
#define CONTROL_ALS                   0x01
#define CONTROL_PS                    0x02
static int record_init_fail = 0;
/*PERI-AC-Zero_Lux-Checking-00+{*/
static int clearcount=0;
/*PERI-AC-Zero_Lux-Checking-00+}*/
//static void sensor_irq_do_work(struct work_struct *work);
//static DECLARE_WORK(sensor_irq_work, sensor_irq_do_work);

struct cm36283_info {
//    struct class *cm36283_class;
//    struct device *ls_dev;
//    struct device *ps_dev;
    
    struct input_dev *ls_input_dev;
    struct input_dev *ps_input_dev;
    #ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend early_suspend;
    #endif
    struct i2c_client *i2c_client;
    struct workqueue_struct *lp_wq;
    struct delayed_work work;
    int intr_pin;
    int als_enable;
    int ps_enable;
    int ps_irq_flag;
    
    uint16_t *adc_table;
    uint16_t cali_table[10];
    int irq;
    
    int ls_calibrate;
    
    int (*power)(int, uint8_t); /* power to the chip */
    int (*init)(void);
    uint32_t als_kadc;
    uint32_t als_gadc;
    uint16_t golden_adc;
    
    struct wake_lock ps_wake_lock;
    int psensor_opened;
    int lightsensor_opened;
    uint8_t slave_addr;
    
    uint8_t ps_close_thd_set;
    uint8_t ps_away_thd_set;	
    int current_level;
    uint16_t current_adc;
    
    uint16_t ps_conf1_val;
    uint16_t ps_conf3_val;
    
    uint16_t ls_cmd;
    uint8_t record_clear_int_fail;
	struct regulator *vreg;
	uint16_t top_tresh;
	uint16_t bottom_tresh;
	int suspend_flag;
	int wakeup_flag;
/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#ifdef LS_cal
	struct cm36283_cali k_data;
#endif
/*MTD-PERIPHERAL-CH-PS_conf00++]*/
};
struct cm36283_info *lp_info;

static struct mutex als_enable_mutex, als_disable_mutex, als_get_adc_mutex;
static struct mutex ps_enable_mutex, ps_disable_mutex, ps_get_adc_mutex;
static struct mutex CM36283_control_mutex;
static int lightsensor_enable(struct cm36283_info *lpi);
static int lightsensor_disable(struct cm36283_info *lpi);
static int initial_cm36283(struct cm36283_info *lpi);
static void psensor_initial_cmd(struct cm36283_info *lpi);

int32_t als_kadc;

static int control_and_report(struct cm36283_info *lpi, uint8_t mode, uint16_t param);

static int I2C_RxData(uint16_t slaveAddr, uint8_t cmd, uint8_t *rxData, int length)
{
    uint8_t loop_i;
    int val;
    struct cm36283_info *lpi = lp_info;
    uint8_t subaddr[1];
    struct i2c_msg msgs[] = \
    {
        {
            .addr = slaveAddr,
            .flags = 0,
            .len = 1,
            .buf = subaddr,
        },
        {
            .addr = slaveAddr,
            .flags = I2C_M_RD,
            .len = length,
            .buf = rxData,
        },		 
    }; 
    subaddr[0] = cmd;	



    for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) 
    {
        if (i2c_transfer(lp_info->i2c_client->adapter, msgs, 2) > 0)
            break;
        
        val = gpio_get_value(lpi->intr_pin);
        /*check intr GPIO when i2c error*/
        if (loop_i == 0 || loop_i == I2C_RETRY_COUNT -1)
        	D("[PS][CM36283 error] %s, i2c err, slaveAddr 0x%x ISR gpio %d  = %d, record_init_fail %d \n",
            __func__, slaveAddr, lpi->intr_pin, val, record_init_fail);
        
            msleep(10);
    }
    if(loop_i >= I2C_RETRY_COUNT)
    {
        printk(KERN_ERR "[PS_ERR][CM36283 error] %s retry over %d\n",
            __func__, I2C_RETRY_COUNT);
        return -EIO;
    }

    return 0;
}

static int I2C_TxData(uint16_t slaveAddr, uint8_t *txData, int length)
{
    uint8_t loop_i;
    int val;
    struct cm36283_info *lpi = lp_info;
    struct i2c_msg msg[] = \
    {
        {
            .addr = slaveAddr,
            .flags = 0,
            .len = length,
            .buf = txData,
        },
    };

    for(loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++)
    {
        if (i2c_transfer(lp_info->i2c_client->adapter, msg, 1) > 0)
            break;
        
        val = gpio_get_value(lpi->intr_pin);
        /*check intr GPIO when i2c error*/
        if (loop_i == 0 || loop_i == I2C_RETRY_COUNT -1)
            D("[PS][CM36283 error] %s, i2c err, slaveAddr 0x%x, value 0x%x, ISR gpio%d  = %d, record_init_fail %d\n",
                __func__, slaveAddr, txData[0], lpi->intr_pin, val, record_init_fail);
        
        msleep(10);
    }

    if(loop_i >= I2C_RETRY_COUNT)
    {
        printk(KERN_ERR "[PS_ERR][CM36283 error] %s retry over %d\n",
            __func__, I2C_RETRY_COUNT);
        return -EIO;
    }

    return 0;
}

static int _cm36283_I2C_Read_Word(uint16_t slaveAddr, uint8_t cmd, uint16_t *pdata)
{
    uint8_t buffer[2];
    int ret = 0;
    
    if (pdata == NULL)
        return -EFAULT;
    
    ret = I2C_RxData(slaveAddr, cmd, buffer, 2);
    if(ret < 0)
    {
        pr_err(
        "[PS_ERR][CM36283 error]%s: I2C_RxData fail [0x%x, 0x%x]\n",
            __func__, slaveAddr, cmd);
        return ret;
    }
    
    *pdata = (buffer[1]<<8)|buffer[0];
    #if 0
    /* Debug use */
    printk(KERN_DEBUG "[CM36283] %s: I2C_RxData[0x%x, 0x%x] = 0x%x\n",
    __func__, slaveAddr, cmd, *pdata);
    #endif
    return ret;
}

static int _cm36283_I2C_Write_Word(uint16_t SlaveAddress, uint8_t cmd, uint16_t data)
{
    char buffer[3];
    int ret = 0;
    #if 0
    /* Debug use */
    printk(KERN_DEBUG
    "[CM36283] %s: _cm36283_I2C_Write_Word[0x%x, 0x%x, 0x%x]\n",
    __func__, SlaveAddress, cmd, data);
    #endif
    buffer[0] = cmd;
    buffer[1] = (uint8_t)(data&0xff);
    buffer[2] = (uint8_t)((data&0xff00)>>8);	
    
    ret = I2C_TxData(SlaveAddress, buffer, 3);
    if(ret < 0)
    {
        pr_err("[PS_ERR][CM36283 error]%s: I2C_TxData fail\n", __func__);
        return -EIO;
    }
    
    return ret;
}

static int get_ls_adc_value(uint16_t *als_step, bool resume)
{
    struct cm36283_info *lpi = lp_info;
    uint32_t tmpResult;
    int ret = 0;
    
    if (als_step == NULL)
        return -EFAULT;
    
    /* Read ALS data: */
    ret = _cm36283_I2C_Read_Word(lpi->slave_addr, ALS_DATA, als_step);
    if(ret < 0)
    {
        pr_err(
        "[LS][CM36283 error]%s: _cm36283_I2C_Read_Word fail\n",__func__);
        return -EIO;
    }
	if(((*als_step)+50) > 0xFFFF)
		lpi->top_tresh=0xFFFF;
	else
		lpi->top_tresh=(*als_step)+50;

/*PERI-AC-Zero_Lux-Checking-00+{*/
	if((*als_step-50) < 0)
	{
		if(clearcount==0)
		{
			lpi->bottom_tresh=3;
			clearcount++;
		}
		else
		{
			lpi->bottom_tresh=0;
		}
	}
	else
	{
		clearcount=0;
		lpi->bottom_tresh=(*als_step)-50;
	}
/*PERI-AC-Zero_Lux-Checking-00+}*/
    
/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#ifdef LS_cal
	if(lpi->k_data.done_LS)
    	{
    		D("[LS][CM36283] %s: raw adc = 0x%x, ls_diff = %d\n",
    __func__, (*als_step), lpi->ls_calibrate);
    		*als_step=*als_step * (720+lpi->k_data.ls_diff)/720;
    	}
	D("[LS][CM36283] %s: steps = 0x%x\n",
    __func__, (*als_step));
#endif
/*MTD-PERIPHERAL-CH-PS_conf00++]*/

    if(!lpi->ls_calibrate)
    {
        tmpResult = (uint32_t)(*als_step) * lpi->als_gadc / lpi->als_kadc;
        if (tmpResult > 0xFFFF)
            *als_step = 0xFFFF;
        else
            *als_step = tmpResult;  			
    }
    
    D("[LS][CM36283] %s: raw adc = 0x%x, ls_calibrate = %d\n",
    __func__, (*als_step), lpi->ls_calibrate);
	*als_step=*als_step*100/12;
    
    return ret;
}

static int set_lsensor_range(uint16_t low_thd, uint16_t high_thd)
{
    int ret = 0;
    struct cm36283_info *lpi = lp_info;
    
    _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_THDH, high_thd);
    _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_THDL, low_thd);
    
    return ret;
}

static int get_ps_adc_value(uint16_t *data)
{
    int ret = 0;
    struct cm36283_info *lpi = lp_info;
    
    if (data == NULL)
        return -EFAULT;	
    
    ret = _cm36283_I2C_Read_Word(lpi->slave_addr, PS_DATA, data);
    
    (*data) &= 0xFF;
    
    if(ret < 0)
    {
        pr_err(
        "[PS][CM36283 error]%s: _cm36283_I2C_Read_Word fail\n",__func__);
        return -EIO;
    }
    else
    {
        pr_err("[PS][CM36283 OK]%s: _cm36283_I2C_Read_Word OK 0x%x\n",__func__, *data);
    }
    
    return ret;
}

static uint16_t mid_value(uint16_t value[], uint8_t size)
{
    int i = 0, j = 0;
    uint16_t temp = 0;
    
    if (size < 3)
        return 0;
    
    for (i = 0; i < (size - 1); i++)
        for (j = (i + 1); j < size; j++)
            if(value[i] > value[j])
            {
                temp = value[i];
                value[i] = value[j];
                value[j] = temp;
            }
    return value[((size - 1) / 2)];
}

static int get_stable_ps_adc_value(uint16_t *ps_adc)
{
    uint16_t value[3] = {0, 0, 0}, mid_val = 0;
    int ret = 0;
    int i = 0;
    int wait_count = 0;
    struct cm36283_info *lpi = lp_info;

    for(i = 0; i < 3; i++)
    {
        /*wait interrupt GPIO high*/
        while(gpio_get_value(lpi->intr_pin) == 0)
        {
            msleep(10);
            wait_count++;
            if(wait_count > 12)
            {
                pr_err("[PS_ERR][CM36283 error]%s: interrupt GPIO low,"
                " get_ps_adc_value\n", __func__);
                return -EIO;
            }
        }
        
        ret = get_ps_adc_value(&value[i]);
        if(ret < 0)
        {
            pr_err("[PS_ERR][CM36283 error]%s: get_ps_adc_value\n",
            __func__);
            return -EIO;
        }
        
        if(wait_count < 60/10)
        {/*wait gpio less than 60ms*/
            msleep(60 - (10*wait_count));
        }
        wait_count = 0;
    }

    /*D("Sta_ps: Before sort, value[0, 1, 2] = [0x%x, 0x%x, 0x%x]",
    value[0], value[1], value[2]);*/
    mid_val = mid_value(value, 3);
    D("Sta_ps: After sort, value[0, 1, 2] = [0x%x, 0x%x, 0x%x]",
    value[0], value[1], value[2]);
    *ps_adc = (mid_val & 0xFF);
    
    return 0;
}

static void sensor_irq_do_work(struct work_struct *work)
{
    struct cm36283_info *lpi = lp_info;
    uint16_t intFlag;
    _cm36283_I2C_Read_Word(lpi->slave_addr, INT_FLAG, &intFlag);
    control_and_report(lpi, CONTROL_INT_ISR_REPORT, intFlag);  
     
    enable_irq(lpi->irq);
}

static irqreturn_t cm36283_irq_handler(int irq, void *data)
{
    struct cm36283_info *lpi = data;
    
    disable_irq_nosync(lpi->irq);
    queue_delayed_work(lpi->lp_wq, &lpi->work,0);
    
    return IRQ_HANDLED;
}

static int als_power(int enable)
{
    int rc = 0;
	struct cm36283_info *lpi = lp_info;
	static struct regulator *vreg_L9;
	//static bool power_on = false;

	printk(KERN_ALERT "[CM36283] Enter %s, on = %d\n", __func__, enable);

	/* Power off, Reset Pin pull LOW must before power source off */

	if (lpi->vreg==NULL) {
		vreg_L9 = regulator_get(&lpi->i2c_client->dev,
				"cm36283_vdd");
		if (IS_ERR(vreg_L9)) {
			pr_err("[CM36283]could not get vreg_L9, rc = %ld\n",
				PTR_ERR(vreg_L9));
			return -ENODEV;
		}
		lpi->vreg=vreg_L9;
	}
	else
		vreg_L9=lpi->vreg;
	

	if (enable) {
		printk("[CM36283] REGULATOR_SET_OPTIMUM_MODE\n");
		rc = regulator_set_optimum_mode(vreg_L9, 195);
		if (rc < 0) {
			pr_err("[DISPLAY]set_optimum_mode vreg_L9 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	} else {
		printk("[CM36283] REGULATOR_DISABLE\n");
		rc = regulator_set_optimum_mode(vreg_L9, 0);
		if (rc < 0) {
			pr_err("[CM36283]set_optimum_mode vreg_L9 failed, rc=%d\n", rc);
			return -EINVAL;
		}
	}
	return 0;
}

static void ls_initial_cmd(struct cm36283_info *lpi)
{	
    /*must disable l-sensor interrupt befrore IST create*//*disable ALS func*/
    lpi->ls_cmd &= CM36283_ALS_INT_MASK;
    lpi->ls_cmd |= CM36283_ALS_SD;
    _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
}

static void psensor_initial_cmd(struct cm36283_info *lpi)
{
    /*must disable p-sensor interrupt befrore IST create*//*disable ALS func*/		
    lpi->ps_conf1_val |= CM36283_PS_SD;
    lpi->ps_conf1_val &= CM36283_PS_INT_MASK;  
    _cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);   
    _cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val);
    _cm36283_I2C_Write_Word(lpi->slave_addr, PS_THD, (lpi->ps_close_thd_set <<8)| lpi->ps_away_thd_set);
    
    D("[PS][CM36283] %s, finish\n", __func__);	
}

static int psensor_enable(struct cm36283_info *lpi)
{
    int ret = -EIO;
    
    mutex_lock(&ps_enable_mutex);
    D("[PS][CM36283] %s\n", __func__);
    
    if( lpi->ps_enable )
    {
        D("[PS][CM36283] %s: already enabled\n", __func__);
        ret = 0;
    }
    else
        ret = control_and_report(lpi, CONTROL_PS, 1);
    
    mutex_unlock(&ps_enable_mutex);
    return ret;
}

static int psensor_disable(struct cm36283_info *lpi)
{
    int ret = -EIO;
    
    mutex_lock(&ps_disable_mutex);
    D("[PS][CM36283] %s\n", __func__);
    
    if( lpi->ps_enable == 0 )
    {
        D("[PS][CM36283] %s: already disabled\n", __func__);
        ret = 0;
    }
    else
        ret = control_and_report(lpi, CONTROL_PS,0);
    
    mutex_unlock(&ps_disable_mutex);
    return ret;
}

static int psensor_open(struct inode *inode, struct file *file)
{
    struct cm36283_info *lpi = lp_info;
    
    D("[PS][CM36283] %s\n", __func__);
    
    if (lpi->psensor_opened)
        return -EBUSY;
    
    lpi->psensor_opened = 1;
    
    return 0;
}

static int psensor_release(struct inode *inode, struct file *file)
{
    struct cm36283_info *lpi = lp_info;
    
    D("[PS][CM36283] %s\n", __func__);
    
    lpi->psensor_opened = 0;
    
    return psensor_disable(lpi);
}

static long psensor_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
    int val=0,rc=0;
    struct cm36283_info *lpi = lp_info;
    
    D("[PS][CM36283] %s cmd %d\n", __func__, _IOC_NR(cmd));
    
    switch(cmd)
    {
        case CAPELLA_CM3602_IOCTL_ENABLE:
            if (get_user(val, (unsigned long __user *)arg))
                return -EFAULT;
            
            D("[PS][CM36283] %s val %d\n", __func__, val);
            rc = val ?psensor_enable(lpi):psensor_disable(lpi);
        break;
        case CAPELLA_CM3602_IOCTL_GET_ENABLED:
            D("[LS][CM36283] %s CAPELLA_CM3602_IOCTL_GET_ENABLED, enabled %d\n",
            __func__, val);
            return put_user(lpi->ps_enable, (unsigned long __user *)arg);
        break;
        default:
            pr_err("[PS][CM36283 error]%s: invalid cmd %d\n",
            __func__, _IOC_NR(cmd));
        return -EINVAL;
    }
    return rc;
}

static const struct file_operations psensor_fops = {
    .owner = THIS_MODULE,
    .open = psensor_open,
    .release = psensor_release,
    .unlocked_ioctl = psensor_ioctl
};

struct miscdevice psensor_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "proximity",
    .fops = &psensor_fops
};

void lightsensor_set_kvalue(struct cm36283_info *lpi)
{
    if(!lpi)
    {
        pr_err("[LS][CM36283 error]%s: ls_info is empty\n", __func__);
        return;
    }

    D("[LS][CM36283] %s: ALS calibrated als_kadc=0x%x\n",
        __func__, als_kadc);

    if(als_kadc >> 16 == ALS_CALIBRATED)
        lpi->als_kadc = als_kadc & 0xFFFF;
    else
    {
        lpi->als_kadc = 0;
        D("[LS][CM36283] %s: no ALS calibrated\n", __func__);
    }

    if(lpi->als_kadc && lpi->golden_adc > 0)
    {
        lpi->als_kadc = (lpi->als_kadc > 0 && lpi->als_kadc < 0x1000) ?
        lpi->als_kadc : lpi->golden_adc;
        lpi->als_gadc = lpi->golden_adc;
    }
    else
    {
        lpi->als_kadc = 1;
        lpi->als_gadc = 1;
    }
    D("[LS][CM36283] %s: als_kadc=0x%x, als_gadc=0x%x\n",
        __func__, lpi->als_kadc, lpi->als_gadc);
}


static int lightsensor_update_table(struct cm36283_info *lpi)
{
    uint32_t tmpData[10];
    int i;
    for(i = 0; i < 10; i++)
    {
        tmpData[i] = (uint32_t)(*(lpi->adc_table + i))
        * lpi->als_kadc / lpi->als_gadc ;
        if( tmpData[i] <= 0xFFFF )
        {
            lpi->cali_table[i] = (uint16_t) tmpData[i];		
        }
        else
        {
            lpi->cali_table[i] = 0xFFFF;    
        }         
        D("[LS][CM36283] %s: Calibrated adc_table: data[%d], %x\n",
        __func__, i, lpi->cali_table[i]);
    }
    
    return 0;
}


static int lightsensor_enable(struct cm36283_info *lpi)
{
    int ret = -EIO;
    
    mutex_lock(&als_enable_mutex);
    D("[LS][CM36283] %s\n", __func__);
    
    if(lpi->als_enable)
    {
        D("[LS][CM36283] %s: already enabled\n", __func__);
        ret = 0;
    }
    else
    {
        ret = control_and_report(lpi, CONTROL_ALS, 1);
    }
    mutex_unlock(&als_enable_mutex);
    return ret;
}

static int lightsensor_disable(struct cm36283_info *lpi)
{
    int ret = -EIO;
    mutex_lock(&als_disable_mutex);
    D("[LS][CM36283] %s\n", __func__);
    
    if( lpi->als_enable == 0 )
    {
        D("[LS][CM36283] %s: already disabled\n", __func__);
        ret = 0;
    }
    else
        ret = control_and_report(lpi, CONTROL_ALS, 0);
    
    mutex_unlock(&als_disable_mutex);
    return ret;
}

static int lightsensor_open(struct inode *inode, struct file *file)
{
    struct cm36283_info *lpi = lp_info;
    int rc = 0;
    
    D("[LS][CM36283] %s\n", __func__);
    if(lpi->lightsensor_opened)
    {
        pr_err("[LS][CM36283 error]%s: already opened\n", __func__);
        rc = -EBUSY;
    }
    lpi->lightsensor_opened = 1;
    return rc;
}

static int lightsensor_release(struct inode *inode, struct file *file)
{
    struct cm36283_info *lpi = lp_info;
    
    D("[LS][CM36283] %s\n", __func__);
    lpi->lightsensor_opened = 0;
    return 0;
}

static long lightsensor_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
    int rc, val;
    struct cm36283_info *lpi = lp_info;
    
    /*D("[CM36283] %s cmd %d\n", __func__, _IOC_NR(cmd));*/
    
    switch(cmd)
    {
        case LIGHTSENSOR_IOCTL_ENABLE:
            if (get_user(val, (unsigned long __user *)arg))
            {
                rc = -EFAULT;
                break;
            }
            D("[LS][CM36283] %s LIGHTSENSOR_IOCTL_ENABLE, value = %d\n",__func__, val);
            rc = val ? lightsensor_enable(lpi) : lightsensor_disable(lpi);
        break;
        case LIGHTSENSOR_IOCTL_GET_ENABLED:
            val = lpi->als_enable;
            D("[LS][CM36283] %s LIGHTSENSOR_IOCTL_GET_ENABLED, enabled %d\n",__func__, val);
            rc = put_user(val, (unsigned long __user *)arg);
        break;
        default:
            pr_err("[LS][CM36283 error]%s: invalid cmd %d\n",__func__, _IOC_NR(cmd));
            rc = -EINVAL;
        break;
    }
    
    return rc;
}

static const struct file_operations lightsensor_fops = {
    .owner = THIS_MODULE,
    .open = lightsensor_open,
    .release = lightsensor_release,
    .unlocked_ioctl = lightsensor_ioctl
};

static struct miscdevice lightsensor_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "lightsensor",
    .fops = &lightsensor_fops
};


static ssize_t ps_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    uint16_t value;
    int ret;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    int intr_val;
    
    intr_val = gpio_get_value(lpi->intr_pin);
    
    get_ps_adc_value(&value);
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    ret = snprintf(buf,sizeof("ADC[0x%04X], ENABLE = %d, intr_pin = %d\n"), "ADC[0x%04X], ENABLE = %d, intr_pin = %d\n", value, lpi->ps_enable, intr_val);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
	
    return ret;
}

static ssize_t ps_value_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    uint16_t value;
    int ret,val;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    
    get_ps_adc_value(&value);
    val = (value >= lpi->ps_close_thd_set) ? 0 : 1;
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
	ret = snprintf(buf,sizeof("%d\n"), "%d\n", val);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    
    return ret;
}

static ssize_t ps_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    int ret;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    ret = snprintf(buf,sizeof("%d\n"), "%d\n",lpi->ps_enable);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
	
    return ret;
}

static ssize_t ps_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    unsigned ps_en = simple_strtoul(buf, NULL, 10);
    
    if (ps_en != 0 && ps_en != 1
        && ps_en != 10 && ps_en != 13 && ps_en != 16)
        return -EINVAL;
    
    if(ps_en)
    {
        D("[PS][CM36283] %s: ps_en=%d\n",
            __func__, ps_en);
        psensor_enable(lpi);
    }
    else
        psensor_disable(lpi);
    
    D("[PS][CM36283] %s\n", __func__);
    
    return count;
}


unsigned PS_cmd_test_value;
static ssize_t ps_parameters_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    int ret;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    ret = snprintf(buf,sizeof("PS_close_thd_set = 0x%x, PS_away_thd_set = 0x%x, PS_cmd_cmd:value = 0x%x\n"), "PS_close_thd_set = 0x%x, PS_away_thd_set = 0x%x, PS_cmd_cmd:value = 0x%x\n",
    lpi->ps_close_thd_set, lpi->ps_away_thd_set, PS_cmd_test_value);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    
    return ret;
}

static ssize_t ps_parameters_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{

    struct cm36283_info *lpi = dev_get_drvdata(dev);
    char *token[10];
    int i;
    
    printk(KERN_INFO "[PS][CM36283] %s\n", buf);
    for (i = 0; i < 3; i++)
        token[i] = strsep((char **)&buf, " ");
    
    lpi->ps_close_thd_set = simple_strtoul(token[0], NULL, 16);
    lpi->ps_away_thd_set = simple_strtoul(token[1], NULL, 16);	
    PS_cmd_test_value = simple_strtoul(token[2], NULL, 16);
    printk(KERN_INFO
    "[PS][CM36283]Set PS_close_thd_set = 0x%x, PS_away_thd_set = 0x%x, PS_cmd_cmd:value = 0x%x\n",
        lpi->ps_close_thd_set, lpi->ps_away_thd_set, PS_cmd_test_value);
    
    D("[PS][CM36283] %s\n", __func__);
    
    return count;
}



static ssize_t ps_conf_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
	return snprintf(buf,sizeof("PS_CONF1 = 0x%x, PS_CONF3 = 0x%x\n"), "PS_CONF1 = 0x%x, PS_CONF3 = 0x%x\n", lpi->ps_conf1_val, lpi->ps_conf3_val);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
}
static ssize_t ps_conf_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    uint16_t code1, code2,force;/*MTD-PERIPHERAL-CH-PS_conf00++*/
    struct cm36283_info *lpi = dev_get_drvdata(dev);
	char *token[10];
    int i;

/*MTD-PERIPHERAL-CH-PS_conf00++[*/
    for (i = 0; i < 3; i++)
        token[i] = strsep((char **)&buf, " ");
	force = simple_strtoul(token[0], NULL, 16);
	code1 = simple_strtoul(token[1], NULL, 16);
	code2 = simple_strtoul(token[2], NULL, 16);

	if((fih_get_product_phase() == PHASE_PQ) || force)
	{
    	D("[PS]%s: store value PS conf1 reg = 0x%x PS conf3 reg = 0x%x\n", __func__, code1, code2);
    
    	lpi->ps_conf1_val = code1;
    	lpi->ps_conf3_val = code2;
    
    	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val );  
    	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val );
	}
	else
		D("[PS]%s: store value PS not acceptable. force=%d\n", __func__, force);
/*MTD-PERIPHERAL-CH-PS_conf00++]*/ 
    return count;
}

static ssize_t ps_thd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    int ret;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    ret = snprintf(buf,sizeof("ps_close_thd = 0x%x, ps_away_thd = 0x%x\n"), "ps_close_thd = 0x%x, ps_away_thd = 0x%x\n",lpi->ps_close_thd_set, lpi->ps_away_thd_set);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    return ret;	
}
/*MTD-PERIPHERAL-CH-PS_conf00++[*/
static ssize_t ps_thd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
	char *token[10];
	unsigned force,code,module_id;
    int i;
    for (i = 0; i < 3; i++)
        token[i] = strsep((char **)&buf, " ");
	force = simple_strtol(token[0], NULL, 16);
	module_id = simple_strtol(token[1], NULL, 16);
	code = simple_strtol(token[2], NULL, 16);

	if(((fih_get_product_phase() == PHASE_PQ) && (module_id==2))|| (fih_get_product_phase() >= PHASE_TP2_MP) || force)
	{
    	D("[PS]%s: store value = 0x%x\n", __func__, code);
    
    	lpi->ps_away_thd_set = code &0xFF;
    	lpi->ps_close_thd_set = (code &0xFF00)>>8;	

		_cm36283_I2C_Write_Word(lpi->slave_addr, PS_THD, (lpi->ps_close_thd_set <<8)| lpi->ps_away_thd_set);/*PERI-CH-PS_wake01++*/
    	D("[PS]%s: ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n", __func__, lpi->ps_close_thd_set, lpi->ps_away_thd_set);
	}
	else
		D("[PS]%s: Not support store PS_THD value\n", __func__);
    
    return count;
}
/*MTD-PERIPHERAL-CH-PS_conf00++]*/

static ssize_t ps_hw_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    int ret = 0;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
	ret = snprintf(buf,sizeof("PS1: reg = 0x%x, PS3: reg = 0x%x, ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n"), "PS1: reg = 0x%x, PS3: reg = 0x%x, ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n",
        lpi->ps_conf1_val, lpi->ps_conf3_val, lpi->ps_close_thd_set, lpi->ps_away_thd_set);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    
    return ret;
}

static ssize_t ls_adc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    int ret;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    
    D("[LS][CM36283] %s: ADC = 0x%04X, Level = %d \n",
        __func__, lpi->current_adc, lpi->current_level);
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    ret = snprintf(buf,sizeof("%x\n"), "%x\n",lpi->current_adc);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    
    return ret;
}


static ssize_t ls_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    int ret = 0;
    struct cm36283_info *lpi = dev_get_drvdata(dev);;
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    ret = snprintf(buf,sizeof("%d\n"), "%d\n",lpi->als_enable);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    
    return ret;
}

static ssize_t ls_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
    int ret = 0;
    uint16_t intFlag;
    struct cm36283_info *lpi = dev_get_drvdata(dev);

	unsigned ls_auto = simple_strtoul(buf, NULL, 16);
    
    if (ls_auto != 0 && ls_auto != 1 && ls_auto != 147)
        return -EINVAL;
    
    if(ls_auto)
    {
        lpi->ls_calibrate = 0;//(ls_auto == 147) ? 1 : 0;
        ret = lightsensor_enable(lpi);
        _cm36283_I2C_Read_Word(lpi->slave_addr, INT_FLAG, &intFlag);
        control_and_report(lpi, CONTROL_INT_ISR_REPORT, intFlag);
    }
    else
    {
        lpi->ls_calibrate = 0;
        ret = lightsensor_disable(lpi);
    }
    
    D("[LS][CM36283] %s: lpi->als_enable = %d, lpi->ls_calibrate = %d, ls_auto=%d\n",
        __func__, lpi->als_enable, lpi->ls_calibrate, ls_auto);
    
    if (ret < 0)
        pr_err("[LS][CM36283 error]%s: set auto light sensor fail\n",__func__);
    
    return count;
}


static ssize_t ls_kadc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    int ret;
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
	ret = snprintf(buf,sizeof("kadc = 0x%x"), "kadc = 0x%x",
        lpi->als_kadc);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    
    return ret;
}

static ssize_t ls_kadc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
	unsigned kadc_temp = simple_strtoul(buf, NULL, 10);
    
    mutex_lock(&als_get_adc_mutex);
    if(kadc_temp != 0)
    {
        lpi->als_kadc = kadc_temp;
        if(  lpi->als_gadc != 0)
        {
            if (lightsensor_update_table(lpi) < 0)
                printk(KERN_ERR "[LS][CM36283 error] %s: update ls table fail\n", __func__);
        }
        else
        {
            printk(KERN_INFO "[LS]%s: als_gadc =0x%x wait to be set\n",__func__, lpi->als_gadc);
        }		
    }
    else
    {
        printk(KERN_INFO "[LS]%s: als_kadc can't be set to zero\n",__func__);
    }
    
    mutex_unlock(&als_get_adc_mutex);
    return count;
}


static ssize_t ls_gadc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    int ret;
    
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    ret = snprintf(buf,sizeof("gadc = 0x%x\n"), "gadc = 0x%x\n", lpi->als_gadc);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
    
    return ret;
}

static ssize_t ls_gadc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
	unsigned gadc_temp = simple_strtoul(buf, NULL, 10);
    
    mutex_lock(&als_get_adc_mutex);
    if(gadc_temp != 0)
    {
        lpi->als_gadc = gadc_temp;
        if(  lpi->als_kadc != 0)
        {
            if (lightsensor_update_table(lpi) < 0)
                printk(KERN_ERR "[LS][CM36283 error] %s: update ls table fail\n", __func__);
        }
        else
        {
            printk(KERN_INFO "[LS]%s: als_kadc =0x%x wait to be set\n",__func__, lpi->als_kadc);
        }		
    }
    else
    {
        printk(KERN_INFO "[LS]%s: als_gadc can't be set to zero\n",__func__);
    }
    mutex_unlock(&als_get_adc_mutex);
    return count;
}


static ssize_t ls_thd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cm36283_info *lpi = dev_get_drvdata(dev);
    int ret;
	ret = snprintf(buf,sizeof("ls_top_thd = 0x%x, ls_bot_thd = 0x%x\n"), "ls_top_thd = 0x%x, ls_bot_thd = 0x%x\n",lpi->top_tresh , lpi->bottom_tresh);
	return ret;

}

static ssize_t ls_thd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    unsigned code1, code2;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
	char *token[10];
    int i;
    for (i = 0; i < 2; i++)
        token[i] = strsep((char **)&buf, " ");
	code1 = simple_strtoul(token[0], NULL, 16);
	code2 = simple_strtoul(token[1], NULL, 16);

    
    D("[PS]%s: store value LS top thd reg = 0x%x LS bot thd reg reg = 0x%x\n", __func__, code1, code2);
    
    lpi->top_tresh= code1;
    lpi->bottom_tresh= code2;
    
    _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_THDH, lpi->top_tresh );  
    _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_THDL, lpi->bottom_tresh );
    
    return count;
}


static ssize_t ls_conf_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    return snprintf(buf,sizeof("ALS_CONF = %x\n"), "ALS_CONF = %x\n", lpi->ls_cmd);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
}
static ssize_t ls_conf_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    struct cm36283_info *lpi = lp_info;
	unsigned value = simple_strtoul(buf, NULL, 16);
    
    lpi->ls_cmd = value;
    printk(KERN_INFO "[LS]set ALS_CONF = %x\n", lpi->ls_cmd);
    
    _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);
    return count;
}

/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#ifdef LS_cal
static ssize_t ls_k_data_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    return snprintf(buf,sizeof("ALS_DONE = %d ALS_diff=%d\n"), "ALS_DONE = %d ALS_diff=%d\n", lpi->k_data.done_LS,lpi->k_data.ls_diff);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
}
static ssize_t ls_k_data_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    long code1, code2;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
	char *token[10];
    int i;
    for (i = 0; i < 2; i++)
        token[i] = strsep((char **)&buf, " ");
	code1 = simple_strtol(token[0], NULL, 10);
	code2 = simple_strtol(token[1], NULL, 10);
    
    lpi->k_data.done_LS=code1;
	lpi->k_data.ls_diff=code2;
    printk(KERN_INFO "[LS]ALS_DONE = %d ALS_diff=%d\n", lpi->k_data.done_LS,lpi->k_data.ls_diff);
    
    return count;
}

static ssize_t ps_k_data_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
    struct cm36283_info *lpi = dev_get_drvdata(dev);
    /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
    return snprintf(buf,sizeof("PS_DONE = %d PS_top=%d PS_bot=%d\n"), "PS_DONE = %d PS_top=%d PS_bot=%d\n", lpi->k_data.done_PS,lpi->k_data.ps_top,lpi->k_data.ps_bot);
	/*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
}
static ssize_t ps_k_data_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
    long code1, code2, code3;
    struct cm36283_info *lpi = dev_get_drvdata(dev);
	char *token[10];
    int i;
    for (i = 0; i < 3; i++)
        token[i] = strsep((char **)&buf, " ");
	code1 = simple_strtol(token[0], NULL, 10);
	code2 = simple_strtol(token[1], NULL, 10);
	code3 = simple_strtol(token[2], NULL, 10);
    lpi->k_data.done_PS=code1;
	lpi->top_tresh=code2;
	lpi->bottom_tresh=code3;
	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_THD, (lpi->ps_close_thd_set <<8)| lpi->ps_away_thd_set);
    printk(KERN_INFO "[PS]PS_DONE = %d PS_top=%d PS_bot=%d\n", lpi->k_data.done_PS,lpi->top_tresh,lpi->bottom_tresh);
    return count;
}
#endif
/*MTD-PERIPHERAL-CH-PS_conf00++]*/


struct device_attribute ps_enable_attr =  __ATTR(enable,0664,ps_enable_show,ps_enable_store);
struct device_attribute ps_value_attr =  __ATTR(value,0664,ps_value_show,NULL);
static DEVICE_ATTR(ps_adc, 0664, ps_adc_show, ps_enable_store);
static DEVICE_ATTR(ps_conf, 0664, ps_conf_show, ps_conf_store);
static DEVICE_ATTR(ps_parameters, 0664,ps_parameters_show, ps_parameters_store);
static DEVICE_ATTR(ps_thd, 0664, ps_thd_show, ps_thd_store);
static DEVICE_ATTR(ps_hw, 0664, ps_hw_show, NULL);
static DEVICE_ATTR(enable, 0664,ls_enable_show, ls_enable_store);
static DEVICE_ATTR(ls_kadc, 0664, ls_kadc_show, ls_kadc_store);
static DEVICE_ATTR(ls_gadc, 0664, ls_gadc_show, ls_gadc_store);
static DEVICE_ATTR(ls_thd, 0664,ls_thd_show, ls_thd_store);
static DEVICE_ATTR(ls_conf, 0664, ls_conf_show, ls_conf_store);
static DEVICE_ATTR(value, 0664, ls_adc_show, NULL);
/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#ifdef LS_cal
static DEVICE_ATTR(ls_k_data, 0664, ls_k_data_show, ls_k_data_store);
static DEVICE_ATTR(ps_k_data, 0664, ps_k_data_show, ps_k_data_store);
#endif
/*MTD-PERIPHERAL-CH-PS_conf00++]*/


static struct attribute *cm36283_proximity_attributes[] = {
    &dev_attr_ps_adc.attr,
    &dev_attr_ps_conf.attr,
    &dev_attr_ps_parameters.attr,
    &dev_attr_ps_thd.attr,
    &dev_attr_ps_hw.attr,
/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#ifdef LS_cal
    &dev_attr_ps_k_data.attr,
#endif
/*MTD-PERIPHERAL-CH-PS_conf00++]*/
    &ps_enable_attr.attr,
    &ps_value_attr.attr,
    NULL
};

static struct attribute *cm36283_light_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_ls_kadc.attr,
    &dev_attr_ls_gadc.attr,
    &dev_attr_ls_thd.attr,
    &dev_attr_ls_conf.attr,
    &dev_attr_value.attr,
/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#ifdef LS_cal
    &dev_attr_ls_k_data.attr,
#endif
/*MTD-PERIPHERAL-CH-PS_conf00++]*/
    NULL
};

static struct attribute_group cm36283_proximity_attributes_group = {
    .attrs = cm36283_proximity_attributes
};
static struct attribute_group cm36283_light_attributes_group = {
    .attrs = cm36283_light_attributes
};

static int lightsensor_setup(struct cm36283_info *lpi)
{
    int ret;
    
    lpi->ls_input_dev = input_allocate_device();
    if(!lpi->ls_input_dev)
    {
        pr_err("[LS][CM36283 error]%s: could not allocate ls input device\n",__func__);
        return -ENOMEM;
    }
    lpi->ls_input_dev->name = "lightsensor-level";
    set_bit(EV_ABS, lpi->ls_input_dev->evbit);
    input_set_abs_params(lpi->ls_input_dev, ABS_MISC, 0, 0x2800, 0, 0);
    input_set_drvdata(lpi->ls_input_dev, lpi);
    ret = input_register_device(lpi->ls_input_dev);
    if(ret < 0)
    {
        pr_err("[LS][CM36283 error]%s: can not register ls input device\n",__func__);
        goto err_free_ls_input_device;
    }

    ret = misc_register(&lightsensor_misc);
    if(ret < 0)
    {
        pr_err("[LS][CM36283 error]%s: can not register ls misc device\n",__func__);
        goto err_unregister_ls_input_device;
    }

    return ret;

err_unregister_ls_input_device:
    input_unregister_device(lpi->ls_input_dev);
err_free_ls_input_device:
    input_free_device(lpi->ls_input_dev);
    return ret;
}

static int psensor_setup(struct cm36283_info *lpi)
{
    int ret;
    
    lpi->ps_input_dev = input_allocate_device();
    if(!lpi->ps_input_dev)
    {
        pr_err("[PS][CM36283 error]%s: could not allocate ps input device\n",__func__);
        return -ENOMEM;
    }
    lpi->ps_input_dev->name = "proximity";

    set_bit(EV_ABS, lpi->ps_input_dev->evbit);
    input_set_abs_params(lpi->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);
    input_set_drvdata(lpi->ps_input_dev, lpi);
    ret = input_register_device(lpi->ps_input_dev);
    if(ret < 0)
    {
        pr_err("[PS][CM36283 error]%s: could not register ps input device\n",__func__);
        goto err_free_ps_input_device;
    }

    ret = misc_register(&psensor_misc);
    if(ret < 0)
    {
        pr_err("[PS][CM36283 error]%s: could not register ps misc device\n",__func__);
        goto err_unregister_ps_input_device;
    }

    return ret;

err_unregister_ps_input_device:
    input_unregister_device(lpi->ps_input_dev);
err_free_ps_input_device:
    input_free_device(lpi->ps_input_dev);
    return ret;
}


static int initial_cm36283(struct cm36283_info *lpi)
{
    int val, ret;
    uint16_t idReg;
    
    val = gpio_get_value(lpi->intr_pin);
    D("[PS][CM36283] %s, INTERRUPT GPIO val = %d\n", __func__, val);
    
    ret = _cm36283_I2C_Read_Word(lpi->slave_addr, ID_REG, &idReg);
    D("[PS][CM36283] %s, Chip ID  = %x\n", __func__, idReg);
    if((ret < 0) || (idReg != 0x0083))
    {
        if (record_init_fail == 0)
            record_init_fail = 1;
        return -ENOMEM;/*If devices without cm36283 chip and did not probe driver*/	
    }
    
    return 0;
}

static int cm36283_setup(struct cm36283_info *lpi)
{
    int ret = 0;
    
    als_power(1);
    msleep(5);
    if(lpi->init == NULL)
    {
        ret = -EIO;
        pr_err(
            "[PS_ERR][CM36283 error]%s: fail to initial GPIO (%d)\n",
            __func__, ret);
        goto fail_free_intr_pin;
    }
    else
    {
        lpi->init();
    }
    
    ret = initial_cm36283(lpi);
    if(ret < 0)
    {
        pr_err(
        "[PS_ERR][CM36283 error]%s: fail to initial cm36283 (%d)\n",
        __func__, ret);
        goto fail_free_intr_pin;
    }
    
    /*Default disable P sensor and L sensor*/
    ls_initial_cmd(lpi);
    psensor_initial_cmd(lpi);
    
    ret = request_irq(lpi->irq,
    cm36283_irq_handler,
    IRQF_TRIGGER_LOW,
    "cm36283",
    lpi);
    if(ret < 0)
    {
        pr_err(
            "[PS][CM36283 error]%s: req_irq(%d) fail for gpio %d (%d)\n",
            __func__, lpi->irq,
            lpi->intr_pin, ret);
            goto fail_free_intr_pin;
    }
    
    return ret;
    
    fail_free_intr_pin:
    gpio_free(lpi->intr_pin);
    return ret;
}

static void cm36283_early_suspend(struct early_suspend *h)
{
    struct cm36283_info *lpi = container_of(h, struct cm36283_info, early_suspend);
    D("[LS][CM36283] %s\n", __func__);
    if(lpi->ps_enable)
    {
        D("[LS][CM36283] power off vote %s\n", __func__);
    }
    else
    {
        als_power(0);
        D("[LS][CM36283] %s\n", __func__);
    }
}

static void cm36283_late_resume(struct early_suspend *h)
{
    struct cm36283_info *lpi = container_of(h, struct cm36283_info, early_suspend);
    D("[LS][CM36283] %s\n", __func__);
    if(lpi->ps_enable)
    {
        D("[LS][CM36283] %s\n", __func__);
    }
    else
    {
        als_power(1);
        D("[LS][CM36283] %s\n", __func__);
    }
}

#ifdef CONFIG_PM
static int cm36283_suspend(struct device *dev)
{    
	return 0;
}

static int cm36283_resume(struct device *dev)
{    
	D("Done.");    
	return 0;
}

static int cm36283_suspend_noirq(struct device *dev)
{    
	return 0;
}

static struct dev_pm_ops cm36283_pm_ops = {    
	.suspend = cm36283_suspend,    
	.resume  = cm36283_resume,    
	.suspend_noirq = cm36283_suspend_noirq
};
#endif	/* CONFIG_PM */


static int cm36283_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
    int ret = 0;
    struct cm36283_info *lpi;
    struct cm36283_platform_data *pdata;
    
    D("[PS][CM36283] %s\n", __func__);
    
    
    lpi = kzalloc(sizeof(struct cm36283_info), GFP_KERNEL);
    if (!lpi)
    return -ENOMEM;
    
    /*D("[CM36283] %s: client->irq = %d\n", __func__, client->irq);*/
    
    lpi->i2c_client = client;
    pdata = client->dev.platform_data;
    if(!pdata)
    {
        pr_err("[PS][CM36283 error]%s: Assign platform_data error!!\n",__func__);
        ret = -EBUSY;
        goto err_platform_data_null;
    }

    lpi->irq = client->irq;
    
    i2c_set_clientdata(client, lpi);
    
    lpi->intr_pin = pdata->intr;
    lpi->adc_table = pdata->levels;
    lpi->power = pdata->power;
    lpi->init = pdata->init;
    lpi->slave_addr = pdata->slave_addr;

/*MTD-PERIPHERAL-CH-PS_conf00++[*/
if(fih_get_product_phase() >= PHASE_TP2_MP)
{
	lpi->ps_away_thd_set = 0x2D;
    lpi->ps_close_thd_set = 0x31;	
    lpi->ps_conf1_val = 0x4025;
}
else
{
    lpi->ps_away_thd_set = pdata->ps_away_thd_set;
    lpi->ps_close_thd_set = pdata->ps_close_thd_set;	
    lpi->ps_conf1_val = pdata->ps_conf1_val;
}
	lpi->ps_conf3_val = pdata->ps_conf3_val;
/*MTD-PERIPHERAL-CH-PS_conf00++]*/
    
    lpi->ls_cmd  = pdata->ls_cmd;
    
    lpi->record_clear_int_fail=0;
    
    D("[PS][CM36283] %s: ls_cmd 0x%x\n",
        __func__, lpi->ls_cmd);

    if(pdata->ls_cmd == 0)
    {
        lpi->ls_cmd  = CM36283_ALS_IT_160ms | CM36283_ALS_GAIN_2;
    }

    lp_info = lpi;

    mutex_init(&CM36283_control_mutex);

    mutex_init(&als_enable_mutex);
    mutex_init(&als_disable_mutex);
    mutex_init(&als_get_adc_mutex);

    ret = lightsensor_setup(lpi);
    if(ret < 0)
    {
        pr_err("[LS][CM36283 error]%s: lightsensor_setup error!!\n",__func__);
        goto err_lightsensor_setup;
    }
    
    mutex_init(&ps_enable_mutex);
    mutex_init(&ps_disable_mutex);
    mutex_init(&ps_get_adc_mutex);
    
    ret = psensor_setup(lpi);
    if(ret < 0)
    {
        pr_err("[PS][CM36283 error]%s: psensor_setup error!!\n",__func__);
        goto err_psensor_setup;
    }

/*MTD-PERIPHERAL-CH-PS_conf00++[*/
#ifdef LS_cal
	lpi->k_data.done_LS=0;
	lpi->k_data.done_PS=0;
	lpi->k_data.ls_diff=0;
	lpi->k_data.ps_top=0;
	lpi->k_data.ps_bot=0;
#endif
/*MTD-PERIPHERAL-CH-PS_conf00++]*/
  //SET LUX STEP FACTOR HERE
  // if adc raw value one step = 5/100 = 1/20 = 0.05 lux
  // the following will set the factor 0.05 = 1/20
  // and lpi->golden_adc = 1;  
  // set als_kadc = (ALS_CALIBRATED <<16) | 20;

    als_kadc = (ALS_CALIBRATED <<16) | 20;
    lpi->golden_adc = 1;

  //ls calibrate always set to 1 
    lpi->ls_calibrate = 1;
    
    lightsensor_set_kvalue(lpi);
    ret = lightsensor_update_table(lpi);
    if(ret < 0)
    {
        pr_err("[LS][CM36283 error]%s: update ls table fail\n",__func__);
        goto err_lightsensor_update_table;
    }

    lpi->lp_wq = create_singlethread_workqueue("cm36283_wq");
    if(!lpi->lp_wq)
    {
        pr_err("[PS][CM36283 error]%s: can't create workqueue\n", __func__);
        ret = -ENOMEM;
        goto err_create_singlethread_workqueue;
    }
    INIT_DELAYED_WORK(&lpi->work, sensor_irq_do_work);
    
    wake_lock_init(&(lpi->ps_wake_lock), WAKE_LOCK_SUSPEND, "proximity");
    
    ret = cm36283_setup(lpi);
    if(ret < 0)
    {
        pr_err("[PS_ERR][CM36283 error]%s: cm36283_setup error!\n", __func__);
        goto err_cm36283_setup;
    }

    ret = sysfs_create_group(&lpi->ps_input_dev->dev.kobj,&cm36283_proximity_attributes_group);
    if(ret)
        goto err_create_ls_device_file;
    
    ret = sysfs_create_group(&lpi->ls_input_dev->dev.kobj,&cm36283_light_attributes_group);
    if(ret)
        goto err_create_ls_device_file;

#ifdef CONFIG_HAS_EARLYSUSPEND
    lpi->early_suspend.level =EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    lpi->early_suspend.suspend = cm36283_early_suspend;
    lpi->early_suspend.resume = cm36283_late_resume;
    register_early_suspend(&lpi->early_suspend);
#endif
    D("[PS][CM36283] %s: Probe success!\n", __func__);

    return ret;

err_create_ls_device_file:
err_cm36283_setup:
    destroy_workqueue(lpi->lp_wq);
    wake_lock_destroy(&(lpi->ps_wake_lock));
    input_unregister_device(lpi->ls_input_dev);
    input_free_device(lpi->ls_input_dev);
    input_unregister_device(lpi->ps_input_dev);
    input_free_device(lpi->ps_input_dev);
err_create_singlethread_workqueue:
err_lightsensor_update_table:
    misc_deregister(&psensor_misc);
err_psensor_setup:
    mutex_destroy(&CM36283_control_mutex);
    mutex_destroy(&ps_enable_mutex);
    mutex_destroy(&ps_disable_mutex);
    mutex_destroy(&ps_get_adc_mutex);
    misc_deregister(&lightsensor_misc);
err_lightsensor_setup:
    mutex_destroy(&als_enable_mutex);
    mutex_destroy(&als_disable_mutex);
    mutex_destroy(&als_get_adc_mutex);
err_platform_data_null:
    kfree(lpi);
    return ret;
}
   
static int control_and_report( struct cm36283_info *lpi, uint8_t mode, uint16_t param ) {
    int ret=0;
    uint16_t adc_value = 0;
    uint16_t ps_data = 0;	
    int val;
    
    mutex_lock(&CM36283_control_mutex);
    if( mode == CONTROL_ALS )
    {
        if(param)
        {
            lpi->ls_cmd &= CM36283_ALS_SD_MASK;      
        }
        else
        {
            lpi->ls_cmd |= CM36283_ALS_SD;
        }
        _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);
        lpi->als_enable=param;
    }
    else if( mode == CONTROL_PS )
    {
        if(param)
        {
            lpi->ps_conf1_val &= CM36283_PS_SD_MASK;
            lpi->ps_conf1_val |= CM36283_PS_INT_IN_AND_OUT;
			irq_set_irq_wake(lpi->irq, 1);/*PERI-CH-PS_wake00++*/
        }
        else
        {
            lpi->ps_conf1_val |= CM36283_PS_SD;
            lpi->ps_conf1_val &= CM36283_PS_INT_MASK;
			irq_set_irq_wake(lpi->irq, 0);/*PERI-CH-PS_wake00++*/
        }
        _cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);    
        lpi->ps_enable=param;
    }
    if((mode == CONTROL_ALS)||(mode == CONTROL_PS))
    {
        if( param==1 )
        {
            msleep(100);  
        }
    }
/*PERI-CH-LS_report00++[*/
#define PS_CLOSE 1
#define PS_AWAY  (1<<1)
#define PS_CLOSE_AND_AWAY PS_CLOSE+PS_AWAY

    if(lpi->ps_enable)
    {
        int ps_status = 0;
        if( mode == CONTROL_PS )
            ps_status = PS_CLOSE_AND_AWAY;   
        else if(mode == CONTROL_INT_ISR_REPORT )
        {
            if ( param & INT_FLAG_PS_IF_CLOSE )
                ps_status |= PS_CLOSE;      
            if ( param & INT_FLAG_PS_IF_AWAY )
                ps_status |= PS_AWAY;
        }
        
        if (ps_status!=0)
        {
            switch(ps_status)
            {
                case PS_CLOSE_AND_AWAY:
                    get_stable_ps_adc_value(&ps_data);
                    val = (ps_data >= lpi->ps_close_thd_set) ? 0 : 1;
                    if(val)
                        wake_lock_timeout(&lpi->ps_wake_lock, 5*HZ);
                break;
                case PS_AWAY:
                    val = 1;
                    wake_lock_timeout(&lpi->ps_wake_lock, 5*HZ);
                break;
                case PS_CLOSE:
                    val = 0;
                break;
                /*MTD-PERIPHERAL-CH-FIX_coverity00++
                		default:
                		break;*/
            };
            D("[LS][CM3628] %s: val =%d, \n",__func__,val);
            input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, val);      
            input_sync(lpi->ps_input_dev);        
        }
    }

    if(lpi->als_enable)
    {
        if( mode == CONTROL_ALS ||
        ( mode == CONTROL_INT_ISR_REPORT && 
        ((param&INT_FLAG_ALS_IF_L)||(param&INT_FLAG_ALS_IF_H))))
        {
            lpi->ls_cmd &= CM36283_ALS_INT_MASK;
            ret = _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
            
            get_ls_adc_value(&adc_value, 0);

#if 0			
            /*MTD-PERIPHERAL-CH-FIX_coverity00++[*/
            if( lpi->ls_calibrate )
            {
                for (i = 0; i <= 9; i++)
                {
                    if (adc_value <= (*(lpi->cali_table + i)))
                    {
                        level = i;
                        if (*(lpi->cali_table + i))
                            break;
                    }
                    if ( i == 9) 
                    {/*avoid  i = 10, because 'cali_table' of size is 10 */
                        level = i;
                        break;
                    }
                }
            }
            else
            {
                for (i = 0; i <= 9; i++)
                {
                    if (adc_value <= (*(lpi->adc_table + i)))
                    {
                        level = i;
                        if (*(lpi->adc_table + i))
                            break;
                    }
                    if( i == 9)
                    {/*avoid  i = 10, because 'cali_table' of size is 10 */
                        level = i;
                        break;
                    }
                }
            }
#endif
            /*MTD-PERIPHERAL-CH-FIX_coverity00++]*/
            /*ret = set_lsensor_range(((i == 0) || (adc_value == 0)) ? 0 :
            *(lpi->cali_table + (i - 1)) + 1,
            *(lpi->cali_table + i));*/
            ret = set_lsensor_range(lpi->bottom_tresh,lpi->top_tresh);
			
            lpi->ls_cmd |= CM36283_ALS_INT_EN;
            
            ret = _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
            
            /*if ((i == 0) || (adc_value == 0))
            D("[LS][CM3628] %s: ADC=0x%03X, Level=%d, l_thd equal 0, h_thd = 0x%x \n",
            __func__, adc_value, level, *(lpi->cali_table + i));
            else
            D("[LS][CM3628] %s: ADC=0x%03X, Level=%d, l_thd = 0x%x, h_thd = 0x%x, i=%d \n",
            __func__, adc_value, level, *(lpi->cali_table + (i - 1)) + 1, *(lpi->cali_table + i),i);
            
            D("[LS][CM3628] %s: adc_table = %d\n",__func__,*(lpi->adc_table + level));
            lpi->current_level = level;*/
            lpi->current_adc = adc_value;
            /*input_report_abs(lpi->ls_input_dev, ABS_MISC, *(lpi->adc_table + level));*/
			input_report_abs(lpi->ls_input_dev, ABS_MISC, lpi->current_adc);
            input_sync(lpi->ls_input_dev);
        }
    }

/*PERI-CH-LS_report00++]*/
    mutex_unlock(&CM36283_control_mutex);
    return ret;
}


static const struct i2c_device_id cm36283_i2c_id[] = {
	{CM36283_I2C_NAME, 0},
	{}
};

static struct i2c_driver cm36283_driver = {
	.id_table = cm36283_i2c_id,
	.probe = cm36283_probe,
	//.remove = cm36283_remove,
	.driver = {
		.name = CM36283_I2C_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM        
		.pm     = &cm36283_pm_ops,
#endif
	},
};

static int __init cm36283_init(void)
{
	return i2c_add_driver(&cm36283_driver);
}

static void __exit cm36283_exit(void)
{
	i2c_del_driver(&cm36283_driver);
}

module_init(cm36283_init);
module_exit(cm36283_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CM36283 Driver");
MODULE_AUTHOR("Capella");
