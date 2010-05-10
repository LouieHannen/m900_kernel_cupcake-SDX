/*
 *  Copyright (C) 2004 Samsung Electronics
 *             SW.LEE <hitchcar@samsung.com>
 *            - based on Russell King : pcf8583.c
 * 	      - added  smdk24a0, smdk2440
 *            - added  poseidon (s3c24a0+wavecom)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Driver for FIMC2.x Camera Decoder
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-id.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/hardware.h>

#include <plat/gpio-cfg.h>
#include <plat/egpio.h>

#include "../s3c_camif.h"

#include "s5k4ca.h"

// Purpose of verifying I2C operaion. must be ignored later.
//#define LOCAL_CONFIG_S5K4CA_I2C_TEST

static struct i2c_driver s5k4ca_driver;

static void s5k4ca_sensor_gpio_init(void);
void s5k4ca_sensor_enable(void);
static void s5k4ca_sensor_disable(void);

static int s5k4ca_sensor_init(void);
static void s5k4ca_sensor_exit(void);

static int s5k4ca_sensor_change_size(struct i2c_client *client, int size);

#ifdef CONFIG_FLASH_AAT1271A
extern int aat1271a_flash_init(void);
extern void aat1271a_flash_exit(void);
extern void aat1271a_falsh_camera_control(int ctrl);
extern void aat1271a_falsh_movie_control(int ctrl);
#endif

/* 
 * MCLK: 24MHz, PCLK: 54MHz
 * 
 * In case of PCLK 54MHz
 *
 * Preview Mode (1024 * 768)  
 * 
 * Capture Mode (2048 * 1536)
 * 
 * Camcorder Mode
 */
static camif_cis_t s5k4ca_data = {
	itu_fmt:       	CAMIF_ITU601,
	order422:      	CAMIF_CRYCBY,
	camclk:        	24000000,		
	source_x:      	1024,		
	source_y:      	768,
	win_hor_ofst:  	0,
	win_ver_ofst:  	0,
	win_hor_ofst2: 	0,
	win_ver_ofst2: 	0,
	polarity_pclk: 	0,
	polarity_vsync:	1,
	polarity_href: 	0,
	reset_type:		CAMIF_RESET,
	reset_udelay: 	5000,
};

/* #define S5K4CA_ID	0x78 */

static unsigned short s5k4ca_normal_i2c[] = { (S5K4CA_ID >> 1), I2C_CLIENT_END };
static unsigned short s5k4ca_ignore[] = { I2C_CLIENT_END };
static unsigned short s5k4ca_probe[] = { I2C_CLIENT_END };

static unsigned short lux_value = 0;

static struct i2c_client_address_data s5k4ca_addr_data = {
	.normal_i2c = s5k4ca_normal_i2c,
	.ignore		= s5k4ca_ignore,
	.probe		= s5k4ca_probe,
};

static int s5k4ca_sensor_read(struct i2c_client *client,
		unsigned short subaddr, unsigned short *data)
{
	int ret;
	unsigned char buf[2];
	struct i2c_msg msg = { client->addr, 0, 2, buf };
	
	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);

	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	msg.flags = I2C_M_RD;
	
	ret = i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	*data = ((buf[0] << 8) | buf[1]);

error:
	return ret;
}

static int s5k4ca_sensor_write(struct i2c_client *client, 
		unsigned short subaddr, unsigned short val)
{
	if(subaddr == 0xdddd)
	{
			if (val == 0x0010)
				msleep(10);
			else if (val == 0x0020)
				msleep(20);
			else if (val == 0x0030)
				msleep(30);
			else if (val == 0x0040)
				msleep(40);
			else if (val == 0x0050)
				msleep(50);
			else if (val == 0x0100)
				msleep(100);
			printk("delay 0x%04x, value 0x%04x\n", subaddr, val);
	}	
	else
	{					
	unsigned char buf[4];
	struct i2c_msg msg = { client->addr, 0, 4, buf };

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);
	buf[2] = (val >> 8);
	buf[3] = (val & 0xFF);

	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}
}

static void s5k4ca_sensor_get_id(struct i2c_client *client)
{
	unsigned short id = 0;
	
	s5k4ca_sensor_write(client, 0x002C, 0x7000);
	s5k4ca_sensor_write(client, 0x002E, 0x01FA);
	s5k4ca_sensor_read(client, 0x0F12, &id);

	printk("Sensor ID(0x%04x) is %s!\n", id, (id == 0x4CA4) ? "Valid" : "Invalid"); 
}

static void s5k4ca_sensor_gpio_init(void)
{
	I2C_CAM_DIS;
	MCAM_RST_DIS;
	VCAM_RST_DIS;
	CAM_PWR_DIS;
	AF_PWR_DIS;
	MCAM_STB_DIS;
	VCAM_STB_DIS;
}

#if defined(CONFIG_LDO_LP8720)
extern void	s5k4ca_sensor_power_init(void);	
#endif

void s5k4ca_sensor_enable(void)
{
	s5k4ca_sensor_gpio_init();

	MCAM_STB_EN;

	/* > 0 ms */
	msleep(1);

	AF_PWR_EN;	

#if defined(CONFIG_LDO_LP8720)
	s5k4ca_sensor_power_init();	
#endif

	CAM_PWR_EN;

	/* > 0 ms */
	msleep(1);

	/* MCLK Set */
	clk_set_rate(cam_clock, s5k4ca_data.camclk);

	/* MCLK Enable */
	clk_enable(cam_clock);
	clk_enable(cam_hclk);
	
	msleep(1);

	MCAM_RST_EN;
	
	msleep(40);
	
	I2C_CAM_EN;
}

static void s5k4ca_sensor_disable(void)
{
	I2C_CAM_DIS;
	
	MCAM_STB_DIS;

	/* > 20 cycles */
	msleep(1);

	/* MCLK Disable */
	clk_disable(cam_clock);
	clk_disable(cam_hclk);

	/* > 0 ms */
	msleep(1);

	MCAM_RST_DIS;

	/* > 0 ms */
	msleep(1);

	AF_PWR_DIS;

	CAM_PWR_DIS;
}

static void sensor_init(struct i2c_client *client)
{
	int i, size;
	if (system_rev >= 0x40)
	{
		size = (sizeof(s5k4ca_init0_04)/sizeof(s5k4ca_init0_04[0]));
		for (i = 0; i < size; i++) { 
			s5k4ca_sensor_write(client,
				s5k4ca_init0_04[i].subaddr, s5k4ca_init0_04[i].value);
		}	

		msleep(10);	

		/* Check Sensor ID */
		s5k4ca_sensor_get_id(client);

		size = (sizeof(s5k4ca_init1_04)/sizeof(s5k4ca_init1_04[0]));
		for (i = 0; i < size; i++) { 
			s5k4ca_sensor_write(client,
				s5k4ca_init1_04[i].subaddr, s5k4ca_init1_04[i].value);
		}
	}
	else
	{
		size = (sizeof(s5k4ca_init0)/sizeof(s5k4ca_init0[0]));
		for (i = 0; i < size; i++) { 
			s5k4ca_sensor_write(client,
			s5k4ca_init0[i].subaddr, s5k4ca_init0[i].value);
		}	

		msleep(10);	

		/* Check Sensor ID */
		s5k4ca_sensor_get_id(client);

		size = (sizeof(s5k4ca_init1)/sizeof(s5k4ca_init1[0]));
		for (i = 0; i < size; i++) { 
			s5k4ca_sensor_write(client,
			s5k4ca_init1[i].subaddr, s5k4ca_init1[i].value);
		}
	}
	//s5k4ca_sensor_change_size(client, SENSOR_XGA);
}

static int s5k4cagx_attach(struct i2c_adapter *adap, int addr, int kind)
{
	struct i2c_client *c;
	
	c = kmalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		return -ENOMEM;

	memset(c, 0, sizeof(struct i2c_client));	

	strcpy(c->name, "s5k4ca");
	c->addr = addr;
	c->adapter = adap;
	c->driver = &s5k4ca_driver;
	s5k4ca_data.sensor = c;

#ifdef LOCAL_CONFIG_S5K4CA_I2C_TEST
	i2c_attach_client(c);
	msleep(10);
	sensor_init(c);

	return 0;
#else
	return i2c_attach_client(c);
#endif
}

static int s5k4ca_sensor_attach_adapter(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &s5k4ca_addr_data, s5k4cagx_attach);
}

static int s5k4ca_sensor_detach(struct i2c_client *client)
{
	i2c_detach_client(client);
	return 0;
}

static int s5k4ca_sensor_mode_set(struct i2c_client *client, int type)
{
	int i, size;
	unsigned short light;
	int delay = 300;

	printk("Sensor Mode ");

	if (system_rev >= 0x40)
	{

		if (type & SENSOR_PREVIEW)
		{	
			printk("-> Preview ");
			
			size = (sizeof(s5k4ca_preview_04)/sizeof(s5k4ca_preview_04[0]));
			for (i = 0; i < size; i++) {
				s5k4ca_sensor_write(client, s5k4ca_preview_04[i].subaddr, s5k4ca_preview_04[i].value);
			}
		}

		else if (type & SENSOR_CAPTURE)
		{	
			printk("-> Capture ");
				
			s5k4ca_sensor_write(client, 0x002C, 0x7000);	
			s5k4ca_sensor_write(client, 0x002E, 0x12FE);
			s5k4ca_sensor_read(client, 0x0F12, &light);
			lux_value = light;
		
			if (light <= 0x20) /* Low light */
			{	

				printk("Normal Low Light\n");
  			delay = 800;

				size = (sizeof(s5k4ca_snapshot_low_04)/sizeof(s5k4ca_snapshot_low_04[0]));
				for (i = 0; i < size; i++)	{
					s5k4ca_sensor_write(client, s5k4ca_snapshot_low_04[i].subaddr, s5k4ca_snapshot_low_04[i].value);
				}
			}
		
			else
			{
				if (light <= 0x40)
				{
				
					printk("Normal Middle Lignt\n");
//					delay = 600;
				}
				else
				{	
					printk("Normal Normal Light\n");
//					delay = 300;
				}
				delay = 200;
				size = (sizeof(s5k4ca_capture_04)/sizeof(s5k4ca_capture_04[0]));
				for (i = 0; i < size; i++)	{
					s5k4ca_sensor_write(client, s5k4ca_capture_04[i].subaddr, s5k4ca_capture_04[i].value);	
				}
			}
		}

		else if (type & SENSOR_FLASH_CAP_LOW)
				{	
  						printk("flash Normal Low Light Capture\n");
				  		delay = 300;
		
						size = (sizeof(s5k4ca_flashcapture_low_04)/sizeof(s5k4ca_flashcapture_low_04[0]));
						for (i = 0; i < size; i++)	{
							s5k4ca_sensor_write(client, s5k4ca_flashcapture_low_04[i].subaddr, s5k4ca_flashcapture_low_04[i].value);
						}
		}
		
		
		else if (type & SENSOR_FLASH_CAPTURE)
				{	
					  		printk("flash Normal Normal Light Capture\n");
							delay = 300;
						
						size = (sizeof(s5k4ca_flashcapture_04)/sizeof(s5k4ca_flashcapture_04[0]));
						for (i = 0; i < size; i++)	{
							s5k4ca_sensor_write(client, s5k4ca_flashcapture_04[i].subaddr, s5k4ca_flashcapture_04[i].value);	
						}
				}
				
		else if (type & SENSOR_CAMCORDER )
		{
			printk("Record\n");
						
						s5k4ca_sensor_write(client, 0xFCFC, 0xD000); 
            s5k4ca_sensor_write(client, 0x0028, 0x7000); 

            s5k4ca_sensor_write(client, 0x002A, 0x030E);  
            s5k4ca_sensor_write(client, 0x0F12, 0x00DF);  //030E = 00FF 입력위해 다른값 임시입력

            s5k4ca_sensor_write(client, 0x002A, 0x030C); 
            s5k4ca_sensor_write(client, 0x0F12, 0x0000); // AF Manual 

            msleep(130); //AF Manual 명령 인식위한 1frame delay (저조도 7.5fps 133ms 고려)
                         //여기까지 lens 움직임 없음
            s5k4ca_sensor_write(client, 0x002A, 0x030E);  
            s5k4ca_sensor_write(client, 0x0F12, 0x00E0);  // 030E = 00FF 입력. lens 움직임 
             
            msleep(50);  //lens가 목표지점까지 도달하기 위해 필요한 delay
            
			delay = 300;
			size = (sizeof(s5k4ca_fps_15fix_04)/sizeof(s5k4ca_fps_15fix_04[0]));
			for (i = 0; i < size; i++)  {
				s5k4ca_sensor_write(client, s5k4ca_fps_15fix_04[i].subaddr, s5k4ca_fps_15fix_04[i].value);	
			}
		}

		msleep(delay);
	
		printk("delay time(%d msec)\n", delay);
	
		return 0;
	}
	
	else  
	{
		if (type & SENSOR_PREVIEW)
		{	
			printk("-> Preview ");
            
			size = (sizeof(s5k4ca_preview)/sizeof(s5k4ca_preview[0]));
			for (i = 0; i < size; i++) {
				s5k4ca_sensor_write(client, s5k4ca_preview[i].subaddr, s5k4ca_preview[i].value);
			}
		}
		
		else if (type & SENSOR_CAPTURE)
		{	
			printk("-> Capture ");
						
			s5k4ca_sensor_write(client, 0x002C, 0x7000);	
			s5k4ca_sensor_write(client, 0x002E, 0x12FE);
			s5k4ca_sensor_read(client, 0x0F12, &light);
				
			if (light <= 0x20) /* Low light */
			{	
		
				printk("Normal Low Light\n");
//				delay = 1200;
		
				size = (sizeof(s5k4ca_snapshot_low)/sizeof(s5k4ca_snapshot_low[0]));
				for (i = 0; i < size; i++)	{
					s5k4ca_sensor_write(client, s5k4ca_snapshot_low[i].subaddr, s5k4ca_snapshot_low[i].value);
				}
			}
		
			else
			{
				if (light <= 0x40)
				{
						
					printk("Normal Middle Lignt\n");
//					delay = 600;
				}
				else
				{	
					printk("Normal Normal Light\n");
//					delay = 300;
				}
		
				size = (sizeof(s5k4ca_capture)/sizeof(s5k4ca_capture[0]));

				for (i = 0; i < size; i++)	{
					s5k4ca_sensor_write(client, s5k4ca_capture[i].subaddr, s5k4ca_capture[i].value);	
				}
			}
		}

		else if (type & SENSOR_FLASH_CAP_LOW)
				{	
  						printk("flash Normal Low Light Capture\n");
		//				delay = 1200;
		
						size = (sizeof(s5k4ca_flashcapture_low)/sizeof(s5k4ca_flashcapture_low[0]));
						for (i = 0; i < size; i++)	{
							s5k4ca_sensor_write(client, s5k4ca_flashcapture_low[i].subaddr, s5k4ca_flashcapture_low[i].value);
						}
		}
		
		
		else if (type & SENSOR_FLASH_CAPTURE)
				{	
					  		printk("flash Normal Normal Light Capture\n");
		//					delay = 300;
						
						size = (sizeof(s5k4ca_flashcapture)/sizeof(s5k4ca_flashcapture[0]));
						for (i = 0; i < size; i++)	{
							s5k4ca_sensor_write(client, s5k4ca_flashcapture[i].subaddr, s5k4ca_flashcapture[i].value);	
						}
				}


	
		else if (type & SENSOR_CAMCORDER )
		{
			printk("Record\n");
			delay = 300;
			size = (sizeof(s5k4ca_fps_15fix)/sizeof(s5k4ca_fps_15fix[0]));
			for (i = 0; i < size; i++)	{
				s5k4ca_sensor_write(client, s5k4ca_fps_15fix[i].subaddr, s5k4ca_fps_15fix[i].value);	
			}
		}
		
		msleep(delay);
		
		printk("delay time(%d msec)\n", delay);
			
		return 0;
	}
}

static int s5k4ca_sensor_change_size(struct i2c_client *client, int size)
{
	switch (size) {
		case SENSOR_XGA:
			s5k4ca_sensor_mode_set(client, SENSOR_PREVIEW);
			break;

		case SENSOR_QXGA:
			s5k4ca_sensor_mode_set(client, SENSOR_CAPTURE);
			break;		
	
		default:
			printk("Unknown Size! (Only XGA & QXGA)\n");
			break;
	}

	return 0;
}


static int s5k4ca_sensor_af_control(struct i2c_client *client, int type)
{

    int count = 50;
    int tmpVal = 0;
    int ret = 0;
    int size = 0;
    int i = 0;
    unsigned short light = 0;

    switch (type)
    {
        case 1:
            printk("Focus Mode -> Single\n");
#if 0
						s5k4ca_sensor_write(client, 0xFCFC, 0xD000);	
						s5k4ca_sensor_write(client, 0x002C, 0x7000);
						s5k4ca_sensor_write(client, 0x002E, 0x12FE);
						msleep(100);
						s5k4ca_sensor_read(client, 0x0F12, &light);
						if (light < 0x80){ /* Low light AF*/
							
							size = (sizeof(s5k4ca_af_low_lux_val)/sizeof(s5k4ca_af_low_lux_val[0]));
							for (i = 0; i < size; i++)	{
								s5k4ca_sensor_write(client, s5k4ca_af_low_lux_val[i].subaddr, s5k4ca_af_low_lux_val[i].value);	
							}
							printk("[CAM-SENSOR] =Low Light AF Single light=0x%04x\n",light);
						}
						else{
							size = (sizeof(s5k4ca_af_normal_lux_val)/sizeof(s5k4ca_af_normal_lux_val[0]));
							for (i = 0; i < size; i++)	{
								s5k4ca_sensor_write(client, s5k4ca_af_normal_lux_val[i].subaddr, s5k4ca_af_normal_lux_val[i].value);	
							}
							printk("[CAM-SENSOR] =Normal Light AF Single light=0x%04x\n",light);
						}
#endif
						
						s5k4ca_sensor_write(client, 0xFCFC, 0xD000);	
						s5k4ca_sensor_write(client, 0x002C, 0x7000);
						s5k4ca_sensor_write(client, 0x002E, 0x12FE);
						//			msleep(100);
						s5k4ca_sensor_read(client, 0x0F12, &light);
						
						lux_value = light;
						
						s5k4ca_sensor_write(client, 0xFCFC, 0xD000); 
            s5k4ca_sensor_write(client, 0x0028, 0x7000); 

            s5k4ca_sensor_write(client, 0x002A, 0x030E);  
            s5k4ca_sensor_write(client, 0x0F12, 0x00DF);  //030E = 00FF 입력위해 다른값 임시입력

            s5k4ca_sensor_write(client, 0x002A, 0x030C); 
            s5k4ca_sensor_write(client, 0x0F12, 0x0000); // AF Manual 

            msleep(130); //AF Manual 명령 인식위한 1frame delay (저조도 7.5fps 133ms 고려)
                         //여기까지 lens 움직임 없음
            s5k4ca_sensor_write(client, 0x002A, 0x030E);  
            s5k4ca_sensor_write(client, 0x0F12, 0x00E0);  //
             
            msleep(50);  //lens가 목표지점까지 도달하기 위해 필요한 delay

       //     s5k4ca_sensor_write(client, 0x002A, 0x030C); 
       //     s5k4ca_sensor_write(client, 0x0F12, 0x0003); // AF Freeze 
       //     msleep(50);  

       //AF freeze 를 하면 AF power off를 하게 되어 약간의 power 소모 개선이 있습니다.
       //필요에따라 사용하시기 바랍니다.

						s5k4ca_sensor_write(client, 0xFCFC, 0xD000);
            s5k4ca_sensor_write(client, 0x0028, 0x7000);
            s5k4ca_sensor_write(client, 0x002A, 0x030C);
            s5k4ca_sensor_write(client, 0x0F12, 0x0002); //AF Single 

						s5k4ca_sensor_write(client, 0x0028, 0x7000);   //AE AWE OFF(unlock)
            s5k4ca_sensor_write(client, 0x002A, 0x12DA);
            s5k4ca_sensor_write(client, 0x0F12, 0x0000);						
            s5k4ca_sensor_write(client, 0x002A, 0x122E);
            s5k4ca_sensor_write(client, 0x0F12, 0x0000);
            
            do
            {
                if( count == 0)
                    break;

	            	s5k4ca_sensor_write(client, 0xFCFC, 0xD000);
                s5k4ca_sensor_write(client, 0x002C, 0x7000);    
                s5k4ca_sensor_write(client, 0x002E, 0x130E);
                if (light < 0x80)
	                msleep(250);
								else
									msleep(100);
                s5k4ca_sensor_read(client, 0x0F12, &tmpVal); 

                count--;

                printk("CAM 3M AF Status Value = %x \n", tmpVal); 

            }
            while( (tmpVal & 0x3) != 0x3 && (tmpVal & 0x3) != 0x2 );

            if(count == 0  )
            {
            		s5k4ca_sensor_write(client, 0xFCFC, 0xD000); 
            		s5k4ca_sensor_write(client, 0x0028, 0x7000); 

		            s5k4ca_sensor_write(client, 0x002A, 0x030E);  
            		s5k4ca_sensor_write(client, 0x0F12, 0x00DF);  //030E = 00FF 입력위해 다른값 임시입력

		            s5k4ca_sensor_write(client, 0x002A, 0x030C); 
            		s5k4ca_sensor_write(client, 0x0F12, 0x0000); // AF Manual 

		            msleep(130); //AF Manual 명령 인식위한 1frame delay (저조도 7.5fps 133ms 고려)
                         //여기까지 lens 움직임 없음
    		        s5k4ca_sensor_write(client, 0x002A, 0x030E);  
        		    s5k4ca_sensor_write(client, 0x0F12, 0x00E0);  // 030E = 00FF 입력. lens 움직임 
             
            		msleep(50);  //lens가 목표지점까지 도달하기 위해 필요한 delay
            		            	
            		ret = 0;
            		printk("CAM 3M AF_Single Mode Fail.==> TIMEOUT \n");
                
            }

            if((tmpVal & 0x3) == 0x02)
            {
            		s5k4ca_sensor_write(client, 0xFCFC, 0xD000); 
            		s5k4ca_sensor_write(client, 0x0028, 0x7000); 

		            s5k4ca_sensor_write(client, 0x002A, 0x030E);  
            		s5k4ca_sensor_write(client, 0x0F12, 0x00DF);  //030E = 00FF 입력위해 다른값 임시입력

		            s5k4ca_sensor_write(client, 0x002A, 0x030C); 
            		s5k4ca_sensor_write(client, 0x0F12, 0x0000); // AF Manual 

		            msleep(130); //AF Manual 명령 인식위한 1frame delay (저조도 7.5fps 133ms 고려)
                         //여기까지 lens 움직임 없음
    		        s5k4ca_sensor_write(client, 0x002A, 0x030E);  
        		    s5k4ca_sensor_write(client, 0x0F12, 0x00E0);  // 030E = 00FF 입력. lens 움직임 
             
            		msleep(50);  //lens가 목표지점까지 도달하기 위해 필요한 delay
              	
              	ret = 0;

	            	printk("CAM 3M AF_Single Mode Fail.==> FAIL \n");
            }

            if(tmpVal & 0x3 == 0x3)
            {
                ret = 1;
                printk("CAM 3M AF_Single Mode SUCCESS. \r\n");
            }
            
            printk("CAM:3M AF_SINGLE SET \r\n");
            
            break;

        case 0:

            printk("Focus Mode -> Manual\n");

#if 0          //충돌 소음 방지

            s5k4ca_sensor_write(client, 0x002C, 0x7000);
            s5k4ca_sensor_write(client, 0x002E, 0x030E);
            s5k4ca_sensor_write(client, 0x0F12, 0x00FE);

            s5k4ca_sensor_write(client, 0x002E, 0x030C);
            s5k4ca_sensor_write(client, 0x0F12, 0x0000);
            msleep(130);

            s5k4ca_sensor_write(client, 0x002E, 0x030E);
            s5k4ca_sensor_write(client, 0x0F12, 0x00FF);
            msleep(50);         

            s5k4ca_sensor_write(client, 0x0F12, 0x0003);
#endif
            break;

        default:
            break;
    }
               
            s5k4ca_sensor_write(client, 0x0028, 0x7000);   //AE AWE ON(lock)
            s5k4ca_sensor_write(client, 0x002A, 0x12DA);
            s5k4ca_sensor_write(client, 0x0F12, 0x0001);						
            s5k4ca_sensor_write(client, 0x002A, 0x122E);
            s5k4ca_sensor_write(client, 0x0F12, 0x0001);
    return ret;
}

static int s5k4ca_sensor_change_effect(struct i2c_client *client, int type)
{
	int i, size;	
	
	printk("Effects Mode ");

	if (system_rev >= 0x40)
	{

		switch (type)
		{
			case 0:
			default:
				printk("-> Mode None\n");
				size = (sizeof(s5k4ca_effect_off_04)/sizeof(s5k4ca_effect_off_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_off_04[i].subaddr, s5k4ca_effect_off_04[i].value);
				}
			break;

			case 1:
				printk("-> Mode Gray\n");
				size = (sizeof(s5k4ca_effect_gray_04)/sizeof(s5k4ca_effect_gray_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_gray_04[i].subaddr, s5k4ca_effect_gray_04[i].value);
				}
			break;

			case 2:
				printk("-> Mode Sepia\n");
				size = (sizeof(s5k4ca_effect_sepia_04)/sizeof(s5k4ca_effect_sepia_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_sepia_04[i].subaddr, s5k4ca_effect_sepia_04[i].value);
				}
			break;

			case 3:
				printk("-> Mode Negative\n");
				size = (sizeof(s5k4ca_effect_negative_04)/sizeof(s5k4ca_effect_negative_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_negative_04[i].subaddr, s5k4ca_effect_negative_04[i].value);
				}
			break;
			
			case 4:
				printk("-> Mode Aqua\n");
				size = (sizeof(s5k4ca_effect_aqua_04)/sizeof(s5k4ca_effect_aqua_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_aqua_04[i].subaddr, s5k4ca_effect_aqua_04[i].value);
				}
			break;

			case 5:
				printk("-> Mode Sketch\n");
				size = (sizeof(s5k4ca_effect_sketch_04)/sizeof(s5k4ca_effect_sketch_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_sketch_04[i].subaddr, s5k4ca_effect_sketch_04[i].value);
				}
			break;
		}

	return 0;

	}
	
	else
	{
		switch (type)
		{
			case 0:
			default:
				printk("-> Mode None\n");
				size = (sizeof(s5k4ca_effect_off)/sizeof(s5k4ca_effect_off[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_off[i].subaddr, s5k4ca_effect_off[i].value);
				}
			break;

			case 1:
				printk("-> Mode Gray\n");
				size = (sizeof(s5k4ca_effect_gray)/sizeof(s5k4ca_effect_gray[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_gray[i].subaddr, s5k4ca_effect_gray[i].value);
				}
			break;
			
			case 2:
				printk("-> Mode Sepia\n");
				size = (sizeof(s5k4ca_effect_sepia)/sizeof(s5k4ca_effect_sepia[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_sepia[i].subaddr, s5k4ca_effect_sepia[i].value);
				}
			break;

			case 3:
				printk("-> Mode Negative\n");
				size = (sizeof(s5k4ca_effect_negative)/sizeof(s5k4ca_effect_negative[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_negative[i].subaddr, s5k4ca_effect_negative[i].value);
				}
			break;

			case 4:
				printk("-> Mode Aqua\n");
				size = (sizeof(s5k4ca_effect_aqua)/sizeof(s5k4ca_effect_aqua[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_aqua[i].subaddr, s5k4ca_effect_aqua[i].value);
				}
			break;

			case 5:
				printk("-> Mode Sketch\n");
				size = (sizeof(s5k4ca_effect_sketch)/sizeof(s5k4ca_effect_sketch[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_effect_sketch[i].subaddr, s5k4ca_effect_sketch[i].value);
				}
			break;
		}

		return 0;
	}
}

static int s5k4ca_sensor_change_br(struct i2c_client *client, int type)
{
	int i, size;

	printk("Brightness Mode \n");

	if (system_rev >= 0x40)
	{

		switch (type)
		{
			case 0: 
			default :
				printk("-> Brightness Minus 4\n");
				size = (sizeof(s5k4ca_br_minus4_04)/sizeof(s5k4ca_br_minus4_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus4_04[i].subaddr, s5k4ca_br_minus4_04[i].value);
				}
			break;

			case 1:
				printk("-> Brightness Minus 3\n");	
				size = (sizeof(s5k4ca_br_minus3_04)/sizeof(s5k4ca_br_minus3_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus3_04[i].subaddr, s5k4ca_br_minus3_04[i].value);
				}
			break;
			
			case 2:
				printk("-> Brightness Minus 2\n");
				size = (sizeof(s5k4ca_br_minus2_04)/sizeof(s5k4ca_br_minus2_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus2_04[i].subaddr, s5k4ca_br_minus2_04[i].value);
				}
			break;
			
			case 3:				
				printk("-> Brightness Minus 1\n");
				size = (sizeof(s5k4ca_br_minus1_04)/sizeof(s5k4ca_br_minus1_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus1_04[i].subaddr, s5k4ca_br_minus1_04[i].value);
				}
			break;
			
			case 4:
				printk("-> Brightness Zero\n");
				size = (sizeof(s5k4ca_br_zero_04)/sizeof(s5k4ca_br_zero_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_zero_04[i].subaddr, s5k4ca_br_zero_04[i].value);
				}
			break;

			case 5:
				printk("-> Brightness Plus 1\n");
				size = (sizeof(s5k4ca_br_plus1_04)/sizeof(s5k4ca_br_plus1_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus1_04[i].subaddr, s5k4ca_br_plus1_04[i].value);
				}
			break;

			case 6:
				printk("-> Brightness Plus 2\n");
				size = (sizeof(s5k4ca_br_plus2_04)/sizeof(s5k4ca_br_plus2_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus2_04[i].subaddr, s5k4ca_br_plus2_04[i].value);
				}
			break;

			case 7:
				printk("-> Brightness Plus 3\n");
				size = (sizeof(s5k4ca_br_plus3_04)/sizeof(s5k4ca_br_plus3_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus3_04[i].subaddr, s5k4ca_br_plus3_04[i].value);
				}
			break;

			case 8:
				printk("-> Brightness Plus 4\n");
				size = (sizeof(s5k4ca_br_plus4_04)/sizeof(s5k4ca_br_plus4_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus4_04[i].subaddr, s5k4ca_br_plus4_04[i].value);
				}
			break;
			
		}

		return 0;

	}

	else
	{
		switch (type)
		{
			case 0:	
			default :
				printk("-> Brightness Minus 4\n");
				size = (sizeof(s5k4ca_br_minus4)/sizeof(s5k4ca_br_minus4[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus4[i].subaddr, s5k4ca_br_minus4[i].value);
				}
			break;

			case 1:
				printk("-> Brightness Minus 3\n");	
				size = (sizeof(s5k4ca_br_minus3)/sizeof(s5k4ca_br_minus3[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus3[i].subaddr, s5k4ca_br_minus3[i].value);
				}
			break;

			case 2:
				printk("-> Brightness Minus 2\n");
				size = (sizeof(s5k4ca_br_minus2)/sizeof(s5k4ca_br_minus2[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus2[i].subaddr, s5k4ca_br_minus2[i].value);
				}
			break;

			case 3:
				printk("-> Brightness Minus 1\n");
				size = (sizeof(s5k4ca_br_minus1)/sizeof(s5k4ca_br_minus1[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_minus1[i].subaddr, s5k4ca_br_minus1[i].value);
				}
			break;

			case 4:
				printk("-> Brightness Zero\n");
				size = (sizeof(s5k4ca_br_zero)/sizeof(s5k4ca_br_zero[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_zero[i].subaddr, s5k4ca_br_zero[i].value);
				}
			break;

			case 5:
				printk("-> Brightness Plus 1\n");
				size = (sizeof(s5k4ca_br_plus1)/sizeof(s5k4ca_br_plus1[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus1[i].subaddr, s5k4ca_br_plus1[i].value);
				}
			break;

			case 6:
				printk("-> Brightness Plus 2\n");
				size = (sizeof(s5k4ca_br_plus2)/sizeof(s5k4ca_br_plus2[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus2[i].subaddr, s5k4ca_br_plus2[i].value);
				}
			break;
			
			case 7:
				printk("-> Brightness Plus 3\n");
				size = (sizeof(s5k4ca_br_plus3)/sizeof(s5k4ca_br_plus3[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus3[i].subaddr, s5k4ca_br_plus3[i].value);
				}
			break;

			case 8:
				printk("-> Brightness Plus 4\n");
				size = (sizeof(s5k4ca_br_plus4)/sizeof(s5k4ca_br_plus4[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_br_plus4[i].subaddr, s5k4ca_br_plus4[i].value);
				}
			break;
		}

		return 0;
	}
}

static int s5k4ca_sensor_change_wb(struct i2c_client *client, int type)
{
	int i, size;
	
	printk("White Balance Mode ");

	if (system_rev >= 0x40)
	{
		switch (type)
		{
			case 0:
			default :
				printk("-> WB auto mode\n");
				size = (sizeof(s5k4ca_wb_auto_04)/sizeof(s5k4ca_wb_auto_04[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_wb_auto_04[i].subaddr, s5k4ca_wb_auto_04[i].value);
				}
			break;
			
			case 1:
				printk("-> WB Sunny mode\n");
				size = (sizeof(s5k4ca_wb_sunny_04)/sizeof(s5k4ca_wb_sunny_04[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_sunny_04[i].subaddr, s5k4ca_wb_sunny_04[i].value);					
				}
			break;

			case 2:
				printk("-> WB Cloudy mode\n");
				size = (sizeof(s5k4ca_wb_cloudy_04)/sizeof(s5k4ca_wb_cloudy_04[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_cloudy_04[i].subaddr, s5k4ca_wb_cloudy_04[i].value);					
				}
			break;

			case 3:
				printk("-> WB Flourescent mode\n");
				size = (sizeof(s5k4ca_wb_fluorescent_04)/sizeof(s5k4ca_wb_fluorescent_04[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_fluorescent_04[i].subaddr, s5k4ca_wb_fluorescent_04[i].value);					
				}
			break;

			case 4:
				printk("-> WB Tungsten mode\n");
				size = (sizeof(s5k4ca_wb_tungsten_04)/sizeof(s5k4ca_wb_tungsten_04[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_tungsten_04[i].subaddr, s5k4ca_wb_tungsten_04[i].value);					
				}
			break;
		}

	return 0;

	}

	else
	{
		switch (type)
		{
			case 0:
			default :
				printk("-> WB auto mode\n");
				size = (sizeof(s5k4ca_wb_auto)/sizeof(s5k4ca_wb_auto[0]));
				for (i = 0; i < size; i++) {
					s5k4ca_sensor_write(client, s5k4ca_wb_auto[i].subaddr, s5k4ca_wb_auto[i].value);
				}
			break;

			case 1:
				printk("-> WB Sunny mode\n");
				size = (sizeof(s5k4ca_wb_sunny)/sizeof(s5k4ca_wb_sunny[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_sunny[i].subaddr, s5k4ca_wb_sunny[i].value);					
				}
			break;

			case 2:
				printk("-> WB Cloudy mode\n");
				size = (sizeof(s5k4ca_wb_cloudy)/sizeof(s5k4ca_wb_cloudy[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_cloudy[i].subaddr, s5k4ca_wb_cloudy[i].value);					
				}
			break;

			case 3:
				printk("-> WB Flourescent mode\n");
				size = (sizeof(s5k4ca_wb_fluorescent)/sizeof(s5k4ca_wb_fluorescent[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_fluorescent[i].subaddr, s5k4ca_wb_fluorescent[i].value);					
				}
			break;

			case 4:
				printk("-> WB Tungsten mode\n");
				size = (sizeof(s5k4ca_wb_tungsten)/sizeof(s5k4ca_wb_tungsten[0]));
				for (i = 0; i < size; i++)  {
					s5k4ca_sensor_write(client, s5k4ca_wb_tungsten[i].subaddr, s5k4ca_wb_tungsten[i].value);					
				}
			break;
		}
	return 0;
	}
}

static int s5k4ca_sensor_user_read(struct i2c_client *client, s5k4ca_t *r_data)
{
	s5k4ca_sensor_write(client, 0x002C, r_data->page);
	s5k4ca_sensor_write(client, 0x002E, r_data->subaddr);
	return s5k4ca_sensor_read(client, 0x0F12, &(r_data->value));
}

static int s5k4ca_sensor_user_write(struct i2c_client *client, unsigned short *w_data)
{
	return s5k4ca_sensor_write(client, w_data[0], w_data[1]);
}

static int s5k4ca_sensor_exif_read(struct i2c_client *client, exif_data_t *exif_data)
{
	int ret = 0;
	
//unsigned short lux = 0;
	unsigned short extime = 0;

	s5k4ca_sensor_write(client, 0xFCFC, 0xD000);	/// exposure time
	s5k4ca_sensor_write(client, 0x002C, 0x7000);
	s5k4ca_sensor_write(client, 0x002E, 0x1C3C);
//	msleep(100);
	s5k4ca_sensor_read(client, 0x0F12, &extime);

/*	msleep(100);

	s5k4ca_sensor_write(client, 0xFCFC, 0xD000);	/// Incident Light value 
	s5k4ca_sensor_write(client, 0x002C, 0x7000);
	s5k4ca_sensor_write(client, 0x002E, 0x12FE);
	msleep(100);
	s5k4ca_sensor_read(client, 0x0F12, &lux); */

	exif_data->exposureTime = extime/100;
	exif_data->lux = lux_value;
	(printk("[CAM-SENSOR] =%s extime=%d, lux=%d,\n",__func__,extime/100,lux_value));

	return ret;
}

static int s5k4ca_sensor_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	struct v4l2_control *ctrl;
	unsigned short *w_data;		/* To support user level i2c */	
	s5k4ca_short_t *r_data;
	exif_data_t *exif_data;

	int ret=0;

	switch (cmd)
	{
		case SENSOR_INIT:
			sensor_init(client);
			break;

		case USER_ADD:
			break;

		case USER_EXIT:
			s5k4ca_sensor_exit();
			break;

		case SENSOR_EFFECT:
			ctrl = (struct v4l2_control *)arg;
			s5k4ca_sensor_change_effect(client, ctrl->value);
			break;

		case SENSOR_BRIGHTNESS:
			ctrl = (struct v4l2_control *)arg;
			s5k4ca_sensor_change_br(client, ctrl->value);
			break;

		case SENSOR_WB:
			ctrl = (struct v4l2_control *)arg;
			s5k4ca_sensor_change_wb(client, ctrl->value);
			break;

		case SENSOR_AF:
			ctrl = (struct v4l2_control *)arg;
			ret = s5k4ca_sensor_af_control(client, ctrl->value);
			break;

		case SENSOR_MODE_SET:
			ctrl = (struct v4l2_control *)arg;
			s5k4ca_sensor_mode_set(client, ctrl->value);
			break;

		case SENSOR_XGA:
			s5k4ca_sensor_change_size(client, SENSOR_XGA);	
			break;

		case SENSOR_QXGA:
			s5k4ca_sensor_change_size(client, SENSOR_QXGA);	
			break;

		case SENSOR_QSVGA:
			s5k4ca_sensor_change_size(client, SENSOR_QSVGA);
			break;

		case SENSOR_VGA:
			s5k4ca_sensor_change_size(client, SENSOR_VGA);
			break;

		case SENSOR_SVGA:
			s5k4ca_sensor_change_size(client, SENSOR_SVGA);
			break;

		case SENSOR_SXGA:
			s5k4ca_sensor_change_size(client, SENSOR_SXGA);
			break;

		case SENSOR_UXGA:
			s5k4ca_sensor_change_size(client, SENSOR_UXGA);
			break;

		case SENSOR_USER_WRITE:
			w_data = (unsigned short *)arg;
			s5k4ca_sensor_user_write(client, w_data);
			break;

		case SENSOR_USER_READ:
			r_data = (s5k4ca_short_t *)arg;
			s5k4ca_sensor_user_read(client, r_data);
			break;
	
		case SENSOR_FLASH_CAMERA:
			ctrl = (struct v4l2_control *)arg;
#ifdef CONFIG_FLASH_AAT1271A
			aat1271a_falsh_camera_control(ctrl->value);	
#endif			
			break;

		case SENSOR_FLASH_MOVIE:
			ctrl = (struct v4l2_control *)arg;
#ifdef CONFIG_FLASH_AAT1271A
			aat1271a_falsh_movie_control(ctrl->value);	
#endif
			break;

		case SENSOR_EXIF_DATA:
			exif_data = (exif_data_t *)arg;
			s5k4ca_sensor_exif_read(client, exif_data);	
			break;

		default:
			break;
	}

	return ret;
}

static struct i2c_driver s5k4ca_driver = {
	.driver = {
		.name = "s5k4ca",
	},
	.id = S5K4CA_ID,
	.attach_adapter = s5k4ca_sensor_attach_adapter,
	.detach_client = s5k4ca_sensor_detach,
	.command = s5k4ca_sensor_command
};

static int s5k4ca_sensor_init(void)
{
	int ret;

//	s5k4ca_sensor_enable();
	
	s3c_camif_open_sensor(&s5k4ca_data);

	if (s5k4ca_data.sensor == NULL)
		if ((ret = i2c_add_driver(&s5k4ca_driver)))
			return ret;

	if (s5k4ca_data.sensor == NULL) {
		i2c_del_driver(&s5k4ca_driver);	
		return -ENODEV;
	}

	s3c_camif_register_sensor(&s5k4ca_data);
	
	return 0;
}

static void s5k4ca_sensor_exit(void)
{
	s5k4ca_sensor_disable();
	
	if (s5k4ca_data.sensor != NULL)
		s3c_camif_unregister_sensor(&s5k4ca_data);
}

static struct v4l2_input s5k4ca_input = {
	.index		= 0,
	.name		= "Camera Input (S5K4CA)",
	.type		= V4L2_INPUT_TYPE_CAMERA,
	.audioset	= 1,
	.tuner		= 0,
	.std		= V4L2_STD_PAL_BG | V4L2_STD_NTSC_M,
	.status		= 0,
};

static struct v4l2_input_handler s5k4ca_input_handler = {
	s5k4ca_sensor_init,
	s5k4ca_sensor_exit	
};

#ifdef CONFIG_VIDEO_SAMSUNG_MODULE
static int s5k4ca_sensor_add(void)
{
#ifdef CONFIG_FLASH_AAT1271A
	aat1271a_flash_init();
#endif
	return s3c_camif_add_sensor(&s5k4ca_input, &s5k4ca_input_handler);
}

static void s5k4ca_sensor_remove(void)
{
	if (s5k4ca_data.sensor != NULL)
		i2c_del_driver(&s5k4ca_driver);
#ifdef CONFIG_FLASH_AAT1271A
	aat1271a_flash_exit();
#endif
	s3c_camif_remove_sensor(&s5k4ca_input, &s5k4ca_input_handler);
}

module_init(s5k4ca_sensor_add)
module_exit(s5k4ca_sensor_remove)

MODULE_AUTHOR("Jinsung, Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("I2C Client Driver For FIMC V4L2 Driver");
MODULE_LICENSE("GPL");
#else
int s5k4ca_sensor_add(void)
{
#ifdef CONFIG_FLASH_AAT1271A
	aat1271a_flash_init();
#endif	
#ifdef LOCAL_CONFIG_S5K4CA_I2C_TEST
	return s5k4ca_sensor_init();
#else
	return s3c_camif_add_sensor(&s5k4ca_input, &s5k4ca_input_handler);
#endif
}

void s5k4ca_sensor_remove(void)
{
	if (s5k4ca_data.sensor != NULL)
		i2c_del_driver(&s5k4ca_driver);
#ifdef CONFIG_FLASH_AAT1271A
	aat1271a_flash_exit();
#endif
	s3c_camif_remove_sensor(&s5k4ca_input, &s5k4ca_input_handler);
}
#endif
