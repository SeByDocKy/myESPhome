#include <Arduino.h>
#include <Wire.h>
#include "esphome.h"

#define I2C_SDA 21
#define I2C_SCL 22

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2021, 10, 0)
#define DELAY delay
#else
#define DELAY delay
#endif


/**\name Macro definitions */

/**\name API success code */
#define BMM150_OK	(0)

/**\name API error codes */
#define BMM150_E_ID_NOT_CONFORM		    (-1)
#define BMM150_E_INVALID_CONFIG         (-2)
// #define BMM150_E_ID_WRONG		    (-3)

/**\name API warning codes */
#define BMM150_W_NORMAL_SELF_TEST_YZ_FAIL	INT8_C(1)
#define BMM150_W_NORMAL_SELF_TEST_XZ_FAIL	INT8_C(2)
#define BMM150_W_NORMAL_SELF_TEST_Z_FAIL	INT8_C(3)
#define BMM150_W_NORMAL_SELF_TEST_XY_FAIL	INT8_C(4)
#define BMM150_W_NORMAL_SELF_TEST_Y_FAIL	INT8_C(5)
#define BMM150_W_NORMAL_SELF_TEST_X_FAIL	INT8_C(6)
#define BMM150_W_NORMAL_SELF_TEST_XYZ_FAIL	INT8_C(7)
#define BMM150_W_ADV_SELF_TEST_FAIL		INT8_C(8)

#define BMM150_I2C_Address  0x13

/**\name CHIP ID & SOFT RESET VALUES      */
#define BMM150_CHIP_ID              0x32
#define BMM150_SET_SOFT_RESET		0x82

/**\name POWER MODE DEFINTIONS      */
#define BMM150_NORMAL_MODE		0x00
#define BMM150_FORCED_MODE		0x01
#define BMM150_SLEEP_MODE		0x03
#define BMM150_SUSPEND_MODE		0x04

/**\name PRESET MODE DEFINITIONS  */
#define BMM150_PRESETMODE_LOWPOWER                 0x01
#define BMM150_PRESETMODE_REGULAR                  0x02
#define BMM150_PRESETMODE_HIGHACCURACY             0x03
#define BMM150_PRESETMODE_ENHANCED                 0x04

/**\name Power mode settings  */
#define	BMM150_POWER_CNTRL_DISABLE	0x00
#define	BMM150_POWER_CNTRL_ENABLE	0x01

/**\name Sensor delay time settings  */
#define BMM150_SOFT_RESET_DELAY		(1)
#define BMM150_NORMAL_SELF_TEST_DELAY	(2)
#define BMM150_START_UP_TIME		(3)
#define BMM150_ADV_SELF_TEST_DELAY	(4)

/**\name ENABLE/DISABLE DEFINITIONS  */
#define BMM150_XY_CHANNEL_ENABLE	0x00
#define BMM150_XY_CHANNEL_DISABLE	0x03

/**\name Register Address */
#define BMM150_CHIP_ID_ADDR		    0x40
#define BMM150_DATA_X_LSB		    0x42
#define BMM150_DATA_X_MSB		    0x43
#define BMM150_DATA_Y_LSB		    0x44
#define BMM150_DATA_Y_MSB		    0x45
#define BMM150_DATA_Z_LSB		    0x46
#define BMM150_DATA_Z_MSB		    0x47
#define BMM150_DATA_READY_STATUS	0x48
#define BMM150_INTERRUPT_STATUS		0x4A
#define BMM150_POWER_CONTROL_ADDR	0x4B
#define BMM150_OP_MODE_ADDR		    0x4C
#define BMM150_INT_CONFIG_ADDR		0x4D
#define BMM150_AXES_ENABLE_ADDR		0x4E
#define BMM150_LOW_THRESHOLD_ADDR	0x4F
#define BMM150_HIGH_THRESHOLD_ADDR	0x50
#define BMM150_REP_XY_ADDR		    0x51
#define BMM150_REP_Z_ADDR		    0x52

/**\name DATA RATE DEFINITIONS  */
#define BMM150_DATA_RATE_10HZ        (0x00)
#define BMM150_DATA_RATE_02HZ        (0x01)
#define BMM150_DATA_RATE_06HZ        (0x02)
#define BMM150_DATA_RATE_08HZ        (0x03)
#define BMM150_DATA_RATE_15HZ        (0x04)
#define BMM150_DATA_RATE_20HZ        (0x05)
#define BMM150_DATA_RATE_25HZ        (0x06)
#define BMM150_DATA_RATE_30HZ        (0x07)

/**\name TRIM REGISTERS      */
/* Trim Extended Registers */
#define BMM150_DIG_X1               UINT8_C(0x5D)
#define BMM150_DIG_Y1               UINT8_C(0x5E)
#define BMM150_DIG_Z4_LSB           UINT8_C(0x62)
#define BMM150_DIG_Z4_MSB           UINT8_C(0x63)
#define BMM150_DIG_X2               UINT8_C(0x64)
#define BMM150_DIG_Y2               UINT8_C(0x65)
#define BMM150_DIG_Z2_LSB           UINT8_C(0x68)
#define BMM150_DIG_Z2_MSB           UINT8_C(0x69)
#define BMM150_DIG_Z1_LSB           UINT8_C(0x6A)
#define BMM150_DIG_Z1_MSB           UINT8_C(0x6B)
#define BMM150_DIG_XYZ1_LSB         UINT8_C(0x6C)
#define BMM150_DIG_XYZ1_MSB         UINT8_C(0x6D)
#define BMM150_DIG_Z3_LSB           UINT8_C(0x6E)
#define BMM150_DIG_Z3_MSB           UINT8_C(0x6F)
#define BMM150_DIG_XY2              UINT8_C(0x70)
#define BMM150_DIG_XY1              UINT8_C(0x71)

/**\name PRESET MODES - REPETITIONS-XY RATES */
#define BMM150_LOWPOWER_REPXY                    (1)
#define BMM150_REGULAR_REPXY                     (4)
#define BMM150_ENHANCED_REPXY                    (7)
#define BMM150_HIGHACCURACY_REPXY                (23)

/**\name PRESET MODES - REPETITIONS-Z RATES */
#define BMM150_LOWPOWER_REPZ                     (2)
#define BMM150_REGULAR_REPZ                      (14)
#define BMM150_ENHANCED_REPZ                     (26)
#define BMM150_HIGHACCURACY_REPZ                 (82)

/**\name Macros for bit masking */
#define	BMM150_PWR_CNTRL_MSK		(0x01)

#define	BMM150_CONTROL_MEASURE_MSK	(0x38)
#define	BMM150_CONTROL_MEASURE_POS	(0x03)

#define BMM150_POWER_CONTROL_BIT_MSK	(0x01)
#define BMM150_POWER_CONTROL_BIT_POS	(0x00)

#define BMM150_OP_MODE_MSK		(0x06)
#define BMM150_OP_MODE_POS		(0x01)

#define BMM150_ODR_MSK			(0x38)
#define BMM150_ODR_POS			(0x03)

#define BMM150_DATA_X_MSK		(0xF8)
#define BMM150_DATA_X_POS		(0x03)

#define BMM150_DATA_Y_MSK		(0xF8)
#define BMM150_DATA_Y_POS		(0x03)

#define BMM150_DATA_Z_MSK		(0xFE)
#define BMM150_DATA_Z_POS		(0x01)

#define BMM150_DATA_RHALL_MSK		(0xFC)
#define BMM150_DATA_RHALL_POS		(0x02)

#define	BMM150_SELF_TEST_MSK		(0x01)

#define	BMM150_ADV_SELF_TEST_MSK	(0xC0)
#define	BMM150_ADV_SELF_TEST_POS	(0x06)

#define	BMM150_DRDY_EN_MSK		(0x80)
#define	BMM150_DRDY_EN_POS		(0x07)

#define	BMM150_INT_PIN_EN_MSK		(0x40)
#define	BMM150_INT_PIN_EN_POS		(0x06)

#define	BMM150_DRDY_POLARITY_MSK	(0x04)
#define	BMM150_DRDY_POLARITY_POS	(0x02)

#define	BMM150_INT_LATCH_MSK		(0x02)
#define	BMM150_INT_LATCH_POS		(0x01)

#define	BMM150_INT_POLARITY_MSK		(0x01)

#define	BMM150_DATA_OVERRUN_INT_MSK	(0x80)
#define	BMM150_DATA_OVERRUN_INT_POS	(0x07)

#define	BMM150_OVERFLOW_INT_MSK		(0x40)
#define	BMM150_OVERFLOW_INT_POS		(0x06)

#define	BMM150_HIGH_THRESHOLD_INT_MSK	(0x38)
#define	BMM150_HIGH_THRESHOLD_INT_POS	(0x03)

#define	BMM150_LOW_THRESHOLD_INT_MSK	(0x07)

#define	BMM150_DRDY_STATUS_MSK		(0x01)

/**\name OVERFLOW DEFINITIONS  */
#define BMM150_XYAXES_FLIP_OVERFLOW_ADCVAL	(-4096)
#define BMM150_ZAXIS_HALL_OVERFLOW_ADCVAL	  (-16384)
#define BMM150_OVERFLOW_OUTPUT			        (-32768)
#define BMM150_NEGATIVE_SATURATION_Z        (-32767)
#define BMM150_POSITIVE_SATURATION_Z        (32767)
#ifdef BMM150_USE_FLOATING_POINT
    #define BMM150_OVERFLOW_OUTPUT_FLOAT		0.0f
#endif

/**\name Register read lengths	*/
#define BMM150_SELF_TEST_LEN			(5)
#define BMM150_SETTING_DATA_LEN			(8)
#define BMM150_XYZR_DATA_LEN			(8)

/**\name Self test selection macros */
#define BMM150_NORMAL_SELF_TEST			(0)
#define BMM150_ADVANCED_SELF_TEST		(1)

/**\name Self test settings */
#define BMM150_DISABLE_XY_AXIS			(0x03)
#define BMM150_SELF_TEST_REP_Z			(0x04)

/**\name Advanced self-test current settings */
#define BMM150_DISABLE_SELF_TEST_CURRENT	(0x00)
#define BMM150_ENABLE_NEGATIVE_CURRENT		(0x02)
#define BMM150_ENABLE_POSITIVE_CURRENT		(0x03)

/**\name Normal self-test status */
#define BMM150_SELF_TEST_STATUS_XYZ_FAIL	(0x00)
#define BMM150_SELF_TEST_STATUS_SUCCESS		(0x07)

/**\name Macro to SET and GET BITS of a register*/
#define BMM150_SET_BITS(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     ((data << bitname##_POS) & bitname##_MSK))

#define BMM150_GET_BITS(reg_data, bitname)  ((reg_data & (bitname##_MSK)) >> \
        (bitname##_POS))

#define BMM150_SET_BITS_POS_0(reg_data, bitname, data) \
    ((reg_data & ~(bitname##_MSK)) | \
     (data & bitname##_MSK))

#define BMM150_GET_BITS_POS_0(reg_data, bitname)  (reg_data & (bitname##_MSK))


struct bmm150_mag_data {
    int16_t x;
    int16_t y;
    int16_t z;
};

/*
    @brief bmm150 un-compensated (raw) magnetometer data
*/
struct bmm150_raw_mag_data {
    /*! Raw mag X data */
    int16_t raw_datax;
    /*! Raw mag Y data */
    int16_t raw_datay;
    /*! Raw mag Z data */
    int16_t raw_dataz;
    /*! Raw mag resistance value */
    uint16_t raw_data_r;
};

/*!
    @brief bmm150 trim data structure
*/
struct bmm150_trim_registers {
    /*! trim x1 data */
    int8_t dig_x1;
    /*! trim y1 data */
    int8_t dig_y1;
    /*! trim x2 data */
    int8_t dig_x2;
    /*! trim y2 data */
    int8_t dig_y2;
    /*! trim z1 data */
    uint16_t dig_z1;
    /*! trim z2 data */
    int16_t dig_z2;
    /*! trim z3 data */
    int16_t dig_z3;
    /*! trim z4 data */
    int16_t dig_z4;
    /*! trim xy1 data */
    uint8_t dig_xy1;
    /*! trim xy2 data */
    int8_t dig_xy2;
    /*! trim xyz1 data */
    uint16_t dig_xyz1;
};

/**
    @brief bmm150 sensor settings
*/
struct bmm150_settings {
    /*! Control measurement of XYZ axes */
    uint8_t xyz_axes_control;
    /*! Power control bit value */
    uint8_t pwr_cntrl_bit;
    /*! Power control bit value */
    uint8_t pwr_mode;
    /*! Data rate value (ODR) */
    uint8_t data_rate;
    /*! XY Repetitions */
    uint8_t xy_rep;
    /*! Z Repetitions */
    uint8_t z_rep;
    /*! Preset mode of sensor */
    uint8_t preset_mode;
    /*! Interrupt configuration settings */
    // struct bmm150_int_ctrl_settings int_settings;
};

//TwoWire I2C = TwoWire();

class BMM150 {

  public:
    BMM150();

    int8_t initialize(void);
    void read_mag_data();
    int16_t compensate_x(int16_t mag_data_z, uint16_t data_rhall);
    int16_t compensate_y(int16_t mag_data_z, uint16_t data_rhall);
    int16_t compensate_z(int16_t mag_data_z, uint16_t data_rhall);
    void set_op_mode(uint8_t op_mode);
    void read_trim_registers();
    void write_op_mode(uint8_t op_mode);
    void set_preset_mode(uint8_t mode);
    void set_power_control_bit(uint8_t pwrcntrl_bit);
    void suspend_to_sleep_mode();
    void set_presetmode(uint8_t preset_mode);
    void set_odr_xyz_rep(struct bmm150_settings settings);
    void set_xy_rep(struct bmm150_settings settings);
    void set_z_rep(struct bmm150_settings settings);
    void set_odr(struct bmm150_settings settings);
    void soft_reset();
	
    // protected:
    struct bmm150_settings settings;
    struct bmm150_raw_mag_data raw_mag_data;
    struct bmm150_mag_data mag_data;
    struct bmm150_trim_registers trim_data;
	
    void i2c_write(short address, short byte);
    void i2c_read(short address, uint8_t* buffer, short length);
    void i2c_read(short address, int8_t* buffer, short length);
    uint8_t i2c_read(short address);
};

BMM150::BMM150() {
}

int8_t BMM150::initialize(void) {
    
	//Wire.begin();
	Wire.begin(I2C_SDA , I2C_SCL); 

    /* Power up the sensor from suspend to sleep mode */
    set_op_mode(BMM150_SLEEP_MODE);
    DELAY(BMM150_START_UP_TIME);

    /* Check chip ID */
    uint8_t id = i2c_read(BMM150_CHIP_ID_ADDR);
    if (id != BMM150_CHIP_ID) {
        return BMM150_E_ID_NOT_CONFORM;
    }

    /* Function to update trim values */
    read_trim_registers();

    /* Setting the power mode as normal */
    set_op_mode(BMM150_NORMAL_MODE);

    /*  Setting the preset mode as Low power mode
        i.e. data rate = 10Hz XY-rep = 1 Z-rep = 2*/
    set_presetmode(BMM150_PRESETMODE_LOWPOWER);
    // set_presetmode(BMM150_HIGHACCURACY_REPZ);

    return BMM150_OK;
}

void BMM150::read_mag_data() {
    int16_t msb_data;
    int8_t reg_data[BMM150_XYZR_DATA_LEN] = {0};

    i2c_read(BMM150_DATA_X_LSB, reg_data, BMM150_XYZR_DATA_LEN);

    /* Mag X axis data */
    reg_data[0] = BMM150_GET_BITS(reg_data[0], BMM150_DATA_X);
    /* Shift the MSB data to left by 5 bits */
    /* Multiply by 32 to get the shift left by 5 value */
    msb_data = ((int16_t)((int8_t)reg_data[1])) * 32;
    /* Raw mag X axis data */
    raw_mag_data.raw_datax = (int16_t)(msb_data | reg_data[0]);
    /* Mag Y axis data */
    reg_data[2] = BMM150_GET_BITS(reg_data[2], BMM150_DATA_Y);
    /* Shift the MSB data to left by 5 bits */
    /* Multiply by 32 to get the shift left by 5 value */
    msb_data = ((int16_t)((int8_t)reg_data[3])) * 32;
    /* Raw mag Y axis data */
    raw_mag_data.raw_datay = (int16_t)(msb_data | reg_data[2]);
    /* Mag Z axis data */
    reg_data[4] = BMM150_GET_BITS(reg_data[4], BMM150_DATA_Z);
    /* Shift the MSB data to left by 7 bits */
    /* Multiply by 128 to get the shift left by 7 value */
    msb_data = ((int16_t)((int8_t)reg_data[5])) * 128;
    /* Raw mag Z axis data */
    raw_mag_data.raw_dataz = (int16_t)(msb_data | reg_data[4]);
    /* Mag R-HALL data */
    reg_data[6] = BMM150_GET_BITS(reg_data[6], BMM150_DATA_RHALL);
    raw_mag_data.raw_data_r = (uint16_t)(((uint16_t)reg_data[7] << 6) | reg_data[6]);

    /* Compensated Mag X data in int16_t format */
    mag_data.x = compensate_x(raw_mag_data.raw_datax, raw_mag_data.raw_data_r);
    /* Compensated Mag Y data in int16_t format */
    mag_data.y = compensate_y(raw_mag_data.raw_datay, raw_mag_data.raw_data_r);
    /* Compensated Mag Z data in int16_t format */
    mag_data.z = compensate_z(raw_mag_data.raw_dataz, raw_mag_data.raw_data_r);
}

int16_t BMM150::compensate_x(int16_t mag_data_x, uint16_t data_rhall) {
    int16_t retval;
    uint16_t process_comp_x0 = 0;
    int32_t process_comp_x1;
    uint16_t process_comp_x2;
    int32_t process_comp_x3;
    int32_t process_comp_x4;
    int32_t process_comp_x5;
    int32_t process_comp_x6;
    int32_t process_comp_x7;
    int32_t process_comp_x8;
    int32_t process_comp_x9;
    int32_t process_comp_x10;

    /* Overflow condition check */
    if (mag_data_x != BMM150_XYAXES_FLIP_OVERFLOW_ADCVAL) {
        if (data_rhall != 0) {
            /* Availability of valid data*/
            process_comp_x0 = data_rhall;
        } else if (trim_data.dig_xyz1 != 0) {
            process_comp_x0 = trim_data.dig_xyz1;
        } else {
            process_comp_x0 = 0;
        }
        if (process_comp_x0 != 0) {
            /* Processing compensation equations*/
            process_comp_x1 = ((int32_t)trim_data.dig_xyz1) * 16384;
            process_comp_x2 = ((uint16_t)(process_comp_x1 / process_comp_x0)) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_x2);
            process_comp_x3 = (((int32_t)retval) * ((int32_t)retval));
            process_comp_x4 = (((int32_t)trim_data.dig_xy2) * (process_comp_x3 / 128));
            process_comp_x5 = (int32_t)(((int16_t)trim_data.dig_xy1) * 128);
            process_comp_x6 = ((int32_t)retval) * process_comp_x5;
            process_comp_x7 = (((process_comp_x4 + process_comp_x6) / 512) + ((int32_t)0x100000));
            process_comp_x8 = ((int32_t)(((int16_t)trim_data.dig_x2) + ((int16_t)0xA0)));
            process_comp_x9 = ((process_comp_x7 * process_comp_x8) / 4096);
            process_comp_x10 = ((int32_t)mag_data_x) * process_comp_x9;
            retval = ((int16_t)(process_comp_x10 / 8192));
            retval = (retval + (((int16_t)trim_data.dig_x1) * 8)) / 16;
        } else {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    } else {
        /* Overflow condition */
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return retval;
}

int16_t BMM150::compensate_y(int16_t mag_data_y, uint16_t data_rhall) {
    int16_t retval;
    uint16_t process_comp_y0 = 0;
    int32_t process_comp_y1;
    uint16_t process_comp_y2;
    int32_t process_comp_y3;
    int32_t process_comp_y4;
    int32_t process_comp_y5;
    int32_t process_comp_y6;
    int32_t process_comp_y7;
    int32_t process_comp_y8;
    int32_t process_comp_y9;

    /* Overflow condition check */
    if (mag_data_y != BMM150_XYAXES_FLIP_OVERFLOW_ADCVAL) {
        if (data_rhall != 0) {
            /* Availability of valid data*/
            process_comp_y0 = data_rhall;
        } else if (trim_data.dig_xyz1 != 0) {
            process_comp_y0 = trim_data.dig_xyz1;
        } else {
            process_comp_y0 = 0;
        }
        if (process_comp_y0 != 0) {
            /*Processing compensation equations*/
            process_comp_y1 = (((int32_t)trim_data.dig_xyz1) * 16384) / process_comp_y0;
            process_comp_y2 = ((uint16_t)process_comp_y1) - ((uint16_t)0x4000);
            retval = ((int16_t)process_comp_y2);
            process_comp_y3 = ((int32_t) retval) * ((int32_t)retval);
            process_comp_y4 = ((int32_t)trim_data.dig_xy2) * (process_comp_y3 / 128);
            process_comp_y5 = ((int32_t)(((int16_t)trim_data.dig_xy1) * 128));
            process_comp_y6 = ((process_comp_y4 + (((int32_t)retval) * process_comp_y5)) / 512);
            process_comp_y7 = ((int32_t)(((int16_t)trim_data.dig_y2) + ((int16_t)0xA0)));
            process_comp_y8 = (((process_comp_y6 + ((int32_t)0x100000)) * process_comp_y7) / 4096);
            process_comp_y9 = (((int32_t)mag_data_y) * process_comp_y8);
            retval = (int16_t)(process_comp_y9 / 8192);
            retval = (retval + (((int16_t)trim_data.dig_y1) * 8)) / 16;
        } else {
            retval = BMM150_OVERFLOW_OUTPUT;
        }
    } else {
        /* Overflow condition*/
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return retval;
}

int16_t BMM150::compensate_z(int16_t mag_data_z, uint16_t data_rhall) {
    int32_t retval;
    int16_t process_comp_z0;
    int32_t process_comp_z1;
    int32_t process_comp_z2;
    int32_t process_comp_z3;
    int16_t process_comp_z4;

    if (mag_data_z != BMM150_ZAXIS_HALL_OVERFLOW_ADCVAL) {
        if ((trim_data.dig_z2 != 0) && (trim_data.dig_z1 != 0)
                && (data_rhall != 0) && (trim_data.dig_xyz1 != 0)) {
            /*Processing compensation equations*/
            process_comp_z0 = ((int16_t)data_rhall) - ((int16_t) trim_data.dig_xyz1);
            process_comp_z1 = (((int32_t)trim_data.dig_z3) * ((int32_t)(process_comp_z0))) / 4;
            process_comp_z2 = (((int32_t)(mag_data_z - trim_data.dig_z4)) * 32768);
            process_comp_z3 = ((int32_t)trim_data.dig_z1) * (((int16_t)data_rhall) * 2);
            process_comp_z4 = (int16_t)((process_comp_z3 + (32768)) / 65536);
            retval = ((process_comp_z2 - process_comp_z1) / (trim_data.dig_z2 + process_comp_z4));

            /* saturate result to +/- 2 micro-tesla */
            if (retval > BMM150_POSITIVE_SATURATION_Z) {
                retval =  BMM150_POSITIVE_SATURATION_Z;
            } else {
                if (retval < BMM150_NEGATIVE_SATURATION_Z) {
                    retval = BMM150_NEGATIVE_SATURATION_Z;
                }
            }
            /* Conversion of LSB to micro-tesla*/
            retval = retval / 16;
        } else {
            retval = BMM150_OVERFLOW_OUTPUT;

        }
    } else {
        /* Overflow condition*/
        retval = BMM150_OVERFLOW_OUTPUT;
    }

    return (int16_t)retval;
}

void BMM150::set_presetmode(uint8_t preset_mode) {
    switch (preset_mode) {
        case BMM150_PRESETMODE_LOWPOWER:
            /*  Set the data rate x,y,z repetition
                for Low Power mode */
            settings.data_rate = BMM150_DATA_RATE_10HZ;
            settings.xy_rep = BMM150_LOWPOWER_REPXY;
            settings.z_rep = BMM150_LOWPOWER_REPZ;
            set_odr_xyz_rep(settings);
            break;
        case BMM150_PRESETMODE_REGULAR:
            /*  Set the data rate x,y,z repetition
                for Regular mode */
            settings.data_rate = BMM150_DATA_RATE_10HZ;
            settings.xy_rep = BMM150_REGULAR_REPXY;
            settings.z_rep = BMM150_REGULAR_REPZ;
            set_odr_xyz_rep(settings);
            break;
        case BMM150_PRESETMODE_HIGHACCURACY:
            /*  Set the data rate x,y,z repetition
                for High Accuracy mode */
            settings.data_rate = BMM150_DATA_RATE_20HZ;
            settings.xy_rep = BMM150_HIGHACCURACY_REPXY;
            settings.z_rep = BMM150_HIGHACCURACY_REPZ;
            set_odr_xyz_rep(settings);
            break;
        case BMM150_PRESETMODE_ENHANCED:
            /*  Set the data rate x,y,z repetition
                for Enhanced Accuracy mode */
            settings.data_rate = BMM150_DATA_RATE_10HZ;
            settings.xy_rep = BMM150_ENHANCED_REPXY;
            settings.z_rep = BMM150_ENHANCED_REPZ;
            set_odr_xyz_rep(settings);
            break;
        default:
            break;
    }
}

void BMM150::set_odr_xyz_rep(struct bmm150_settings settings) {
    /* Set the ODR */
    set_odr(settings);
    /* Set the XY-repetitions number */
    set_xy_rep(settings);
    /* Set the Z-repetitions number */
    set_z_rep(settings);
}

void BMM150::set_xy_rep(struct bmm150_settings settings) {
    uint8_t rep_xy;
    rep_xy = settings.xy_rep;
    i2c_write(BMM150_REP_XY_ADDR, rep_xy);

}

void BMM150::set_z_rep(struct bmm150_settings settings) {
    uint8_t rep_z;
    rep_z = settings.z_rep;
    i2c_write(BMM150_REP_Z_ADDR, rep_z);
}


void BMM150::soft_reset() {
    uint8_t reg_data;

    reg_data = i2c_read(BMM150_POWER_CONTROL_ADDR);
    reg_data = reg_data | BMM150_SET_SOFT_RESET;
    i2c_write(BMM150_POWER_CONTROL_ADDR, reg_data);
    DELAY(BMM150_SOFT_RESET_DELAY);
}


void BMM150::set_odr(struct bmm150_settings settings) {
    uint8_t reg_data;

    reg_data = i2c_read(BMM150_OP_MODE_ADDR);
    /*Set the ODR value */
    reg_data = BMM150_SET_BITS(reg_data, BMM150_ODR, settings.data_rate);
    i2c_write(BMM150_OP_MODE_ADDR, reg_data);
}

void BMM150::i2c_write(short address, short data) {
    Wire.beginTransmission(BMM150_I2C_Address);
    Wire.write(address);
    Wire.write(data);
    Wire.endTransmission();
}

void BMM150::i2c_read(short address, uint8_t* buffer, short length) {
    Wire.beginTransmission(BMM150_I2C_Address);
    Wire.write(address);
    Wire.endTransmission();
    Wire.requestFrom(BMM150_I2C_Address, length);

    if (Wire.available() == length) {
        for (uint8_t i = 0; i < length; i++) {
            buffer[i] = Wire.read();
        }
    }
}


void BMM150::i2c_read(short address, int8_t* buffer, short length) {
    Wire.beginTransmission(BMM150_I2C_Address);
    Wire.write(address);
    Wire.endTransmission();
    Wire.requestFrom(BMM150_I2C_Address, length);

    if (Wire.available() == length) {
        for (uint8_t i = 0; i < length; i++) {
            buffer[i] = Wire.read();
        }
    }
}

uint8_t BMM150::i2c_read(short address) {
    uint8_t byte;

    Wire.beginTransmission(BMM150_I2C_Address);
    Wire.write(address);
    Wire.endTransmission();
    Wire.requestFrom(BMM150_I2C_Address, 1);
    byte = Wire.read();
    return byte;
}

void BMM150::set_op_mode(uint8_t pwr_mode) {
    /* Select the power mode to set */
    switch (pwr_mode) {
        case BMM150_NORMAL_MODE:
            /*  If the sensor is in suspend mode
                put the device to sleep mode */
            suspend_to_sleep_mode();
            /* write the op mode */
            write_op_mode(pwr_mode);
            break;
        case BMM150_FORCED_MODE:
            /*  If the sensor is in suspend mode
                put the device to sleep mode */
            suspend_to_sleep_mode();
            /* write the op mode */
            write_op_mode(pwr_mode);
            break;
        case BMM150_SLEEP_MODE:
            /*  If the sensor is in suspend mode
                put the device to sleep mode */
            suspend_to_sleep_mode();
            /* write the op mode */
            write_op_mode(pwr_mode);
            break;
        case BMM150_SUSPEND_MODE:
            /* Set the power control bit to zero */
            set_power_control_bit(BMM150_POWER_CNTRL_DISABLE);
            break;
        default:
            break;
    }
}

void BMM150::suspend_to_sleep_mode(void) {
    set_power_control_bit(BMM150_POWER_CNTRL_ENABLE);
    /* Start-up time delay of 3ms*/
    DELAY(3);
}


void BMM150::read_trim_registers() {
    uint8_t trim_x1y1[2] = {0};
    uint8_t trim_xyz_data[4] = {0};
    uint8_t trim_xy1xy2[10] = {0};
    uint16_t temp_msb = 0;

    /* Trim register value is read */
    i2c_read(BMM150_DIG_X1, trim_x1y1, 2);
    i2c_read(BMM150_DIG_Z4_LSB, trim_xyz_data, 4);
    i2c_read(BMM150_DIG_Z2_LSB, trim_xy1xy2, 10);
    /*  Trim data which is read is updated
        in the device structure */
    trim_data.dig_x1 = (int8_t)trim_x1y1[0];
    trim_data.dig_y1 = (int8_t)trim_x1y1[1];
    trim_data.dig_x2 = (int8_t)trim_xyz_data[2];
    trim_data.dig_y2 = (int8_t)trim_xyz_data[3];
    temp_msb = ((uint16_t)trim_xy1xy2[3]) << 8;
    trim_data.dig_z1 = (uint16_t)(temp_msb | trim_xy1xy2[2]);
    temp_msb = ((uint16_t)trim_xy1xy2[1]) << 8;
    trim_data.dig_z2 = (int16_t)(temp_msb | trim_xy1xy2[0]);
    temp_msb = ((uint16_t)trim_xy1xy2[7]) << 8;
    trim_data.dig_z3 = (int16_t)(temp_msb | trim_xy1xy2[6]);
    temp_msb = ((uint16_t)trim_xyz_data[1]) << 8;
    trim_data.dig_z4 = (int16_t)(temp_msb | trim_xyz_data[0]);
    trim_data.dig_xy1 = trim_xy1xy2[9];
    trim_data.dig_xy2 = (int8_t)trim_xy1xy2[8];
    temp_msb = ((uint16_t)(trim_xy1xy2[5] & 0x7F)) << 8;
    trim_data.dig_xyz1 = (uint16_t)(temp_msb | trim_xy1xy2[4]);

}

void BMM150::write_op_mode(uint8_t op_mode) {
    uint8_t reg_data = 0;
    reg_data = i2c_read(BMM150_OP_MODE_ADDR);
    /* Set the op_mode value in Opmode bits of 0x4C */
    reg_data = BMM150_SET_BITS(reg_data, BMM150_OP_MODE, op_mode);
    i2c_write(BMM150_OP_MODE_ADDR, reg_data);
}

void BMM150::set_power_control_bit(uint8_t pwrcntrl_bit) {
    uint8_t reg_data = 0;
    /* Power control register 0x4B is read */
    reg_data = i2c_read(BMM150_POWER_CONTROL_ADDR);
    /* Sets the value of power control bit */
    reg_data = BMM150_SET_BITS_POS_0(reg_data, BMM150_PWR_CNTRL, pwrcntrl_bit);
    i2c_write(BMM150_POWER_CONTROL_ADDR, reg_data);
}


class BMM150CustomSensor : public PollingComponent, public Sensor 
{
 public:
  
  BMM150 bmm;
  
  Sensor *heading_sensor = new Sensor();
  
  BMM150CustomSensor() : PollingComponent(180) {}   //170
  void setup() override 
  {
    bmm.initialize();	
    Serial.begin(9600);

  }
  void update() override 
  {
    bmm150_mag_data value;
    
	bmm.read_mag_data();

    value.x = bmm.raw_mag_data.raw_datax;
    value.y = bmm.raw_mag_data.raw_datay;
    value.z = bmm.raw_mag_data.raw_dataz;

    float heading = atan2(value.x, value.y);

    if (heading < 0) {
        heading += 2 * PI;
    }
    if (heading > 2 * PI) {
        heading -= 2 * PI;
    }
    float headingDegrees = heading * 180 / M_PI;
  	
	heading_sensor->publish_state(headingDegrees);
		
	//delay(1000);

	}
  };
